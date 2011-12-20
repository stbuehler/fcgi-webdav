#include "fcgi-webdav-intern.h"

void dav_append_trailing_slash(GString *s) {
	if (0 == s->len || '/' != s->str[s->len-1]) g_string_append_len(s, "/", 1);
}

void dav_free_string(gpointer data) {
	GString *s = data;
	g_string_free(s, TRUE);
}

GPtrArray* dav_new_stringlist(void) {
	return g_ptr_array_new_with_free_func(dav_free_string);
}

/* from lighttpd2 */
void dav_path_simplify(GString *path) {
	int toklen;
	char c, pre1;
	char *start, *slash, *walk, *out;
	unsigned short pre;

	if (path == NULL)
		return;

	walk  = start = out = slash = path->str;
	while (*walk == ' ') {
		walk++;
	}

	pre1 = *(walk++);
	c    = *(walk++);
	pre  = pre1;
	if (pre1 != '/') {
		pre = ('/' << 8) | pre1;
		*(out++) = '/';
	}
	*(out++) = pre1;

	if (pre1 == '\0') {
		g_string_set_size(path, out - start);
		return;
	}

	while (1) {
		if (c == '/' || c == '\0') {
			toklen = out - slash;
			if (toklen == 3 && pre == (('.' << 8) | '.')) {
				out = slash;
				if (out > start) {
					out--;
					while (out > start && *out != '/') {
						out--;
					}
				}

				if (c == '\0')
					out++;
			} else if (toklen == 1 || pre == (('/' << 8) | '.')) {
				out = slash;
				if (c == '\0')
					out++;
			}

			slash = out;
		}

		if (c == '\0')
			break;

		pre1 = c;
		pre  = (pre << 8) | pre1;
		c    = *walk;
		*out = pre1;

		out++;
		walk++;
	}

	g_string_set_size(path, out - start);
}


int dav_intrsafe_open(const char *path, int oflag) {
	int fd;
	while (-1 == (fd = open(path, oflag)) && EINTR == errno) ;
	return fd;
}

int dav_intrsafe_close(int fd) {
	int r;
	while (-1 == (r = close(fd)) && EINTR == errno) ;
	return r;
}
