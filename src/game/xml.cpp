#include "types.h"
#include "xml.h"

extern "C" {
unsigned int strlen(const char* s);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, unsigned int n);
char* strcat(char* dst, const char* src);
char* strstr(const char* s, const char* sub);
int sprintf(char* buf, const char* fmt, ...);
}

int XmlSimple::GetXmlStart(char** out, const char* src, const char* tag)
{
    char buf[256];

    strcpy(buf, "<");
    strcat(buf, tag);
    *out = strstr(src, buf);
    if (*out == NULL) {
        return 0;
    }
    return 1;
}

int XmlSimple::GetXmlNext(char** out, const char* src, const char* tag)
{
    return GetXmlStart(out, src + 1, tag);
}

int XmlSimple::GetXmlElem(char* out, const char* src, const char* tag)
{
    char start[256];
    char end[256];
    char* p;
    char* q;
    int len;

    strcpy(start, "<");
    strcat(start, tag);
    strcat(start, ">");
    p = strstr(src, start);
    if (p == NULL) {
        return 0;
    }
    p += strlen(start);
    strcpy(end, "</");
    strcat(end, tag);
    strcat(end, ">");
    q = strstr(src, end);
    if (q == NULL) {
        return 0;
    }
    len = q - p;
    strncpy(out, p, len);
    out[len] = '\0';
    return 1;
}

int XmlSimple::SetXmlStart(int* size, char* buf)
{
    strcpy(buf, "<XSDSchemaSof xmlns=\"http://tempuri.org/XSDSchemaSof.xsd\">\n");
    *size += strlen(buf);
    return 1;
}

int XmlSimple::SetXmlEnd(int* size, char* buf)
{
    strcpy(buf, "</XSDSchemaSof>\n");
    *size += strlen(buf);
    return 1;
}

int XmlSimple::SetXmlElemStart(int* size, char* buf)
{
    strcpy(buf, "\t<Node xmlns=\"\">\n");
    *size += strlen(buf);
    return 1;
}

int XmlSimple::SetXmlElemEnd(int* size, char* buf)
{
    strcpy(buf, "\t</Node>\n");
    *size += strlen(buf);
    return 1;
}

int XmlSimple::SetXmlElem(int* size, char* buf, const char* name, const char* value)
{
    char tmp[256];

    sprintf(tmp, "\t\t<%s>%s</%s>\n", name, value, name);
    strcpy(buf, tmp);
    *size += strlen(buf);
    return 1;
}
