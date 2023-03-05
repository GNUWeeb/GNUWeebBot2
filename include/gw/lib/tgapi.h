// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#ifndef GNUWEEB__LIB__TGAPI_H
#define GNUWEEB__LIB__TGAPI_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef uint32_t not_impl_t;
struct tg_message;
typedef uint64_t time64_t;

/*
 * See: https://core.telegram.org/bots/api#user
 */
struct tg_user {
	uint64_t		id;
	const char		*first_name;
	const char		*last_name;
	const char		*username;
	const char		*language_code;
	bool			is_bot;
	bool			can_join_group;
	bool			can_read_all_group_messages;
	bool			supports_inline_queries;
};

/*
 * See: https://core.telegram.org/bots/api#chat
 */
enum tg_chat_type {
	TG_CHAT_UNKNOWN		= 0u,
	TG_CHAT_PRIVATE		= (1u << 0u),
	TG_CHAT_GROUP		= (1u << 1u),
	TG_CHAT_SUPERGROUP	= (1u << 2u),
	TG_CHAT_CHANNEL		= (1u << 3u),
};
struct tg_chat {
	int64_t			id;
	const char		*title;
	const char		*username;
	const char		*first_name;
	const char		*last_name;
	const char		*bio;
	const char		*description;
	const char		*invite_link;
	struct tg_message	*pinned_message;
	uint32_t		slow_mode_delay;
	uint32_t		message_auto_delete_time;
	const char		*sticker_set_name;
	int64_t			linked_chat_id;
	enum tg_chat_type	type;
	not_impl_t		location;
	bool			has_private_fowards;
	bool			has_protected_content;
	bool			can_set_sticker_set;
};

/*
 * See: https://core.telegram.org/bots/api#messageentity
 */
enum tg_msg_entity_type {
	TG_MSG_ENTITY_UNKNOWN		= 0,
	TG_MSG_ENTITY_MENTION		= (1u << 0u),
	TG_MSG_ENTITY_HASHTAG		= (1u << 1u),
	TG_MSG_ENTITY_CASHTAG		= (1u << 2u),
	TG_MSG_ENTITY_BOT_CMD		= (1u << 3u),
	TG_MSG_ENTITY_URL		= (1u << 4u),
	TG_MSG_ENTITY_EMAIL		= (1u << 5u),
	TG_MSG_ENTITY_PHONE		= (1u << 6u),
	TG_MSG_ENTITY_BOLD		= (1u << 7u),
	TG_MSG_ENTITY_ITALIC		= (1u << 8u),
	TG_MSG_ENTITY_UNDERLINE		= (1u << 9u),
	TG_MSG_ENTITY_STRIKETHROUGH	= (1u << 10u),
	TG_MSG_ENTITY_SPOILER		= (1u << 11u),
	TG_MSG_ENTITY_CODE		= (1u << 12u),
	TG_MSG_ENTITY_PRE		= (1u << 13u),
	TG_MSG_ENTITY_TEXT_LINK		= (1u << 14u),
	TG_MSG_ENTITY_TEXT_MENTION	= (1u << 15u)
};
struct tg_msg_entity {
	enum tg_msg_entity_type		type;
	uint16_t			offset;
	uint16_t			length;
	union {
		/*
		 * Optional. For "text_link" only, url that will be opened
		 * after user taps on the text.
		 */
		const char	*url;

		/*
		 * Optional. For "text_mention" only, the mentioned user.
		 */
		struct tg_user	*user;

		/*
		 * Optional. For "pre" only, the programming language of
		 * the entity text.
		 */
		const char	*lang;
	};
};

/*
 * See: https://core.telegram.org/bots/api#message
 *
 * Make it sequential enum from 0 to N.
 */
enum tg_msg_type {
	TG_MSG_UNKNOWN					= (1ull << 0u),
	TG_MSG_TEXT					= (1ull << 1u),
	TG_MSG_ANIMATION				= (1ull << 2u),
	TG_MSG_AUDIO					= (1ull << 3u),
	TG_MSG_DOCUMENT					= (1ull << 4u),
	TG_MSG_PHOTO					= (1ull << 5u),
	TG_MSG_STICKER					= (1ull << 6u),
	TG_MSG_VIDEO					= (1ull << 7u),
	TG_MSG_VIDEO_NOTE				= (1ull << 8u),
	TG_MSG_VOICE					= (1ull << 9u),
	TG_MSG_CONTACT					= (1ull << 10u),
	TG_MSG_DICE					= (1ull << 11u),
	TG_MSG_GAME					= (1ull << 12u),
	TG_MSG_POLL					= (1ull << 13u),
	TG_MSG_VENUE					= (1ull << 14u),
	TG_MSG_LOCATION					= (1ull << 15u),
	TG_MSG_NEW_CHAT_MEMBERS				= (1ull << 16u),
	TG_MSG_LEFT_CHAT_MEMBER				= (1ull << 17u),
	TG_MSG_NEW_CHAT_TITLE				= (1ull << 18u),
	TG_MSG_NEW_CHAT_PHOTO				= (1ull << 19u),
	TG_MSG_DELETE_CHAT_PHOTO			= (1ull << 20u),
	TG_MSG_GROUP_CHAT_CREATED			= (1ull << 21u),
	TG_MSG_SUPERGROUP_CHAT_CREATED			= (1ull << 22u),
	TG_MSG_CHANNEL_CHAT_CREATED			= (1ull << 23u),
	TG_MSG_MIGRATE_TO_CHAT_ID			= (1ull << 24u),
	TG_MSG_MIGRATE_FROM_CHAT_ID			= (1ull << 25u),
	TG_MSG_PINNED_MESSAGE				= (1ull << 26u),
	TG_MSG_INVOICE					= (1ull << 27u),
	TG_MSG_SUCCESSFUL_PAYMENT			= (1ull << 28u),
	TG_MSG_CONNECTED_WEBSITE			= (1ull << 29u),
	TG_MSG_PASSPORT_DATA				= (1ull << 30u),
	TG_MSG_PROXIMITY_ALERT_TRIGGERED		= (1ull << 31u),
	TG_MSG_VOICE_CHAT_SCHEDULED			= (1ull << 32u),
	TG_MSG_VOICE_CHAT_STARTED			= (1ull << 33u),
	TG_MSG_VOICE_CHAT_ENDED				= (1ull << 34u),
	TG_MSG_VOICE_CHAT_PARTICIPANTS_INVITED		= (1ull << 35u),
	TG_MSG_REPLY_MARKUP				= (1ull << 36u),
};
struct tg_message {
	uint64_t		message_id;
	struct tg_user		*from;
	struct tg_chat		*sender_chat;
	time64_t		date;
	struct tg_chat		*chat;
	struct tg_user		*forward_from;
	struct tg_chat		*forward_from_chat;
	uint64_t		forward_from_message_id;
	const char		*forward_signature;
	const char		*forward_sender_name;
	time64_t		forward_date;
	struct tg_message	*reply_to_message;
	struct tg_user		*via_bot;
	time64_t		edit_date;
	const char		*media_group_id;
	const char		*author_signature;
	union {
		struct {
			const char		*text;
			struct tg_msg_entity	*entities;
			size_t			entities_len;
		};
	};
	enum tg_msg_type	type;
	bool			has_protected_content;
	bool			is_automatic_forward;
};

enum tg_update_type {
	TG_UPDATE_UNKNOWN	= 0u,
	TG_UPDATE_MESSAGE	= (1u << 0u),
};
struct tg_update {
	uint64_t		update_id;
	enum tg_update_type	type;
	union {
		struct tg_message	message;
	};

	/*
	 * We need to hold the JSON object to release
	 * the resource, as it holds the reference
	 * to `const char *`.
	 *
	 * It hurts the performance if we allocate
	 * new memory and copy it. So let it here.
	 */
	void	*json;
};

struct tg_updates {
	size_t			len;
	struct tg_update	updates[];
};

struct tg_api_ctx {
	const char		*token;
};

struct tga_call_send_message {
	int64_t		chat_id;
	const char	*text;
	const char	*parse_mode;
	bool		disable_web_page_preview;
	bool		disable_notification;
	int64_t		reply_to_message_id;
	const char	*reply_markup;
};

int tgapi_parse_update_len(struct tg_update *update, const char *json,
			   size_t len);
int tgapi_parse_update(struct tg_update *update, const char *json);
void tgapi_free_update(struct tg_update *update);

int tgapi_parse_updates(struct tg_updates **updates_p, const char *json);
int tgapi_parse_updates_len(struct tg_updates **updates_p, const char *json,
			    size_t len);
int tgapi_parse_updates_len_max(struct tg_updates **updates_p, const char *json,
				size_t len, size_t max_updates);
void tgapi_free_updates(struct tg_updates *updates);

int tgapi_call_get_updates(struct tg_api_ctx *ctx,
			   struct tg_updates **updates_p, int64_t offset);

int tgapi_call_send_message(struct tg_api_ctx *ctx,
			    const struct tga_call_send_message *call);

void tgapi_inc_ref_update(struct tg_update *update);

#endif /* #ifndef GNUWEEB__LIB__TGAPI_H */
