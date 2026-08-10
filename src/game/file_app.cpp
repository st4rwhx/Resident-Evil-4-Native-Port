#include "types.h"
#include "file.h"
#include "main_mem.h"
#include "joy.h"
#include "eprintf.h"
#include "scheduler.h"

extern "C" {
void OSReport(const char* fmt, ...);
unsigned int strlen(const char* s);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
char* strstr(const char* s, const char* sub);
char* strncpy(char* dst, const char* src, unsigned int n);
int strcmp(const char* a, const char* b);
}

extern char* pUser_name;

// The original object carries 16 zero bytes of .sdata that no function references: four
// zero-initialised statics (the names are lost) kept alive by an inline function this build never
// calls. GCC 2.95 emits a static as soon as an inline body mentions it.
static int hd_stat0 = 0;
static int hd_stat1 = 0;
static int hd_stat2 = 0;
static int hd_stat3 = 0;
static inline void hd_stat_clear()
{
    hd_stat0 = hd_stat1 = hd_stat2 = hd_stat3 = 0;
}

static int hdRead_malloc(const char* path, void** buf, int mode, int flag);
static int hdWrite_main(const char* path, void* buf, int size);
static void createBackupFile(const char* path);

int HDRead(const char* path, void* buf)
{
    int fd;
    int size;
    int ret = 0;

    OSReport("HDread : \"%s\"  ", path);
    fd = file_open(path, FILE_OPEN_READ);
    if (fd == 0) {
        OSReport("not found!!!\n");
        return 0;
    }
    size = file_seek(fd, 0, 2);
    if (size != 0) {
        file_seek(fd, 0, 0);
        ret = file_read(fd, buf, size);
        OSReport("read complete...\n");
    } else {
        OSReport("size 0\n");
    }
    file_close(fd);
    return ret;
}

int HDReadSeekLen(const char* path, void* buf, u32 ofs, int len)
{
    int fd;
    int ret = 0;

    OSReport("HDread : \"%s\"  ", path);
    fd = file_open(path, FILE_OPEN_READ);
    if (fd == 0) {
        OSReport("not found!!!\n");
        return 0;
    }
    if ((u32) file_seek(fd, 0, 2) > ofs) {
        file_seek(fd, ofs, 0);
        ret = file_read(fd, buf, len);
        OSReport("read complete...\n");
    } else {
        OSReport("size 0\n");
    }
    file_close(fd);
    return ret;
}

int HDReadMemAlloc(const char* path, void** buf)
{
    return hdRead_malloc(path, buf, 0, 0);
}

int HDReadDebugAlloc(const char* path, void** buf, int flag)
{
    return hdRead_malloc(path, buf, 1, flag);
}

static int hdRead_malloc(const char* path, void** buf, int mode, int flag)
{
    int fd;
    int size;
    int ret = 0;

    *buf = NULL;
    OSReport("HDread : \"%s\"  ", path);
    fd = file_open(path, FILE_OPEN_READ);
    if (fd == 0) {
        OSReport("not found!!!\n");
        return 0;
    }
    size = file_seek(fd, 0, 2);
    if (size != 0) {
        switch (mode) {
        case 0:
#line 165 "D:/Bio4/Prog/file_app.cpp"
            *buf = MEM_ALLOC(size, 1, 13);
            break;
        case 1:
            *buf = Debug_alloc(size, flag);
            break;
        default:
            return 0;
        }
        if (*buf == NULL) {
            OSReport("malloc failed!!!\n");
            file_close(fd);
            return 0;
        }
        file_seek(fd, 0, 0);
        ret = file_read(fd, *buf, size);
        OSReport("read complete...\n");
    } else {
        OSReport("size 0\n");
    }
    file_close(fd);
    return ret;
}

int HDWrite(const char* path, void* buf, int size)
{
    if (file_lock_check(path) == 1) {
        OSReport("not write...\n");
        return 0;
    }
    createBackupFile(path);
    return hdWrite_main(path, buf, size);
}

int HDWrite_only(const char* path, void* buf, int size)
{
    return hdWrite_main(path, buf, size);
}

static int hdWrite_main(const char* path, void* buf, int size)
{
    int fd;
    int ret;

    OSReport("HDwrite: \"%s\"  ", path);
    ret = 0;
    if (size == 0) {
        fd = file_open(path, FILE_OPEN_WRITE);
        if (fd == 0) {
            OSReport("write failed!\n");
            return 0;
        }
        OSReport("write complete...\n");
    } else {
        fd = file_open(path, FILE_OPEN_RDWR);
        if (fd == 0) {
            OSReport("write failed!\n");
            return 0;
        }
        ret = file_write(fd, buf, size);
        OSReport("write complete...\n");
    }
    file_close(fd);
    return ret;
}

static void createBackupFile(const char* path)
{
    void* data;
    int size;
    char* bak;
    int fd;

    size = HDReadDebugAlloc(path, &data, 1);
    if (size == 0) {
        return;
    }
    bak = (char*) Debug_alloc(strlen(path) + 5, 1);
    strcpy(bak, path);
    strcat(bak, ".bak");
    OSReport("HDwrite: \"%s\"  ", bak);
    fd = file_open(bak, FILE_OPEN_RDWR);
    if (fd == 0) {
        Debug_free(data);
        Debug_free(bak);
        OSReport("backup failed!\n");
        return;
    }
    file_write(fd, data, size);
    Debug_free(data);
    Debug_free(bak);
    OSReport("backup complete...\n");
    file_close(fd);
}

int file_lock_check(const char* path)
{
    char lock[64];
    char* user;

    strcpy(lock, path);
    if (get_lock_file(lock) == NULL) {
        return 0;
    }
    if (file_exist(lock) == 1) {
        if (HDReadDebugAlloc(lock, (void**) &user, 1) != 0) {
            if (strcmp(user, pUser_name) != 0) {
                if (file_lock_msg(1, path, user) == 1) {
                    Debug_free(user);
                    return 1;
                }
            }
            Debug_free(user);
        }
    }
    return 0;
}

int file_lock(const char* path)
{
    char lock[64];
    char* user;

    strcpy(lock, path);
    if (get_lock_file(lock) == NULL) {
        return 0;
    }
    // `pUser_name` read directly in each arm: a `path = pUser_name` reassignment gives the
    // pseudo a second set, which global-allocates it with a phantom r31 (`stmw r30` vs `stw r31`).
    if (file_exist(lock) == 1) {
        if (HDReadDebugAlloc(lock, (void**) &user, 1) == 0) {
            HDWrite_only(lock, (void*) pUser_name, (strlen(pUser_name) + 32) & ~31);
        } else {
            if (strcmp(user, pUser_name) != 0) {
                if (file_lock_msg(0, path, user) != 0) {
                    Debug_free(user);
                    return 0;
                }
                HDWrite_only(lock, (void*) pUser_name, (strlen(pUser_name) + 32) & ~31);
            }
            Debug_free(user);
        }
    } else {
        HDWrite_only(lock, (void*) pUser_name, (strlen(pUser_name) + 32) & ~31);
    }
    return 1;
}

int file_unlock(const char* path)
{
    char lock[64];
    char* user;

    strcpy(lock, path);
    if (get_lock_file(lock) == NULL) {
        return 0;
    }
    if (file_exist(lock) == 1) {
        if (HDReadDebugAlloc(lock, (void**) &user, 1) != 0) {
            if (strcmp(user, pUser_name) == 0) {
                HDWrite_only(lock, user, 0);
                Debug_free(user);
                return 1;
            }
            Debug_free(user);
        }
    }
    return 0;
}

char* get_lock_file(char* path)
{
    char* p;

    p = strstr(path, "room");
    if (p == NULL) {
        p = strstr(path, "Room");
        if (p == NULL) {
            p = strstr(path, "ROOM");
        }
    }
    if (p == NULL) {
        return NULL;
    }
    strncpy(p, "lock", 4);
    strcat(path, ".lock");
    return path;
}

int file_lock_msg(int mode, const char* path, const char* user)
{
    int cur = 1;
    int n;
    int y = 80;

    while (1) {
        switch (mode) {
        case 0:
            eprintf2(10, 20, 30, 100, 6, 0, "(%s)", path);
            y = 160;
            eprintf2(10, 20, 30, 120, 22, 0, " is locked by the other user [%s].", user);
            eprintf2(10, 20, 30, 160, 2, 0, " UNLOCKED?");
            break;
        case 1:
            eprintf2(10, 20, 30, 100, 6, 0, "(%s)", path);
            y = 160;
            eprintf2(10, 20, 30, 120, 22, 0, " is locked by the other user [%s].", user);
            eprintf2(10, 20, 30, 160, 2, 0, " OVER WRITE?");
            break;
        }
        y += 20;
        eprintf2(10, 20, 30, y, 0, 0, "   YES");
        y += 20;
        eprintf2(10, 20, 30, y, 0, 0, "   NO");
        if (Joy[0].trg & JOY_UP) {
            cur--;
        }
        if (Joy[0].trg & JOY_DOWN) {
            cur++;
        }
        if (cur >= 0) {
            n = cur;
            if (n > 1) {
                n = 1;
            }
        } else {
            n = 0;
        }
        cur = n;
        if (n == 0) {
            eprintf2(10, 20, 30, y - 20, 0, 0, "  >");
        } else {
            eprintf2(10, 20, 30, y, 0, 0, "  >");
        }
        TaskSleep(1);
        if (Joy[0].trg & JOY_A) {
            break;
        }
    }
    Joy[0].trg = 0;
    return n;
}
