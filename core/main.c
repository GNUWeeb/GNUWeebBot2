// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gw.org>
 *
 * The entry point of the GNU/Weeb bot.
 */

#include <gw/common.h>
#include <gw/module.h>
#include <gw/lib/tgapi.h>
#include <gw/lib/curl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

static void arm_update_sqe(struct tg_bot_ctx *ctx)
{
	struct gw_ring_sqe *sqe;

	sqe = gw_ring_get_sqe(&ctx->ring);
	if (unlikely(!sqe)) {
		gw_ring_submit(&ctx->ring);
		sqe = gw_ring_get_sqe(&ctx->ring);
	}

	ctx->updates = NULL;
	gw_ring_prep_tg_get_updates(sqe, &ctx->tctx, &ctx->updates,
				    ctx->max_update_id + 1);
	sqe->user_data = TG_API_GET_UPDATES;
}

static int prep_update_handle(struct tg_bot_ctx *ctx, struct tg_update *up)
{
	struct gw_ring_sqe *sqe;

	sqe = gw_ring_get_sqe(&ctx->ring);
	if (unlikely(!sqe)) {
		gw_ring_submit(&ctx->ring);
		sqe = gw_ring_get_sqe(&ctx->ring);
	}

	gw_ring_prep_tg_module_handle(sqe, ctx, up);
	sqe->user_data = 0;
	return 0;
}

static int process_tg_api_update(struct tg_bot_ctx *ctx, struct tg_update *up)
{
	tgapi_inc_ref_update(up);
	return prep_update_handle(ctx, up);
}

static int process_tg_api_updates(struct tg_bot_ctx *ctx, int res)
{
	struct tg_update *update;
	uint32_t i;

	if (unlikely(res < 0)) {
		fprintf(stderr, "Failed to get updates: %s\n", strerror(-res));
		res = 0;
		goto out;
	}

	if (unlikely(!ctx->updates))
		goto out;

	if (ctx->updates->len)
		printf("Got new %zu update(s)\n", ctx->updates->len);

	for (i = 0; i < ctx->updates->len; i++) {
		update = &ctx->updates->updates[i];
		if ((int64_t)update->update_id > ctx->max_update_id)
			ctx->max_update_id = update->update_id;
		process_tg_api_update(ctx, update);
	}

out:
	arm_update_sqe(ctx);
	return res;
}

static int process_tg_api_cqe(struct tg_bot_ctx *ctx, struct gw_ring_cqe *cqe)
{
	int res = (int)cqe->res;
	int ret = 0;

	switch (cqe->user_data) {
	case TG_API_GET_UPDATES:
		ret = process_tg_api_updates(ctx, res);
		break;
	}

	return ret;
}

static int process_cqe(struct tg_bot_ctx *ctx, struct gw_ring_cqe *cqe)
{
	int ret = 0;

	switch (cqe->op) {
	case GW_RING_OP_NOP:
		break;
	case GW_RING_OP_TG_API_CALL:
		ret = process_tg_api_cqe(ctx, cqe);
		break;
	}

	return ret;
}

static int run_tg_bot_loop(struct tg_bot_ctx *ctx)
{
	struct gw_ring_cqe *cqe;
	uint32_t head;
	uint32_t i;
	int ret;

	ret = gw_ring_submit(&ctx->ring);
	if (unlikely(ret < 0)) {
		fprintf(stderr, "Failed to submit sqe: %s\n", strerror(-ret));
		return ret;
	}

	ret = gw_ring_wait_cqe(&ctx->ring, &cqe);
	if (unlikely(ret < 0)) {
		fprintf(stderr, "Failed to wait cqe: %s\n", strerror(-ret));
		return ret;
	}

	i = 0;
	gw_ring_for_each_cqe(&ctx->ring, head, cqe) {
		i++;
		ret = process_cqe(ctx, cqe);
		if (unlikely(ret))
			break;
	}
	gw_ring_cq_advance(&ctx->ring, i);
	return ret;
}

static int run_tg_bot(struct tg_bot_ctx *ctx)
{
	int ret;

	ret = gw_init_modules(ctx, true);
	if (unlikely(ret)) {
		fprintf(stderr, "Failed to init modules: %s\n", strerror(-ret));
		return ret;
	}

	arm_update_sqe(ctx);
	pr_info("GNU/Weeb bot is running");
	while (true) {
		ret = run_tg_bot_loop(ctx);
		if (unlikely(ret))
			break;
	}

	gw_shutdown_modules(ctx);
	return ret;
}

int main(void)
{
	struct tg_bot_ctx ctx;
	int ret;

	memset(&ctx, 0, sizeof(ctx));

	ctx.tctx.token = getenv("GNUWEEB_TG_BOT_TOKEN");
	if (!ctx.tctx.token) {
		fprintf(stderr, "GNUWEEB_TG_BOT_TOKEN is not set\n");
		return 1;
	}

	ret = gw_curl_global_init(0);
	if (ret) {
		fprintf(stderr, "Failed to init curl: %s\n", strerror(-ret));
		return 1;
	}

	ret = gw_print_global_init();
	if (ret) {
		fprintf(stderr, "Failed to init print: %s\n", strerror(-ret));
		goto out;
	}

	ret = gw_ring_init(&ctx.ring, 8192);
	if (ret) {
		fprintf(stderr, "Failed to init ring: %s\n", strerror(-ret));
		goto out;
	}

	ret = run_tg_bot(&ctx);
	if (ret)
		fprintf(stderr, "Failed to run tg bot: %s\n", strerror(-ret));

	gw_ring_destroy(&ctx.ring);
	gw_print_global_destroy();
out:
	if (ret < 0)
		ret = -ret;

	gw_curl_global_cleanup();
	return ret;
}
