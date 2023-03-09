// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gw.org>
 */

#include <mutex>
#include <cerrno>
#include <thread>
#include <cstdlib>
#include <functional>
#include <condition_variable>
#include <gw/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct thread_struct {
	std::thread	thread;
	void		*ret;

	thread_struct(std::function<void(void)> f);
	~thread_struct(void) = default;
};

struct mutex_struct {
	std::mutex	mutex;
};

struct cond_struct {
	std::condition_variable cond;
};

inline thread_struct::thread_struct(std::function<void(void)> f):
	thread(f)
{
}

static void __thread_entry(struct thread_struct *ts, void *(*func)(void *),
			   void *arg)
{
	ts->ret = func(arg);
}

int thread_create(struct thread_struct **ts_p, void *(*func)(void *), void *arg)
{
	struct thread_struct *ts;

	ts = static_cast<thread_struct *>(malloc(sizeof(*ts)));
	if (!ts)
		return -ENOMEM;

	ts = new (ts) thread_struct(std::bind(__thread_entry, ts, func, arg));
	*ts_p = ts;
	return 0;
}

int thread_join(struct thread_struct *ts, void **ret)
{
	ts->thread.join();
	if (ret)
		*ret = ts->ret;
	ts->~thread_struct();
	free(ts);
	return 0;
}

void thread_detach(struct thread_struct *ts)
{
	ts->thread.detach();
	ts->~thread_struct();
	free(ts);
}

int mutex_init(struct mutex_struct **m)
{
	struct mutex_struct *mutex;

	mutex = static_cast<mutex_struct *>(malloc(sizeof(*mutex)));
	if (!mutex)
		return -ENOMEM;

	mutex = new (mutex) mutex_struct();
	*m = mutex;
	return 0;
}

int mutex_lock(struct mutex_struct **m)
{
	(*m)->mutex.lock();
	return 0;
}

int mutex_unlock(struct mutex_struct **m)
{
	(*m)->mutex.unlock();
	return 0;
}

void mutex_destroy(struct mutex_struct **m)
{
	(*m)->~mutex_struct();
	free(*m);
	*m = nullptr;
}

int cond_init(struct cond_struct **c)
{
	struct cond_struct *cond;

	cond = static_cast<cond_struct *>(malloc(sizeof(*cond)));
	if (!cond)
		return -ENOMEM;

	cond = new (cond) cond_struct();
	*c = cond;
	return 0;
}

int cond_wait(struct cond_struct **c, struct mutex_struct **m)
{
	std::unique_lock<std::mutex> lock((*m)->mutex, std::defer_lock);
	(*c)->cond.wait(lock);
	return 0;
}

int cond_signal(struct cond_struct **c)
{
	(*c)->cond.notify_one();
	return 0;
}

int cond_broadcast(struct cond_struct **c)
{
	(*c)->cond.notify_all();
	return 0;
}

void cond_destroy(struct cond_struct **c)
{
	(*c)->~cond_struct();
	free(*c);
	*c = nullptr;
}

#ifdef __cplusplus
} // extern "C"
#endif
