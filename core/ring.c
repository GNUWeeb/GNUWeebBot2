// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gw.org>
 */

#include <gw/module.h>
#include <gw/common.h>
#include <gw/ring.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct wq_sqe_data {
	struct gw_ring		*ring;
	struct gw_ring_sqe	sqe;
};

static void gw_ring_wq_sqe_exec(void *data);
static void gw_ring_wq_sqe_delete(void *data);
static bool punt_to_io_wq(struct gw_ring *ring, struct gw_ring_sqe *sqe);

int gw_ring_init(struct gw_ring *ring, uint32_t size)
{
	static const struct workqueue_attr attr = {
		.name = "gw-ring-wq",
		.flags = WQ_F_LAZY_THREAD_CREATION,
		.max_threads = 1024u,
		.min_threads = 32u,
		.max_pending_works = 4096u,
	};
	uint32_t max = 2u;
	int ret;

	memset(ring, 0, sizeof(*ring));

	/*
	 * Make sure that the size is power of 2 to avoid costly
	 * modulo operation.
	 */
	while (max < size)
		max *= 2u;

	ring->sq_mask = max - 1u;
	ring->cq_mask = (max * 2u) - 1u;

	ring->sqes = calloc(ring->sq_mask + 1u, sizeof(ring->sqes[0]));
	if (!ring->sqes)
		return -ENOMEM;

	ring->cqes = calloc(ring->cq_mask + 1u, sizeof(ring->cqes[0]));
	if (!ring->cqes) {
		ret = -ENOMEM;
		goto out_free_sqes;
	}

	ret = mutex_init(&ring->sq_lock);
	if (ret)
		goto out_free_cqes;
	
	ret = mutex_init(&ring->cq_lock);
	if (ret)
		goto out_free_sq_lock;

	ret = cond_init(&ring->wait_cqe_cond);
	if (ret)
		goto out_free_cq_lock;

	ret = alloc_workqueue(&ring->wq, &attr);
	if (ret)
		goto out_free_wait_cqe_cond;

	return 0;

out_free_wait_cqe_cond:
	cond_destroy(&ring->wait_cqe_cond);
out_free_cq_lock:
	mutex_destroy(&ring->cq_lock);
out_free_sq_lock:
	mutex_destroy(&ring->sq_lock);
out_free_cqes:
	free(ring->cqes);
out_free_sqes:
	free(ring->sqes);
	return ret;
}

void gw_ring_destroy(struct gw_ring *ring)
{
	mutex_lock(&ring->sq_lock);
	mutex_lock(&ring->cq_lock);
	ring->should_stop = true;
	if (ring->wait_cqe_cond_flag)
		cond_broadcast(&ring->wait_cqe_cond);
	mutex_unlock(&ring->cq_lock);
	mutex_unlock(&ring->sq_lock);
	destroy_workqueue(ring->wq);

	mutex_destroy(&ring->cq_lock);
	mutex_destroy(&ring->sq_lock);
	cond_destroy(&ring->wait_cqe_cond);
	free(ring->cqes);
	free(ring->sqes);
}

static void wake_up_wait_cqe_callers(struct gw_ring *ring)
	__must_hold(&ring->cq_lock)
{
	if (ring->wait_cqe_cond_flag)
		cond_broadcast(&ring->wait_cqe_cond);
}

static bool post_cqe(struct gw_ring *ring, struct gw_ring_sqe *sqe,
		     int64_t res)
{
	struct gw_ring_cqe *cqe;
	uint32_t cq_mask = ring->cq_mask;
	uint32_t cq_tail;
	uint32_t cq_head;

	mutex_lock(&ring->cq_lock);
	cq_tail = smp_load_acquire(&ring->cq_tail);
	cq_head = smp_load_acquire(&ring->cq_head);

	if (unlikely(u32_diff(cq_tail, cq_head) >= cq_mask + 1u)) {
		mutex_unlock(&ring->cq_lock);
		return false;
	}

	cqe = &ring->cqes[cq_tail & cq_mask];
	cqe->op = sqe->op;
	cqe->res = res;
	cqe->flags = 0;
	cqe->user_data = sqe->user_data;
	smp_store_release(&ring->cq_tail, cq_tail + 1u);
	wake_up_wait_cqe_callers(ring);
	mutex_unlock(&ring->cq_lock);
	return true;
}

static bool issue_op_nop(struct gw_ring *ring, struct gw_ring_sqe *sqe)
	__must_hold(&ring->sq_lock)
{
	return post_cqe(ring, sqe, 0);
}

static bool issue_op_tg_api_call(struct gw_ring *ring, struct gw_ring_sqe *sqe)
	__must_hold(&ring->sq_lock)
{
	return punt_to_io_wq(ring, sqe);
}

static bool issue_op_module_handle(struct gw_ring *ring, struct gw_ring_sqe *sqe)
	__must_hold(&ring->sq_lock)
{
	if (!punt_to_io_wq(ring, sqe)) {
		tgapi_free_update(sqe->tg_module_handle.update);
		return false;
	}

	return true;
}

static bool submit_sqe(struct gw_ring *ring, struct gw_ring_sqe *sqe)
	__must_hold(&ring->sq_lock)
{
	switch (sqe->op) {
	case GW_RING_OP_NOP:
		return issue_op_nop(ring, sqe);
	case GW_RING_OP_TG_API_CALL:
		return issue_op_tg_api_call(ring, sqe);
	case GW_RING_OP_MODULE_HANDLE:
		return issue_op_module_handle(ring, sqe);
	default:
		return false;
	}
}

int gw_ring_submit(struct gw_ring *ring)
{
	struct gw_ring_sqe *sqe;
	uint32_t sq_mask = ring->sq_mask;
	uint32_t sq_tail;
	uint32_t sq_head;
	uint32_t idx;
	int ret = 0;

	mutex_lock(&ring->sq_lock);
	if (unlikely(ring->should_stop)) {
		ret = -EOWNERDEAD;
		goto out;
	}

	sq_head = smp_load_acquire(&ring->sq_head);
	sq_tail = smp_load_acquire(&ring->sq_tail);

	while (sq_head != sq_tail) {
		idx = sq_head++ & sq_mask;
		sqe = &ring->sqes[idx];
		if (likely(submit_sqe(ring, sqe)))
			ret++;
	}

	smp_store_release(&ring->sq_head, sq_head);
out:
	mutex_unlock(&ring->sq_lock);
	return ret;
}

struct gw_ring_sqe *gw_ring_get_sqe(struct gw_ring *ring)
{
	struct gw_ring_sqe *sqe = NULL;
	uint32_t sq_head, sq_tail;

	mutex_lock(&ring->sq_lock);
	sq_head = smp_load_acquire(&ring->sq_head);
	sq_tail = smp_load_acquire(&ring->sq_tail);
	if (unlikely(u32_diff(sq_head, sq_tail) >= ring->sq_mask + 1u))
		goto out;
	sqe = &ring->sqes[sq_tail & ring->sq_mask];
	smp_store_release(&ring->sq_tail, sq_tail + 1u);
out:
	mutex_unlock(&ring->sq_lock);
	return sqe;
}

static int __gw_ring_wait_cqe(struct gw_ring *ring, struct gw_ring_cqe **cqe_p)
	__must_hold(&ring->cq_lock)
{
	struct gw_ring_cqe *cqe;
	uint32_t cq_mask = ring->cq_mask;
	uint32_t cq_tail;
	uint32_t cq_head;

	cq_tail = smp_load_acquire(&ring->cq_tail);
	cq_head = smp_load_acquire(&ring->cq_head);
	if (unlikely(cq_tail == cq_head))
		return -EAGAIN;

	cqe = &ring->cqes[cq_head & cq_mask];
	*cqe_p = cqe;
	return u32_diff(cq_tail, cq_head);
}

int gw_ring_wait_cqe(struct gw_ring *ring, struct gw_ring_cqe **cqe_p)
{
	int ret;

	mutex_lock(&ring->cq_lock);
	while (1) {
		if (unlikely(ring->should_stop)) {
			ret = -EOWNERDEAD;
			break;
		}

		ret = __gw_ring_wait_cqe(ring, cqe_p);
		if (likely(ret > 0))
			break;

		ring->wait_cqe_cond_flag = true;
		cond_wait(&ring->wait_cqe_cond, &ring->cq_lock);
		ring->wait_cqe_cond_flag = false;
	}
	mutex_unlock(&ring->cq_lock);
	return ret;
}

static bool __issue_op_tg_api_call(struct gw_ring *ring,
				   struct gw_ring_sqe *sqe)
{
	struct tg_api_call *call = &sqe->tg_api_call;
	int res;

	switch (call->op) {
	case TG_API_GET_UPDATES:
		res = tgapi_call_get_updates(call->ctx, call->updates_p,
					     call->offset);
		break;
	default:
		res = -EOPNOTSUPP;
		break;
	}

	return post_cqe(ring, sqe, res);
}

static bool __issue_op_module_handle(struct gw_ring *ring,
				     struct gw_ring_sqe *sqe)
{
	struct tg_module_handle *handle = &sqe->tg_module_handle;
	int res;

	res = gw_module_handle(handle->ctx, handle->update);
	return post_cqe(ring, sqe, res);
}

static bool punt_to_io_wq(struct gw_ring *ring, struct gw_ring_sqe *sqe)
	__must_hold(&ring->sq_lock)
{
	struct wq_sqe_data *data;
	int ret;

	data = malloc(sizeof(*data));
	if (unlikely(!data))
		return false;

	data->ring = ring;
	data->sqe = *sqe;
	ret = queue_work(ring->wq, gw_ring_wq_sqe_exec, data,
			 gw_ring_wq_sqe_delete);
	if (unlikely(ret < 0)) {
		free(data);
		return false;
	}

	return true;
}

static void gw_ring_wq_sqe_exec(void *data)
{
	struct wq_sqe_data *sqe_data = data;
	struct gw_ring *ring = sqe_data->ring;
	struct gw_ring_sqe *sqe = &sqe_data->sqe;

	switch (sqe->op) {
	case GW_RING_OP_NOP:
		issue_op_nop(ring, sqe);
		break;
	case GW_RING_OP_TG_API_CALL:
		__issue_op_tg_api_call(ring, sqe);
		break;
	case GW_RING_OP_MODULE_HANDLE:
		__issue_op_module_handle(ring, sqe);
		break;
	}
}

static void gw_ring_wq_sqe_delete(void *data)
{
	struct wq_sqe_data *sqe_data = data;
	struct gw_ring_sqe *sqe = &sqe_data->sqe;

	switch (sqe->op) {
	case GW_RING_OP_MODULE_HANDLE:
		tgapi_free_update(sqe->tg_module_handle.update);
		break;
	default:
		break;
	}

	free(data);
}
