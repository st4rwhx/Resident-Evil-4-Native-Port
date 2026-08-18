#ifndef RE4_GX_FRAME_H
#define RE4_GX_FRAME_H

/* What the emulated GameCube graphics processor hands to a renderer
 * (port/docs/gx-plan.md, step G2). Plain C and fixed-width types: the app's
 * GLES3 backend (G3) is compiled separately and never sees game headers.
 *
 * The core decodes the game's GX command stream (runtime/gx_decode.cpp) on
 * its own thread, at once, and resolves everything that lives in game
 * memory: vertex arrays and display lists become vertices and indices,
 * matrices loaded by index become XF writes, textures and palettes are copied
 * out. A renderer never reads game memory.
 *
 * The work is cut into chunks: one chunk ends with each display copy
 * (GXCopyDisp, BP 0x52 with bit 14), i.e. one game frame. Chunks are a
 * stream: each continues the register state of the one before (the BP, XF
 * and TMEM writes it contains are changes). A chunk with
 * RE4_GX_CHUNK_RESET starts over: it begins with the full state, and every
 * texture the renderer kept may be dropped (the core sends again what it
 * uses). Chunks are queued in order; the core waits while the queue is full
 * and the renderer takes chunks (a slow renderer slows the game down, as a
 * slow graphics processor would); a renderer that stops taking chunks for a
 * second loses the queue and gets a RESET chunk next.
 *
 *   const Re4GxChunk* c;
 *   while ((c = re4_core_gx_acquire()) != NULL) { run(c); re4_core_gx_release(c); }
 *
 * Before the first acquire no chunks are built (the decoder only keeps the
 * state); re4_core_gx_detach() returns to that. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RE4_GX_FRAME_VERSION 1

/* --- the op stream ---------------------------------------------------------
 * ops[] is a sequence of records: a header word (op in bits 0-7, payload
 * length in words in bits 8-31), then the payload. Unknown ops are skipped by
 * their length. */
enum Re4GxOp {
    /* payload 1 word: reg << 24 | value. The register's new full value (the
     * BP mask is applied already). Written registers with side effects come
     * in order: 0x52 (EFB copy: to the XFB or to a texture, clear), 0x55/0x56
     * (bounding box), 0x66 (texture cache invalidate), 0x67 (metrics), and
     * 0x5A, which the GameCube does not have: the port's marker for the
     * app's gameplay autofocus (runtime/dof.cpp; the renderer blurs the EFB
     * behind the focus there, a renderer without it ignores it). Writes
     * that leave a plain state register unchanged are left out; 0x45/0x47/
     * 0x48 (draw done, tokens), 0x64/0x65 (TLUT loads, see RE4_GX_OP_TLUT)
     * and 0xFE (mask) are handled by the core. */
    RE4_GX_OP_BP = 1,
    /* payload 1 + n words: XF address (memory 0x0000-0x07FF, registers
     * 0x1000-0x1057), then n words in host order (floats as their bits).
     * Indexed loads (GXLoadPosMtxIndx ...) arrive as such writes. */
    RE4_GX_OP_XF = 2,
    /* payload: Re4GxDraw */
    RE4_GX_OP_DRAW = 3,
    /* payload: Re4GxTexture. Binds texture map `map` for the draws that
     * follow; comes before a draw whenever the map's texture changes. */
    RE4_GX_OP_TEXTURE = 4,
    /* payload 3 words: TMEM byte offset of a palette load (BP 0x65 tmem
     * address << 9; 0..0x7FFFF), byte count, blob offset. The renderer keeps
     * 512 KB of TLUT memory; the entries stay big-endian as in TMEM. */
    RE4_GX_OP_TLUT = 5,
    /* payload 2 words: key of a texture the core will not bind again until
     * it sends it anew (not used for a while); the renderer may free it. */
    RE4_GX_OP_FORGET = 6,
    /* payload 256 words: all BP registers (the start of a RESET chunk), no
     * side effects. */
    RE4_GX_OP_BP_STATE = 7
};

/* Primitive classes (Re4GxDraw.primitive bits 0-7). Quads, strips and fans
 * arrive as triangle lists with GX's winding kept (strips: every second
 * triangle flipped back), line strips as line lists. */
enum Re4GxPrimitive {
    RE4_GX_TRIANGLES = 0,
    RE4_GX_LINES = 1,
    RE4_GX_POINTS = 2
};

/* Vertex layout (Re4GxDraw.layout). Every vertex holds, in this order:
 *   position         3 floats (always; 2-component positions get z = 0)
 *   NORMAL           3 floats
 *   BINORMAL_TANGENT 6 floats (binormal, then tangent; with NORMAL)
 *   COLOR0, COLOR1   4 bytes each, R G B A
 *   TEX0..TEX7       2 floats each (1-component coordinates get t = 0)
 *   MATRIX           12 bytes: position matrix index, texture matrix
 *                    indices 0..7, 3 bytes padding. Present when the vertex
 *                    format carries any index; the missing ones are the
 *                    draw's defaults (Re4GxDraw.matrices).
 * Positions, normals and texture coordinates are as the game sent them
 * (object space, dequantised): the transforms, lighting and texture
 * coordinate generation of the XF unit are the renderer's (XF state from
 * RE4_GX_OP_XF). Matrix indices are XF row numbers as in GX (position
 * matrix n = 3n, texture matrix n = 30 + 3n, 60 = identity). */
enum Re4GxLayout {
    RE4_GX_V_NORMAL = 1u << 0,
    RE4_GX_V_BINORMAL_TANGENT = 1u << 1,
    RE4_GX_V_COLOR0 = 1u << 2,
    RE4_GX_V_COLOR1 = 1u << 3,
    RE4_GX_V_TEX0 = 1u << 4, /* TEX0 << n for coordinate n */
    RE4_GX_V_MATRIX = 1u << 12
};

typedef struct Re4GxDraw {
    uint32_t primitive;    /* Re4GxPrimitive | GX command (0x80..0xB8) << 8 | vertex format << 16 */
    uint32_t layout;       /* Re4GxLayout bits */
    uint32_t stride;       /* bytes per vertex */
    uint32_t vertexOffset; /* bytes into Re4GxChunk.vertices */
    uint32_t vertexCount;
    uint32_t indexOffset;  /* elements into Re4GxChunk.indices */
    uint32_t indexCount;   /* indices count from the draw's first vertex */
    uint32_t matrices[3];  /* default matrix indices (CP MATINDEX_A/B):
                              [0] pos | tex0 << 8 | tex1 << 16 | tex2 << 24,
                              [1] tex3 .. tex6, [2] tex7 */
} Re4GxDraw;

enum Re4GxTextureFlags {
    /* the texture is the EFB copy last written to `address` (GXCopyTex);
     * no data, the key is address and copy number */
    RE4_GX_TEX_COPY = 1u << 0
};

typedef struct Re4GxTexture {
    uint32_t map;          /* texture map 0..7 */
    uint32_t keyLo, keyHi; /* content key: same key, same texels (and palette) */
    uint32_t flags;        /* Re4GxTextureFlags */
    uint32_t address;      /* physical address of level 0 */
    uint32_t width, height;
    uint32_t format;       /* GX texture format (BP TX_SETIMAGE0 bits 20-23) */
    uint32_t levels;       /* mipmap levels in the data (1 = none) */
    uint32_t tlut;         /* CI formats: BP TX_SETTLUT (TMEM offset >> 9, format << 10) */
    uint32_t dataOffset;   /* bytes into Re4GxChunk.blob */
    uint32_t dataBytes;    /* 0: sent before under this key; else all levels, GX tiled, big-endian */
} Re4GxTexture;

enum Re4GxChunkFlags {
    RE4_GX_CHUNK_RESET = 1u << 0, /* starts over (full state, textures sent anew) */
    RE4_GX_CHUNK_XFB = 1u << 1,   /* ends with a display copy */
    RE4_GX_CHUNK_HOLD = 1u << 2,  /* the VI does not switch to this display copy
                                     (SYS_SCREEN_STOP): run it, keep showing the
                                     picture before */
    RE4_GX_CHUNK_MARK = 1u << 3,  /* the core marked it (re4_gx_mark: a hit on the
                                     player); the chunk after it starts over */
    RE4_GX_CHUNK_BLACK = 1u << 4  /* the VI shows black (VISetBlack): run it, show
                                     black instead of its picture until a chunk
                                     without the flag */
};

typedef struct Re4GxChunk {
    uint32_t version;      /* RE4_GX_FRAME_VERSION */
    uint32_t sequence;     /* counts chunks built, gaps where chunks were dropped */
    uint32_t flags;        /* Re4GxChunkFlags */
    const uint32_t* ops;
    uint32_t opWords;
    const uint8_t* vertices;
    uint32_t vertexBytes;
    const uint16_t* indices;
    uint32_t indexCount;
    const uint8_t* blob;   /* texture and palette data */
    uint32_t blobBytes;
    uint32_t xfbAddress;   /* the display copy: destination (physical) */
    uint32_t xfbWidth, xfbHeight;
    uint32_t draws;        /* statistics */
    uint32_t vertexCount;
    uint32_t textureBytes; /* texture data sent in this chunk */
} Re4GxChunk;

/* The oldest finished chunk, or NULL; attaches the renderer. */
const Re4GxChunk* re4_core_gx_acquire(void);
/* Gives a chunk back (chunks may be released in any order). */
void re4_core_gx_release(const Re4GxChunk* chunk);
/* The renderer goes away: queued chunks are dropped, none are built. */
void re4_core_gx_detach(void);
/* `notify` is called (on the core's thread, with the queue locked: it may
 * only wake someone) whenever a chunk is queued; NULL: none. The core
 * process's server (host/coreproc.cpp) takes chunks at once with it. */
void re4_core_gx_notify(void (*notify)(void));

#ifdef __cplusplus
}
#endif

#endif
