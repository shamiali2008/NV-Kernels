// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright(c) 2019 Huawei Technologies Co., Ltd
 * Author: Zengruan Ye <yezengruan@huawei.com>
 */

#include <linux/spinlock.h>
#include <asm/paravirt.h>

#ifdef CONFIG_PARAVIRT_SPINLOCKS
bool pv_is_native_spin_unlock(void)
{
	return pv_ops.lock.queued_spin_unlock == __native_queued_spin_unlock;
}
#endif /* CONFIG_PARAVIRT_SPINLOCKS */
