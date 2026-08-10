#include "types.h"
#include "global.h"
#include "fileserver.h"
#include "file.h"

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
}

static int usb_fd = -1;
static void* usb_buf = (void*) 0x81800000;
static int usb_size = 0;
static int usb_pos = 0;

// Never called in this build; only its strings survive in .rodata.
static inline void usb_connect(int port)
{
    OSReport("USB server connect wait...\n");
    OSReport("connect done\n");
    OSReport("USB port: %d\n", port);
}

int InitFile()
{
    PCinit();
    OSReport("SN PC file system Initialize\n");
    PCopen("SETROOT:d:\\bio4", 0, 0);
    return 1;
}

int file_open(const char* name, int mode)
{
    int fd;

    if (pG->flags_54 & 0x20000) {
        if (mode == 0 || mode == 2) {
            fd = PCcreat(name, 0);
            if (fd == -1) {
                return 0;
            }
        } else {
            if (mode != 0) {
                mode--;
            }
            fd = PCopen(name, mode, 0);
            if (fd == -1) {
                fd = 0;
            }
        }
        return fd;
    }
    return 0;
}

int file_close(int fd)
{
    int ret;

    if (pG->flags_54 & 0x20000) {
        if (PCclose(fd) != 0) {
            ret = -1;
            return ret;
        }
        ret = 0;
    } else {
        ret = -1;
    }
    return ret;
}

int file_read(int fd, void* buf, int size)
{
    int ret = 0;

    if (pG->flags_54 & 0x20000) {
        ret = PCread(fd, buf, size);
    }
    return ret;
}

int file_write(int fd, const void* buf, int size)
{
    int ret = 0;

    if (pG->flags_54 & 0x20000) {
        ret = PCwrite(fd, buf, size);
    }
    return ret;
}

int file_seek(int fd, int offset, int whence)
{
    int ret = -1;

    if (pG->flags_54 & 0x20000) {
        ret = PClseek(fd, offset, whence);
    }
    return ret;
}

int file_exist(const char* name)
{
    int fd;

    if (!(pG->flags_54 & 0x20000)) {
        return 0;
    }
    fd = PCopen(name, 0, 0);
    if (fd == -1) {
        return 0;
    }
    PCclose(fd);
    return 1;
}

int file_path(const char* dir)
{
    char buf[64];

    if (!(pG->flags_54 & 0x20000)) {
        return 0;
    }
    sprintf(buf, "SETROOT:%s", dir);
    PCopen(buf, 0, 0);
    return 1;
}
