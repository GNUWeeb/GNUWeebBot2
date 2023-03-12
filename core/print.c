// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Alviro Iskandar Setiawan <alviro.iskandar@gnuweeb.org>
 */

#include <gw/print.h>
#include <gw/thread.h>
#include <gw/common.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

static mutex_t print_mutex;

static char *alloc_print_buf(int *ret_p, char *prebuf, size_t prebuf_size,
			     bool *need_free, const char *fmt, va_list va)
{
	char *buf = prebuf;
	size_t ret;

	ret = (size_t)vsnprintf(buf, prebuf_size, fmt, va);
	*ret_p = (int)ret;
	if (unlikely(ret >= prebuf_size)) {
		buf = malloc(ret + 1);
		if (!buf) {
			*need_free = false;
			return prebuf;
		}

		ret = (size_t)vsnprintf(buf, ret + 1, fmt, va);
		*need_free = true;
	} else {
		*need_free = false;
	}

	return buf;
}

static const char *get_timestamp(char *buf, size_t buf_size)
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm);

	return buf;
}

static const char *get_print_type(uint32_t type)
{
	switch (type) {
	case PR_INFO:
		return "INFO";
	case PR_ERROR:
		return "ERROR";
	case PR_DEBUG:
		return "DEBUG";
	case PR_WARN:
		return "WARN";
	default:
		return "UNKNOWN";
	}
}

int __gw_print(uint32_t type, const char *file, uint32_t line, const char *fmt,
	       ...)
{
	char stack_buffer[8192];
	char time_buffer[32];
	bool need_free;
	va_list args;
	char *buf;
	int ret;

	va_start(args, fmt);
	buf = alloc_print_buf(&ret, stack_buffer, sizeof(stack_buffer),
			      &need_free, fmt, args);
	va_end(args);

	mutex_lock(&print_mutex);
	get_timestamp(time_buffer, sizeof(time_buffer));
	printf("[%s][%s]: %s\n", time_buffer, get_print_type(type), buf);
	(void)file;
	(void)line;
	mutex_unlock(&print_mutex);

	if (unlikely(need_free))
		free(buf);

	return ret;
}

int gw_print_global_init(void)
{
	setvbuf(stdout, NULL, _IOLBF, 4096);
	return mutex_init(&print_mutex);
}

void gw_print_global_destroy(void)
{
	mutex_destroy(&print_mutex);
}
