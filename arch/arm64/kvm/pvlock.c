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

int kvm_arm_pvlock_set_attr(struct kvm_vcpu *vcpu,
			    struct kvm_device_attr *attr)
{
	u64 __user *user = (u64 __user *)attr->addr;
	struct kvm *kvm = vcpu->kvm;
	u64 ipa;
	int ret = 0;
	int idx;

	if (attr->attr != KVM_ARM_VCPU_PVLOCK_IPA)
		return -ENXIO;

	if (get_user(ipa, user))
		return -EFAULT;
	if (!IS_ALIGNED(ipa, 64))
		return -EINVAL;
	if (vcpu->arch.pv_lock.base != INVALID_GPA)
		return -EEXIST;

	/* Check the address is in a valid memslot */
	idx = srcu_read_lock(&kvm->srcu);
	if (kvm_is_error_hva(gfn_to_hva(kvm, ipa >> PAGE_SHIFT)))
		ret = -EINVAL;
	srcu_read_unlock(&kvm->srcu, idx);

	if (!ret)
		vcpu->arch.pv_lock.base = ipa;

	return ret;
}

int kvm_arm_pvlock_get_attr(struct kvm_vcpu *vcpu,
			    struct kvm_device_attr *attr)
{
	u64 __user *user = (u64 __user *)attr->addr;
	u64 ipa;

	if (attr->attr != KVM_ARM_VCPU_PVLOCK_IPA)
		return -ENXIO;

	ipa = vcpu->arch.pv_lock.base;

	if (put_user(ipa, user))
		return -EFAULT;
	return 0;
}

int kvm_arm_pvlock_has_attr(struct kvm_vcpu *vcpu,
			    struct kvm_device_attr *attr)
{
	switch (attr->attr) {
	case KVM_ARM_VCPU_PVLOCK_IPA:
		return 0;
	}
	return -ENXIO;
}