// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gw.org>
 */

#ifndef GNUWEEB__RING_H
#define GNUWEEB__RING_H

#include <gw/common.h>
#include <gw/workqueue.h>
#include <gw/thread.h>
#include <gw/lib/tgapi.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>

enum {
	GW_RING_OP_NOP = 0,
	GW_RING_OP_TG_API_CALL = 1,
	GW_RING_OP_MODULE_HANDLE = 2,
};

enum {
	TG_API_GET_UPDATES = 0,
};

struct tg_api_call {
	uint8_t			op;
	struct tg_api_ctx	*ctx;
	union {
		struct {
			struct tg_updates	**updates_p;
			int64_t			 offset;
		};
	};
};

struct tg_module_handle {
	struct tg_bot_ctx	*ctx;
	struct tg_update	*update;
};

struct gw_ring_sqe {
	uint8_t		op;
	uint64_t	user_data;
	union {
		struct tg_api_call	tg_api_call;
		struct tg_module_handle	tg_module_handle;
	};
};

struct gw_ring_cqe {
	uint8_t		op;
	int64_t		res;
	uint32_t	flags;
	uint64_t	user_data;
};

struct gw_ring {
	volatile bool		should_stop;
	volatile bool		wait_cqe_cond_flag;

	_Atomic(uint32_t)	sq_head;
	_Atomic(uint32_t)	sq_tail;
	uint32_t		sq_mask;
	mutex_t			sq_lock;

	_Atomic(uint32_t)	cq_head;
	_Atomic(uint32_t)	cq_tail;
	uint32_t		cq_mask;
	mutex_t			cq_lock;

	cond_t			wait_cqe_cond;

	struct gw_ring_cqe	*cqes;
	struct gw_ring_sqe	*sqes;
	struct workqueue_struct	*wq;
};

int gw_ring_init(struct gw_ring *ring, uint32_t size);
void gw_ring_destroy(struct gw_ring *ring);
int gw_ring_submit(struct gw_ring *ring);
struct gw_ring_sqe *gw_ring_get_sqe(struct gw_ring *ring);
int gw_ring_wait_cqe(struct gw_ring *ring, struct gw_ring_cqe **cqe_p);

static inline void gw_ring_prep_tg_get_updates(struct gw_ring_sqe *sqe,
					       struct tg_api_ctx *ctx,
					       struct tg_updates **updates_p,
					       int64_t offset)
{
	struct tg_api_call *call = &sqe->tg_api_call;

	sqe->op = GW_RING_OP_TG_API_CALL;
	call->op = TG_API_GET_UPDATES;
	call->ctx = ctx;
	call->updates_p = updates_p;
	call->offset = offset;
}

static inline void gw_ring_prep_tg_module_handle(struct gw_ring_sqe *sqe,
						 struct tg_bot_ctx *ctx,
						 struct tg_update *update)
{
	struct tg_module_handle *handle = &sqe->tg_module_handle;

	sqe->op = GW_RING_OP_MODULE_HANDLE;
	handle->ctx = ctx;
	handle->update = update;
}

static inline uint32_t u32_diff(uint32_t a, uint32_t b)
{
	return (uint32_t)llabs((int64_t)a - (int64_t)b);
}

static inline void gw_ring_cq_advance(struct gw_ring *ring, uint32_t n)
{
	atomic_fetch_add_explicit(&ring->cq_head, n, memory_order_release);
}

static inline struct gw_ring_cqe *gw_ring_load_head_cqe(struct gw_ring *ring,
							uint32_t head)
{
	uint32_t tail = smp_load_acquire(&ring->cq_tail);
	uint32_t cq_mask = ring->cq_mask;
	struct gw_ring_cqe *cqe;

	if (head != tail)
		cqe = &ring->cqes[head & cq_mask];
	else
		cqe = NULL;

	return cqe;
}

#define gw_ring_for_each_cqe(ring, head, cqe)		\
for (head = smp_load_acquire(&(ring)->cq_head);		\
     (cqe = gw_ring_load_head_cqe(ring, head)) != NULL;	\
     head++)

#endif /* #ifndef GNUWEEB__RING_H */
