// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * Copyright (C) 2013 Citrix Systems
 *
 * Author: Stefano Stabellini <stefano.stabellini@eu.citrix.com>
 */

#define pr_fmt(fmt) "arm-pv: " fmt

#include <linux/arm-smccc.h>
#include <linux/cpuhotplug.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/jump_label.h>
#include <linux/printk.h>
#include <linux/psci.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/static_call.h>
#include <linux/sched/cputime.h>

#include <asm/hypervisor.h>
#include <asm/paravirt.h>
#include <asm/pvclock-abi.h>
#include <asm/pvlock-abi.h>
#include <asm/qspinlock_paravirt.h>
#include <asm/smp_plat.h>

struct pv_time_stolen_time_region {
	struct pvclock_vcpu_stolen_time __rcu *kaddr;
};

struct pv_lock_state_region {
	struct pvlock_vcpu_state __rcu *kaddr;
};

static DEFINE_PER_CPU(struct pv_time_stolen_time_region, stolen_time_region);
static DEFINE_PER_CPU(struct pv_lock_state_region, lock_state_region);

static bool steal_acc = true;
static int __init parse_no_stealacc(char *arg)
{
	steal_acc = false;
	return 0;
}

early_param("no-steal-acc", parse_no_stealacc);

/* return stolen time in ns by asking the hypervisor */
static u64 para_steal_clock(int cpu)
{
	struct pvclock_vcpu_stolen_time *kaddr = NULL;
	struct pv_time_stolen_time_region *reg;
	u64 ret = 0;

	reg = per_cpu_ptr(&stolen_time_region, cpu);

	/*
	 * paravirt_steal_clock() may be called before the CPU
	 * online notification callback runs. Until the callback
	 * has run we just return zero.
	 */
	rcu_read_lock();
	kaddr = rcu_dereference(reg->kaddr);
	if (!kaddr) {
		rcu_read_unlock();
		return 0;
	}

	ret = le64_to_cpu(READ_ONCE(kaddr->stolen_time));
	rcu_read_unlock();
	return ret;
}

static int stolen_time_cpu_down_prepare(unsigned int cpu)
{
	struct pvclock_vcpu_stolen_time *kaddr = NULL;
	struct pv_time_stolen_time_region *reg;

	reg = this_cpu_ptr(&stolen_time_region);
	if (!reg->kaddr)
		return 0;

	kaddr = rcu_replace_pointer(reg->kaddr, NULL, true);
	synchronize_rcu();
	memunmap(kaddr);

	return 0;
}

static int stolen_time_cpu_online(unsigned int cpu)
{
	struct pvclock_vcpu_stolen_time *kaddr = NULL;
	struct pv_time_stolen_time_region *reg;
	struct arm_smccc_res res;

	reg = this_cpu_ptr(&stolen_time_region);

	arm_smccc_1_1_invoke(ARM_SMCCC_HV_PV_TIME_ST, &res);

	if (res.a0 == SMCCC_RET_NOT_SUPPORTED)
		return -EINVAL;

	kaddr = memremap(res.a0,
			      sizeof(struct pvclock_vcpu_stolen_time),
			      MEMREMAP_WB);

	rcu_assign_pointer(reg->kaddr, kaddr);

	if (!reg->kaddr) {
		pr_warn("Failed to map stolen time data structure\n");
		return -ENOMEM;
	}

	if (le32_to_cpu(kaddr->revision) != 0 ||
	    le32_to_cpu(kaddr->attributes) != 0) {
		pr_warn_once("Unexpected revision or attributes in stolen time data\n");
		return -ENXIO;
	}

	return 0;
}

static int __init pv_time_init_stolen_time(void)
{
	int ret;

	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				"hypervisor/arm/pvtime:online",
				stolen_time_cpu_online,
				stolen_time_cpu_down_prepare);
	if (ret < 0)
		return ret;
	return 0;
}

static bool __init has_pv_steal_clock(void)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_invoke(ARM_SMCCC_ARCH_FEATURES_FUNC_ID,
			     ARM_SMCCC_HV_PV_TIME_FEATURES, &res);

	if (res.a0 != SMCCC_RET_SUCCESS)
		return false;

	arm_smccc_1_1_invoke(ARM_SMCCC_HV_PV_TIME_FEATURES,
			     ARM_SMCCC_HV_PV_TIME_ST, &res);

	return (res.a0 == SMCCC_RET_SUCCESS);
}

int __init pv_time_init(void)
{
	int ret;

	if (!has_pv_steal_clock())
		return 0;

	ret = pv_time_init_stolen_time();
	if (ret)
		return ret;

	static_call_update(pv_steal_clock, para_steal_clock);

	static_key_slow_inc(&paravirt_steal_enabled);
	if (steal_acc)
		static_key_slow_inc(&paravirt_steal_rq_enabled);

	pr_info("using stolen time PV\n");

	return 0;
}

static bool native_vcpu_is_preempted(int cpu)
{
	return false;
}

DEFINE_STATIC_CALL(pv_vcpu_is_preempted, native_vcpu_is_preempted);

static bool para_vcpu_is_preempted(int cpu)
{
	struct pv_lock_state_region *reg;
	__le64 preempted_le;

	reg = per_cpu_ptr(&lock_state_region, cpu);
	if (!reg->kaddr) {
		pr_warn_once("PV lock enabled but not configured for cpu %d\n",
			     cpu);
		return false;
	}

	preempted_le = le64_to_cpu(READ_ONCE(reg->kaddr->preempted));

	return !!(preempted_le);
}

static int pvlock_vcpu_state_dying_cpu(unsigned int cpu)
{
	struct pv_lock_state_region *reg;

	reg = this_cpu_ptr(&lock_state_region);
	if (!reg->kaddr)
		return 0;

	memunmap(reg->kaddr);
	memset(reg, 0, sizeof(*reg));

	return 0;
}

static int init_pvlock_vcpu_state(unsigned int cpu)
{
	struct pv_lock_state_region *reg;
	struct arm_smccc_res res;

	reg = this_cpu_ptr(&lock_state_region);

	arm_smccc_1_1_invoke(ARM_SMCCC_VENDOR_HYP_KVM_PV_LOCK_FUNC_ID, &res);

	if (res.a0 == SMCCC_RET_NOT_SUPPORTED) {
		pr_warn("Failed to init PV lock data structure\n");
		return -EINVAL;
	}

	reg->kaddr = memremap(res.a0,
			      sizeof(struct pvlock_vcpu_state),
			      MEMREMAP_WB);

	if (!reg->kaddr) {
		pr_warn("Failed to map PV lock data structure\n");
		return -ENOMEM;
	}

	return 0;
}

static int kvm_arm_init_pvlock(void)
{
	int ret;

	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				"hypervisor/arm/pvlock:online",
				init_pvlock_vcpu_state,
				pvlock_vcpu_state_dying_cpu);
	if (ret < 0) {
		pr_warn("PV-lock init failed\n");
		return ret;
	}

	return 0;
}

static bool has_kvm_pvlock(void)
{
	return kvm_arm_hyp_service_available(ARM_SMCCC_KVM_FUNC_PV_LOCK);
}

int __init pv_lock_init(void)
{
	int ret;

	if (is_hyp_mode_available())
		return 0;

	if (!has_kvm_pvlock())
		return 0;

	ret = kvm_arm_init_pvlock();
	if (ret)
		return ret;

	static_call_update(pv_vcpu_is_preempted, para_vcpu_is_preempted);
	pr_info("using PV-lock preempted\n");

	return 0;
}
early_initcall(pv_lock_init);

#ifdef CONFIG_PARAVIRT_SPINLOCKS
extern bool nopvspin;

static void kvm_kick_cpu(int cpu)
{
	struct arm_smccc_res res;

	arm_smccc_1_1_invoke(ARM_SMCCC_VENDOR_KICK_CPU, cpu, &res);

}

static void kvm_wait(u8 *ptr, u8 val)
{
	unsigned long flags;

	if (in_nmi())
		return;

	local_irq_save(flags);

	if (READ_ONCE(*ptr) != val)
		goto out;

	dsb(sy);
	wfi();

out:
	local_irq_restore(flags);
}

int __init pv_qspinlock_init(void)
{
	struct arm_smccc_res res;

	if (nopvspin) {
		pr_info("PV qspinlocks disabled, forced by \"nopvspin\" parameter\n");
		return 0;
	}

	/* Check if KICK_CPU is supported by hypervisor */
	arm_smccc_1_1_invoke(ARM_SMCCC_ARCH_FEATURES_FUNC_ID,
			     ARM_SMCCC_VENDOR_KICK_CPU, &res);
	if (res.a0 != SMCCC_RET_SUCCESS) {
		pr_info("PV qspinlocks disabled, no KICK_CPU support\n");
		return 0;
	}

	/* Don't use the PV qspinlock code if there is only 1 vCPU. */
	if (num_possible_cpus() == 1) {
		pr_info("PV qspinlocks disabled, single CPU\n");
		return 0;
	}
	pr_info("PV qspinlocks enabled\n");

	__pv_init_lock_hash();

	pv_ops.lock.queued_spin_lock_slowpath = __pv_queued_spin_lock_slowpath;
	pv_ops.lock.queued_spin_unlock = __pv_queued_spin_unlock;
	pv_ops.lock.wait = kvm_wait;
	pv_ops.lock.kick = kvm_kick_cpu;

	return 0;
}
early_initcall(pv_qspinlock_init);
#endif  /* CONFIG_PARAVIRT_SPINLOCKS */
