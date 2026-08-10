#ifndef FILESERVER_H
#define FILESERVER_H

#include "types.h"

// SN Systems ProDG PC file server (lib/fileserver.c stubs). Paths are host paths; the
// "SETROOT:<dir>" pseudo file sets the server's root directory.
extern "C" {
int PCinit();
int PCcreat(const char* path, int mode);
int PCopen(const char* path, int flags, int mode);
int PCclose(int fd);
int PCread(int fd, void* buf, int size);
int PCwrite(int fd, const void* buf, int size);
int PClseek(int fd, int offset, int whence);
}

#endif
