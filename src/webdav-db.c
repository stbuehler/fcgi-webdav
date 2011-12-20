#include "fcgi-webdav-intern.h"

guint dav_db_get_property(GString* buffer, DavServer* server, DavRequest* request, GString* physpath, GString* prop_ns, GString* prop_name) {
	UNUSED(buffer);
	UNUSED(server);
	UNUSED(request);
	UNUSED(physpath);
	UNUSED(prop_ns);
	UNUSED(prop_name);

	/* TODO: implement */

	return 404;
}

void dav_db_all_properties(GString* buffer, DavServer* server, DavRequest* request, GString* physpath) {
	UNUSED(buffer);
	UNUSED(server);
	UNUSED(request);
	UNUSED(physpath);

	/* TODO: implement */
}

void dav_db_name_properties(GString* buffer, DavServer* server, DavRequest* request, GString* physpath) {
	UNUSED(buffer);
	UNUSED(server);
	UNUSED(request);
	UNUSED(physpath);

	/* TODO: implement */
}

gboolean dav_db_set_properties_begin(DavServer* server, DavRequest* request) {
	UNUSED(server);
	UNUSED(request);

	/* TODO: implement */

	return TRUE;
}

guint dav_db_set_property_check(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value) {
	UNUSED(server);
	UNUSED(request);
	UNUSED(physpath);
	UNUSED(prop_ns);
	UNUSED(prop_name);

	/* TODO: implement */

	if (NULL == value) return 200;

	return 403;
}

gboolean dav_db_set_property(DavServer *server, DavRequest *request, GString* physpath, GString* prop_ns, GString* prop_name, GString *value) {
	UNUSED(server);
	UNUSED(request);
	UNUSED(physpath);
	UNUSED(prop_ns);
	UNUSED(prop_name);
	UNUSED(value);

	/* changes are already done in _check */
	return TRUE;
}

void dav_db_set_properties_rollback(DavServer *server, DavRequest *request) {
	UNUSED(server);
	UNUSED(request);

	/* TODO: implement */
}

gboolean dav_db_set_properties_commit(DavServer* server, DavRequest* request) {
	UNUSED(server);
	UNUSED(request);

	/* TODO: implement */

	return TRUE;
}
