#ifndef _FCGIWEBDAV_FCGI_WEBDAV_H
#define _FCGIWEBDAV_FCGI_WEBDAV_H _FCGIWEBDAV_FCGI_WEBDAV_H

#include <glib.h>

typedef struct DavRequest DavRequest;
typedef struct DavServer DavServer;

typedef enum {
	DAV_OPTIONS,
	DAV_PROPFIND,
	DAV_PROPPATCH,
	DAV_MKCOL,
	DAV_GET,
	DAV_HEAD,
	DAV_DELETE,
	DAV_PUT,
	DAV_COPY,
	DAV_MOVE,
	DAV_LOCK,
	DAV_UNLOCK
} DavMethod;

enum DavConsts {
	DAV_DEPTH_UNSPECIFIED = -2,
	DAV_DEPTH_INFINITY = -1,
	
	DAV_TIMEOUT_INFINITY = -1
};

struct DavServer {
	GString *docroot; /* no trailing slash needed */
};

struct DavRequest {
	DavMethod method;
	GString *url_path;

	gint depth;

	/* TODO: "If" header */

	GString *lock_token;
	gboolean overwrite;
	gint64 timeout;

	/* only one of xml/filename can be != NULL. neither the xml string nor the named file can be empty! if the request body was empty, set both to NULL. */
	GString *request_xml;
	GString *request_filename;

	GString *http_base; /* "http://example.com" or "https://example.com:8443" - base for urls */

	gint response_status;
	GString *response_header;
	GString *response_text;
	GString *response_filename;
	int response_fd;
};

void dav_request_init(DavServer *server, DavRequest *request);
void dav_request_execute(DavServer *server, DavRequest *request);
void dav_request_clear(DavServer *server, DavRequest *request);

const char* dav_response_status_str(guint status);

#endif
