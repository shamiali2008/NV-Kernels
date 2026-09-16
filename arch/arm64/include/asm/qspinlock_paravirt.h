/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright(c) 2026 Huawei Technologies Co., Ltd
 * Author: Jia Qingtong <jiaqingtong@huawei.com>
 */

#ifndef __ASM_QSPINLOCK_PARAVIRT_H
#define __ASM_QSPINLOCK_PARAVIRT_H

void __lockfunc __pv_queued_spin_unlock_slowpath(struct qspinlock *lock, u8 locked);
extern void __pv_queued_spin_unlock(struct qspinlock *lock);

#endif
