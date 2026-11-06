/*
 * expat.h: the part of Expat's API that wxXmlDocument (wx's src/xml/xml.cpp)
 * calls, served by OXP, the Team's own XML parser (DalsinAI/gfopenxml oxp/),
 * through src/amiga/wxexpat_oxp.c. No Expat code is used: this header is
 * written from the calls xml.cpp makes, so wx is built with it in place of
 * the copy of Expat it carries.
 * Copyright (c) 2026 AmiCradle Team
 */
#ifndef WXTK_EXPAT_H
#define WXTK_EXPAT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef char XML_Char;
typedef char XML_LChar;
typedef unsigned char XML_Bool;
#define XML_TRUE  ((XML_Bool)1)
#define XML_FALSE ((XML_Bool)0)
typedef unsigned long XML_Size;
typedef long XML_Index;

typedef struct wxtk_xml_parser *XML_Parser;

enum XML_Status { XML_STATUS_ERROR = 0, XML_STATUS_OK = 1, XML_STATUS_SUSPENDED = 2 };

/* the parse's error: OXP's own code (0: none) */
enum XML_Error { XML_ERROR_NONE = 0 };

/* the version wxXmlDocument::GetLibraryVersionInfo reports: OXP's */
#define XML_MAJOR_VERSION 0
#define XML_MINOR_VERSION 1
#define XML_MICRO_VERSION 0

typedef struct {
    int map[256];
    void *data;
    int (*convert)(void *data, const char *s);
    void (*release)(void *data);
} XML_Encoding;

typedef void (*XML_StartElementHandler)(void *userData, const XML_Char *name, const XML_Char **atts);
typedef void (*XML_EndElementHandler)(void *userData, const XML_Char *name);
typedef void (*XML_CharacterDataHandler)(void *userData, const XML_Char *s, int len);
typedef void (*XML_StartCdataSectionHandler)(void *userData);
typedef void (*XML_EndCdataSectionHandler)(void *userData);
typedef void (*XML_CommentHandler)(void *userData, const XML_Char *data);
typedef void (*XML_ProcessingInstructionHandler)(void *userData, const XML_Char *target, const XML_Char *data);
typedef void (*XML_StartDoctypeDeclHandler)(void *userData, const XML_Char *doctypeName,
                                            const XML_Char *sysid, const XML_Char *pubid,
                                            int has_internal_subset);
typedef void (*XML_EndDoctypeDeclHandler)(void *userData);
typedef void (*XML_DefaultHandler)(void *userData, const XML_Char *s, int len);
typedef int (*XML_UnknownEncodingHandler)(void *encodingHandlerData, const XML_Char *name, XML_Encoding *info);

XML_Parser XML_ParserCreate(const XML_Char *encoding);
void XML_ParserFree(XML_Parser parser);
void XML_SetUserData(XML_Parser parser, void *userData);
void XML_SetElementHandler(XML_Parser parser, XML_StartElementHandler start, XML_EndElementHandler end);
void XML_SetCharacterDataHandler(XML_Parser parser, XML_CharacterDataHandler handler);
void XML_SetCdataSectionHandler(XML_Parser parser, XML_StartCdataSectionHandler start, XML_EndCdataSectionHandler end);
void XML_SetCommentHandler(XML_Parser parser, XML_CommentHandler handler);
void XML_SetProcessingInstructionHandler(XML_Parser parser, XML_ProcessingInstructionHandler handler);
void XML_SetDoctypeDeclHandler(XML_Parser parser, XML_StartDoctypeDeclHandler start, XML_EndDoctypeDeclHandler end);
/* called with the XML declaration only, as "<?xml version=... encoding=...?>" */
void XML_SetDefaultHandler(XML_Parser parser, XML_DefaultHandler handler);
/* OXP reads UTF-8, UTF-16, ISO-8859-1, US-ASCII and Windows-1252 itself: kept, never called */
void XML_SetUnknownEncodingHandler(XML_Parser parser, XML_UnknownEncodingHandler handler, void *encodingHandlerData);
enum XML_Status XML_Parse(XML_Parser parser, const char *s, int len, int isFinal);
enum XML_Error XML_GetErrorCode(XML_Parser parser);
const XML_LChar *XML_ErrorString(enum XML_Error code);
XML_Size XML_GetCurrentLineNumber(XML_Parser parser);
XML_Size XML_GetCurrentColumnNumber(XML_Parser parser);

#ifdef __cplusplus
}
#endif
#endif
