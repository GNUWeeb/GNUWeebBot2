// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gw.org>
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <gw/thread.h>
#include <gw/workqueue.h>

#ifdef __CHECKER__
#define __must_hold(x)		__attribute__((__context__(x, 1, 1)))
#define __acquires(x)		__attribute__((__context__(x, 0, 1)))
#define __releases(x)		__attribute__((__context__(x, 1, 0)))
#else
#define __must_hold(x)
#define __acquires(x)
#define __releases(x)
#endif

#ifdef __GNUC__
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)
#else
#define likely(x)		(!!(x))
#define unlikely(x)		(!!(x))
#endif

#if 1
#define pr_debug(FMT, ...)						\
do {									\
	printf("%s:%u " FMT "\n", __FILE__, __LINE__, ## __VA_ARGS__);	\
} while (0)
#else
#define pr_debug(FMT, ...) do {} while (0)
#endif

struct work_struct {
	void			(*func)(void *);
	void			*arg;
	void			(*deleter)(void *);
};

struct worker_thread {
	uint32_t		id;
	thread_t		thread;
	struct workqueue_struct	*wq;
};

struct workqueue_struct {
	volatile bool		should_stop;
	volatile bool		queue_is_blocked;
	volatile bool		wait_all_is_waiting;
	uint32_t		nr_sleeping_workers;
	uint32_t		nr_sleeping_queuers;
	uint32_t		nr_online_workers;
	uint32_t		nr_running_workers;
	struct workqueue_attr	attr;
	struct worker_thread	*workers;
	cond_t			worker_cond;
	cond_t			wait_all_cond;
	cond_t			queue_work_cond;
	uint32_t		mask;
	uint32_t		head;
	uint32_t		tail;
	mutex_t			work_list_lock;
	struct work_struct	work_list[];
};

static void *worker_func(void *arg);

static int64_t count_pending_works(struct workqueue_struct *wq)
{
	return (int64_t)llabs((int64_t)wq->tail - (int64_t)wq->head);
}

static int validate_and_adjust_workqueue_attr(struct workqueue_attr *attr)
{
	uint32_t max;

	if (attr->flags & ~WQ_F_ALL)
		return -EINVAL;

	if (!attr->max_threads)
		attr->max_threads = 4;

	if (attr->min_threads > attr->max_threads)
		return -EINVAL;

	/*
	 * The max_pending_works must be a power of 2. If it's not, round it
	 * up to the next power of 2. This is needed to avoid division in
	 * the circular queue implementation.
	 */
	max = 2u;
	while (max < attr->max_pending_works)
		max *= 2u;
	attr->max_pending_works = max;
	return 0;
}

static int alloc_workers(struct workqueue_struct *wq)
{
	struct worker_thread *workers, *worker;
	uint32_t nr_thread_to_create;
	uint32_t i;
	int ret;

	workers = calloc(wq->attr.max_threads, sizeof(*workers));
	if (!workers)
		return -ENOMEM;

	for (i = 0; i < wq->attr.max_threads; i++)
		workers[i].id = i;

	if (wq->attr.flags & WQ_F_LAZY_THREAD_CREATION)
		nr_thread_to_create = wq->attr.min_threads;
	else
		nr_thread_to_create = wq->attr.max_threads;

	for (i = 0; i < nr_thread_to_create; i++) {
		worker = &workers[i];
		worker->wq = wq;
		ret = thread_create(&worker->thread, &worker_func, worker);
		if (ret)
			goto out_err;
	}

	wq->workers = workers;
	return 0;

out_err:
	mutex_lock(&wq->work_list_lock);
	wq->should_stop = true;
	cond_broadcast(&wq->worker_cond);
	mutex_unlock(&wq->work_list_lock);

	while (i--) {
		worker = &workers[i];
		thread_join(worker->thread, NULL);
	}
	free(workers);
	return ret;
}

int alloc_workqueue(struct workqueue_struct **wq_p,
		    const struct workqueue_attr *attr_arg)
{
	struct workqueue_attr attr = *attr_arg;
	struct workqueue_struct *wq;
	size_t size;
	int ret;

	ret = validate_and_adjust_workqueue_attr(&attr);
	if (ret)
		return ret;

	size = sizeof(*wq) + attr.max_pending_works * sizeof(wq->work_list[0]);
	wq = calloc(1u, size);
	if (!wq)
		return -ENOMEM;

	wq->attr = attr;
	wq->mask = attr.max_pending_works - 1u;
	wq->head = 0u;
	wq->tail = 0u;

	ret = mutex_init(&wq->work_list_lock);
	if (ret)
		goto err_free_wq;
	ret = cond_init(&wq->worker_cond);
	if (ret)
		goto err_free_work_list_lock;
	ret = cond_init(&wq->wait_all_cond);
	if (ret)
		goto err_free_work_list_cond;
	ret = cond_init(&wq->queue_work_cond);
	if (ret)
		goto err_free_wait_all_cond;
	ret = alloc_workers(wq);
	if (ret)
		goto err_free_queue_work_cond;

	*wq_p = wq;
	return 0;

err_free_queue_work_cond:
	cond_destroy(&wq->queue_work_cond);
err_free_wait_all_cond:
	cond_destroy(&wq->wait_all_cond);
err_free_work_list_cond:
	cond_destroy(&wq->worker_cond);
err_free_work_list_lock:
	mutex_destroy(&wq->work_list_lock);
err_free_wq:
	free(wq);
	return ret;
}

void wait_all_work_done(struct workqueue_struct *wq)
{
	mutex_lock(&wq->work_list_lock);
	wq->queue_is_blocked = true;

	while (count_pending_works(wq) > 0 || wq->nr_running_workers) {

		if (wq->should_stop)
			break;

		wq->wait_all_is_waiting = true;
		cond_wait(&wq->wait_all_cond, &wq->work_list_lock);
		wq->wait_all_is_waiting = false;
	}

	wq->queue_is_blocked = false;
	mutex_unlock(&wq->work_list_lock);
}

static struct worker_thread *get_free_worker_slot(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	struct worker_thread *worker;
	uint32_t i;

	for (i = 0; i < wq->attr.max_threads; i++) {
		worker = &wq->workers[i];
		if (!worker->wq)
			return worker;
	}

	return NULL;
}

static int arm_spawn_worker(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	struct worker_thread *worker;

	worker = get_free_worker_slot(wq);
	if (!worker)
		return -EAGAIN;

	worker->wq = wq;
	return thread_create(&worker->thread, &worker_func, worker);
}

static int arm_worker(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	if (unlikely(!wq->nr_sleeping_workers))
		return arm_spawn_worker(wq);

	cond_signal(&wq->worker_cond);
	return 0;
}

static int try_queue_work_locked(struct workqueue_struct *wq,
				 struct work_struct *work)
	__must_hold(&wq->work_list_lock)
{
	if (unlikely(wq->should_stop))
		return -EOWNERDEAD;

	if (unlikely(wq->queue_is_blocked))
		return -EAGAIN;

	if (unlikely(count_pending_works(wq) >= wq->attr.max_pending_works))
		return -EAGAIN;

	wq->work_list[wq->tail++ & wq->mask] = *work;
	arm_worker(wq);
	return 0;
}

int queue_work(struct workqueue_struct *wq, void (*func)(void *), void *arg,
	       void (*deleter)(void *))
{
	struct work_struct work;
	int ret;

	work.func = func;
	work.arg = arg;
	work.deleter = deleter;
	mutex_lock(&wq->work_list_lock);
	while (1) {
		ret = try_queue_work_locked(wq, &work);
		if (likely(ret <= 0 && ret != -EAGAIN))
			break;

		wq->nr_sleeping_queuers++;
		cond_wait(&wq->queue_work_cond, &wq->work_list_lock);
		wq->nr_sleeping_queuers--;
	}
	mutex_unlock(&wq->work_list_lock);
	return ret;
}

static void clear_pending_works(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	while (wq->head != wq->tail) {
		struct work_struct *work;

		work = &wq->work_list[wq->head++ & wq->mask];
		if (work->deleter)
			work->deleter(work->arg);
	}
}

static void wake_up_all_workers(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	uint32_t i = wq->nr_sleeping_workers;

	if (i == 1u)
		cond_signal(&wq->worker_cond);
	else if (i > 1u)
		cond_broadcast(&wq->worker_cond);
	
	if (wq->wait_all_is_waiting)
		cond_broadcast(&wq->wait_all_cond);
}

static void join_all_workers(struct workqueue_struct *wq)
{
	struct worker_thread *worker, *tmp_ret;
	uint32_t i;

	for (i = 0; i < wq->attr.max_threads; i++) {
		worker = &wq->workers[i];
		if (!worker->wq)
			continue;

		thread_join(worker->thread, (void **)&tmp_ret);
		assert(tmp_ret == worker);
		assert(worker->id == i);
	}
}

void destroy_workqueue(struct workqueue_struct *wq)
{
	mutex_lock(&wq->work_list_lock);
	wq->should_stop = true;
	clear_pending_works(wq);
	wake_up_all_workers(wq);
	mutex_unlock(&wq->work_list_lock);

	join_all_workers(wq);
	mutex_lock(&wq->work_list_lock);
	mutex_unlock(&wq->work_list_lock);
	cond_destroy(&wq->queue_work_cond);
	cond_destroy(&wq->wait_all_cond);
	cond_destroy(&wq->worker_cond);
	mutex_destroy(&wq->work_list_lock);
	free(wq->workers);
	free(wq);
}

static bool wait_for_event(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	while (1) {
		if (unlikely(wq->should_stop && !wq->queue_is_blocked))
			return false;

		if (likely(wq->head != wq->tail))
			return true;

		if (unlikely(wq->wait_all_is_waiting))
			cond_broadcast(&wq->wait_all_cond);

		wq->nr_sleeping_workers++;
		cond_wait(&wq->worker_cond, &wq->work_list_lock);
		wq->nr_sleeping_workers--;
	}
}

static void wake_up_all_queue_work_callers(struct workqueue_struct *wq)
	__must_hold(&wq->work_list_lock)
{
	uint32_t i;

	if (likely(!wq->nr_running_workers))
		return;

	i = wq->nr_sleeping_queuers;
	if (i == 1u)
		cond_signal(&wq->queue_work_cond);
	else if (i > 1u)
		cond_broadcast(&wq->queue_work_cond);
}

static void *worker_func(void *arg)
{
	struct worker_thread *worker = arg;
	struct workqueue_struct *wq = worker->wq;
	struct work_struct work;

	mutex_lock(&wq->work_list_lock);
	wq->nr_online_workers++;
	while (wait_for_event(wq)) {
		work = wq->work_list[wq->head++ & wq->mask];
		wq->nr_running_workers++;
		mutex_unlock(&wq->work_list_lock);

		if (likely(work.func))
			work.func(work.arg);
		if (work.deleter)
			work.deleter(work.arg);

		mutex_lock(&wq->work_list_lock);
		wq->nr_running_workers--;
		wake_up_all_queue_work_callers(wq);
	}
	wq->nr_online_workers--;
	mutex_unlock(&wq->work_list_lock);
	return arg;
}
