#ifndef _FCGIWEBDAV_FCGI_WEBDAV_INTERN_H
#define _FCGIWEBDAV_FCGI_WEBDAV_INTERN_H _FCGIWEBDAV_FCGI_WEBDAV_INTERN_H

#include "fcgi-webdav.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* db */
guint dav_db_get_property(GString *buffer, DavServer *server, DavRequest *request, GString *physpath, GString *prop_ns, GString *prop_name);
void dav_db_all_properties(GString *buffer, DavServer *server, DavRequest *request, GString *physpath);
void dav_db_name_properties(GString *buffer, DavServer *server, DavRequest *request, GString *physpath);
gboolean dav_db_set_properties_begin(DavServer *server, DavRequest *request);
guint dav_db_set_property_check(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value);
gboolean dav_db_set_property(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value);
void dav_db_set_properties_rollback(DavServer *server, DavRequest *request);
gboolean dav_db_set_properties_commit(DavServer *server, DavRequest *request);

/* server */
void dav_server_map_path(GString *physpath, DavServer *server, GString *urlpath);
GString *dav_server_get_mimetype(DavServer *server, GString *physpath);
guint dav_server_get_property(GString *buffer, DavServer *server, DavRequest *request, GString *physpath, struct stat *st, GString *prop_ns, GString *prop_name);
void dav_server_all_properties(GString *buffer, DavServer *server, DavRequest *request, GString *physpath, struct stat *st);
void dav_server_name_properties(GString *buffer, DavServer *server, DavRequest *request, GString *physpath, struct stat *st);
gboolean dav_server_set_properties_begin(DavServer *server, DavRequest *request);
/* check: changes done here must be undone in rollback. value == NULL is remove */
guint dav_server_set_property_check(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value);
/* set: failing here results in 500, changes here may be committed even if rollback is called later */
gboolean dav_server_set_property(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value);
/* if check or set failed, rollback is called, on success commit */
void dav_server_set_properties_rollback(DavServer *server, DavRequest *request);
gboolean dav_server_set_properties_commit(DavServer *server, DavRequest *request);

/* response */
void dav_response_clear(DavServer *server, DavRequest *request);
void dav_response_error(DavServer *server, DavRequest *request, int status);
GString* dav_response_get_header(DavServer *server, DavRequest *request);
GString* dav_response_prepare_buffer(DavServer *server, DavRequest *request, guint alloc_len);
void dav_response_location(DavServer *server, DavRequest *request, GString *newpath);
void dav_response_xml_header(DavServer* server, DavRequest* request);

/* utils */
void dav_append_trailing_slash(GString *s);
void dav_free_string(gpointer data);
GPtrArray* dav_new_stringlist(void);
void dav_path_simplify(GString *path);
int dav_intrsafe_open(const char *path, int oflag);
int dav_intrsafe_close(int fd);

/* walk */
typedef struct dav_walk_element dav_walk_element;
typedef struct dav_walk dav_walk;

struct dav_walk_element {
	gboolean self_done;
	GDir *current_dir;
	guint pathlen, urllen;
};

struct dav_walk {
	GString *currentpath, *currenturl;
	GArray *stack;
	struct stat st;
};

void dav_walk_init(DavServer *server, DavRequest *request, dav_walk *ctx, GString *path, GString *url, struct stat *st);
void dav_walk_clear(DavServer *server, DavRequest *request, dav_walk *ctx);
gboolean dav_walk_next(DavServer *server, DavRequest *request, dav_walk *ctx, gint depth);

/* xml */
gboolean dav_xml_parse_propfind(DavServer* server, DavRequest* request, gboolean* allprops, gboolean* nameprops, GPtrArray** proplist);
gboolean dav_xml_parse_proppatch(DavServer* server, DavRequest* request, GPtrArray** proplist);

/* macros */

#define CONST_STR_LEN(x) (x), (sizeof(x) - 1)
#define GSTR_LEN(x) ((x)->str), ((x)->len)

#define GSTR_EQUAL(s, data) (sizeof(data)-1 == s->len && g_str_equal(s->str, data))

#define UNUSED(x) ((void)(x))

#endif
