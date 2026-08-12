/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_ARM64_QSPINLOCK_H
#define _ASM_ARM64_QSPINLOCK_H

#ifdef CONFIG_PARAVIRT_SPINLOCKS
#include <linux/jump_label.h>
#include <asm-generic/qspinlock_types.h>
#include <asm/paravirt.h>

extern void native_queued_spin_lock_slowpath(struct qspinlock *lock, u32 val);
extern void __pv_init_lock_hash(void);
extern void __pv_queued_spin_lock_slowpath(struct qspinlock *lock, u32 val);
extern bool nopvspin;

#define	queued_spin_unlock queued_spin_unlock
/**
 * queued_spin_unlock - release a queued spinlock
 * @lock : Pointer to queued spinlock structure
 *
 * A smp_store_release() on the least-significant byte.
 */
static inline void native_queued_spin_unlock(struct qspinlock *lock)
{
	smp_store_release(&lock->locked, 0);
}

static inline void queued_spin_lock_slowpath(struct qspinlock *lock, u32 val)
{
	pv_queued_spin_lock_slowpath(lock, val);
}

static inline void queued_spin_unlock(struct qspinlock *lock)
{
	pv_queued_spin_unlock(lock);
}
#endif /* CONFIG_PARAVIRT_SPINLOCKS */

#include <asm-generic/qspinlock.h>

#endif /* _ASM_ARM64_QSPINLOCK_H */
