// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */

#ifndef GNUWEEB__PRINT_H
#define GNUWEEB__PRINT_H

#include <stdint.h>
#include <stdlib.h>

enum {
	PR_INFO,
	PR_ERROR,
	PR_DEBUG,
	PR_WARN,
};

int __gw_print(uint32_t type, const char *file, uint32_t line, const char *fmt,
	       ...);
int gw_print_global_init(void);
void gw_print_global_destroy(void);

#define pr_err(fmt, ...) __gw_print(PR_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) __gw_print(PR_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) __gw_print(PR_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...) __gw_print(PR_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* #ifndef GNUWEEB__PRINT_H */
