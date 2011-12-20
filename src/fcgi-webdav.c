#include "fcgi-webdav.h"

#include <glib/gprintf.h>

int main() {
	DavServer server;
	DavRequest req;

	server.docroot = g_string_new("/tmp/webdav");

	dav_request_init(&server, &req);

	req.method = DAV_PROPFIND;
	g_string_assign(req.url_path, "/");
	g_string_assign(req.http_base, "http://example.com");

	dav_request_execute(&server, &req);

	g_printf("HTTP/1.1 %i %s\r\n", req.response_status, dav_response_status_str(req.response_status));
	g_printf("%s\r\n%s", (NULL != req.response_header) ? req.response_header->str : "", (NULL != req.response_text) ? req.response_text->str : "");

	return -1;
}
