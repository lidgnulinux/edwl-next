/* See LICENSE.dwm file for copyright and license details. */
#ifndef TYPES_WLR_BUFFER
#define TYPES_WLR_BUFFER

#include <wlr/types/wlr_buffer.h>

/* Due to this struct is hidden in wlroots, here's a copy, cause we need it */
struct wlr_readonly_data_buffer {
	struct wlr_buffer base;

	const void *data;
	uint32_t format;
	size_t stride;

	void *saved_data;
};

struct wlr_readonly_data_buffer *readonly_data_buffer_create(uint32_t format,
		size_t stride, uint32_t width, uint32_t height, const void *data);

bool readonly_data_buffer_drop(struct wlr_readonly_data_buffer *buffer);
#endif

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
