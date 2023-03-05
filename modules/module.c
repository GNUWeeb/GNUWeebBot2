// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#include <gw/module.h>
#include "module_list.h"

struct gw_bot_module *gw_bot_modules[] = {
#include "module_table.h"
};

#include <stdio.h>
#include <stdlib.h>

int gw_init_modules(struct tg_bot_ctx *ctx, bool allow_fail)
{
	size_t len = sizeof(gw_bot_modules) / sizeof(gw_bot_modules[0]);
	struct gw_bot_module *mod;
	size_t i;
	int ret;

	for (i = 0; i < len; i++) {
		mod = gw_bot_modules[i];
		if (!mod->init)
			continue;

		ret = mod->init(ctx);
		if (ret && !allow_fail) {
			fprintf(stderr, "Failed to init module %s\n", mod->name);
			return ret;
		}
	}

	return 0;
}

void gw_shutdown_modules(struct tg_bot_ctx *ctx)
{
	size_t len = sizeof(gw_bot_modules) / sizeof(gw_bot_modules[0]);
	struct gw_bot_module *mod;
	size_t i;

	for (i = 0; i < len; i++) {
		mod = gw_bot_modules[i];
		if (!mod->shutdown)
			mod->shutdown(ctx);
	}
}

int gw_module_handle(struct tg_bot_ctx *ctx, struct tg_update *up)
{
	size_t len = sizeof(gw_bot_modules) / sizeof(gw_bot_modules[0]);
	struct gw_bot_module *mod;
	size_t i;
	int ret;

	for (i = 0; i < len; i++) {
		mod = gw_bot_modules[i];
		if (!mod->handle)
			continue;

		if (!(mod->listen_update_types & up->type))
			continue;

		ret = mod->handle(ctx, up);
		if (ret)
			return ret;
	}

	return 0;
}
