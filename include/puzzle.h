#ifndef PUZZLE_H
#define PUZZLE_H

#include "types.h"
#include "vec.h"

// Attache case packing puzzle (game/puzzle.cpp): pieces on a grid board.
struct ItemWork;
class cModel;

// Piece shape data (piece_info entry + 4).
struct PieceData {
    s8 w;             // 0x00
    s8 h;             // 0x01
    u8 pad_2[2];
    f32 cx;           // 0x04  centre offset in cells
    f32 cy;           // 0x08
    char shape[0x40]; // 0x0C  row-major, '1' = filled
};

// One piece_info entry (0x78 bytes; the table ends with id 0xFFFF).
struct PieceInfo {
    u16 id;           // 0x00  item id
    u16 pad_2;
    PieceData data;   // 0x04
    u8 model[0x28];   // 0x50  model data (searchItemModelData)
};

class pzlPiece {
public:
    u8 flags;         // 0x00  bit0 in use
    u8 pad_1[3];
    PieceData* data;  // 0x04
    u32 x8;           // 0x08
    f32 cx;           // 0x0C  rotated centre offset
    f32 cy;           // 0x10
    f32 x;            // 0x14  centre position on the board (cells)
    f32 y;            // 0x18
    u32 x1C;
    s8 orient;        // 0x20  0..3 rotations, 4..7 mirrored
    u8 state;         // 0x21  1 = on a board, 2 = in hand
    u8 pad_22[2];
    ItemWork* item;   // 0x24
    cModel* model;    // 0x28

    void orientation(int o);
    void rotate(int dir);
    void mirror(int axis);
    void init(PieceData* d);
    f32 ver0_x();
    f32 ver0_y();
    int size_x();
    int size_y();
    void snap();
    int shape(int x, int y);
};

class pzlBoard {
public:
    u8* cells;        // 0x00  w * h state bytes (bit0 occupied, bit1 inside, bit6 wall)
    s8 w;             // 0x04
    s8 h;             // 0x05
    u8 pieceMax;      // 0x06
    u8 pad_7;
    pzlPiece** pieces;// 0x08
    Mtx mat;          // 0x0C  board -> world matrix (Sscrn ss_pzzl caseModelMove)
    s8 curX;          // 0x3C  cursor
    s8 curY;          // 0x3D
    s8 wallDir;       // 0x3E  ckInsideWall result side (1 left, 2 right, 3 up, 4 down)
    s8 outDir;        // 0x3F  outPiece result side

    int init(int w, int h, int pieceMax);
    void quit();
    int getPieceNum();
    int search(pzlPiece* p);
    int ckInsideWall(pzlPiece* p);
    int outPiece(pzlPiece* p);
    int putPiece(pzlPiece* p);
    pzlPiece* lapPiece(pzlPiece* p);
    pzlPiece* getPiece(int x, int y);
    int rmPiece(pzlPiece* p);
    pzlPiece* rmPiece(int x, int y);
    u8* cell(int x, int y);
    int cellState(int x, int y);
    void clearState(u8 mask);
};

class pzlPlayer {
public:
    pzlBoard* caseBoard;   // 0x00
    pzlBoard* spaceBoard;  // 0x04
    pzlPiece* pieces;      // 0x08
    u8 pieceNum_;          // 0x0C
    u8 pad_D[3];
    pzlPiece* hand;        // 0x10
    pzlPiece* extra;       // 0x14
    f32 handX;             // 0x18
    f32 handY;             // 0x1C
    s8 handOrient;         // 0x20
    u8 pad_21[3];
    pzlBoard* handBoard;   // 0x24
    pzlBoard* saveBoard;   // 0x28
    s8 saveX;              // 0x2C
    s8 saveY;              // 0x2D
    u8 pad_2E[2];
    pzlBoard* cur;         // 0x30

    int init(int type);
    void quit();
    int pieceNum();
    pzlPiece* piecePtr(int no);
    pzlPiece* piecePtr(ItemWork* item);
    void save();
    int appendExtraPiece(ItemWork* item);
    int removeExtraPiece();
    void inHandExtraPiece();
    void giveupExtraPiece();
    int selPiece(pzlBoard* b);
    pzlPiece* ptrPiece(pzlBoard* b);
    void getPiece(pzlBoard* b);
    int putPiece(pzlBoard* b);
    int relPiece(pzlBoard* b);
    int chgPiece(pzlBoard* b);
    pzlPiece* cmbPiece(pzlBoard* b);
    int movePiece();
    void rehash();
    void saveCursor();
    void loadCursor();
    void salvCursor();
};

extern PieceInfo piece_info[];

extern "C" {
PieceData* searchItemPieceData(int id, PieceInfo* tbl);
u8* searchItemModelData(int id, PieceInfo* tbl);
int PutInCase(u16 id, u16 num, int type);
}

#endif
