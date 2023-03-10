// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023  Ammar Faizi <ammarfaizi2@gnuweeb.org>
 */

#include <gw/lib/curl.h>
#include <gw/thread.h>
#include <gw/common.h>
#include <curl/curl.h>
#include <stdlib.h>

struct curl_handle_list {
	CURL		**handles;
	size_t		allocated;
	size_t		nr_handles;
	mutex_t		mutex;
};

static struct curl_handle_list *g_gw_curl_data;
static __thread CURL *current_thread_curl_handle;

int gw_curl_global_init(long flags)
{
	struct curl_handle_list *cd;
	int ret;

	if (!flags)
		flags = CURL_GLOBAL_ALL;

	if (curl_global_init(flags) != CURLE_OK)
		return -ENOMEM;
	
	cd = malloc(sizeof(*g_gw_curl_data));
	if (!cd) {
		ret = -ENOMEM;
		goto out_free_curl;
	}

	cd->allocated = 1024;
	cd->nr_handles = 0;
	ret = mutex_init(&cd->mutex);
	if (ret)
		goto out_free_cd;

	cd->handles = calloc(cd->allocated, sizeof(*cd->handles));
	if (!cd->handles) {
		ret = -ENOMEM;
		goto out_free_mutex;
	}

	g_gw_curl_data = cd;
	return 0;

out_free_mutex:
	mutex_destroy(&cd->mutex);
out_free_cd:
	free(cd);
out_free_curl:
	curl_global_cleanup();
	return ret;
}

void gw_curl_global_cleanup(void)
{
	struct curl_handle_list *cd = g_gw_curl_data;
	size_t i;

	mutex_lock(&cd->mutex);
	for (i = 0; i < cd->nr_handles; i++)
		curl_easy_cleanup(cd->handles[i]);
	free(cd->handles);
	curl_global_cleanup();
	mutex_unlock(&cd->mutex);
	free(cd);
}

static int gw_curl_realloc_handles(struct curl_handle_list *cd)
{
	size_t new_allocated = cd->allocated * 2;
	CURL **new_handles = realloc(cd->handles, new_allocated);

	if (!new_handles)
		return -ENOMEM;

	cd->handles = new_handles;
	cd->allocated = new_allocated;
	return 0;
}

void *gw_curl_thread_init(void)
{
	CURL *ret = current_thread_curl_handle;
	struct curl_handle_list *cd;

	if (likely(ret))
		return ret;

	cd = g_gw_curl_data;
	ret = curl_easy_init();
	if (!ret)
		return NULL;

	mutex_lock(&cd->mutex);
	if (cd->nr_handles >= cd->allocated) {
		if (gw_curl_realloc_handles(cd)) {
			mutex_unlock(&cd->mutex);
			curl_easy_cleanup(ret);
			return NULL;
		}
	}
	cd->handles[cd->nr_handles++] = ret;
	mutex_unlock(&cd->mutex);
	return ret;
}
