
#include "fcgi-webdav-intern.h"

void dav_server_map_path(GString *physpath, DavServer *server, GString *urlpath) {
	dav_path_simplify(urlpath);

	g_string_truncate(physpath, 0);
	g_string_append_len(physpath, GSTR_LEN(server->docroot));
	g_string_append_len(physpath, GSTR_LEN(urlpath));

	/* remove trailing slash(es) */
	while (physpath->len > 1 && '/' == physpath->str[physpath->len-1]) {
		physpath->str[--physpath->len] = '\0';
	}
}

GString *dav_server_get_mimetype(DavServer *server, GString *physpath) {
	static const GString appoctstream = { CONST_STR_LEN("application/octet-stream"), 0 };
	UNUSED(server);
	UNUSED(physpath);

	/* TODO: implement mimetypes */

	return (GString*) &appoctstream;
}

/* strftime uses "locale's abbreviated" names, so this is strftime("%a, %d %b %Y %H:%M:%S GMT") without locale */
static void string_append_rfc1123_date(GString *s, time_t ts) {
	const char wkdays[] = "SunMonTueWedThuFriSat";
	const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	struct tm t;
	gmtime_r(&ts, &t);

	g_string_append_printf(s, "%.3s, %02i %.3s %04i %02i:%02i:%02i GMT", wkdays + 3*t.tm_wday, t.tm_mday, months + 3*t.tm_mon, 1900+t.tm_year, t.tm_hour, t.tm_min, t.tm_sec);
}

static void string_append_iso8601_date(GString *s, time_t ts) {
	char ctime_buf[sizeof("0000-00-00T00:00:00Z")] = { 0 };
	struct tm t;
	gmtime_r(&ts, &t);

	strftime(ctime_buf, sizeof(ctime_buf), "%Y-%m-%dT%H:%M:%SZ", &t);
	g_string_append(s, ctime_buf);
}

guint dav_server_get_property(GString* buffer, DavServer* server, DavRequest* request, GString* physpath, struct stat* st, GString* prop_ns, GString* prop_name) {
	if (GSTR_EQUAL(prop_ns, "DAV:")) {
		if (GSTR_EQUAL(prop_name, "creationdate")) {
			g_string_append_len(buffer, CONST_STR_LEN("<D:creationdate ns0:dt=\"dateTime.tz\">"));
			string_append_iso8601_date(buffer, st->st_ctime);
			g_string_append_len(buffer, CONST_STR_LEN("</D:creationdate>"));
			return 200;
		} else if (GSTR_EQUAL(prop_name, "displayname")) {
			/* TODO: read from db, defaults to filename? */
			const gchar *fname = strrchr(physpath->str, '/');
			if (NULL != fname) {
				g_string_append_printf(buffer, "<D:displayname>%s</D:displayname>", fname+1);
				return 200;
			}
		} else if (GSTR_EQUAL(prop_name, "getcontentlanguage")) {
			/* we don't use Content-Language */
		} else if (GSTR_EQUAL(prop_name, "getcontentlength")) {
			if(S_ISREG(st->st_mode)) {
				g_string_append_printf(buffer, "<D:getcontentlength>%" G_GOFFSET_FORMAT "</D:getcontentlength>", (goffset) st->st_size);
				return 200;
			}
		} else if (GSTR_EQUAL(prop_name, "getcontenttype")) {
			if (S_ISDIR(st->st_mode)) {
				g_string_append_len(buffer, CONST_STR_LEN("<D:getcontenttype>httpd/unix-directory</D:getcontenttype>"));
				return 200;
			} else if(S_ISREG(st->st_mode)) {
				GString *mimetype = dav_server_get_mimetype(server, physpath);
				g_string_append_len(buffer, CONST_STR_LEN("<D:getcontenttype>"));
				g_string_append_len(buffer, GSTR_LEN(mimetype));
				g_string_append_len(buffer, CONST_STR_LEN("</D:getcontenttype>"));
				return 200;
			}
		} else if (GSTR_EQUAL(prop_name, "getetag")) {
			/* TODO: etag */
		} else if (GSTR_EQUAL(prop_name, "getlastmodified")) {
			g_string_append_len(buffer, CONST_STR_LEN("<D:getlastmodified ns0:dt=\"dateTime.rfc1123\">"));
			string_append_rfc1123_date(buffer, st->st_mtime);
			g_string_append_len(buffer, CONST_STR_LEN("</D:getlastmodified>"));
			return 200;
		} else if (GSTR_EQUAL(prop_name, "lockdiscovery")) {
			/* "access control": we don't tell anyone :) */
			return 403;
		} else if (GSTR_EQUAL(prop_name, "resourcetype")) {
			if (S_ISDIR(st->st_mode)) {
				g_string_append_len(buffer, CONST_STR_LEN("<D:resourcetype><D:collection/></D:resourcetype>"));
				return 200;
			} else {
				g_string_append_len(buffer, CONST_STR_LEN("<D:resourcetype/>"));
				return 200;
			}
		} else if (GSTR_EQUAL(prop_name, "source")) {
			/* we don't manage source links */
		} else if (GSTR_EQUAL(prop_name, "supportedlock")) {
			/* "access control": we don't tell anyone :) */
			return 403;
		}
	} else {
		return dav_db_get_property(buffer, server, request, physpath, prop_ns, prop_name);
	}
	return 404;
}

void dav_server_all_properties(GString* buffer, DavServer* server, DavRequest* request, GString* physpath, struct stat* st) {
	g_string_append_len(buffer, CONST_STR_LEN("<D:creationdate ns0:dt=\"dateTime.tz\">"));
	string_append_iso8601_date(buffer, st->st_ctime);
	g_string_append_len(buffer, CONST_STR_LEN("</D:creationdate>"));

	{
		const gchar *fname = strrchr(physpath->str, '/');
		if (NULL != fname) {
			g_string_append_printf(buffer, "<D:displayname>%s</D:displayname>", fname+1);
		}
	}

	if(S_ISREG(st->st_mode)) {
		g_string_append_printf(buffer, "<D:getcontentlength>%" G_GOFFSET_FORMAT "</D:getcontentlength>", (goffset) st->st_size);
	}

	if (S_ISDIR(st->st_mode)) {
		g_string_append_len(buffer, CONST_STR_LEN("<D:getcontenttype>httpd/unix-directory</D:getcontenttype>"));
	} else if(S_ISREG(st->st_mode)) {
		GString *mimetype = dav_server_get_mimetype(server, physpath);
		g_string_append_len(buffer, CONST_STR_LEN("<D:getcontenttype>"));
		g_string_append_len(buffer, GSTR_LEN(mimetype));
		g_string_append_len(buffer, CONST_STR_LEN("</D:getcontenttype>"));
	}

	/* TODO: etag */

	g_string_append_len(buffer, CONST_STR_LEN("<D:getlastmodified ns0:dt=\"dateTime.rfc1123\">"));
	string_append_rfc1123_date(buffer, st->st_mtime);
	g_string_append_len(buffer, CONST_STR_LEN("</D:getlastmodified>"));

	if (S_ISDIR(st->st_mode)) {
		g_string_append_len(buffer, CONST_STR_LEN("<D:resourcetype><D:collection/></D:resourcetype>"));
	} else {
		g_string_append_len(buffer, CONST_STR_LEN("<D:resourcetype/>"));
	}

	dav_db_all_properties(buffer, server, request, physpath);
}

void dav_server_name_properties(GString* buffer, DavServer* server, DavRequest* request, GString* physpath, struct stat* st) {
	g_string_append_len(buffer, CONST_STR_LEN("<D:creationdate/>"));
	g_string_append_len(buffer, CONST_STR_LEN("<D:displayname/>"));

	if(S_ISREG(st->st_mode)) {
		g_string_append_len(buffer, CONST_STR_LEN("<D:getcontentlength/>"));
	}

	g_string_append_len(buffer, CONST_STR_LEN("<D:getcontenttype/>"));

	/* TODO: etag */

	g_string_append_len(buffer, CONST_STR_LEN("<D:getlastmodified/>"));
	g_string_append_len(buffer, CONST_STR_LEN("<D:resourcetype/>"));

	dav_db_name_properties(buffer, server, request, physpath);
}

gboolean dav_server_set_properties_begin(DavServer *server, DavRequest *request) {
	if (!dav_db_set_properties_begin(server, request)) return FALSE;

	return TRUE;
}

guint dav_server_set_property_check(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value) {
	if (GSTR_EQUAL(prop_ns, "DAV:")) {
		if (GSTR_EQUAL(prop_name, "creationdate")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "displayname")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "getcontentlanguage")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "getcontentlength")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "getcontenttype")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "getetag")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "getlastmodified")) {
			/* TODO: support something like "touch" ? */
			return 403;
		} else if (GSTR_EQUAL(prop_name, "lockdiscovery")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "resourcetype")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "source")) {
			return 403;
		} else if (GSTR_EQUAL(prop_name, "supportedlock")) {
			return 403;
		} else {
			if (NULL == value) return 200;
			return 403;
		}
	} else {
		return dav_db_set_property_check(server, request, physpath, prop_ns, prop_name, value);
	}
}

gboolean dav_server_set_property(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value) {
	if (GSTR_EQUAL(prop_ns, "DAV:")) {
		return TRUE;
	} else {
		return dav_db_set_property(server, request, physpath, prop_ns, prop_name, value);
	}
}

void dav_server_set_properties_rollback(DavServer *server, DavRequest *request) {
	dav_db_set_properties_rollback(server, request);
}

gboolean dav_server_set_properties_commit(DavServer* server, DavRequest* request) {
	if (!dav_db_set_properties_commit(server, request)) return FALSE;

	return TRUE;
}
