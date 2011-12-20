#include "fcgi-webdav-intern.h"

#define WEBDAV_FILE_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
#define WEBDAV_DIR_MODE  S_IRWXU | S_IRWXG | S_IRWXO

#define STATUS_LINE_424 "HTTP/1.1 424 Failed Dependency"
#define STATUS_LINE_200 "HTTP/1.1 200 OK"
#define XML_NAMESPACES " xmlns:D=\"DAV:\" xmlns:ns0=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\""

static const GString XML_Start = { CONST_STR_LEN("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"), 0 };
static const GString MultiStatus_Start = { CONST_STR_LEN("<D:multistatus" XML_NAMESPACES ">\n"), 0 };
static const GString MultiStatus_End = { CONST_STR_LEN("</D:multistatus>\n"), 0 };

static void request_options(DavServer *server, DavRequest *request) {
	g_string_append_len(dav_response_get_header(server, request), CONST_STR_LEN("DAV: 1,2\r\nMS-Author-Via: DAV\r\n"));
	g_string_append_len(dav_response_get_header(server, request), CONST_STR_LEN("Allow: PROPFIND\r\n"));
#if 0
	g_string_append_len(dav_response_get_header(server, request), CONST_STR_LEN("Allow: PROPFIND, DELETE, MKCOL, PUT, MOVE, COPY, PROPPATCH, LOCK, UNLOCK\r\n"));
#endif

	request->response_status = 200;

	if (!g_str_equal(request->url_path->str, "*")) {
		struct stat st;
		GString *physpath = g_string_sized_new(0);

		dav_server_map_path(physpath, server, request->url_path);

		if (-1 == stat(physpath->str, &st)) {
			switch (errno) {
			case EACCES:
				request->response_status = 403;
				return;
			case ENOENT:
			case ENOTDIR:
				request->response_status = 404;
				return;
			default:
				request->response_status = 500;
				/* TODO: log error */
				return;
			}
		}
		
		g_string_free(physpath, TRUE);
	}
}

static void request_propfind_element(DavServer *server, DavRequest *request, GString *buffer, GString *url, GString *path, struct stat *st, GPtrArray *proplist) {
	GString *failed = g_string_sized_new(0), *prop = g_string_sized_new(512);
	guint i, l;

	g_string_append_len(buffer, CONST_STR_LEN("<D:response><D:href>"));
	g_string_append_len(buffer, GSTR_LEN(request->http_base));
	g_string_append_len(buffer, GSTR_LEN(url)); /* TODO: encode url */
	g_string_append_len(buffer, CONST_STR_LEN("</D:href><D:propstat><D:prop>"));

	for (i = 0, l = proplist->len; i < l; i += 2) {
		guint status;
		GString *prop_ns = g_ptr_array_index(proplist, i);
		GString *prop_name = g_ptr_array_index(proplist, i+1);

		g_string_truncate(prop, 0);
		status = dav_server_get_property(prop, server, request, path, st, prop_ns, prop_name);

		if (200 == status) {
			g_string_append_len(buffer, GSTR_LEN(prop));
		} else {
			g_string_append_len(failed, CONST_STR_LEN("<D:propstat><D:prop>"));
			if (GSTR_EQUAL(prop_ns, "DAV:")) {
				g_string_append_printf(failed, "<D:%s/>", prop_name->str);
			} else {
				g_string_append_printf(failed, "<R:%s xmlns:R=\"%s\"/>", prop_name->str, prop_ns->str);
			}
			g_string_append_printf(failed, "</D:prop><D:status>HTTP/1.1 %i %s</D:status></D:propstat>", status, dav_response_status_str(status));
		}
	}
	g_string_append_len(buffer, CONST_STR_LEN("</D:prop><D:status>" STATUS_LINE_200 "</D:status></D:propstat>"));
	g_string_append_len(buffer, GSTR_LEN(failed));
	g_string_append_len(buffer, CONST_STR_LEN("</D:response>\n"));
}

static void request_propfind_element_allprops(DavServer *server, DavRequest *request, GString *buffer, GString *url, GString *path, struct stat *st) {
	g_string_append_len(buffer, CONST_STR_LEN("<D:response><D:href>"));
	g_string_append_len(buffer, GSTR_LEN(request->http_base));
	g_string_append_len(buffer, GSTR_LEN(url)); /* TODO: encode url */
	g_string_append_len(buffer, CONST_STR_LEN("</D:href><D:propstat><D:prop>"));
	dav_server_all_properties(buffer, server, request, path, st);
	g_string_append_len(buffer, CONST_STR_LEN("</D:prop><D:status>" STATUS_LINE_200 "</D:status></D:propstat></D:response>\n"));
}

static void request_propfind_element_nameprops(DavServer *server, DavRequest *request, GString *buffer, GString *url, GString *path, struct stat *st) {
	g_string_append_len(buffer, CONST_STR_LEN("<D:response><D:href>"));
	g_string_append_len(buffer, GSTR_LEN(request->http_base));
	g_string_append_len(buffer, GSTR_LEN(url)); /* TODO: encode url */
	g_string_append_len(buffer, CONST_STR_LEN("</D:href><D:propstat><D:prop>"));
	dav_server_name_properties(buffer, server, request, path, st);
	g_string_append_len(buffer, CONST_STR_LEN("</D:prop><D:status>" STATUS_LINE_200 "</D:status></D:propstat></D:response>\n"));
}

static void request_propfind(DavServer *server, DavRequest *request) {
	GString *buffer;
	GString *physpath = g_string_sized_new(0);
	struct stat st;
	gboolean allprops, nameprops;
	GPtrArray *proplist = NULL;
	dav_walk walkctx;

	if (!dav_xml_parse_propfind(server, request, &allprops, &nameprops, &proplist)) goto cleanup;

	dav_server_map_path(physpath, server, request->url_path);

	if (-1 == stat(physpath->str, &st)) {
		switch (errno) {
		case EACCES:
			dav_response_error(server, request, 403);
			goto cleanup;
		case ENOENT:
		case ENOTDIR:
			dav_response_error(server, request, 404);
			goto cleanup;
		default:
			dav_response_error(server, request, 500);
			/* TODO: log error */
			goto cleanup;
		}
	}

	buffer = dav_response_prepare_buffer(server, request, 0);

	if (S_ISDIR(st.st_mode) && '/' != request->url_path->str[request->url_path->len-1]) {
		dav_append_trailing_slash(request->url_path);
		dav_response_location(server, request, request->url_path);
	}

	request->response_status = 207;
	g_string_append_len(buffer, GSTR_LEN(&XML_Start));
	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_Start));

	dav_walk_init(server, request, &walkctx, physpath, request->url_path, &st);

	while (dav_walk_next(server, request, &walkctx, request->depth)) {
		if (allprops) {
			request_propfind_element_allprops(server, request, buffer, walkctx.currenturl, walkctx.currentpath, &walkctx.st);
		} else if (nameprops) {
			request_propfind_element_nameprops(server, request, buffer, walkctx.currenturl, walkctx.currentpath, &walkctx.st);
		} else {
			request_propfind_element(server, request, buffer, walkctx.currenturl, walkctx.currentpath, &walkctx.st, proplist);
		}
	}

	dav_walk_clear(server, request, &walkctx);

	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_End));
	dav_response_xml_header(server, request);

cleanup:
	if (NULL != proplist) g_ptr_array_free(proplist, TRUE);
	g_string_free(physpath, TRUE);
}

static void request_proppatch(DavServer *server, DavRequest *request) {
	GString *buffer;
	GString *physpath = g_string_sized_new(0);
	struct stat st;
	GPtrArray *proplist = NULL;
	guint i, l;
	guint status;

	if (!dav_xml_parse_proppatch(server, request, &proplist)) goto cleanup;

	dav_server_map_path(physpath, server, request->url_path);

	if (-1 == stat(physpath->str, &st)) {
		switch (errno) {
		case EACCES:
			dav_response_error(server, request, 403);
			goto cleanup;
		case ENOENT:
		case ENOTDIR:
			dav_response_error(server, request, 404);
			goto cleanup;
		default:
			dav_response_error(server, request, 500);
			/* TODO: log error */
			goto cleanup;
		}
	}

	if (S_ISDIR(st.st_mode) && '/' != request->url_path->str[request->url_path->len-1]) {
		dav_append_trailing_slash(request->url_path);
		dav_response_location(server, request, request->url_path);
	}

	if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
		dav_response_error(server, request, 403);
		goto cleanup;
	}

	if (!dav_server_set_properties_begin(server, request)) {
		/* hard fail - internal server error. reason should be logged already */
		dav_response_error(server, request, 500);
		goto cleanup;
	}

	buffer = dav_response_prepare_buffer(server, request, 0);

	request->response_status = 207;
	g_string_append_len(buffer, GSTR_LEN(&XML_Start));
	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_Start));

	g_string_append_len(buffer, CONST_STR_LEN("<D:response><D:href>"));
	g_string_append_len(buffer, GSTR_LEN(request->http_base));
	g_string_append_len(buffer, GSTR_LEN(request->url_path)); /* TODO: encode url */
	g_string_append_len(buffer, CONST_STR_LEN("</D:href><D:propstat><D:prop>"));

	for (i = 0, l = proplist->len; i < l; i += 3) {
		GString *prop_ns = g_ptr_array_index(proplist, i);
		GString *prop_name = g_ptr_array_index(proplist, i+1);
		GString *value = g_ptr_array_index(proplist, i+2);

		status = dav_server_set_property_check(server, request, physpath, prop_ns, prop_name, value);

		if (200 != status) goto failed_check;
	}

	/* everything looks fine. now really set and commit */

	for (i = 0, l = proplist->len; i < l; i += 3) {
		GString *prop_ns = g_ptr_array_index(proplist, i);
		GString *prop_name = g_ptr_array_index(proplist, i+1);
		GString *value = g_ptr_array_index(proplist, i+2);

		if (!dav_server_set_property(server, request, physpath, prop_ns, prop_name, value)) {
			/* hard fail - internal server error. reason should be logged already */
			dav_server_set_properties_rollback(server, request);
			dav_response_error(server, request, 500);
			goto cleanup;
		}

		if (GSTR_EQUAL(prop_ns, "DAV:")) {
			g_string_append_printf(buffer, "<D:%s/>", prop_name->str);
		} else {
			g_string_append_printf(buffer, "<R:%s xmlns:R=\"%s\"/>", prop_name->str, prop_ns->str);
		}
	}

	if (!dav_server_set_properties_commit(server, request)) {
		/* hard fail - internal server error. reason should be logged already */
		dav_response_error(server, request, 500);
		goto cleanup;
	}

	g_string_append_len(buffer, CONST_STR_LEN("</D:prop><D:status>" STATUS_LINE_200 "</D:status></D:propstat>"));
	g_string_append_len(buffer, CONST_STR_LEN("</D:response>\n"));

	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_End));
	dav_response_xml_header(server, request);

	goto cleanup;

failed_check:
	{
		guint failed_ndx = i;
		{
			GString *prop_ns = g_ptr_array_index(proplist, failed_ndx);
			GString *prop_name = g_ptr_array_index(proplist, failed_ndx+1);

			if (GSTR_EQUAL(prop_ns, "DAV:")) {
				g_string_append_printf(buffer, "<D:%s/>", prop_name->str);
			} else {
				g_string_append_printf(buffer, "<R:%s xmlns:R=\"%s\"/>", prop_name->str, prop_ns->str);
			}
			g_string_append_printf(buffer, "</D:prop><D:status>HTTP/1.1 %i %s</D:status></D:propstat>", status, dav_response_status_str(status));
		}

		if (proplist->len > 3) {
			for (i = 0, l = proplist->len; i < l; i += 3) {
				GString *prop_ns = g_ptr_array_index(proplist, i);
				GString *prop_name = g_ptr_array_index(proplist, i+1);

				if (failed_ndx == i) continue;

				if (GSTR_EQUAL(prop_ns, "DAV:")) {
					g_string_append_printf(buffer, "<D:%s/>", prop_name->str);
				} else {
					g_string_append_printf(buffer, "<R:%s xmlns:R=\"%s\"/>", prop_name->str, prop_ns->str);
				}
			}
			g_string_append_len(buffer, CONST_STR_LEN("</D:prop><D:status>" STATUS_LINE_424 "</D:status></D:propstat>"));
		}
	}
	g_string_append_len(buffer, CONST_STR_LEN("</D:response>\n"));

	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_End));
	dav_response_xml_header(server, request);

cleanup:
	g_ptr_array_free(proplist, TRUE);
	g_string_free(physpath, TRUE);
}


static void request_mkcol(DavServer *server, DavRequest *request) {
	GString *physpath = g_string_sized_new(0);

	if (NULL != request->request_xml || NULL != request->request_filename) {
		dav_response_error(server, request, 415);
		goto cleanup;
	}

	dav_server_map_path(physpath, server, request->url_path);

	if (-1 == mkdir(physpath->str, WEBDAV_DIR_MODE)) {
		switch (errno) {
		case EACCES:
		case EROFS:
		case EPERM: /* linux: The file system containing pathname does not support the creation of directories. */
			dav_response_error(server, request, 403);
			goto cleanup;
		case EEXIST:
			dav_response_error(server, request, 405);
			goto cleanup;
		case ENOTDIR:
		case ENOENT:
			dav_response_error(server, request, 409);
			goto cleanup;
		case ENOSPC:
			dav_response_error(server, request, 507);
			goto cleanup;
		default:
			/* TODO: log error */
			dav_response_error(server, request, 500);
			goto cleanup;
		}
	}

	request->response_status = 201;

	if ('/' != request->url_path->str[request->url_path->len-1]) {
		dav_append_trailing_slash(request->url_path);
		dav_response_location(server, request, request->url_path);
	}

cleanup:
	g_string_free(physpath, TRUE);
}

static void request_get_and_head(DavServer *server, DavRequest *request) {
	GString *physpath = g_string_sized_new(0);
	GString *mimetype;
	struct stat st;
	int fd = -1;

	dav_server_map_path(physpath, server, request->url_path);

	if (-1 == dav_intrsafe_open(physpath->str, O_RDONLY)) {
		switch (errno) {
		case EACCES:
			dav_response_error(server, request, 403);
			goto cleanup;
		case ENOENT:
		case ENOTDIR:
			dav_response_error(server, request, 404);
			goto cleanup;
		default:
			dav_response_error(server, request, 500);
			/* TODO: log error */
			goto cleanup;
		}
	}

	if (-1 == fstat(fd, &st)) {
		dav_response_error(server, request, 500);
		/* TODO: log error */
		goto cleanup;
	}

	if (S_ISDIR(st.st_mode) && '/' != request->url_path->str[request->url_path->len-1]) {
		dav_append_trailing_slash(request->url_path);
		dav_response_location(server, request, request->url_path);
		request->response_status = 301;
		goto cleanup;
	}

	if (S_ISDIR(st.st_mode)) {
		/* TODO: print dirlist? */
		request->response_status = 200;
		goto cleanup;
	} else if (!S_ISREG(st.st_mode)) {
		dav_response_error(server, request, 403);
		goto cleanup;
	}

	if (DAV_HEAD != request->method) {
		/* no real content on head */
		request->response_filename = physpath;
		physpath = NULL;
		request->response_fd = fd;
		fd = -1;
	}

	/* TODO: support etag/if-modified and similar/ranged requests... */

	mimetype = dav_server_get_mimetype(server, physpath);
	g_string_append_printf(dav_response_get_header(server, request), "Content-Length: %" G_GOFFSET_FORMAT "\r\nContent-Type: %s\r\n", (goffset) st.st_size, mimetype->str);

	request->response_status = 200;

cleanup:
	if (NULL != physpath) g_string_free(physpath, TRUE);
	if (-1 != fd) dav_intrsafe_close(fd);
}

static gboolean request_del_recurse(DavServer *server, DavRequest *request, GString *url, GString *phys, GString *buffer, GString *failed) {
	struct stat st;

	if (-1 == stat(phys->str, &st)) {
		guint status;
		switch (errno) {
		case EACCES:
			status = 403;
			break;
		case ENOENT:
		case ENOTDIR:
			status = 404;
			break;
		default:
			/* TODO: log error */
			status = 500;
			break;
		}
		/* TODO: encode url */
		g_string_append_printf(failed, "<D:response><D:href>%s</D:href><D:status>HTTP/1.1 %i %s</D:status></D:response>\n", url->str, status, dav_response_status_str(status));
		return FALSE;
	} else if (S_ISDIR(st.st_mode)) {
		guint curphyslen, cururllen;
		GDir *dir;
		gboolean recerrors = FALSE;

		dav_append_trailing_slash(phys);
		dav_append_trailing_slash(url);

		curphyslen = phys->len;
		cururllen = url->len;

		dir = g_dir_open(phys->str, 0, NULL);
		if (NULL != dir) {
			const char *entry;
			while (NULL != (entry = g_dir_read_name(dir))) {
				g_string_truncate(phys, curphyslen);
				g_string_append(phys, entry);
				g_string_truncate(url, cururllen);
				g_string_append(url, entry);

				if (!request_del_recurse(server, request, url, phys, buffer, failed)) {
					recerrors = TRUE;
				}
			}
			g_dir_close(dir);
		}

		g_string_truncate(phys, curphyslen-1);
		g_string_truncate(url, cururllen);

		if (-1 == rmdir(phys->str)) {
			guint status;
			switch (errno) {
			case EPERM:
			case EACCES:
			case EBUSY:
			case EROFS:
				status = 403;
				break;
			case EEXIST:
			case ENOTEMPTY:
				if (!recerrors) return TRUE;
				status = 403;
				break;
			case ENOENT:
			case ENOTDIR:
				status = 404;
				break;
			default:
				/* TODO: log error */
				status = 500;
				break;
			}
			/* TODO: encode url */
			g_string_append_printf(failed, "<D:response><D:href>%s</D:href><D:status>HTTP/1.1 %i %s</D:status></D:response>\n", url->str, status, dav_response_status_str(status));
			return FALSE;
		} else {
			g_string_append_printf(buffer, "<D:href>%s</D:href>\n", url->str);
			return TRUE;
		}
	} else {
		if (-1 == unlink(phys->str)) {
			guint status;
			switch (errno) {
			case EPERM:
			case EACCES:
			case EBUSY:
			case EROFS:
				status = 403;
				break;
			case ENOENT:
			case ENOTDIR:
				status = 404;
				break;
			default:
				/* TODO: log error */
				status = 500;
				break;
			}
			/* TODO: encode url */
			g_string_append_printf(failed, "<D:response><D:href>%s</D:href><D:status>HTTP/1.1 %i %s</D:status></D:response>\n", url->str, status, dav_response_status_str(status));
			return FALSE;
		} else {
			g_string_append_printf(buffer, "<D:href>%s</D:href>\n", url->str);
			return TRUE;
		}
	}
}


static void request_del(DavServer *server, DavRequest *request) {
	GString *physpath = g_string_sized_new(0);
	GString *buffer = dav_response_prepare_buffer(server, request, 0);
	GString *failed = g_string_sized_new(0);
	guint rawbufsize, oldbufsize;

	/* Depth must be unspecified or infinity for directories - we don't check it */

	dav_server_map_path(physpath, server, request->url_path);

	g_string_append_len(buffer, GSTR_LEN(&XML_Start));
	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_Start));

	rawbufsize = buffer->len;
	g_string_append_len(buffer, CONST_STR_LEN("<D:response>"));
	oldbufsize = buffer->len;

	request_del_recurse(server, request, request->url_path, physpath, buffer, failed);

	/* TODO: del db properties */

	if (oldbufsize == buffer->len) {
		g_string_truncate(buffer, rawbufsize);
	} else {
		g_string_append_len(buffer, CONST_STR_LEN("<D:status>" STATUS_LINE_200 "</D:status></D:response>"));
	}

	g_string_append_len(buffer, GSTR_LEN(failed));

	g_string_append_len(buffer, GSTR_LEN(&MultiStatus_End));
	dav_response_xml_header(server, request);
	request->response_status = 207;
}

static void request_put(DavServer *server, DavRequest *request) {
	GString *physpath = g_string_sized_new(0);
	int fd;

	dav_server_map_path(physpath, server, request->url_path);

	if (NULL != request->request_xml) {
		/* write a tmp file */
		GString *tmpfile = g_string_sized_new(physpath->len + 7);
		g_string_append_len(tmpfile, GSTR_LEN(physpath));
		g_string_append_len(tmpfile, CONST_STR_LEN(".XXXXXX"));

		if (-1 == (fd = mktemp(tmpfile->str))) {
			switch (errno) {
			}
		}
	}

cleanup:
	;
}

void dav_request_execute(DavServer *server, DavRequest *request) {
	dav_response_clear(server, request);

	/* check url */
	if (0 == request->url_path->len) {
		dav_response_error(server, request, 400);
		return;
	}

	if ('/' != request->url_path->str[0]) {
		/* only the OPTIONS method allows an url not starting with "/", and it must be "*", nothing else */
		if ('*' != request->url_path->str[0] || 1 != request->url_path->len || DAV_OPTIONS != request->method) {
			dav_response_error(server, request, 400);
			return;
		}
	}

	switch (request->method) {
	case DAV_OPTIONS:
		request_options(server, request);
		break;
	case DAV_PROPFIND:
		request_propfind(server, request);
		break;
	case DAV_PROPPATCH:
		request_proppatch(server, request);
		break;
	case DAV_MKCOL:
		request_mkcol(server, request);
		break;
	case DAV_GET:
	case DAV_HEAD:
		request_get_and_head(server, request);
		break;
	case DAV_DELETE:
		request_del(server, request);
		break;
	case DAV_PUT:
		request_put(server, request);
		break;
	case DAV_COPY:
	case DAV_MOVE:
	case DAV_LOCK:
	case DAV_UNLOCK:
		break;
	}

	if (0 == request->response_status) {
		dav_response_error(server, request, 500);
	}

	if (NULL != request->response_text) {
		g_string_append_printf(dav_response_get_header(server, request), "Content-Length: %" G_GOFFSET_FORMAT "\r\n", (goffset) request->response_text->len);
	} else if (NULL == request->response_filename && -1 == request->response_fd) {
		if (DAV_HEAD != request->method || 200 != request->response_status) {
			g_string_append_len(dav_response_get_header(server, request), CONST_STR_LEN("Content-Length: 0\r\n"));
		}
	}
}

void dav_request_init(DavServer *server, DavRequest *request) {
	UNUSED(server);
	memset(request, 0, sizeof(*request));

	request->method = (DavMethod) -1;
	request->url_path = g_string_sized_new(128);
	request->depth = DAV_DEPTH_UNSPECIFIED;
	request->lock_token = NULL;
	request->overwrite = TRUE;
	request->timeout = DAV_TIMEOUT_INFINITY;
	request->request_xml = NULL;
	request->request_filename = NULL;
	request->http_base = g_string_sized_new(128);

	request->response_status = 0;
	request->response_header = NULL;
	request->response_text = NULL;
	request->response_filename = NULL;
	request->response_fd = -1;
}

void dav_request_clear(DavServer *server, DavRequest *request) {
	dav_response_clear(server, request);

	g_string_free(request->url_path, TRUE);
	request->url_path = NULL;
	g_string_free(request->lock_token, TRUE);
	request->lock_token = NULL;
	g_string_free(request->request_xml, TRUE);
	request->request_xml = NULL;
	g_string_free(request->request_filename, TRUE);
	request->request_filename = NULL;
	g_string_free(request->http_base, TRUE);
	request->http_base = NULL;
}
