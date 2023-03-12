// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#ifndef GNUWEEB__THREAD_H
#define GNUWEEB__THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(CONFIG_CPP_THREAD)
#include <pthread.h>
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
#else /* #if defined(__linux__) */
typedef struct thread_struct *thread_t;
typedef struct mutex_struct *mutex_t;
typedef struct cond_struct *cond_t;
#endif /* #if defined(__linux__) */

int thread_create(thread_t *ts_p, void *(*func)(void *), void *arg);
int thread_join(thread_t ts, void **ret);
void thread_detach(thread_t ts);
int mutex_init(mutex_t *m);
int mutex_lock(mutex_t *m);
int mutex_unlock(mutex_t *m);
void mutex_destroy(mutex_t *m);
int cond_init(cond_t *c);
int cond_wait(cond_t *c, mutex_t *m);
int cond_signal(cond_t *c);
int cond_broadcast(cond_t *c);
void cond_destroy(cond_t *c);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* #ifndef GNUWEEB__THREAD_H */
