// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#include "mod_info.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static int ping_init(struct tg_bot_ctx *ctx)
{
	(void)ctx;
	return 0;
}

static int __ping_handle(struct tg_bot_ctx *ctx, struct tg_message *msg)
{
	const struct tga_call_send_message call = {
		.chat_id = msg->chat->id,
		.text = "Pong!",
		.parse_mode = "",
		.disable_web_page_preview = true,
		.disable_notification = true,
		.reply_to_message_id = msg->message_id,
		.reply_markup = NULL,
	};

	return tgapi_call_send_message(&ctx->tctx, &call);
}

/*
 * Handle: /ping, .ping, !ping
 */
static int ping_handle(struct tg_bot_ctx *ctx, struct tg_update *up)
{
	const char *text;

	assert(up->type & TG_UPDATE_MESSAGE);

	if (up->type != TG_UPDATE_MESSAGE)
		return 0;

	if (up->message.type != TG_MSG_TEXT)
		return 0;

	text = up->message.text;
	if (!text)
		return 0;

	if (!(text[0] == '/' || text[0] == '!' || text[0] == '.'))
		return 0;

	if (!strcmp(&text[1], "ping"))
		return __ping_handle(ctx, &up->message);

	return 0;
}

static void ping_shutdown(struct tg_bot_ctx *ctx)
{
	(void)ctx;
}

struct gw_bot_module gw_mod_info_ping = {
	.name = "ping",
	.listen_update_types = TG_UPDATE_MESSAGE,
	.init = ping_init,
	.handle = ping_handle,
	.shutdown = ping_shutdown,
};
