#include "fcgi-webdav-intern.h"

void dav_walk_init(DavServer *server, DavRequest *request, dav_walk *ctx, GString *path, GString *url, struct stat *st) {
	dav_walk_element *i;
	GDir *dir;
	UNUSED(server);
	UNUSED(request);

	memset(ctx, 0, sizeof(*ctx));

	dir = g_dir_open(path->str, 0, NULL);
	/* if dir == NULL we still process the dir entry itself */

	ctx->currentpath = g_string_new_len(GSTR_LEN(path));
	ctx->currenturl = g_string_new_len(GSTR_LEN(url));
	ctx->stack = g_array_new(FALSE, TRUE, sizeof(dav_walk_element));
	ctx->st = *st;

	dav_append_trailing_slash(ctx->currentpath);
	dav_append_trailing_slash(ctx->currenturl);

	g_array_set_size(ctx->stack, 1);
	i = &g_array_index(ctx->stack, dav_walk_element, 0);

	i->self_done = FALSE;
	i->current_dir = dir;
	i->pathlen = ctx->currentpath->len;
	i->urllen = ctx->currenturl->len;
}

void dav_walk_clear(DavServer *server, DavRequest *request, dav_walk *ctx) {
	UNUSED(server);
	UNUSED(request);

	if (NULL != ctx->stack) {
		guint k, l;
		for (k = 0, l = ctx->stack->len; k < l; k++) {
			dav_walk_element *i = &g_array_index(ctx->stack, dav_walk_element, k);
			g_dir_close(i->current_dir);
		}
		g_array_free(ctx->stack, TRUE);
	}
	if (NULL != ctx->currentpath) g_string_free(ctx->currentpath, TRUE);
	if (NULL != ctx->currenturl) g_string_free(ctx->currenturl, TRUE);
	memset(ctx, 0, sizeof(*ctx));
}

gboolean dav_walk_next(DavServer *server, DavRequest *request, dav_walk *ctx, gint depth) {
	dav_walk_element *i;
	UNUSED(server);
	UNUSED(request);

	if (0 == ctx->stack->len) return FALSE;

	i = &g_array_index(ctx->stack, dav_walk_element, ctx->stack->len - 1);

	if (!i->self_done) {
		i->self_done = TRUE;
		if (S_ISDIR(ctx->st.st_mode) || S_ISREG(ctx->st.st_mode)) return TRUE;
	}

	if (0 == depth || NULL == i->current_dir) return FALSE;

	for ( ;; ) {
		const gchar *name = g_dir_read_name(i->current_dir);
		if (NULL == name) {
			g_dir_close(i->current_dir);
			g_array_set_size(ctx->stack, ctx->stack->len - 1);
			if (0 == ctx->stack->len) return FALSE;
			i = &g_array_index(ctx->stack, dav_walk_element, ctx->stack->len - 1);
			g_string_truncate(ctx->currentpath, i->pathlen);
			g_string_truncate(ctx->currenturl, i->urllen);
			continue;
		}

		g_string_truncate(ctx->currentpath, i->pathlen);
		g_string_append(ctx->currentpath, name);
		if (-1 == stat(ctx->currentpath->str, &ctx->st)) continue;

		g_string_truncate(ctx->currenturl, i->urllen);
		g_string_append(ctx->currenturl, name); /* TODO: convert to utf8 somehow??? */
		if (S_ISDIR(ctx->st.st_mode)) {
			dav_append_trailing_slash(ctx->currentpath);
			dav_append_trailing_slash(ctx->currenturl);

			if (depth < 0 || ((guint) depth) >= ctx->stack->len) {
				GDir *dir = g_dir_open(ctx->currentpath->str, 0, NULL);
				if (NULL == dir) continue;

				g_array_set_size(ctx->stack, ctx->stack->len + 1);
				i = &g_array_index(ctx->stack, dav_walk_element, ctx->stack->len - 1);

				i->self_done = TRUE;
				i->current_dir = dir;
				i->pathlen = ctx->currentpath->len;
				i->urllen = ctx->currenturl->len;
			}
		} else if (!S_ISREG(ctx->st.st_mode)) {
			continue;
		}
		return TRUE;
	}
}

