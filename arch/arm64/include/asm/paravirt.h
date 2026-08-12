/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_ARM64_PARAVIRT_H
#define _ASM_ARM64_PARAVIRT_H

#ifdef CONFIG_PARAVIRT

int __init pv_time_init(void);
int __init pv_lock_init(void);

#if defined(CONFIG_SMP) && defined(CONFIG_PARAVIRT_SPINLOCKS)
int __init pv_qspinlock_init(void);
bool pv_is_native_spin_unlock(void);

#include <asm/paravirt_types.h>

static inline void pv_queued_spin_lock_slowpath(struct qspinlock *lock, u32 val)
{
	pv_ops.lock.queued_spin_lock_slowpath(lock, val);
}

static inline void pv_queued_spin_unlock(struct qspinlock *lock)
{
	pv_ops.lock.queued_spin_unlock(lock);
}

static inline void pv_wait(u8 *ptr, u8 val)
{
	pv_ops.lock.wait(ptr, val);
}

static inline void pv_kick(int cpu)
{
	pv_ops.lock.kick(cpu);
}
#else
#define pv_qspinlock_init() do {} while (0)
#endif /* CONFIG_SMP && CONFIG_PARAVIRT_SPINLOCKS */

#else

#define pv_time_init() do {} while (0)
#define pv_lock_init() do {} while (0)
#define pv_qspinlock_init() do {} while (0)

#endif // CONFIG_PARAVIRT

#endif
