#ifndef MERCHANT_H
#define MERCHANT_H

#include "types.h"
#include "item.h"

// game/merchant.cpp: the merchant's stock / weapon-tune tables and the shop price logic.

// Stock table entry (8 bytes); tables end with id 0xFFFF.
struct StockEntry {
    u16 id;       // 0x00  item id
    s16 num;      // 0x02  pieces in stock; -1 = unlimited (1000), -2 = never for sale
    u8 isNew;     // 0x04  added since the last shop visit
    u8 pad_5[3];
};

// Weapon tune table entry (8 bytes); tables end with id 0xFFFF.
struct LevelEntry {
    u16 id;       // 0x00  weapon item id
    u8 lv[4];     // 0x02  max tune level per type (fire, magazine, speed, exclusive)
    u8 isNew;     // 0x06
    u8 pad_7;
};

struct StockTable {
    StockEntry e[64];  // 0x200
};

struct LevelTable {
    LevelEntry e[32];  // 0x100
};

// Per-merchant persistent data (saved with the game).
struct MerchantData {
    StockTable stock;  // 0x000
    LevelTable level;  // 0x200
    s8 favor;          // 0x300  0..100
    u8 x301;
    s8 discount;       // 0x302  percent off the selling price
    u8 x303;
};                     // 0x304

// Price table entry (6 bytes: sell / exercise / item price tables); tables end with id 0xFFFF.
struct PriceEntry {
    u16 id;       // 0x00
    u16 price;    // 0x02  price / 10
    u8 unit;      // 0x04  pieces per purchase
    u8 pad_5;
};

// Weapon tune price table entry (0x2A bytes): price / 10 per level (index lv - 2).
struct LevelPrice {
    u16 id;       // 0x00
    s16 fire[7];  // 0x02
    s16 mag[3];   // 0x10
    s16 speed[3]; // 0x16
    s16 ex[7];    // 0x1C
};

// Merchant personality constants (merchant_info_A).
struct MerchantInfo {
    u32 x0;
    s8 x4;
    s8 x5;
    s8 x6;
    s8 buyFavor;   // 0x07  favor change per purchase
    s32 sellBig;   // 0x08  sell points from which sellFavorBig applies
    u8 sellFavorBig;   // 0x0C
    u8 sellFavor;      // 0x0D
    u8 xE;
    u8 xF;
    u8 x10;
    u8 x11;
    u8 x12;
    u8 x13;
    u8 x14;
};

// The merchant selected for the current room (merchantChar).
class MerchantCharacter {
public:
    MerchantInfo* info;      // 0x00
    MerchantData* data;      // 0x04
    PriceEntry* pSell;       // 0x08
    PriceEntry* pExer;       // 0x0C
    LevelPrice* pLevel;      // 0x10

    MerchantCharacter() {}
    ~MerchantCharacter() {}
    void setChar(MerchantInfo* info, MerchantData* data, PriceEntry* sell, PriceEntry* exer, LevelPrice* level);
};                           // 0x14

// Shop session: a working copy of the merchant data plus the item lists shown in the shop.
class Merchant {
public:
    MerchantInfo* info;      // 0x000
    PriceEntry* pSell;       // 0x004  selling price table
    PriceEntry* pExer;       // 0x008  exercise (buy-up) price table
    LevelPrice* pLevel;      // 0x00C  weapon tune price table
    StockTable stock;        // 0x010
    LevelTable level;        // 0x210
    s8 favor;                // 0x310
    u8 x311;
    s8 discount;             // 0x312
    u8 x313;
    u8 exerciseNum;          // 0x314
    u8 sellingNum;           // 0x315
    u8 exerciseList[0xFF];   // 0x316  cItemMgr slot indexes of the items the player can sell
    u8 sellingList[0xFF];    // 0x415  sellPrice indexes of the items for sale

    Merchant(MerchantCharacter* c);
    void save(MerchantData* d);
    void load(MerchantData* d);
    StockEntry* stockPtr(u16 id);
    void stockAdd(u16 id, int num);
    void stockSub(u16 id, int num);
    int stockNum(u16 id);
    int stockNew(u16 id);
    int stockNew();
    LevelEntry* levelPtr(u16 id);
    int levelNew(u16 id);
    int levelNew();
    s8 levelMax(u16 id, int type);
    int stockSpecial(u16 id);
    int specialTunable(ItemWork* item);
    int specialTuned(ItemWork* item);
    int tunable(ItemWork* item);
    void makeList();
    int makeSellingList();
    u8 sellingItemNum();
    PriceEntry* sellingItemNo(int no);
    PriceEntry* sellingItemId(u16 id);
    int makeExerciseList();
    u8 exerciseItemNum();
    ItemWork* exerciseItemPtr(int no);
    PriceEntry* exerciseItemNo(int no);
    PriceEntry* exerciseItemId(u16 id);
    int buyupPrice(u16 id, int num);
    int buyupPrice(ItemWork* item, int num);
    int buyup(ItemWork* item, int num, int* money);
    int sellPrice(u16 id, int num);
    int sellUnit(u16 id);
    int sell(u16 id, int num, int* money);
    int levelupItemNum();
    LevelEntry* levelupItemNo(int no);
    ItemWork* levelupItemPtr(int no);
    LevelPrice* levelupItemPrice(u16 id);
    int levelupPrice(u16 id, int type, int lv);
    int levelupPrice(ItemWork* item, int type, int lv);
};                           // 0x514

extern MerchantCharacter merchantChar;
extern MerchantData merchantData[1];
extern MerchantInfo merchant_info_A;
extern LevelPrice level_price[];
extern PriceEntry g_item_price_tbl[];

extern "C" {
void merchant_stage1_full();
void merchant_stage2_full();
void merchant_stage3_full();
void MerchantGameInit();
void Merchant2ndRoundInit();
void MerchantRoomInit();
int MerchantDataSize();
void MerchantDataSave(void* dst);
void MerchantDataLoad(void* src);
void stockDataInit(MerchantData* d);
void add_stock(StockEntry* dst, StockEntry* src);
void stockDataAdd(MerchantData* d, StockEntry* tbl);
void levelDataInit(MerchantData* d);
void levelDataAdd(MerchantData* d, LevelEntry* tbl);
int checkSellingItem(u16 id);
int checkExerciseItem(u16 id);
}

#endif
