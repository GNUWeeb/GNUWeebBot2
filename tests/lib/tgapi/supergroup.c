// SPDX-License-Identifier: GPL-2.0-only

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <gw/lib/tgapi.h>

static const char json_str_text_message[] =
"{"
"    \"update_id\": 167024941,"
"    \"message\": {"
"        \"message_id\": 24543,"
"        \"from\": {"
"            \"id\": 243692601,"
"            \"is_bot\": false,"
"            \"first_name\": \"Ammar\","
"            \"last_name\": \"Faizi\","
"            \"username\": \"ammarfaizi2\","
"            \"language_code\": \"en\""
"        },"
"        \"chat\": {"
"            \"id\": -1001226739827,"
"            \"title\": \"Test Group\","
"            \"type\": \"supergroup\","
"            \"username\": null"
"        },"
"        \"date\": 1650440986,"
"        \"text\": \"/debug\","
"        \"entities\": ["
"            {"
"                \"offset\": 0,"
"                \"length\": 6,"
"                \"type\": \"bot_command\""
"            }"
"        ],"
"        \"reply_to_message\": null"
"    }"
"}";

static int text_message(void)
{
	struct tg_message *msg;
	struct tg_user *from;
	struct tg_chat *chat;
	struct tg_update up;
	int ret;

	ret = tgapi_parse_update(&up, json_str_text_message);
	assert(!ret);

	msg = &up.message;
	assert(up.update_id == 167024941);
	assert(up.type == TG_UPDATE_MESSAGE);
	assert(msg->message_id == 24543);

	from = msg->from;
	assert(from);
	assert(from->id == 243692601);
	assert(from->is_bot == false);
	assert(from->first_name);
	assert(!strcmp(from->first_name, "Ammar"));
	assert(from->last_name);
	assert(!strcmp(from->last_name, "Faizi"));
	assert(from->language_code);
	assert(!strcmp(from->language_code, "en"));

	chat = msg->chat;
	assert(chat);
	assert(chat->id == -1001226739827);
	assert(chat->title);
	assert(!strcmp(chat->title, "Test Group"));
	assert(chat->type == TG_CHAT_SUPERGROUP);
	assert(!chat->username);

	assert(msg->date == 1650440986);
	assert(msg->type == TG_MSG_TEXT);
	assert(msg->text);
	assert(!strcmp(msg->text, "/debug"));
	assert(msg->entities);
	assert(msg->entities_len == 1);

	tgapi_free_update(&up);
	return ret;
}

int main(void)
{
	assert(!text_message());
	return 0;
}
