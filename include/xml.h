#ifndef XML_H
#define XML_H

#include "types.h"
#include <new>
#include "db_log.h"

extern "C" {
int sprintf(char* buf, const char* fmt, ...);
int strcmp(const char* a, const char* b);
}

// game/xml.cpp: minimal XML reader/writer used by the debug tools (XSDSchemaSof documents).
// Member functions do not touch `this`; `buf`/`out` are caller-provided character buffers.
// The inline helpers below are not called by any matched unit; they exist because their string
// literals are what the original xml.o carries in .rodata ahead of the writer's own strings.
class XmlSimple {
public:
    int GetXmlStart(char** out, const char* src, const char* tag);
    int GetXmlNext(char** out, const char* src, const char* tag);
    int GetXmlElem(char* out, const char* src, const char* tag);
    int SetXmlStart(int* size, char* buf);
    int SetXmlEnd(int* size, char* buf);
    int SetXmlElemStart(int* size, char* buf);
    int SetXmlElemEnd(int* size, char* buf);
    int SetXmlElem(int* size, char* buf, const char* name, const char* value);

    int SetXmlElem(int* size, char* buf, const char* name, long value)
    {
        char tmp[32];
        sprintf(tmp, "%ld", value);
        return SetXmlElem(size, buf, name, tmp);
    }
    int SetXmlElem(int* size, char* buf, const char* name, bool value)
    {
        return SetXmlElem(size, buf, name, value ? "true" : "false");
    }
    int GetXmlElem(bool* out, const char* src, const char* tag)
    {
        char tmp[256];
        if (!GetXmlElem(tmp, src, tag)) {
            return 0;
        }
        *out = strcmp(tmp, "true") == 0 || strcmp(tmp, "True") == 0 || strcmp(tmp, "TRUE") == 0;
        return 1;
    }
};

// Debug-tool XML document helpers (file layout of the "Node" records written by the tools).
inline int ReadXml(const char* name, char* buf, int max, int size)
{
    if (size > max) {
        pLog->err(0, 0, "ReadXml : FileSize over [%d]", size);
        return 0;
    }
    if (buf == NULL) {
        pLog->err(0, 0, "ReadXml : File Not Found [%s]", name);
        return 0;
    }
    return 1;
}

inline void WriteNode(XmlSimple* xml, int* size, char* buf, const char* value)
{
    xml->SetXmlElem(size, buf, "Node", value);
    xml->SetXmlElem(size, buf, "SetFlg", value);
    xml->SetXmlElem(size, buf, "SetOwner", value);
    xml->SetXmlElem(size, buf, "SetEdit", value);
    xml->SetXmlElem(size, buf, "NamePac", value);
    xml->SetXmlElem(size, buf, "CutNo", value);
    xml->SetXmlElem(size, buf, "Frame", value);
    xml->SetXmlElem(size, buf, "ComFlag", value);
    xml->SetXmlElem(size, buf, "SetBin", value);
    xml->SetXmlElem(size, buf, "SetTpl", value);
    xml->SetXmlElem(size, buf, "Dat0", value);
    xml->SetXmlElem(size, buf, "Dat1", value);
}

inline void WriteDefaultNode(XmlSimple* xml, int* size, char* buf)
{
    xml->SetXmlElem(size, buf, "CutNo", "3");
    xml->SetXmlElem(size, buf, "NamePac", "\203\201\203b\203Z\201[\203W");  // "message" (SJIS)
    xml->SetXmlElem(size, buf, "Frame", "0");
}

inline int ReadData(const char* name, void* data)
{
    if (data == NULL) {
        pLog->err(0, 0, "ReadData : File Not Found [%s]", name);
        return 0;
    }
    return 1;
}

#endif
