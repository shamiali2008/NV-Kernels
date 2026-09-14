/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright(c) 2019 Huawei Technologies Co., Ltd
 * Author: Zengruan Ye <yezengruan@huawei.com>
 *         Usama Arif <usama.arif@bytedance.com>
 */

#ifndef __ASM_PVLOCK_ABI_H
#define __ASM_PVLOCK_ABI_H

struct pvlock_vcpu_state {
	__le64 preempted;
} __aligned(64);

#endif
