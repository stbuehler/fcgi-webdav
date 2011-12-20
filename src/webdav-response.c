#include "fcgi-webdav-intern.h"

void dav_response_clear(DavServer *server, DavRequest *request) {
	UNUSED(server);

	request->response_status = 0;
	if (NULL != request->response_text) {
		g_string_free(request->response_text, TRUE);
		request->response_text = NULL;
	}
	if (NULL != request->response_header) {
		g_string_free(request->response_header, TRUE);
		request->response_header = NULL;
	}
	if (NULL != request->response_filename) {
		g_string_free(request->response_filename, TRUE);
		request->response_filename = NULL;
	}
	if (-1 != request->response_fd) {
		while (-1 == close(request->response_fd) && EINTR == errno) ;
		request->response_fd = -1;
	}
}

void dav_response_error(DavServer *server, DavRequest *request, int status) {
	dav_response_clear(server, request);
	request->response_status = status;
}

GString* dav_response_get_header(DavServer *server, DavRequest *request) {
	UNUSED(server);

	if (NULL == request->response_header) {
		request->response_header = g_string_sized_new(2*1024);
	}
	return request->response_header;
}

GString* dav_response_prepare_buffer(DavServer *server, DavRequest *request, guint alloc_len) {
	dav_response_clear(server, request);

	request->response_text = g_string_sized_new(alloc_len > 0 ? alloc_len : 16*1024);
	return request->response_text;
}

void dav_response_location(DavServer *server, DavRequest *request, GString *newpath) {
	g_string_append_printf(dav_response_get_header(server, request), "Location: %s%s\r\n", request->http_base->str, newpath->str);
}

void dav_response_xml_header(DavServer *server, DavRequest *request) {
	g_string_append_len(dav_response_get_header(server, request), CONST_STR_LEN("Content-Type: text/xml; charset=\"utf-8\"\r\n"));
}

const char* dav_response_status_str(guint status) {
	switch (status) {
	case 102: return "Processing";
	case 200: return "OK";
	case 207: return "Multi-Status";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 422: return "Unprocessable Entity";
	case 423: return "Locked";
	case 424: return "Failed Dependency";
	case 500: return "Internal error";
	case 507: return "Insufficient Storage";
	default: return "Unkown error";
	}
}
