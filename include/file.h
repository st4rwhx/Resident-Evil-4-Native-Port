#ifndef FILE_H
#define FILE_H

#include "types.h"

// Host (SN debugger) file access (game/file.cpp). Returns a handle (0 = failure).
#define FILE_OPEN_WRITE  0  // truncate/create
#define FILE_OPEN_READ   1
#define FILE_OPEN_RDWR   2  // create for writing

// game/file.cpp (C linkage; enabled by pG->flags_54 & 0x20000).
extern "C" {
int InitFile();
int file_open(const char* path, int mode);
int file_close(int fd);   // 0 = ok, -1 = failed
int file_read(int fd, void* buf, int size);
int file_write(int fd, const void* buf, int size);
int file_seek(int fd, int ofs, int whence);
int file_exist(const char* path);
int file_path(const char* dir);   // "SETROOT:<dir>"
}

// game/file_app.cpp: whole-file helpers on top of the above.
int HDRead(const char* path, void* buf);
int HDReadSeekLen(const char* path, void* buf, u32 ofs, int len);
int HDReadMemAlloc(const char* path, void** buf);
int HDReadDebugAlloc(const char* path, void** buf, int flag);
int HDWrite(const char* path, void* buf, int size);
int HDWrite_only(const char* path, void* buf, int size);
int file_lock_check(const char* path);
int file_lock(const char* path);
int file_unlock(const char* path);
char* get_lock_file(char* path);
int file_lock_msg(int mode, const char* path, const char* user);

#endif
