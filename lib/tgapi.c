// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gw.org>
 */

#include <gw/lib/tgapi.h>
#include <gw/common.h>
#include <json-c/json.h>
#include <json-c/json_tokener.h>
#include <curl/curl.h>
#include <gw/lib/curl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

static int parse_json(json_object **jobj_p, const char *json_str, size_t len)
{
	enum json_tokener_error jerr;
	json_object *jobj = NULL;
	struct json_tokener *tok;

	tok = json_tokener_new();
	if (unlikely(!tok))
		return -ENOMEM;

	do {
		jobj = json_tokener_parse_ex(tok, json_str, (int)len);
		jerr = json_tokener_get_error(tok);
	} while (jerr == json_tokener_continue);

	json_tokener_free(tok);
	if (unlikely(jerr != json_tokener_success))
		return -EINVAL;

	*jobj_p = jobj;
	return 0;
}

static int tgj_get_user(struct tg_user *user, json_object *juser)
{
	json_object *res;

	memset(user, 0, sizeof(*user));
	if (unlikely(!json_object_object_get_ex(juser, "id", &res)))
		return -ENOENT;
	user->id = json_object_get_uint64(res);

	if (unlikely(!json_object_object_get_ex(juser, "first_name", &res)))
		return -ENOENT;
	user->first_name = json_object_get_string(res);

	/*
	 * `last_name`, `username`, `language_code` and `is_bot` are not
	 * a mandatory field. They can be empty (zero or NULL).
	 */
	if (json_object_object_get_ex(juser, "last_name", &res))
		user->last_name = json_object_get_string(res);

	if (json_object_object_get_ex(juser, "username", &res))
		user->username = json_object_get_string(res);

	if (json_object_object_get_ex(juser, "language_code", &res))
		user->language_code = json_object_get_string(res);

	if (json_object_object_get_ex(juser, "is_bot", &res))
		user->is_bot = json_object_get_boolean(res) ? true : false;

	return 0;
}

static int tgj_get_chat(struct tg_chat *chat, json_object *jchat)
{
	json_object *res;

	memset(chat, 0, sizeof(*chat));
	if (unlikely(!json_object_object_get_ex(jchat, "id", &res)))
		return -ENOENT;
	chat->id = json_object_get_int64(res);

	if (json_object_object_get_ex(jchat, "title", &res))
		chat->title = json_object_get_string(res);

	if (likely(json_object_object_get_ex(jchat, "type", &res))) {
		const char *p = json_object_get_string(res);
		if (unlikely(!p))
			return -EINVAL;

		if (!strcmp(p, "supergroup"))
			chat->type = TG_CHAT_SUPERGROUP;
		else
			chat->type = TG_CHAT_UNKNOWN;
	} else {
		chat->type = TG_CHAT_UNKNOWN;
	}

	/*
	 * `username` is not a mandatory field.
	 */
	if (json_object_object_get_ex(jchat, "username", &res))
		chat->username = json_object_get_string(res);

	return 0;
}

static int tgj_get_user_and_alloc(struct tg_user **user_p, json_object *jobj)
{
	struct tg_user *user;
	int ret;

	user = malloc(sizeof(*user));
	if (unlikely(!user))
		return -ENOMEM;

	ret = tgj_get_user(user, jobj);
	if (likely(!ret)) {
		*user_p = user;
		return 0;
	}

	free(user);
	return ret;
}

static int tgj_get_chat_and_alloc(struct tg_chat **chat_p, json_object *jobj)
{
	struct tg_chat *chat;
	int ret;

	chat = malloc(sizeof(*chat));
	if (unlikely(!chat))
		return -ENOMEM;

	ret = tgj_get_chat(chat, jobj);
	if (likely(!ret)) {
		*chat_p = chat;
		return 0;
	}

	free(chat);
	return ret;
}

static int tgj_get_message_entities(struct tg_msg_entity *ent,
				    json_object *jent, size_t len)
{
	(void)ent;
	(void)jent;

/*
 * TODO(ammarfaizi2):
 * Complete this function later. We don't need this for now.
 */
#if 0
	json_object *res;
	size_t i;

	for (i = 0; i < len; i++) {
		res = json_object_array_get_idx(jent, (int)i);
		if (unlikely(!res))
			continue;

		if (unlikely(!json_object_object_get_ex(res, "type", &res)))
			continue;
		ent[i].type = json_object_get_string(res);

		if (unlikely(!json_object_object_get_ex(res, "offset", &res)))
			continue;
		ent[i].offset = json_object_get_int(res);

		if (unlikely(!json_object_object_get_ex(res, "length", &res)))
			continue;
		ent[i].length = json_object_get_int(res);

		if (unlikely(!json_object_object_get_ex(res, "url", &res)))
			continue;
		ent[i].url = json_object_get_string(res);
	}
	
#endif
	return (int)len;
}

/*
 * Return the number of elements in the array if success.
 * Otherwise a negative value is returned.
 */
static int tgj_get_entities_and_alloc(struct tg_msg_entity **ent_p,
				      json_object *jent)
{
	struct tg_msg_entity *ent;
	size_t len;
	int ret;

	len = json_object_array_length(jent);
	ent = calloc(len, sizeof(*ent));
	if (unlikely(!ent))
		return -ENOMEM;

	ret = tgj_get_message_entities(ent, jent, len);
	if (likely(ret >= 0)) {
		*ent_p = ent;
		return (int)len;
	}

	free(ent);
	return ret;
}

/*
 * Parse all possible message variants. The list of possible variants is
 * in the tg_msg_type enum. I am not sure if we will ever parse all possible
 * variants, but we will try.
 */
static int tgj_arrange_message_variant(struct tg_message *msg,
				       json_object *jmsg)
{
	json_object *res;
	int ret;

	if (json_object_object_get_ex(jmsg, "text", &res)) {
		msg->type = TG_MSG_TEXT;
		msg->text = json_object_get_string(res);

		/*
		 * `entities` is not a mandatory field.
		 */
		if (!json_object_object_get_ex(jmsg, "entities", &res))
			return 0;

		ret = tgj_get_entities_and_alloc(&msg->entities, res);
		if (unlikely(ret < 0))
			return ret;
		
		msg->entities_len = (size_t)ret;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "photo", &res)) {
		msg->type = TG_MSG_PHOTO;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "sticker", &res)) {
		msg->type = TG_MSG_STICKER;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "video", &res)) {
		msg->type = TG_MSG_VIDEO;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "voice", &res)) {
		msg->type = TG_MSG_VOICE;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "audio", &res)) {
		msg->type = TG_MSG_AUDIO;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "document", &res)) {
		msg->type = TG_MSG_DOCUMENT;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "location", &res)) {
		msg->type = TG_MSG_LOCATION;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "contact", &res)) {
		msg->type = TG_MSG_CONTACT;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "new_chat_members", &res)) {
		msg->type = TG_MSG_NEW_CHAT_MEMBERS;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "left_chat_member", &res)) {
		msg->type = TG_MSG_LEFT_CHAT_MEMBER;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "new_chat_title", &res)) {
		msg->type = TG_MSG_NEW_CHAT_TITLE;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "new_chat_photo", &res)) {
		msg->type = TG_MSG_NEW_CHAT_PHOTO;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "delete_chat_photo", &res)) {
		msg->type = TG_MSG_DELETE_CHAT_PHOTO;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "group_chat_created", &res)) {
		msg->type = TG_MSG_GROUP_CHAT_CREATED;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "supergroup_chat_created", &res)) {
		msg->type = TG_MSG_SUPERGROUP_CHAT_CREATED;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "channel_chat_created", &res)) {
		msg->type = TG_MSG_CHANNEL_CHAT_CREATED;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "migrate_to_chat_id", &res)) {
		msg->type = TG_MSG_MIGRATE_TO_CHAT_ID;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "migrate_from_chat_id", &res)) {
		msg->type = TG_MSG_MIGRATE_FROM_CHAT_ID;
		return 0;
	}

	if (json_object_object_get_ex(jmsg, "pinned_message", &res)) {
		msg->type = TG_MSG_PINNED_MESSAGE;
		return 0;
	}

	msg->type = TG_MSG_UNKNOWN;
	return 0;
}

static int tgj_get_message(struct tg_message *msg, json_object *jmsg)
{
	json_object *res;
	int ret;

	if (unlikely(!json_object_object_get_ex(jmsg, "message_id", &res)))
		return -ENOENT;
	msg->message_id = json_object_get_uint64(res);

	if (unlikely(!json_object_object_get_ex(jmsg, "date", &res)))
		return -ENOENT;
	msg->date = (time64_t)json_object_get_uint64(res);

	if (unlikely(!json_object_object_get_ex(jmsg, "from", &res))) {
		msg->message_id = 0;
		return -ENOENT;
	}

	ret = tgj_get_user_and_alloc(&msg->from, res);
	if (unlikely(ret))
		return ret;

	if (unlikely(!json_object_object_get_ex(jmsg, "chat", &res))) {
		ret = -ENOENT;
		goto err_free_from;
	}

	ret = tgj_get_chat_and_alloc(&msg->chat, res);
	if (unlikely(ret))
		goto err_free_from;

	ret = tgj_arrange_message_variant(msg, jmsg);
	if (unlikely(ret))
		goto err_free_chat;

	return 0;

err_free_chat:
	free(msg->chat);
	msg->chat = NULL;
err_free_from:
	free(msg->from);
	msg->from = NULL;
	return ret;
}

static int tgj_get_update(struct tg_update *update, json_object *jobj)
{
	json_object *res;
	int ret;

	if (!json_object_object_get_ex(jobj, "update_id", &res))
		return -ENOENT;
	update->update_id = json_object_get_uint64(res);

	if (json_object_object_get_ex(jobj, "message", &res)) {
		update->type = TG_UPDATE_MESSAGE;
		ret = tgj_get_message(&update->message, res);
		if (unlikely(ret))
			return ret;
	} else {
		update->type = TG_UPDATE_UNKNOWN;
	}

	return 0;
}

__no_inline int tgapi_parse_update_len(struct tg_update *update,
				       const char *json, size_t len)
{
	json_object *jobj;
	int ret;

	ret = parse_json(&jobj, json, len);
	if (unlikely(ret))
		return ret;

	update->json = jobj;
	ret = tgj_get_update(update, update->json);
	if (unlikely(ret)) {
		json_object_put(update->json);
		memset(update, 0, sizeof(*update));
		return ret;
	}

	return 0;
}

int tgapi_parse_update(struct tg_update *update, const char *json)
{
	return tgapi_parse_update_len(update, json, strlen(json) + 1);
}

static void tgapi_free_message_text(struct tg_message *msg)
{
	free(msg->entities);
}

static void tgapi_free_message(struct tg_message *msg)
{
	switch (msg->type) {
	case TG_MSG_TEXT:
		tgapi_free_message_text(msg);
		break;
	default:
		break;
	}
	free(msg->from);
	free(msg->chat);
}

void tgapi_inc_ref_update(struct tg_update *update)
{
	json_object_get(update->json);
}

void tgapi_free_update(struct tg_update *update)
{
	if (unlikely(!update->json))
		return;

	if (json_object_put(update->json) > 0)
		return;

	update->json = NULL;
	switch (update->type) {
	case TG_UPDATE_MESSAGE:
		tgapi_free_message(&update->message);
		break;
	default:
		break;
	}
}

int tgapi_parse_updates(struct tg_updates **updates_p, const char *json)
{
	return tgapi_parse_updates_len(updates_p, json, strlen(json) + 1);
}

int tgapi_parse_updates_len(struct tg_updates **updates_p, const char *json,
			    size_t len)
{
	return tgapi_parse_updates_len_max(updates_p, json, len, (size_t)~0ul);
}

int tgapi_parse_updates_len_max(struct tg_updates **updates_p, const char *json,
				size_t len, size_t max_updates)
{
	struct tg_updates *updates;
	json_object *jobj, *result;
	size_t size;
	size_t i, j;
	int ret;

	ret = parse_json(&jobj, json, len);
	if (unlikely(ret))
		return ret;

	if (unlikely(!json_object_object_get_ex(jobj, "result", &result))) {
		printf("here!!\n");
		ret = -ENOENT;
		goto out;
	}

	len = json_object_array_length(result);
	if (len > max_updates)
		len = max_updates;

	size = sizeof(*updates) + len * sizeof(updates->updates[0]);
	updates = calloc(1u, size);
	if (unlikely(!updates)) {
		ret = -ENOMEM;
		goto out;
	}

	updates->len = len;
	for (i = 0, j = 0; i < len; i++) {
		json_object *update;

		update = json_object_array_get_idx(result, i);
		if (unlikely(!update))
			goto out_clean;

		ret = tgj_get_update(&updates->updates[j], update);
		if (unlikely(ret)) {
			ret = 0;
			continue;
		}

		json_object_get(update);
		j++;
	}

out:
	json_object_put(jobj);
	if (likely(!ret))
		*updates_p = updates;
	return ret;

out_clean:
	for (i = 0; i < len; i++)
		tgapi_free_update(&updates->updates[i]);
	free(updates);
	goto out;
}

void tgapi_free_updates(struct tg_updates *updates)
{
	size_t i;

	for (i = 0; i < updates->len; i++)
		tgapi_free_update(&updates->updates[i]);
	free(updates);
}

struct curl_data {
	char	*data;
	size_t	len;
	size_t	allocated;
	int	err;
};

static size_t tgapi_curl_write(char *ptr, size_t size, size_t nmemb, void *dd)
{
	size_t real_size = size * nmemb;
	struct curl_data *d = dd;
	size_t new_len;

	if (!dd)
		return real_size;

	if (!d->data) {
		d->allocated = 4096;
		d->data = malloc(d->allocated);
		if (!d->data) {
			d->err = -ENOMEM;
			return 0;
		}
	}

	new_len = d->len + real_size + 1;
	if (d->allocated < new_len) {
		size_t new_allocated;
		char *data;

		new_allocated = (d->allocated * 2) + new_len;
		data = realloc(d->data, new_allocated);
		if (!data) {
			free(d->data);
			d->data = NULL;
			d->err = -ENOMEM;
			return 0;
		}
		d->data = data;
		d->allocated = new_allocated;
	}

	memcpy(&d->data[d->len], ptr, real_size);
	d->len += real_size;
	d->data[d->len] = '\0';
	return real_size;
}

/*
 * TODO(ammarfaizi2): Avoid curl repetition.
 */

int tgapi_call_get_updates(struct tg_api_ctx *ctx,
			   struct tg_updates **updates_p, int64_t offset)
{
	struct curl_data data = { 0 };
	char url[256];
	CURLcode res;
	CURL *ch;
	int ret;

	ch = gw_curl_thread_init();
	if (unlikely(!ch))
		return -ENOMEM;

	snprintf(url, sizeof(url),
		 "https://api.telegram.org/bot%s/getUpdates?offset=%" PRId64,
		 ctx->token, offset);

	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, tgapi_curl_write);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, &data);

	res = curl_easy_perform(ch);
	if (res != CURLE_OK) {
		fprintf(stderr, "Curl with URL \"%s\" failed: %s\n", url,
			curl_easy_strerror(res));
		return -1;
	}

	ret = tgapi_parse_updates_len(updates_p, data.data, data.len + 1u);
	free(data.data);
	return ret;
}

int tgapi_call_send_message(struct tg_api_ctx *ctx,
			    const struct tga_call_send_message *call)
{
	struct curl_data data = { 0 };
	char url[256];
	CURLcode res;
	CURL *ch;

	ch = gw_curl_thread_init();
	if (unlikely(!ch))
		return -ENOMEM;

	/*
	 * TODO(ammarfaizi2): Use POST method and encode the data.
	 */
	snprintf(url, sizeof(url),
		 "https://api.telegram.org/bot%s/sendMessage?chat_id=%" PRId64
		 "&text=%s&reply_to_message_id=%" PRId64,
		 ctx->token, call->chat_id, call->text,
		 call->reply_to_message_id);

	printf("Curl exec to: %s\n", url);
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, tgapi_curl_write);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, &data);

	res = curl_easy_perform(ch);
	if (res != CURLE_OK) {
		fprintf(stderr, "Curl with URL \"%s\" failed: %s\n", url,
			curl_easy_strerror(res));
		return -1;
	}

	free(data.data);
	return 0;
}
