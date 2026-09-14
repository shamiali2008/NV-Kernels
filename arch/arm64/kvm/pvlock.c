/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright(c) 2019 Huawei Technologies Co., Ltd
 * Author: Zengruan Ye <yezengruan@huawei.com>
 *         Usama Arif <usama.arif@bytedance.com>
 */

#include <linux/arm-smccc.h>
#include <linux/kvm_host.h>

#include <asm/pvlock-abi.h>

#include <kvm/arm_hypercalls.h>

gpa_t kvm_init_pvlock(struct kvm_vcpu *vcpu)
{
	struct pvlock_vcpu_state init_values = {};
	struct kvm *kvm = vcpu->kvm;
	u64 base = vcpu->arch.pv_lock.base;
	int idx;

	if (base == INVALID_GPA)
		return base;

	idx = srcu_read_lock(&kvm->srcu);
	kvm_write_guest(kvm, base, &init_values, sizeof(init_values));
	srcu_read_unlock(&kvm->srcu, idx);

	return base;
}

void kvm_update_pvlock_preempted(struct kvm_vcpu *vcpu, u64 preempted)
{
	int idx;
	u64 offset;
	struct kvm *kvm = vcpu->kvm;
	u64 base = vcpu->arch.pv_lock.base;

	idx = srcu_read_lock(&kvm->srcu);
	offset = offsetof(struct pvlock_vcpu_state, preempted);
	kvm_put_guest(kvm, base + offset, cpu_to_le64(preempted));
	srcu_read_unlock(&kvm->srcu, idx);
}
