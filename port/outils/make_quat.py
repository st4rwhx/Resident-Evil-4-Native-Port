"""port/quat.cpp : les fonctions C_QUAT* du SDK (src/lib/quat.c), sans les variantes PSQUAT* en assembleur PowerPC."""
import re

ROOT = r'./'
src = open(ROOT + 'src/lib/quat.c', encoding='utf-8').read().splitlines()
out, i, n = [], 0, 0
while i < len(src):
    line = src[i]
    if re.match(r'^(void|f32) C_QUAT\w+\(', line):
        body = []
        while True:
            body.append(src[i])
            if src[i] == '}':
                break
            i += 1
        out.append('\n'.join(body))
        n += 1
    i += 1

head = '''// -*- coding: utf-8 -*-
// GENERE par make_quat.py depuis src/lib/quat.c : les quaternions du SDK en C (C_QUAT*), sans les variantes
// en assembleur PowerPC. Avant (12/11), C_QUATMtx et C_QUATSlerp n'etaient que des souches : le melange des
// mouvements (motion.cpp, model.cpp) rendait des matrices fausses (Leon fige dans une pose).
#include <dolphin.h>
#include <dolphin/mtx.h>
#include "fake_tgmath.h"

#ifndef ASSERTMSGLINE
#define ASSERTMSGLINE(line, cond, msg) ((void) 0)
#endif

extern "C" {
'''
open(ROOT + 'port/quat.cpp', 'w', encoding='utf-8', newline='\n').write(head + '\n\n'.join(out) + '\n\n}  // extern "C"\n')
print(n, 'fonctions')
