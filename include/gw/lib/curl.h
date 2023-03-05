// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#ifndef GNUWEEB__LIB__CURL_H
#define GNUWEEB__LIB__CURL_H

int gw_curl_global_init(long flags);
void gw_curl_global_cleanup(void);
void *gw_curl_thread_init(void);

#endif /* #ifndef GNUWEEB__LIB__CURL_H */
