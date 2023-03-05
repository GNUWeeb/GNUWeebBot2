// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#ifndef GNUWEEB__MODULE_H
#define GNUWEEB__MODULE_H

#include <gw/common.h>
#include <gw/lib/tgapi.h>

struct gw_bot_module {
	const char	*name;
	uint64_t	listen_update_types;
	int		(*init)(struct tg_bot_ctx *ctx);
	int		(*handle)(struct tg_bot_ctx *ctx, struct tg_update *up);
	void		(*shutdown)(struct tg_bot_ctx *ctx);
};

extern struct gw_bot_module *gw_bot_modules[];

int gw_init_modules(struct tg_bot_ctx *ctx, bool allow_fail);
void gw_shutdown_modules(struct tg_bot_ctx *ctx);
int gw_module_handle(struct tg_bot_ctx *ctx, struct tg_update *up);

#endif /* #ifndef GNUWEEB__MODULE_H */
