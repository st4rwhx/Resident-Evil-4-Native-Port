#ifndef DB_FILELIST_H
#define DB_FILELIST_H

#include "types.h"

// Host file list of the debug tools (tools/db_filelist.cpp; the object with cFileList and the
// constructors keyed to cFileList::init in t_event / t_sce, also inside Sscrn's ss_term): the names under
// one host directory (d:\bio4\room\filelist.txt through the SN file server) as a scrolling list.
class cFileList {
public:
    char* filter;   // 0x00  prefix stripped from every name
    char* pattern;  // 0x04  search pattern
    char* text;     // 0x08  file list text
    char** list;    // 0x0C  one pointer per line
    int num;        // 0x10
    s16 cursor;     // 0x14
    s16 top;        // 0x16

    // Empty: the file-scope instance gives the unit its (empty) static init/destroy pair.
    cFileList() {}
    ~cFileList() {}
    void init();
    char* disp(int x, int y, int rows);
    int update();
    void dir(char* d, char* f);
};

extern cFileList DbgFileList;

#endif
