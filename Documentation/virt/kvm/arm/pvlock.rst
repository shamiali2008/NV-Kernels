.. SPDX-License-Identifier: GPL-2.0

Paravirtualized lock support for arm64
======================================

KVM/arm64 provides a hypervisor service call for paravirtualized guests to
determine whether a VCPU is currently running or not.

A new SMCCC compatible hypercall is defined:

* ARM_SMCCC_VENDOR_HYP_KVM_PV_LOCK_FUNC_ID:   0xC6000002

ARM_SMCCC_VENDOR_HYP_KVM_PV_LOCK_FUNC_ID

    ============= ========    ==========================================
    Function ID:  (uint32)    0xC6000002
    Return value: (int64)     IPA of the pv lock data structure for this
                              VCPU. On failure:
                              NOT_SUPPORTED (-1)
    ============= ========    ==========================================

The IPA returned by PV_LOCK_PREEMPTED should be mapped by the guest as normal
memory with inner and outer write back caching attributes, in the inner
shareable domain.

PV_LOCK_PREEMPTED returns the structure for the calling VCPU.

PV lock state
-------------

The structure pointed to by the PV_LOCK_PREEMPTED hypercall is as follows:

+-----------+-------------+-------------+---------------------------------+
| Field     | Byte Length | Byte Offset | Description                     |
+===========+=============+=============+=================================+
| preempted |      8      |      0      | Used to indicate if the VCPU    |
|           |             |             | which owns this struct is       |
|           |             |             | running or not.                 |
|           |             |             | A non-zero value mean the VCPU  |
|           |             |             | has been scheduled out. A zero  |
|           |             |             | value means the VCPU has been   |
|           |             |             | scheduled in.                   |
+-----------+-------------+-------------+---------------------------------+

The preempted field will be updated to 0 by the hypervisor prior to scheduling
a VCPU. When the VCPU is scheduled out, the preempted field will be updated
to 1 by the hypervisor.

The structure will be present within a reserved region of the normal memory
given to the guest. The guest should not attempt to write into this memory.
There is a structure per VCPU of the guest.

For the user space interface see
:ref:`Documentation/virt/kvm/devices/vcpu.rst <kvm_arm_vcpu_pvlock_ctrl>`.