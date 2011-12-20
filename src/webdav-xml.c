#include "fcgi-webdav-intern.h"

gboolean dav_xml_parse_propfind(DavServer *server, DavRequest *request, gboolean *allprops, gboolean *nameprops, GPtrArray **proplist) {
	UNUSED(server);
	UNUSED(request);

	/* TODO: parse request_xml data */
	*proplist = NULL;
	*allprops = TRUE;
	*nameprops = FALSE;

	return TRUE;
}

gboolean dav_xml_parse_proppatch(DavServer* server, DavRequest* request, GPtrArray** proplist) {
	UNUSED(server);
	UNUSED(request);

	/* TODO: parse request_xml data */
	*proplist = dav_new_stringlist();

	return TRUE;
}
