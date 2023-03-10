// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#ifndef GNUWEEB__COMMON_H
#define GNUWEEB__COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif

#ifndef __no_inline
#define __no_inline	__attribute__((__noinline__))
#endif

#ifndef __always_inline
#define __always_inline	__attribute__((__always_inline__))
#endif

#ifndef __used
#define __used		__attribute__((__used__))
#endif

#ifndef __unused
#define __unused	__attribute__((__unused__))
#endif

#ifndef __maybe_unused
#define __maybe_unused	__attribute__((__unused__))
#endif

#ifndef __packed
#define __packed	__attribute__((__packed__))
#endif

#ifndef __aligned
#define __aligned(x)	__attribute__((__aligned__(x)))
#endif

#ifdef __CHECKER__
#define __must_hold(x)		__attribute__((__context__(x, 1, 1)))
#define __acquires(x)		__attribute__((__context__(x, 0, 1)))
#define __releases(x)		__attribute__((__context__(x, 1, 0)))
#else
#define __must_hold(x)
#define __acquires(x)
#define __releases(x)
#endif

#ifndef smp_load_acquire
#define smp_load_acquire(p) atomic_load_explicit(p, memory_order_acquire)
#endif

#ifndef smp_store_release
#define smp_store_release(p, v) atomic_store_explicit(p, v, memory_order_release)
#endif

#include <gw/ring.h>
struct tg_bot_ctx {
	struct gw_ring		ring;
	struct tg_updates	*updates;
	struct tg_api_ctx	tctx;
	int64_t			max_update_id;
};
#include <gw/print.h>

#endif /* #ifndef GNUWEEB__COMMON_H */
