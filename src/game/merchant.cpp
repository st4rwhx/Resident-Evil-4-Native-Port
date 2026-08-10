// game/merchant: the merchant's stock, weapon tune tables and shop prices (D:/Bio4/Prog/merchant.cpp).
#include "types.h"
#include "global.h"
#include "main.h"
#include "atari.h"
#include "item.h"
#include "room_data.h"
#include "merchant.h"

#define MERCHANT_NUM 1
#define STOCK_MAX 64
#define LEVEL_MAX 32
#define LIST_MAX 0xFF

// ItemWork::x6 tune levels (the exclusive nibble is read as a byte)
#define LV_FIRE(it) ((it)->x6 >> 12)
#define LV_MAG(it) (((it)->x6 >> 8) & 0xF)
#define LV_SPEED(it) (((it)->x6 >> 4) & 0xF)
#define LV_EX(it) ((u8) (it)->x6 & 0xF)

MerchantInfo merchant_info_A = {0, -10, -10, -10, -10, 10000, 5, 10, 10, 10, 20, 30, 70, 30, 10};

LevelPrice level_price[] = {
    {0x23, {700, 1000, 1500, 2000, 3000, 4000, 0}, {500, 1200, 0}, {400, 1000, 0}, {400, 600, 1000, 1500, 2000, 0, 0}},
    {0x25, {1500, 2000, 2400, 2800, 4500, 8000, 0}, {1000, 1500, 0}, {600, 1000, 0}, {600, 800, 1200, 1600, 2200, 0, 0}},
    {0x03, {1500, 1700, 2000, 2500, 3500, 0, 0}, {0, 0, 0}, {600, 1500, 0}, {700, 1000, 1200, 1600, 2000, 3500, 0}},
    {0x21, {1000, 1500, 2000, 2500, 3500, 4000, 0}, {1000, 2000, 0}, {800, 1800, 0}, {800, 1000, 1500, 1800, 2400, 0, 0}},
    {0x27, {1500, 1800, 2400, 3000, 4000, 8000, 0}, {1000, 2000, 0}, {800, 1500, 0}, {800, 1000, 1500, 2000, 2500, 0, 0}},
    {0x29, {2500, 3000, 3500, 5000, 7000, 15000, 0}, {0, 0, 0}, {1500, 2000, 0}, {1500, 2000, 2500, 0, 0, 0, 0}},
    {0x2C, {1500, 2000, 2500, 3000, 4500, 9000, 0}, {0, 0, 0}, {700, 1500, 0}, {800, 1000, 1200, 1500, 2000, 0, 0}},
    {0x2D, {2500, 2800, 3200, 4000, 6000, 0, 0}, {0, 0, 0}, {800, 1500, 0}, {1000, 1200, 1600, 1800, 2500, 6000, 0}},
    {0x94, {2000, 2400, 2800, 3200, 5000, 12000, 0}, {0, 0, 0}, {700, 2000, 0}, {1000, 1200, 1500, 2000, 2500, 0, 0}},
    {0x2E, {1000, 1200, 2000, 2500, 3500, 8000, 0}, {0, 0, 0}, {800, 1800, 0}, {600, 800, 1200, 1800, 2500, 0, 0}},
    {0x2F, {1500, 1800, 2400, 3000, 4000, 0, 0}, {8000, 0, 0}, {900, 1800, 0}, {1000, 1200, 1500, 2000, 2500, 0, 0}},
    {0x30, {700, 1400, 1800, 2400, 3500, 10000, 0}, {0, 0, 0}, {500, 1500, 0}, {700, 1500, 2000, 2500, 3500, 0, 0}},
    {0x36, {2500, 4500, 3000, 0, 0, 0, 0}, {0, 0, 0}, {1800, 0, 0}, {2500, 4000, 0, 0, 0, 0, 0}},
    {0x34, {2500, 2500, 3000, 3000, 3500, 5000, 0}, {0, 0, 0}, {1500, 2000, 0}, {1500, 1800, 2000, 2500, 3000, 0, 0}},
    {0x2A, {6000, 8000, 0, 0, 0, 0, 0}, {0, 0, 0}, {2000, 3000, 0}, {3000, 4000, 0, 0, 0, 0, 0}},
    {0x37, {4000, 5000, 7000, 9000, 12000, 20000, 0}, {0, 0, 0}, {2500, 5000, 0}, {1500, 2000, 2500, 3500, 5000, 0, 0}},
};

LevelEntry level_first[] = {
    {0x23, {2, 2, 2, 2}},
    {0x2C, {2, 1, 2, 2}},
    {0x2E, {2, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_styer[] = {
    {0x30, {2, 1, 2, 2}},
    {0xFFFF},
};

static LevelEntry level_r10e_day[] = {
    {0x21, {2, 2, 2, 2}},
    {0xFFFF},
};

LevelEntry level_1st_night[] = {
    {0x23, {3, 2, 2, 3}},
    {0x2C, {3, 1, 2, 3}},
    {0x2E, {3, 1, 2, 3}},
    {0xFFFF},
};

LevelEntry level_r112[] = {
    {0x25, {2, 2, 2, 2}},
    {0x21, {3, 2, 2, 3}},
    {0x30, {3, 1, 2, 3}},
    {0xFFFF},
};

LevelEntry level_r200[] = {
    {0x23, {4, 3, 3, 4}},
    {0x25, {3, 2, 2, 3}},
    {0x27, {2, 2, 2, 2}},
    {0x29, {2, 1, 1, 2}},
    {0x2C, {4, 1, 3, 4}},
    {0x2E, {4, 1, 3, 4}},
    {0x2F, {2, 1, 2, 2}},
    {0x30, {4, 1, 2, 3}},
    {0x94, {2, 1, 1, 1}},
    {0x36, {1, 1, 1, 2}},
    {0xFFFF},
};

LevelEntry level_r202[] = {
    {0x94, {3, 1, 1, 1}},
    {0xFFFF},
};

LevelEntry level_r204[] = {
    {0x23, {5, 3, 3, 5}},
    {0x25, {4, 3, 3, 4}},
    {0x21, {4, 3, 3, 4}},
    {0x2E, {5, 1, 3, 5}},
    {0x2F, {3, 1, 2, 3}},
    {0x30, {5, 1, 3, 3}},
    {0x94, {3, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_r20b[] = {
    {0x27, {3, 2, 2, 3}},
    {0x29, {3, 1, 2, 2}},
    {0x94, {4, 1, 2, 3}},
    {0x36, {2, 1, 1, 2}},
    {0xFFFF},
};

LevelEntry level_r211[] = {
    {0x2C, {5, 1, 3, 5}},
    {0x2F, {4, 1, 3, 4}},
    {0x30, {6, 1, 3, 4}},
    {0xFFFF},
};

static LevelEntry level_r214[] = {
    {0x23, {6, 3, 3, 6}},
    {0x25, {5, 3, 3, 5}},
    {0x21, {5, 3, 3, 5}},
    {0x27, {4, 3, 3, 4}},
    {0x29, {4, 1, 2, 3}},
    {0x2E, {6, 1, 3, 6}},
    {0x94, {5, 1, 2, 4}},
    {0x36, {2, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_r229[] = {
    {0x2D, {2, 1, 2, 2}},
    {0xFFFF},
};

static LevelEntry level_r220[] = {
    {0x94, {5, 1, 2, 5}},
    {0x36, {3, 1, 2, 2}},
    {0xFFFF},
};

LevelEntry level_r225[] = {
    {0x25, {6, 3, 3, 5}},
    {0x21, {6, 3, 3, 6}},
    {0x27, {5, 3, 3, 5}},
    {0x29, {5, 1, 3, 3}},
    {0x2C, {6, 1, 3, 6}},
    {0x2D, {3, 1, 2, 3}},
    {0x2F, {5, 1, 3, 5}},
    {0x30, {6, 1, 3, 5}},
    {0x23, {7, 3, 3, 6}},
    {0x2E, {7, 1, 3, 6}},
    {0xFFFF},
};

static LevelEntry level_r227[] = {
    {0x25, {6, 3, 3, 6}},
    {0xFFFF},
};

LevelEntry level_r22a[] = {
    {0x27, {6, 3, 3, 6}},
    {0x2D, {4, 1, 3, 4}},
    {0x94, {6, 1, 3, 6}},
    {0x30, {6, 1, 3, 6}},
    {0x36, {3, 1, 2, 3}},
    {0x25, {7, 3, 3, 6}},
    {0x21, {7, 3, 3, 6}},
    {0x27, {7, 3, 3, 6}},
    {0x2C, {7, 1, 3, 6}},
    {0x94, {7, 1, 3, 6}},
    {0x30, {7, 1, 3, 6}},
    {0xFFFF},
};

LevelEntry level_r301[] = {
    {0x29, {5, 1, 3, 4}},
    {0x2D, {5, 1, 3, 5}},
    {0x2F, {6, 1, 3, 6}},
    {0x2F, {6, 2, 3, 6}},
    {0x36, {4, 1, 2, 3}},
    {0xFFFF},
};

static LevelEntry level_r305[] = {
    {0x2D, {6, 1, 3, 6}},
    {0x2A, {2, 1, 2, 2}},
    {0x2D, {6, 1, 3, 7}},
    {0xFFFF},
};

LevelEntry level_r31a[] = {
    {0x29, {6, 1, 3, 4}},
    {0x2A, {3, 1, 3, 3}},
    {0xFFFF},
};

LevelEntry level_r31d[] = {
    {0x29, {7, 1, 3, 4}},
    {0xFFFF},
};

// declared before its definition so that its size is unknown at the uses (full address, not @sda21)
extern LevelEntry level_null[];
LevelEntry level_null[] = {
    {0xFFFF},
};

static LevelEntry level_r329[] = {
    {0xFFFF},
};

LevelEntry level_ext_normal[] = {
    {0x03, {6, 1, 3, 7}},
    {0xFFFF},
};

static LevelEntry level_ext_sw500[] = {
    {0x37, {7, 1, 3, 6}},
    {0xFFFF},
};

LevelEntry level_ext_tompson[] = {
    {0x34, {7, 1, 3, 6}},
    {0xFFFF},
};

PriceEntry exer_price_1st[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1}, {0x59, 1000, 1},
    {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1}, {0x5D, 1000, 1},
    {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1}, {0x61, 300, 1},
    {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1}, {0x65, 1500, 1},
    {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1}, {0xC6, 300, 1},
    {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1}, {0xCA, 1000, 1},
    {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1}, {0xCE, 1500, 1},
    {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1}, {0x8A, 100, 1},
    {0xFFFF},
};

PriceEntry sell_price_r104[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x2C, 1820, 1},
    {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1}, {0x44, 700, 1},
    {0x7D, 3000, 1}, {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r102_r10d_r10e[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x2C, 1820, 1},
    {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1}, {0x43, 400, 1},
    {0x44, 700, 1}, {0x7D, 3000, 1}, {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r112[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r11c[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r10f[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1},
    {0xFFFF},
};

StockEntry stock_r104[] = {
    {0x05, 1},
    {0x23, 1},
    {0x2C, 1},
    {0x2E, 1},
    {0x30, 1},
    {0x44, 1},
    {0x35, 1},
    {0x7D, 1},
    {0xA9, 1},
    {0xFFFF},
};

static StockEntry stock_r102[] = {
    {0x43, 1},
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r10e_day[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r10d[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r10e_night[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r112[] = {
    {0x05, 1},
    {0x25, 1},
    {0x42, 1},
    {0xFFFF},
};

StockEntry stock_r11c[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r11c_after_event[] = {
    {0x21, 1},
    {0xFFFF},
};

StockEntry stock_r10f[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_1st_mission[] = {
    {0x40, 1},
    {0xFFFF},
};

PriceEntry exer_price_2st[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0xAA, 800, 1}, {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1},
    {0x59, 1000, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1},
    {0x61, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xC6, 300, 1}, {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1},
    {0xCA, 1000, 1}, {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1},
    {0xCE, 1500, 1}, {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1},
    {0x8A, 100, 1}, {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2D, 3240, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x46, 100, 1},
    {0x00, 100, 1}, {0x45, 1000, 1}, {0x8F, 850, 1}, {0x98, 1200, 1},
    {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1}, {0x91, 1200, 1},
    {0x96, 1200, 1}, {0x9A, 900, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1}, {0xB8, 450, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xBC, 650, 1},
    {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1}, {0xC0, 1300, 1},
    {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0x56, 250, 1},
    {0xFFFF},
};

static PriceEntry sell_price_2st_first[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x36, 2300, 1},
    {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1},
    {0x45, 1000, 1},
    {0xFFFF},
};

PriceEntry sell_price_r20f[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2F, 3200, 1},
    {0x94, 2990, 1}, {0x45, 1000, 1},
    {0xFFFF},
};

static PriceEntry sell_price_r229[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1},
    {0xFFFF},
};

StockEntry stock_2st_first[] = {
    {0x05, 1},
    {0x54, 1},
    {0x7E, 1},
    {0x27, 1},
    {0x29, 1},
    {0x2F, 1},
    {0x36, 1},
    {0x94, 1},
    {0x45, 1},
    {0xAA, 1},
    {0x21, 1},
    {0x23, 1},
    {0x25, 1},
    {0x2C, 1},
    {0x2E, 1},
    {0x30, 1},
    {0x42, 1},
    {0x43, 1},
    {0x44, 1},
    {0xFFFF},
};

StockEntry stock_2st_general[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r20f[] = {
    {0x7F, 1},
    {0xFFFF},
};

static StockEntry stock_r229[] = {
    {0x2D, 1},
    {0xFFFF},
};

static PriceEntry exer_price_3st[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0xAA, 800, 1}, {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1},
    {0x59, 1000, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1},
    {0x61, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xC6, 300, 1}, {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1},
    {0xCA, 1000, 1}, {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1},
    {0xCE, 1500, 1}, {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1},
    {0x8A, 100, 1}, {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2D, 3240, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x46, 100, 1},
    {0x00, 100, 1}, {0x45, 1000, 1}, {0x8F, 850, 1}, {0x98, 1200, 1},
    {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1}, {0x91, 1200, 1},
    {0x96, 1200, 1}, {0x9A, 900, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1}, {0xB8, 450, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xBC, 650, 1},
    {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1}, {0xC0, 1300, 1},
    {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0x56, 250, 1}, {0x6A, 10, 1},
    {0x2A, 6300, 1}, {0xFE, 6000, 1}, {0xA1, 300, 1}, {0xD1, 1500, 1},
    {0xD2, 350, 1}, {0xD3, 350, 1}, {0xD4, 350, 1}, {0xD5, 2000, 1},
    {0xD6, 2000, 1}, {0xD7, 2000, 1}, {0xD8, 2500, 1}, {0xD9, 2500, 1},
    {0xDA, 2500, 1}, {0xDB, 3000, 1},
    {0xFFFF},
};

static PriceEntry sell_price_r301[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1}, {0x2A, 6300, 1},
    {0xFFFF},
};

PriceEntry sell_price_r305[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1}, {0x2A, 6300, 1},
    {0xFE, 6000, 1},
    {0xFFFF},
};

StockEntry stock_3st_general[] = {
    {0x05, 1},
    {0xFFFF},
};

StockEntry stock_r301[] = {
    {0x55, 1},
    {0x05, 1},
    {0x7E, 1},
    {0x27, 1},
    {0x29, 1},
    {0x2F, 1},
    {0x36, 1},
    {0x94, 1},
    {0x45, 1},
    {0xAA, 1},
    {0x21, 1},
    {0x23, 1},
    {0x25, 1},
    {0x2C, 1},
    {0x2E, 1},
    {0x30, 1},
    {0x42, 1},
    {0x43, 1},
    {0x44, 1},
    {0x2D, 1},
    {0x2A, 1},
    {0xFFFF},
};

StockEntry stock_r305[] = {
    {0x05, 1},
    {0xFE, 1},
    {0xFFFF},
};

static PriceEntry exer_price_ext[] = {
    {0x01, 400, 1}, {0x02, 200, 1}, {0x04, 10, 1}, {0x05, 1000, 1},
    {0x06, 100, 1}, {0x95, 60, 1}, {0x97, 240, 1}, {0x07, 30, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x0E, 100, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x15, 2000, 1},
    {0x16, 900, 1}, {0x18, 30, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x20, 6, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0xA8, 1000, 1},
    {0xAA, 800, 1}, {0x57, 200, 1}, {0x77, 1000, 1}, {0x58, 1000, 1},
    {0x59, 1000, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5E, 300, 1}, {0x5F, 300, 1}, {0x60, 300, 1},
    {0x61, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xC6, 300, 1}, {0xC7, 300, 1}, {0xC8, 300, 1}, {0xC9, 300, 1},
    {0xCA, 1000, 1}, {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1},
    {0xCE, 1500, 1}, {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x89, 100, 1},
    {0x8A, 100, 1}, {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2D, 3240, 1}, {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x46, 100, 1},
    {0x00, 100, 1}, {0x45, 1000, 1}, {0x8F, 850, 1}, {0x98, 1200, 1},
    {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1}, {0x91, 1200, 1},
    {0x96, 1200, 1}, {0x9A, 900, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1}, {0xB8, 450, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xBC, 650, 1},
    {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1}, {0xC0, 1300, 1},
    {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0x56, 250, 1}, {0x6A, 10, 1},
    {0x2A, 6300, 1}, {0xFE, 6000, 1}, {0xA1, 300, 1}, {0xD1, 1500, 1},
    {0xD2, 350, 1}, {0xD3, 350, 1}, {0xD4, 350, 1}, {0xD5, 2000, 1},
    {0xD6, 2000, 1}, {0xD7, 2000, 1}, {0xD8, 2500, 1}, {0xD9, 2500, 1},
    {0xDA, 2500, 1}, {0xDB, 3000, 1}, {0x03, 7000, 1}, {0x6D, 65535, 1},
    {0x37, 30000, 1}, {0x34, 20000, 1}, {0x6A, 10, 1}, {0x1A, 120, 1},
    {0xFFFF},
};

PriceEntry sell_price_ext[] = {
    {0x05, 1000, 1}, {0x21, 1900, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x2C, 1820, 1}, {0x2E, 1050, 1}, {0x30, 1320, 1}, {0x35, 3000, 1},
    {0x42, 400, 1}, {0x43, 400, 1}, {0x44, 700, 1}, {0x7D, 3000, 1},
    {0xA9, 1000, 1}, {0xAA, 800, 1}, {0x7E, 4000, 1}, {0x7F, 6000, 1},
    {0x36, 2300, 1}, {0x27, 2250, 1}, {0x29, 3200, 1}, {0x2D, 3240, 1},
    {0x2F, 3200, 1}, {0x94, 2990, 1}, {0x45, 1000, 1}, {0x2A, 6300, 1},
    {0xFE, 6000, 1}, {0x03, 7000, 1}, {0x6D, 65535, 1}, {0x37, 30000, 1},
    {0x34, 20000, 1},
    {0xFFFF},
};

StockEntry stock_ext_normal[] = {
    {0x03, 1},
    {0x6D, 1},
    {0xFFFF},
};

StockEntry stock_ext_sw500[] = {
    {0x37, 1},
    {0xFFFF},
};

StockEntry stock_ext_tompson[] = {
    {0x34, 1},
    {0xFFFF},
};

PriceEntry g_item_price_tbl[] = {
    {0x7F, 6000, 1}, {0x7E, 4000, 1}, {0x7D, 3000, 1}, {0xFE, 6000, 1},
    {0xA9, 1000, 1}, {0x54, 1000, 1}, {0x55, 1000, 1}, {0x04, 10, 1},
    {0x18, 30, 1}, {0x00, 100, 1}, {0x07, 30, 1}, {0x20, 6, 1},
    {0x46, 100, 1}, {0x6A, 10, 1}, {0x1A, 120, 1}, {0x0E, 100, 1},
    {0x02, 200, 1}, {0x01, 400, 1}, {0x23, 700, 1}, {0x25, 1320, 1},
    {0x40, 0, 1}, {0x21, 1900, 1}, {0x27, 2250, 1}, {0x29, 3200, 1},
    {0x2A, 6300, 1}, {0x03, 6850, 1}, {0x37, 0, 1}, {0x2C, 1820, 1},
    {0x94, 2990, 1}, {0x2D, 3240, 1}, {0x2E, 1050, 1}, {0x2F, 3200, 1},
    {0x30, 1320, 1}, {0x36, 2300, 1}, {0x34, 65535, 1}, {0x35, 3000, 1},
    {0x17, 6000, 1}, {0x6D, 65535, 1}, {0x42, 400, 1}, {0x43, 400, 1},
    {0x44, 700, 1}, {0x45, 1000, 1}, {0xAA, 800, 1}, {0xC5, 2000, 1},
    {0x08, 60, 1}, {0x09, 120, 1}, {0x0A, 600, 1}, {0x95, 150, 1},
    {0x97, 460, 1}, {0x06, 100, 1}, {0x19, 200, 1}, {0x1C, 600, 1},
    {0x12, 240, 1}, {0x13, 400, 1}, {0x14, 400, 1}, {0x16, 900, 1},
    {0xA8, 1000, 1}, {0x15, 2000, 1}, {0x05, 1000, 1}, {0x57, 200, 1},
    {0x77, 1000, 1}, {0x58, 1000, 1}, {0x89, 100, 1}, {0x59, 1000, 1},
    {0x8A, 100, 1}, {0x5A, 1000, 1}, {0x5B, 1000, 1}, {0x5C, 1000, 1},
    {0x5D, 1000, 1}, {0x5F, 300, 1}, {0x60, 300, 1}, {0x61, 300, 1},
    {0x5E, 300, 1}, {0x62, 1000, 1}, {0x63, 1000, 1}, {0x64, 1000, 1},
    {0x65, 1500, 1}, {0x66, 1500, 1}, {0x67, 1500, 1}, {0x68, 2000, 1},
    {0xB9, 100, 1}, {0xBA, 150, 1}, {0xBB, 300, 1}, {0xB8, 450, 1},
    {0xBC, 650, 1}, {0xBD, 700, 1}, {0xBE, 850, 1}, {0xBF, 1100, 1},
    {0xC0, 1300, 1}, {0xC1, 1500, 1}, {0xC2, 3200, 1}, {0xC7, 300, 1},
    {0xC8, 300, 1}, {0xC9, 300, 1}, {0xC6, 300, 1}, {0xCA, 1000, 1},
    {0xCB, 1000, 1}, {0xCC, 1000, 1}, {0xCD, 1500, 1}, {0xCE, 1500, 1},
    {0xCF, 1500, 1}, {0xD0, 2000, 1}, {0x56, 250, 1}, {0x8F, 850, 1},
    {0x98, 1200, 1}, {0x70, 2000, 1}, {0x93, 1300, 1}, {0x90, 1000, 1},
    {0x91, 1200, 1}, {0x96, 1200, 1}, {0x9B, 1100, 1}, {0x9C, 1300, 1},
    {0x9A, 900, 1}, {0x9D, 2500, 1}, {0x9E, 2700, 1}, {0x9F, 4800, 1},
    {0xA1, 300, 1}, {0xD1, 1500, 1}, {0xD2, 350, 1}, {0xD3, 350, 1},
    {0xD4, 350, 1}, {0xD5, 2000, 1}, {0xD6, 2000, 1}, {0xD7, 2000, 1},
    {0xD8, 2500, 1}, {0xD9, 2500, 1}, {0xDA, 2500, 1}, {0xDB, 3500, 1},
    {0xFFFF},
};


static int g_item_price_tbl_num = sizeof(g_item_price_tbl) / sizeof(g_item_price_tbl[0]);

MerchantCharacter merchantChar;
MerchantData merchantData[MERCHANT_NUM];

// the tests below are kept apart (fold would merge two masks of one lvalue into a single andis.)
static inline u32 chkFlag6C(u32 b)
{
    return pG->flags_6C & b;
}

static inline u32 chkFlag51C0(u32 b)
{
    return pG->flags_51C0 & b;
}

void merchant_stage1_full()
{
    stockDataAdd(merchantData, stock_r104);
    stockDataAdd(merchantData, stock_r102);
    stockDataAdd(merchantData, stock_r10e_day);
    stockDataAdd(merchantData, stock_r10d);
    stockDataAdd(merchantData, stock_r10e_night);
    stockDataAdd(merchantData, stock_r112);
    stockDataAdd(merchantData, stock_r11c);
    stockDataAdd(merchantData, stock_r11c_after_event);
    stockDataAdd(merchantData, stock_r10f);
    stockDataAdd(merchantData, stock_1st_mission);
    levelDataAdd(merchantData, level_first);
    levelDataAdd(merchantData, level_styer);
    levelDataAdd(merchantData, level_r112);
    levelDataAdd(merchantData, level_r10e_day);
    levelDataAdd(merchantData, level_1st_night);
}

void merchant_stage2_full()
{
    stockDataAdd(merchantData, stock_2st_first);
    stockDataAdd(merchantData, stock_r20f);
    stockDataAdd(merchantData, stock_r229);
    levelDataAdd(merchantData, level_r200);
    levelDataAdd(merchantData, level_r202);
    levelDataAdd(merchantData, level_r204);
    levelDataAdd(merchantData, level_r20b);
    levelDataAdd(merchantData, level_r211);
    levelDataAdd(merchantData, level_r214);
    levelDataAdd(merchantData, level_r220);
    levelDataAdd(merchantData, level_r225);
    levelDataAdd(merchantData, level_r22a);
}

void merchant_stage3_full()
{
    stockDataAdd(merchantData, stock_r301);
    stockDataAdd(merchantData, stock_r305);
    stockDataAdd(merchantData, stock_3st_general);
    levelDataAdd(merchantData, level_r301);
    levelDataAdd(merchantData, level_r305);
    levelDataAdd(merchantData, level_r31a);
    levelDataAdd(merchantData, level_r31d);
    levelDataAdd(merchantData, level_r329);
}

void MerchantGameInit()
{
    int i;

    for (i = 0; i < MERCHANT_NUM; i++) {
        MerchantData* d = &merchantData[i];
        d->favor = 50;
        d->x301 = 0;
        d->discount = 0;
        d->x303 = 0;
    }
    stockDataInit(merchantData);
    levelDataInit(merchantData);
    merchantChar.setChar(0, 0, 0, 0, 0);
    if (chkFlag6C(0x00800000) || chkFlag6C(0x00040000)) {
        if (pG->stage_no > 1) {
            merchant_stage1_full();
        }
        if (pG->stage_no > 2) {
            merchant_stage2_full();
        }
    }
}

void Merchant2ndRoundInit()
{
    levelDataAdd(merchantData, level_ext_normal);
    stockDataAdd(merchantData, stock_ext_normal);
    merchantChar.setChar(&merchant_info_A, merchantData, sell_price_ext, exer_price_ext, level_price);
}

void MerchantRoomInit()
{
    if (pG->x4F8E != 0) {
        if (pSys->x4 & 0x20000000) {
            levelDataAdd(merchantData, level_ext_sw500);
            stockDataAdd(merchantData, stock_ext_sw500);
        }
        if (pSys->x4 & 0x10000000) {
            levelDataAdd(merchantData, level_ext_tompson);
            stockDataAdd(merchantData, stock_ext_tompson);
        }
        merchantChar.setChar(&merchant_info_A, merchantData, g_item_price_tbl, g_item_price_tbl, level_price);
        return;
    }
    merchantChar.setChar(0, 0, 0, 0, 0);
    switch (pG->room_id) {
    case 0x104:
        if (!RoomData.checkPassed(0x104, 0)) {
            levelDataAdd(merchantData, level_first);
            stockDataAdd(merchantData, stock_r104);
        }
        break;
    case 0x102:
        if (!RoomData.checkPassed(0x102, 0)) {
            levelDataAdd(merchantData, level_styer);
            stockDataAdd(merchantData, stock_r102);
        }
        break;
    case 0x10D:
        if (!RoomData.checkPassed(0x10D, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r10d);
        }
        break;
    case 0x10F:
        if (!RoomData.checkPassed(0x10F, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r10f);
        }
        break;
    case 0x112:
        if (!RoomData.checkPassed(0x112, 0)) {
            levelDataAdd(merchantData, level_r112);
            stockDataAdd(merchantData, stock_r112);
        }
        break;
    case 0x11C:
        break;
    }
    if (pG->room_id == 0x10E) {
        if (pG->flags_51C0 & 0x01000000) {
            if (!(pG->flags_51C0 & 0x00100000)) {
                levelDataAdd(merchantData, level_null);
                stockDataAdd(merchantData, stock_r10e_night);
                pG->flags_51C0 |= 0x00100000;
            }
        } else {
            if (!(pG->flags_51C0 & 0x00200000)) {
                levelDataAdd(merchantData, level_r10e_day);
                stockDataAdd(merchantData, stock_r10e_day);
                pG->flags_51C0 |= 0x00200000;
            }
        }
    }
    if (chkFlag51C0(0x01000000) && !chkFlag51C0(0x2000)) {
        levelDataAdd(merchantData, level_1st_night);
        pG->flags_51C0 |= 0x2000;
    }
    switch (pG->room_id) {
    case 0x200:
        break;
    case 0x202:
        if (!RoomData.checkPassed(0x202, 0)) {
            levelDataAdd(merchantData, level_r202);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x204:
        if (!RoomData.checkPassed(0x204, 0)) {
            levelDataAdd(merchantData, level_r204);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x20B:
        if (!RoomData.checkPassed(0x20B, 0)) {
            levelDataAdd(merchantData, level_r20b);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x20F:
        if (!RoomData.checkPassed(0x20F, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r20f);
        }
        break;
    case 0x211:
        if (!RoomData.checkPassed(0x211, 0)) {
            levelDataAdd(merchantData, level_r211);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x214:
        if (!RoomData.checkPassed(0x214, 0)) {
            levelDataAdd(merchantData, level_r214);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x229:
        if (!RoomData.checkPassed(0x229, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_r229);
        }
        break;
    case 0x220:
        if (!RoomData.checkPassed(0x220, 0)) {
            levelDataAdd(merchantData, level_r220);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x225:
        if (!RoomData.checkPassed(0x225, 0)) {
            levelDataAdd(merchantData, level_r225);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x227:
        if (!RoomData.checkPassed(0x227, 0)) {
            levelDataAdd(merchantData, level_r227);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    case 0x22A:
        if (!RoomData.checkPassed(0x22A, 0)) {
            levelDataAdd(merchantData, level_r22a);
            stockDataAdd(merchantData, stock_2st_general);
        }
        break;
    }
    switch (pG->room_id) {
    case 0x301:
        if (!RoomData.checkPassed(0x301, 0)) {
            levelDataAdd(merchantData, level_r301);
            stockDataAdd(merchantData, stock_r301);
        }
        break;
    case 0x305:
        if (!RoomData.checkPassed(0x305, 0)) {
            levelDataAdd(merchantData, level_r305);
            stockDataAdd(merchantData, stock_r305);
        }
        break;
    case 0x30A:
        if (!RoomData.checkPassed(0x30A, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x31A:
        if (!RoomData.checkPassed(0x31A, 0)) {
            levelDataAdd(merchantData, level_r31a);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x30F:
    case 0x312:
        if (!RoomData.checkPassed(pG->room_id, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x315:
        if (!RoomData.checkPassed(0x315, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x31D:
        if (!RoomData.checkPassed(0x31D, 0)) {
            levelDataAdd(merchantData, level_r31d);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x329:
        if (!RoomData.checkPassed(0x329, 0)) {
            levelDataAdd(merchantData, level_r329);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    case 0x331:
        if (!RoomData.checkPassed(0x331, 0)) {
            levelDataAdd(merchantData, level_null);
            stockDataAdd(merchantData, stock_3st_general);
        }
        break;
    }
    if (pG->flags_6C & 0x10) {
        stockDataInit(merchantData);
        levelDataInit(merchantData);
        merchantChar.setChar(0, 0, 0, 0, 0);
        merchant_stage1_full();
        merchant_stage2_full();
        merchant_stage3_full();
    }
    merchantChar.setChar(&merchant_info_A, merchantData, g_item_price_tbl, g_item_price_tbl, level_price);
}

int MerchantDataSize()
{
    return sizeof(MerchantData) * MERCHANT_NUM;
}

void MerchantDataSave(void* dst)
{
    MerchantData* p = (MerchantData*) dst;
    int i;

    for (i = 0; i < MERCHANT_NUM; i++) {
        *p++ = merchantData[i];
    }
}

void MerchantDataLoad(void* src)
{
    MerchantData* p = (MerchantData*) src;
    int i;

    for (i = 0; i < MERCHANT_NUM; i++) {
        merchantData[i] = *p++;
    }
}

void stockDataInit(MerchantData* d)
{
    StockEntry* s = d->stock.e;
    int i;

    memclr_asm(d->stock.e, sizeof(StockTable));
    for (i = 0; i < STOCK_MAX; i++, s++) {
        s->id = 0xFFFF;
    }
}

void add_stock(StockEntry* dst, StockEntry* src)
{
    ItemInfo info;

    itemInfo(src->id, &info);
    switch (info.type) {
    case 1:
    case 9:
        dst->num = 1;
        break;
    case 3:
        if (src->id == 0x35 && pSys->language != 0) {
            dst->num = 1;
        } else {
            dst->num += src->num;
        }
        break;
    default:
        dst->num += src->num;
        break;
    }
}

void stockDataAdd(MerchantData* d, StockEntry* tbl)
{
    StockEntry* s;
    int i;

    for (s = d->stock.e, i = 0; i < STOCK_MAX && s->id != 0xFFFF; i++, s++) {
        s->isNew = 0;
    }
    for (; tbl->id != 0xFFFF; tbl++) {
        int found;
        int i;

        s = d->stock.e;
        found = 0;
        for (i = 0; i < STOCK_MAX; i++, s++) {
            if (s->id == 0xFFFF) {
                break;
            }
            if (s->id == tbl->id) {
                add_stock(s, tbl);
                found = 1;
                break;
            }
        }
        if (!found) {
            int added;
            int i;

            s = d->stock.e;
            added = 0;
            for (i = 0; i < STOCK_MAX; i++, s++) {
                if (s->id == 0xFFFF) {
                    s->id = tbl->id;
                    add_stock(s, tbl);
                    s->isNew = 1;
                    added = 1;
                    break;
                }
            }
            if (!added) {
                pLog->err(0, 0, "stockDataAdd(): lack of stock table");
            }
        }
    }
}

void levelDataInit(MerchantData* d)
{
    LevelEntry* l = d->level.e;
    int i;

    memclr_asm(d->level.e, sizeof(LevelTable));
    for (i = 0; i < LEVEL_MAX; i++, l++) {
        l->id = 0xFFFF;
    }
}

void levelDataAdd(MerchantData* d, LevelEntry* tbl)
{
    LevelEntry* l;
    int j;
    int i;

    for (l = d->level.e, i = 0; i < LEVEL_MAX && l->id != 0xFFFF; i++, l++) {
        l->isNew = 0;
    }
    for (; tbl->id != 0xFFFF; tbl++) {
        int found;
        int i;

        l = d->level.e;
        found = 0;
        for (i = 0; i < LEVEL_MAX; i++, l++) {
            if (l->id == 0xFFFF) {
                break;
            }
            if (l->id == tbl->id) {
                for (j = 0; j < 4; j++) {
                    if (l->lv[j] < tbl->lv[j]) {
                        l->isNew = 1;
                        l->lv[j] = tbl->lv[j];
                    }
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            int added;
            int i;

            l = d->level.e;
            added = 0;
            for (i = 0; i < LEVEL_MAX; i++, l++) {
                if (l->id == 0xFFFF) {
                    l->id = tbl->id;
                    for (j = 0; j < 4; j++) {
                        l->lv[j] = tbl->lv[j];
                    }
                    l->isNew = 1;
                    added = 1;
                    break;
                }
            }
            if (!added) {
                pLog->err(0, 0, "levelDataAdd(): lack of level table");
            }
        }
    }
}

void MerchantCharacter::setChar(MerchantInfo* info_, MerchantData* data_, PriceEntry* sell, PriceEntry* exer, LevelPrice* level)
{
    info = info_;
    data = data_;
    pSell = sell;
    pExer = exer;
    pLevel = level;
}

Merchant::Merchant(MerchantCharacter* c)
{
    info = c->info;
    pSell = c->pSell;
    pExer = c->pExer;
    pLevel = c->pLevel;
    memclr_asm(&stock, sizeof(StockTable));
    memclr_asm(&level, sizeof(LevelTable));
    load(c->data);
}

void Merchant::save(MerchantData* d)
{
    d->stock = stock;
    d->level = level;
    d->favor = favor;
    d->x301 = x311;
    d->discount = discount;
    d->x303 = x313;
}

void Merchant::load(MerchantData* d)
{
    if (d == 0) {
        pLog->err(0, 0, "Merchant::load() Data is empty.");
        return;
    }
    stock = d->stock;
    level = d->level;
    favor = d->favor;
    x311 = d->x301;
    discount = d->discount;
    x313 = d->x303;
}

StockEntry* Merchant::stockPtr(u16 id)
{
    StockEntry* s;

    for (s = stock.e;; s++) {
        if (s->id == 0xFFFF) {
            return 0;
        }
        if (id == s->id) {
            break;
        }
    }
    return s;
}

void Merchant::stockAdd(u16 id, int num)
{
    StockEntry* s = stockPtr(id);

    if (s && s->num != -2 && s->num != -1) {
        s->num += num;
    }
}

void Merchant::stockSub(u16 id, int num)
{
    StockEntry* s = stockPtr(id);

    if (s && s->num != -2 && s->num != -1 && s->num >= num) {
        s->num -= num;
    }
}

int Merchant::stockNum(u16 id)
{
    ItemInfo info;
    StockEntry* s = stockPtr(id);

    itemInfo(id, &info);
    if (info.type == 1) {
        return ItemMgr.search(id) == 0;
    }
    itemInfo(id, &info);
    if (info.type == 9) {
        return ItemMgr.search(id) == 0;
    }
    if (id == 0x38 || id == 0x35) {
        return ItemMgr.search(id) == 0;
    }
    switch (id) {
    case 0xFE:
        if (ItemMgr.num(0xFE) != 0) {
            return 0;
        }
        return pG->costume != 2 && pG->costume != 3;
    case 0x7D:
        if (ItemMgr.num(0x7F) != 0 || ItemMgr.num(0x7E) != 0 || ItemMgr.num(0x7D) != 0) {
            return 0;
        }
        return 1;
    case 0x7E:
        if (ItemMgr.num(0x7F) != 0 || ItemMgr.num(0x7E) != 0) {
            return 0;
        }
        return 1;
    case 0x7F:
        return ItemMgr.num(0x7F) == 0;
    }
    if (s == 0 || s->num == -2) {
        return 0;
    }
    if (s->num == -1) {
        return 1000;
    }
    return s->num;
}

int Merchant::stockNew(u16 id)
{
    StockEntry* s = stockPtr(id);

    if (s && s->isNew) {
        return 1;
    }
    return 0;
}

int Merchant::stockNew()
{
    StockEntry* s;

    for (s = stock.e; s->id != 0xFFFF; s++) {
        if (s->isNew) {
            return 1;
        }
    }
    return 0;
}

LevelEntry* Merchant::levelPtr(u16 id)
{
    LevelEntry* l = level.e;

    if (id == 0x21) {
        if (stockPtr(0x21) == 0 && stockPtr(0x40) == 0) {
            return 0;
        }
    } else {
        if (stockPtr(id) == 0) {
            return 0;
        }
    }
    for (;; l++) {
        if (l->id == 0xFFFF) {
            return 0;
        }
        if (id == l->id) {
            break;
        }
    }
    return l;
}

int Merchant::levelNew(u16 id)
{
    LevelEntry* l = levelPtr(id);

    if (l && l->isNew) {
        return 1;
    }
    return 0;
}

int Merchant::levelNew()
{
    LevelEntry* l;

    for (l = level.e; l->id != 0xFFFF; l++) {
        if (l->isNew) {
            if (l->id == 0x21) {
                if (stockPtr(0x21) || stockPtr(0x40)) {
                    return 1;
                }
            } else if (stockPtr(l->id)) {
                return 1;
            }
        }
    }
    return 0;
}

s8 Merchant::levelMax(u16 id, int type)
{
    LevelEntry* l = levelPtr(id);

    if (l != 0) {
        return l->lv[type];
    }
    return 1;
}

int Merchant::stockSpecial(u16 id)
{
    if (levelMax(id, 0) > WeaponId2MaxLevel(id, 0) || levelMax(id, 1) > WeaponId2MaxLevel(id, 1) ||
        levelMax(id, 2) > WeaponId2MaxLevel(id, 2) || levelMax(id, 3) > WeaponId2MaxLevel(id, 3)) {
        return 1;
    }
    return 0;
}

int Merchant::specialTunable(ItemWork* item)
{
    if (stockSpecial(item->id) != 0 && LV_FIRE(item) + 1 == WeaponId2MaxLevel(item->id, 0) &&
        LV_MAG(item) + 1 == WeaponId2MaxLevel(item->id, 1) && LV_SPEED(item) + 1 == WeaponId2MaxLevel(item->id, 2) &&
        LV_EX(item) + 1 == WeaponId2MaxLevel(item->id, 3)) {
        return 1;
    }
    return 0;
}

int Merchant::specialTuned(ItemWork* item)
{
    if (LV_FIRE(item) + 1 > WeaponId2MaxLevel(item->id, 0) || LV_MAG(item) + 1 > WeaponId2MaxLevel(item->id, 1) ||
        LV_SPEED(item) + 1 > WeaponId2MaxLevel(item->id, 2) || LV_EX(item) + 1 > WeaponId2MaxLevel(item->id, 3)) {
        return 1;
    }
    return 0;
}

int Merchant::tunable(ItemWork* item)
{
    if (item == 0) {
        return 0;
    }
    if (stockSpecial(item->id) == 0) {
        if (LV_FIRE(item) + 1 >= levelMax(item->id, 0) && LV_MAG(item) + 1 >= levelMax(item->id, 1) &&
            LV_SPEED(item) + 1 >= levelMax(item->id, 2) && LV_EX(item) + 1 >= levelMax(item->id, 3)) {
            return 0;
        }
    } else {
        if (specialTuned(item) == 1) {
            return 0;
        }
    }
    return 1;
}

void Merchant::makeList()
{
    sellingNum = makeSellingList();
    exerciseNum = makeExerciseList();
}

int checkSellingItem(u16 id)
{
    int ret = 1;

    switch (id) {
    case 0x40:
        if (pG->flags_51BC & 0x00040000) {
            u32 sold = pG->item_flags[0] & 0x10000000;
            ret = sold == 0;
        } else {
            ret = 0;
        }
        break;
    case 0xC5:
        ret = 0;
        break;
    }
    return ret;
}

int Merchant::makeSellingList()
{
    PriceEntry* p = pSell;
    int n;
    int i;

    for (i = 0; i < LIST_MAX; i++) {
        sellingList[i] = 0;
    }
    n = 0;
    for (i = 0; i < g_item_price_tbl_num; i++, p++) {
        if (checkSellingItem(p->id) && stockPtr(p->id)) {
            sellingList[n] = i;
            n++;
        }
    }
    return n;
}

u8 Merchant::sellingItemNum()
{
    return sellingNum;
}

PriceEntry* Merchant::sellingItemNo(int no)
{
    return &pSell[sellingList[no]];
}

PriceEntry* Merchant::sellingItemId(u16 id)
{
    PriceEntry* p = pSell;

    if (p->id != 0xFFFF) {
        do {
            if (p->id == id) {
                return p;
            }
            p++;
        } while (p->id != 0xFFFF);
    }
    return 0;
}

int checkExerciseItem(u16 id)
{
    int ret = 1;

    switch (id) {
    case 0x54:
    case 0x55:
    case 0x7C:
    case 0x7D:
    case 0x7E:
    case 0x7F:
    case 0xA9:
        ret = 0;
        break;
    }
    return ret;
}

int Merchant::makeExerciseList()
{
    PriceEntry* p = pExer;
    int n;
    int j;
    ItemInfo info;

    for (int i = 0; i < LIST_MAX; i++) {
        exerciseList[i] = 0;
    }
    n = 0;
    for (int i = 0; i < g_item_price_tbl_num; i++, p++) {
        ItemWork* item = ItemMgr.search(p->id);

        if (item == 0) {
            continue;
        }
        if (checkExerciseItem(p->id) == 0) {
            continue;
        }
        itemInfo(p->id, &info);
        if (info.type == 1) {
            ItemMgr.ordering(p->id);
            if (ItemMgr.nOrder > 0) {
                for (j = 0; j < ItemMgr.nOrder; j++) {
                    exerciseList[n] = ItemMgr.searchAt(ItemMgr.pOrder[j].item);
                    n++;
                }
            }
        } else {
            exerciseList[n] = ItemMgr.searchAt(item);
            n++;
        }
    }
    return n;
}

u8 Merchant::exerciseItemNum()
{
    return exerciseNum;
}

ItemWork* Merchant::exerciseItemPtr(int no)
{
    return ItemMgr.at(exerciseList[no]);
}

PriceEntry* Merchant::exerciseItemNo(int no)
{
    return exerciseItemId(ItemMgr.at(exerciseList[no])->id);
}

PriceEntry* Merchant::exerciseItemId(u16 id)
{
    PriceEntry* p = pExer;

    if (p->id != 0xFFFF) {
        do {
            if (p->id == id) {
                return p;
            }
            p++;
        } while (p->id != 0xFFFF);
    }
    return 0;
}

int Merchant::buyupPrice(u16 id, int num)
{
    ItemInfo info;
    PriceEntry* p;
    int price;
    int n;
    int type;

    // The 0.5 arm is written three times (one per test group) and the last test as
    // `id == 0xFE`: jump1 swaps that arm ahead of the 0.9 arm, so all three 0.5 loads sit
    // on a cse1 path from block 0 and share the `half` declaration's high (4 refs: no
    // update_equiv_regs move, global gives it r29); jump2 cross-jumps the three arms into
    // one. Declaring `half`/`nine` first keeps the pool order [string][0.5][0.9][0x4330].
    const f32 half = 0.5f;
    const f32 nine = 0.9f;
    p = exerciseItemId(id);
    if (p == 0) {
        pLog->err(0, 0, "buyupPriece() : 0x%02x not found", id);
        return 0;
    }
    n = num * 10;
    price = p->price * n;
    itemInfo(id, &info);
    type = info.type;
    if (type == 5 || type == 0xC) {
        return (int) ((f32) price * 1.0f);
    }
    if (type == 1 || type == 2) {
        return (int) ((f32) price * half);
    }
    if (type == 3 || type == 6) {
        return (int) ((f32) price * half);
    }
    if (id == 0xFE) {
        return (int) ((f32) price * half);
    }
    return (int) ((f32) price * nine);
}

int Merchant::buyupPrice(ItemWork* item, int num)
{
    ItemInfo info;
    int price = buyupPrice(item->id, num);
    const f32 rate = 0.5f; // pool entry before the 0x4330 magic; the literal is folded at every use
    int type;
    int lv;

    itemInfo(item->id, &info);
    if (info.type == 1 && num == 1) {
        price += buyupPrice(WeaponId2BulletId(item->id, item->x8 >> 13), item->x8 & 0x1FFF);
        for (type = 0; type <= 3; type++) {
            int lvMax = 0;

            switch (type) {
            case 0:
                lvMax = LV_FIRE(item) + 1;
                break;
            case 1:
                lvMax = LV_MAG(item) + 1;
                break;
            case 2:
                lvMax = LV_SPEED(item) + 1;
                break;
            case 3:
                lvMax = LV_EX(item) + 1;
                break;
            }
            for (lv = 2; lv <= lvMax; lv++) {
                price += (int) ((f32) levelupPrice(item, type, lv) * rate);
            }
            // COMPILER-DIFF: candidate (loop.c pass-2 insn_count). The original's outer loop had
            // >= 72 real insns at the second loop pass, so the two pool `lis` hoisted into the
            // lv-loop preheader by pass 1 stay there (threshold 71 * 1 * 1 < insn_count); ours has
            // 65 and hoists them to the function top. The two dead tests (num is dead after the
            // `num == 1` test) add the missing insns and vanish in flow/jump2.
            if (price == 0) {
                num = 0;
            }
            if (item->x8 == 0) {
                num = 1;
            }
        }
    }
    return price;
}

int Merchant::buyup(ItemWork* item, int num, int* money)
{
    ItemInfo ii;

    *money += buyupPrice(item, num);
    stockAdd(item->id, num);
    itemInfo(item->id, &ii);
    if (ii.type == 1 && num == 1) {
        stockAdd(WeaponId2BulletId(item->id, item->x8 >> 13), item->x8 & 0x1FFF);
    }
    favor += info->buyFavor;
    favor = favor < 0 ? 0 : (favor > 100 ? 100 : favor);
    return 1;
}

int Merchant::sellPrice(u16 id, int num)
{
    ItemInfo info;
    PriceEntry* p = sellingItemId(id);
    f32 rate = 1.0f - (f32) discount / 100.0f;
    int price;
    int n;

    if (p == 0) {
        pLog->err(0, 0, "sellPriece() : 0x%02x not found", id);
        return 0;
    }
    n = num * 10;
    price = p->price * n;
    if (id == 0x40 || id == 0x37) {
    } else if (id == 0x6D || id == 0x34) {
        price = 1000000;
    } else {
        itemInfo(id, &info);
        if (info.type == 1 && num == 1) {
            u16 bid = WeaponId2BulletId(id, 0);
            int m = WeaponId2ChargeNum(id, 1);
            p = exerciseItemId(bid);

            if (p) {
                int n2 = m * 10;
                price += p->price * n2;
            } else {
                pLog->err(0, 0, "sellPrice() : 0x%02x not found", id);
            }
        }
    }
    return (int) ((f32) price * rate);
}

int Merchant::sellUnit(u16 id)
{
    PriceEntry* p = sellingItemId(id);

    if (p == 0) {
        pLog->err(0, 0, "sellUnit() : 0x%02x not found", id);
        return 0;
    }
    return p->unit;
}

int Merchant::sell(u16 id, int num, int* money)
{
    ItemInfo ii;
    int price = sellPrice(id, num);
    int point = (int) ((f32) price * 0.02f);

    if (*money >= price) {
        if (id == 0x40) {
            pG->item_flags[0] |= 0x10000000;
        }
        *money -= price;
        stockSub(id, num);
        itemInfo(id, &ii);
        if (ii.type == 1 && num != 0) {
            u16 bid = WeaponId2BulletId(id, 0);
            stockSub(bid, WeaponId2ChargeNum(id, 1));
        }
        if (point >= info->sellBig) {
            favor += info->sellFavorBig;
        } else {
            favor += info->sellFavor;
        }
        favor = favor < 0 ? 0 : (favor > 100 ? 100 : favor);
        discount = 0;
        return 1;
    }
    return 0;
}

// Dead-stripped in the original (STRIP_UNUSED): only its constant pool (0.0f, 0.01f) survives after sell's.
static f32 merchant_dead_rate(f32 rate)
{
    if (rate != 0.0f) {
        rate = rate * 0.01f;
    }
    return rate;
}

int Merchant::levelupItemNum()
{
    LevelEntry* l = level.e;
    int n = 0;

    while (l->id != 0xFFFF) {
        if (levelPtr(l->id) == 0) {
            l++;
            continue;
        }
        ItemMgr.ordering(l->id);
        if (ItemMgr.nOrder > 0) {
            n += ItemMgr.nOrder;
        } else {
            n++;
        }
        l++;
    }
    return n;
}

LevelEntry* Merchant::levelupItemNo(int no)
{
    LevelEntry* l = level.e;
    int cnt = 0;
    int j;

    while (l->id != 0xFFFF) {
        if (levelPtr(l->id) == 0) {
            l++;
            continue;
        }
        ItemMgr.ordering(l->id);
        if (ItemMgr.nOrder > 0) {
            for (j = 0; j < ItemMgr.nOrder; j++) {
                if (cnt == no) {
                    return l;
                }
                cnt++;
            }
        } else {
            if (cnt == no) {
                return l;
            }
            cnt++;
        }
        l++;
    }
    return 0;
}

ItemWork* Merchant::levelupItemPtr(int no)
{
    LevelEntry* l = level.e;
    int cnt = 0;
    int j;

    while (l->id != 0xFFFF) {
        if (levelPtr(l->id) == 0) {
            l++;
            continue;
        }
        ItemMgr.ordering(l->id);
        if (ItemMgr.nOrder > 0) {
            for (j = 0; j < ItemMgr.nOrder; j++) {
                if (cnt == no) {
                    return ItemMgr.pOrder[j].item;
                }
                cnt++;
            }
        } else {
            if (cnt == no) {
                return 0;
            }
            cnt++;
        }
        l++;
    }
    return 0;
}

LevelPrice* Merchant::levelupItemPrice(u16 id)
{
    LevelPrice* p = pLevel;

    if (p->id != 0xFFFF) {
        do {
            if (p->id == id) {
                return p;
            }
            p++;
        } while (p->id != 0xFFFF);
    }
    return 0;
}

int Merchant::levelupPrice(u16 id, int type, int lv)
{
    LevelEntry* l = levelPtr(id);
    LevelPrice* p = levelupItemPrice(id);
    int price = 0;

    if (l && lv <= l->lv[type] && p) {
        switch (type) {
        case 0:
            price = p->fire[lv - 2];
            break;
        case 1:
            price = p->mag[lv - 2];
            break;
        case 2:
            price = p->speed[lv - 2];
            break;
        case 3:
            price = p->ex[lv - 2];
            break;
        }
    }
    return price * 10;
}

int Merchant::levelupPrice(ItemWork* item, int type, int lv)
{
    return levelupPrice(item->id, type, lv);
}

asm(".section .sdata,\"aw\"\n\t.balign 8\n\t.text");
