#include "types.h"

extern "C" {
void* memset(void* dst, int c, unsigned int n);
unsigned long strtoul(const char* s, char** end, int base);
void yz2Decode_Decode(void* ctx, void* dst, u32 size, void* ev);
u32 Yz2DecodeSet(char* str, void* buf);
void Yz2DecodeExec(void* dst);
}

// Input state shared with the assembly decoder (yz2asm).
struct Yz2InEv {
    u8* src;      // 0x00  compressed stream read pointer
    u8* heap;     // 0x04  work buffer start
    u8* free;     // 0x08  work buffer allocation pointer
    u32 size0;    // 0x0C  first hex field of the header string
    u32 size1;    // 0x10  second hex field of the header string
};

static Yz2InEv in_ev;

// Adaptive frequency table.
struct Yz2Freq {
    u16* freq;    // 0x00
    u32 max;      // 0x04
    int bits;     // 0x08
    u32 range;    // 0x0C
    u16* cum;     // 0x10
    int n;        // 0x14
    u32 acc;      // 0x18
};

struct Yz2Dec;

struct Yz2Model {
    Yz2Dec* dec;    // 0x00
    Yz2Freq fd;     // 0x04
    u8* table;      // 0x20
};

// Bit reader and the two models.
struct Yz2Dec {
    u32 mask;       // 0x00
    u32 byte;       // 0x04
    Yz2Model m1;    // 0x08
    Yz2Model m2;    // 0x2C
};

// Dictionary entry (0x1004 bytes, 0x100 of them).
struct Yz2DicEnt {
    u32 cnt;        // 0x00
    u8 a[0x800];    // 0x04
    u8 b[0x800];    // 0x804
};

struct Yz2Ctx {
    Yz2DicEnt* dic; // 0x00
    Yz2Dec d;       // 0x04
    u32 pad_54;     // 0x54
    int n;          // 0x58
};

u32 Yz2DecodeSet(char* str, void* buf)
{
    char* p = str;
    u32 size;

    in_ev.size0 = strtoul(p, &p, 16);
    p++;
    size = strtoul(p, &p, 16);
    in_ev.size1 = size;
    in_ev.heap = (u8*) buf;
    in_ev.free = (u8*) buf;
    p = (char*) (((u32) p + 0x20) & ~0x1F);
    in_ev.src = (u8*) p;
    return size;
}

static inline void* yz2Alloc(u32 size)
{
    void* p = in_ev.free;
    in_ev.free += size;
    return p;
}

#define FREQ_RESET(fdp)                                   \
    {                                                     \
        Yz2Freq* f = (fdp);                                \
        int i;                                            \
        for (i = 0; i < f->n; i++) {                     \
            f->freq[i] = 1;                              \
        }                                                 \
        f->bits = 0;                                     \
        f->max = f->n;                                  \
        if (f->max >= (one << f->bits)) {               \
            u32 lim = 1;                                  \
            do {                                          \
                f->bits++;                               \
                if (f->bits > 14) {                      \
                    break;                                \
                }                                         \
            } while (f->max >= (lim << f->bits));       \
        }                                                 \
        f->range = 1 << f->bits;                        \
        f->acc = 0;                                      \
    }

#define MODEL_SETUP(mp, dp, cnt)                          \
    {                                                     \
        Yz2Model* m = (mp);                               \
        Yz2Freq* fd = &m->fd;                             \
        int i;                                            \
        int j;                                            \
        int n;                                            \
        u16 s;                                            \
        m->dec = (dp);                                    \
        fd->n = (cnt);                                    \
        fd->freq = (u16*) yz2Alloc((cnt) * sizeof(u16));  \
        memset(fd->freq, 0, (cnt) * sizeof(u16));         \
        fd->cum = (u16*) yz2Alloc((cnt) * sizeof(u16) * 2); \
        j = 0;                                            \
        for (i = 0; i < 0x8000; i++) {                    \
            fd->freq[j]++;                                \
            j++;                                          \
            if (j >= fd->n) {                             \
                j = 0;                                    \
            }                                             \
        }                                                 \
        s = 0;                                            \
        n = fd->n;                                        \
        for (i = 0; i < n; i++) {                         \
            fd->cum[i * 2] = fd->freq[i];                 \
            fd->cum[i * 2 + 1] = s;                       \
            s += fd->freq[i];                             \
        }                                                 \
        FREQ_RESET(fd);                                   \
        m->table = (u8*) yz2Alloc(0x20000);               \
    }

void Yz2DecodeExec(void* dst)
{
    Yz2Ctx ctx;
    Yz2Ctx* c = &ctx;
    Yz2Dec* d = &c->d;
    int i;
    u32 one = 1;

    d->mask = 0x80;
    d->byte = *in_ev.src++;
    ctx.n = 0x500;

    MODEL_SETUP(&d->m1, d, ctx.n);
    MODEL_SETUP(&d->m2, d, 0x100);

    c->dic = (Yz2DicEnt*) yz2Alloc(0x100 * sizeof(Yz2DicEnt));
    for (i = 0; i < 0x100; i++) {
        Yz2DicEnt* p = (Yz2DicEnt*) (i * sizeof(Yz2DicEnt) + (u32) c->dic);
        p->cnt = 0;
        memset(p->a, 0, sizeof(p->a));
        memset(p->b, 0, sizeof(p->b));
    }

    {
        Yz2Dec* dd = &c->d;
        FREQ_RESET(&dd->m1.fd);
        FREQ_RESET(&dd->m2.fd);
    }
    yz2Decode_Decode(&ctx, dst, in_ev.size1, &in_ev);
}
