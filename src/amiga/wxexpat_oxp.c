/*
 * wxexpat_oxp.c: the Expat calls wxXmlDocument makes (src/amiga/expat/expat.h),
 * on OXP, the Team's own XML parser (DalsinAI/gfopenxml oxp/, built into the
 * same archive). It takes the place of the Expat wx carries: libwxexpat holds
 * this and OXP.
 *
 * Names are plain XML 1.0 names (no namespace processing, as
 * XML_ParserCreate gives), every string is UTF-8, and a CDATA section is a
 * start, its text and an end, as Expat reports it. The XML declaration goes
 * to the default handler, which is all wxXmlDocument uses it for.
 * Copyright (c) 2026 AmiCradle Team
 */
#include <stdlib.h>
#include <string.h>
#include "expat.h"
#include "oxp.h"

struct wxtk_xml_parser {
    oxp_parser *oxp;
    void *ud;
    XML_StartElementHandler start;
    XML_EndElementHandler end;
    XML_CharacterDataHandler chars;
    XML_StartCdataSectionHandler cdata_start;
    XML_EndCdataSectionHandler cdata_end;
    XML_CommentHandler comment;
    XML_ProcessingInstructionHandler pi;
    XML_StartDoctypeDeclHandler doctype_start;
    XML_EndDoctypeDeclHandler doctype_end;
    XML_DefaultHandler deflt;
    const char **atts;              /* name, value, ..., NULL for start */
    int natts;
    int error;
};

static void on_start_document(void *u, const char *version, const char *encoding, int standalone)
{
    XML_Parser p = u;
    char *d;
    size_t n;
    (void)standalone;
    if (!p->deflt || !version) return;
    n = 32 + strlen(version) + (encoding ? strlen(encoding) : 0);
    d = malloc(n);
    if (!d) return;
    strcpy(d, "<?xml version=\"");
    strcat(d, version);
    strcat(d, "\"");
    if (encoding) { strcat(d, " encoding=\""); strcat(d, encoding); strcat(d, "\""); }
    strcat(d, "?>");
    p->deflt(p->ud, d, (int)strlen(d));
    free(d);
}

static void on_doctype(void *u, const char *name, const char *pub, const char *sys, int internal)
{
    XML_Parser p = u;
    if (p->doctype_start) p->doctype_start(p->ud, name, sys, pub, internal);
}

static void on_end_doctype(void *u)
{
    XML_Parser p = u;
    if (p->doctype_end) p->doctype_end(p->ud);
}

/* with OXP_OPT_NO_NAMESPACES the local part is the whole name */
static const char *qname(const char *local, const char *prefix, char **made)
{
    size_t a, b;
    *made = NULL;
    if (!prefix) return local;
    a = strlen(prefix); b = strlen(local);
    *made = malloc(a + b + 2);
    if (!*made) return local;
    memcpy(*made, prefix, a);
    (*made)[a] = ':';
    memcpy(*made + a + 1, local, b + 1);
    return *made;
}

static void on_start(void *u, const char *local, const char *prefix, const char *uri,
                     int nns, const oxp_ns *ns, int nattrs, const oxp_attr *attrs)
{
    XML_Parser p = u;
    char *made, **names;
    int i;
    (void)uri; (void)nns; (void)ns;
    if (!p->start) return;
    if (nattrs + 1 > p->natts) {
        const char **a = realloc(p->atts, (size_t)(nattrs + 1) * 2 * sizeof *a);
        if (!a) { oxp_stop(p->oxp); return; }
        p->atts = a; p->natts = nattrs + 1;
    }
    names = calloc((size_t)nattrs + 1, sizeof *names);
    for (i = 0; i < nattrs; i++) {
        p->atts[2 * i] = qname(attrs[i].local, attrs[i].prefix, names ? &names[i] : &made);
        p->atts[2 * i + 1] = attrs[i].value;
    }
    p->atts[2 * nattrs] = NULL;
    {
        const char *n = qname(local, prefix, &made);
        p->start(p->ud, n, p->atts);
        free(made);
    }
    if (names) { for (i = 0; i < nattrs; i++) free(names[i]); free(names); }
}

static void on_end(void *u, const char *local, const char *prefix, const char *uri)
{
    XML_Parser p = u;
    char *made;
    (void)uri;
    if (!p->end) return;
    p->end(p->ud, qname(local, prefix, &made));
    free(made);
}

static void text(XML_Parser p, const char *s, size_t len)
{
    while (len) {
        int n = len > 0x7fff0000UL ? 0x7fff0000 : (int)len;
        p->chars(p->ud, s, n);
        s += n; len -= (size_t)n;
    }
}

static void on_chars(void *u, const char *s, size_t len)
{
    XML_Parser p = u;
    if (p->chars) text(p, s, len);
}

static void on_cdata(void *u, const char *s, size_t len)
{
    XML_Parser p = u;
    if (p->cdata_start) p->cdata_start(p->ud);
    if (p->chars && len) text(p, s, len);
    if (p->cdata_end) p->cdata_end(p->ud);
}

static void on_comment(void *u, const char *s)
{
    XML_Parser p = u;
    if (p->comment) p->comment(p->ud, s);
}

static void on_pi(void *u, const char *target, const char *data)
{
    XML_Parser p = u;
    if (p->pi) p->pi(p->ud, target, data ? data : "");
}

static const oxp_handler handler = {
    on_start_document, NULL, on_doctype, on_end_doctype, NULL,
    on_start, on_end, on_chars, on_cdata, on_comment, on_pi,
    NULL, NULL, NULL
};

XML_Parser XML_ParserCreate(const XML_Char *encoding)
{
    XML_Parser p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->oxp = oxp_new(&handler, p, NULL);
    if (!p->oxp) { free(p); return NULL; }
    oxp_set_options(p->oxp, OXP_OPT_NO_NAMESPACES);
    if (encoding) {
        oxp_encoding e = OXP_ENC_AUTO;
        if (!strcmp(encoding, "UTF-8")) e = OXP_ENC_UTF8;
        else if (!strcmp(encoding, "ISO-8859-1")) e = OXP_ENC_LATIN1;
        else if (!strcmp(encoding, "US-ASCII")) e = OXP_ENC_ASCII;
        else if (!strcmp(encoding, "UTF-16LE")) e = OXP_ENC_UTF16LE;
        else if (!strcmp(encoding, "UTF-16BE")) e = OXP_ENC_UTF16BE;
        if (e != OXP_ENC_AUTO) {
            oxp_set_encoding(p->oxp, e);
            oxp_set_options(p->oxp, OXP_OPT_NO_NAMESPACES | OXP_OPT_IGNORE_DECL_ENCODING);
        }
    }
    return p;
}

void XML_ParserFree(XML_Parser p)
{
    if (!p) return;
    oxp_free(p->oxp);
    free(p->atts);
    free(p);
}

void XML_SetUserData(XML_Parser p, void *ud) { p->ud = ud; }
void XML_SetElementHandler(XML_Parser p, XML_StartElementHandler s, XML_EndElementHandler e) { p->start = s; p->end = e; }
void XML_SetCharacterDataHandler(XML_Parser p, XML_CharacterDataHandler h) { p->chars = h; }
void XML_SetCdataSectionHandler(XML_Parser p, XML_StartCdataSectionHandler s, XML_EndCdataSectionHandler e) { p->cdata_start = s; p->cdata_end = e; }
void XML_SetCommentHandler(XML_Parser p, XML_CommentHandler h) { p->comment = h; }
void XML_SetProcessingInstructionHandler(XML_Parser p, XML_ProcessingInstructionHandler h) { p->pi = h; }
void XML_SetDoctypeDeclHandler(XML_Parser p, XML_StartDoctypeDeclHandler s, XML_EndDoctypeDeclHandler e) { p->doctype_start = s; p->doctype_end = e; }
void XML_SetDefaultHandler(XML_Parser p, XML_DefaultHandler h) { p->deflt = h; }
void XML_SetUnknownEncodingHandler(XML_Parser p, XML_UnknownEncodingHandler h, void *d) { (void)p; (void)h; (void)d; }

enum XML_Status XML_Parse(XML_Parser p, const char *s, int len, int isFinal)
{
    oxp_code c = oxp_parse(p->oxp, s, len > 0 ? (size_t)len : 0, isFinal);
    if (c == OXP_OK) return XML_STATUS_OK;
    p->error = (int)c;
    return XML_STATUS_ERROR;
}

enum XML_Error XML_GetErrorCode(XML_Parser p) { return (enum XML_Error)p->error; }

const XML_LChar *XML_ErrorString(enum XML_Error code)
{
    return code == XML_ERROR_NONE ? "no error" : oxp_code_name((oxp_code)code);
}

XML_Size XML_GetCurrentLineNumber(XML_Parser p) { return oxp_line(p->oxp); }
XML_Size XML_GetCurrentColumnNumber(XML_Parser p) { return oxp_column(p->oxp); }
