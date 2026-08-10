#include "types.h"
#include "global.h"
#include "joy.h"
#include "eprintf.h"
#include "main_mem.h"
#include "db_log.h"

extern "C" {
void OSReport(const char* fmt, ...);
int printf(const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list ap);
int strcmp(const char* a, const char* b);
char* strncpy(char* dst, const char* src, unsigned int n);
}

#define HALT()                                                    \
    {                                                             \
        OSReport("HALT %s(%d)\n", __FILE__, __LINE__);            \
        *(volatile u32*) 0x11111111 = 0;                          \
    }

#line 20 "D:/Bio4/Prog/db_log.cpp"
static inline void logHalt()
{
    HALT();
}

cLogPtr pLog;

void LogInit()
{
    cLog* p = (cLog*) Debug_alloc(sizeof(cLog), 1);
    pLog.p = p;
    p->init();
}

void cLog::init()
{
    modeReset();
    clear();
}

void cLog::mes(int a, int b, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vmes(a, b, fmt, ap);
    va_end(ap);
}

void cLog::err(int a, int b, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verr(a, b, fmt, ap);
    va_end(ap);
}

void cLog::warn(int a, int b, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vwarn(a, b, fmt, ap);
    va_end(ap);
}

void cLog::vmes(int a, int b, const char* fmt, va_list ap)
{
    if (!(pG->flags_6C & 0x04000000)) {
        cLogWork* w = add(a, 0, fmt, ap);
        w->color = b;
    }
}

void cLog::verr(int a, int b, const char* fmt, va_list ap)
{
    if (!(pG->flags_6C & 0x04000000)) {
        cLogWork* w = add(a, b, fmt, ap);
        w->color = 0x16;
    }
}

void cLog::vwarn(int a, int b, const char* fmt, va_list ap)
{
    if (!(pG->flags_6C & 0x04000000)) {
        cLogWork* w = add(a, b, fmt, ap);
        w->color = 0x10;
    }
}

void cLog::clear()
{
    cLogWork* w;
    timer = 0;
    cur = 0;
    for (w = work; w < &work[100]; w++) {
        w->clear();
    }
}

int cLog::modeReset()
{
    modeSet(160, 378, 90, 5);
    timer = 0;
    scr = 0;
    return 1;
}

int cLog::modeSet(int x, int y, int time, int lines)
{
    cLog* l = pLog.p;
    l->x = x;
    l->y = y;
    this->time = time;
    this->lines = lines;
    return 1;
}

void cLog::disp()
{
    static int disp_rep_cnt = 0;
    int i;
    int idx;
    s16 xx;
    s16 yy;

    if ((Joy[0].on & JOY_START) && (Joy[0].trg & JOY_Z)) {
        timer = time;
    }
    if (timer == 0) {
        return;
    }
    xx = x;
    yy = y;
    idx = (cur + 100 - lines - scr + 1) % 100;
    for (i = 0; i < lines; i++) {
        work[idx].print(xx, yy);
        yy += 14;
        idx = (idx + 1) % 100;
    }
    if (flags & 2) {
        disp_rep_cnt++;
        if (disp_rep_cnt & 1) {
            eprintf(x + blink - 16, y + (lines - 1) * 14, 0, 0, "+");
        } else {
            eprintf(x + blink - 16, y + (lines - 1) * 14, 0, 0, "*");
        }
    } else if (flags & 1) {
        eprintf(x + blink - 16, y + (lines - 1) * 14, 0, 0, ">");
        flags &= ~1;
        blink = (blink + 1) % 8;
    }
    if (timer != 0xFF) {
        timer--;
    }
    flags &= ~2;
}

int cLog::dispLineNum(int x, int y)
{
    int i;
    for (i = 0; i < lines; i++) {
        eprintf(x, y, 0, 0, "%02d", i - (lines - 100) - scr);
        y = (s16) (y + 14);
    }
    return 1;
}

int cLog::on(int flag)
{
    timer = flag;
    return 1;
}

int cLog::scrSet(s8 n)
{
    int s = scr + n;
    if (s < 0) {
        s = 0;
    }
    if (s > 100 - lines) {
        s = 100 - lines;
    }
    scr = s;
    return 1;
}

cLogWork* cLog::add(int flag, int key, const char* fmt, va_list ap)
{
    char buf[256];
    int dup = 0;
    cLogWork* w = &work[cur];

    vsprintf(buf, fmt, ap);
    flags |= 2;
    if (key != 0) {
        if (strcmp(w->str, buf) == 0 || w->key == key) {
            dup = 1;
        }
    }
    if (dup) {
        flags |= 1;
        if (!(flag & LOG_DUP_QUIET)) {
            if (!(flag & LOG_NO_SHOW)) {
                timer = time;
            }
        }
    } else {
        cur = (cur + 1) % 100;
        w = &work[cur];
        strncpy(w->str, buf, 63);
        w->str[63] = 0;
        if (!(flag & LOG_NO_PRINTF)) {
            printf("%s\n", buf);
        }
        w->key = key;
        if (!(flag & LOG_NO_SHOW)) {
            timer = time;
        }
    }
    return w;
}

void cLogWork::print(int x, int y)
{
    eprintf(x, y, color, 0, str);
}

void cLogWork::clear()
{
    color = 0;
    key = 0;
    str[0] = 0;
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
