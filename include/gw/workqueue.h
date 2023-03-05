// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#ifndef GNUWEEB__WORKQUEUE_H
#define GNUWEEB__WORKQUEUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	WQ_F_LAZY_THREAD_CREATION = (1ul << 0),
};

enum {
	WQ_NAME_MAX_LEN = 32,
};

#define WQ_F_ALL (			\
	WQ_F_LAZY_THREAD_CREATION	\
)

struct workqueue_struct;

struct workqueue_attr {
	char		name[WQ_NAME_MAX_LEN];
	uint32_t	flags;
	uint32_t	max_threads;
	uint32_t	min_threads;
	uint32_t	max_pending_works;
};

int alloc_workqueue(struct workqueue_struct **wq_p,
		    const struct workqueue_attr *attr);
int queue_work(struct workqueue_struct *wq, void (*func)(void *), void *arg,
	       void (*deleter)(void *));
int try_queue_work(struct workqueue_struct *wq, void (*func)(void *), void *arg,
		   void (*deleter)(void *));
void destroy_workqueue(struct workqueue_struct *wq);
void wait_all_work_done(struct workqueue_struct *wq);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* #ifndef GNUWEEB__WORKQUEUE_H */
