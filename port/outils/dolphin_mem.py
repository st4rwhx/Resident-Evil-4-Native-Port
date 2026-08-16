# -*- coding: utf-8 -*-
"""Lecture (et ecriture sur demande) de la memoire du jeu dans Dolphin, depuis l'exterieur (Windows) — jumeau du Studio.

    python -I dolphin_mem.py                 # trouve Dolphin et la memoire emulee, affiche l'identifiant du disque
    from dolphin_mem import Dolphin ; d = Dolphin() ; d.lire(0x80314BC8, 4)

Meme principe que Dolphin Memory Engine : la MEM1 emulee (24 Mo, adresses 0x80000000..) est une region du processus
Dolphin dont les premiers octets sont l'identifiant du disque (« G4BE08 »). Les valeurs y sont gros-boutistes.
Lecture seule par defaut ; `ecrire` n'est utilise que pour piloter le jeu de reference (saut de salle) a la demande.
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import struct
import sys

PROCESS_VM_READ, PROCESS_VM_WRITE, PROCESS_VM_OPERATION, PROCESS_QUERY_INFORMATION = 0x10, 0x20, 0x8, 0x400
MEM_COMMIT = 0x1000


class MBI(ctypes.Structure):
    _fields_ = [('BaseAddress', ctypes.c_void_p), ('AllocationBase', ctypes.c_void_p), ('AllocationProtect', wt.DWORD),
                ('PartitionId', wt.WORD), ('RegionSize', ctypes.c_size_t), ('State', wt.DWORD), ('Protect', wt.DWORD),
                ('Type', wt.DWORD)]


k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenProcess.restype = wt.HANDLE
k32.VirtualQueryEx.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(MBI), ctypes.c_size_t]
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.WriteProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


def pid_dolphin():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq Dolphin.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if parts and parts[0].lower() == 'dolphin.exe':
            return int(parts[1])
    return None


class Dolphin:
    def __init__(self, ecriture=False, jeu=b'G4BE08'):
        self.pid = pid_dolphin()
        if not self.pid:
            raise SystemExit('Dolphin introuvable')
        acces = PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | ((PROCESS_VM_WRITE | PROCESS_VM_OPERATION) if ecriture else 0)
        self.h = k32.OpenProcess(acces, False, self.pid)
        if not self.h:
            raise SystemExit('ouverture du processus Dolphin refusee (%d)' % ctypes.get_last_error())
        self.base = self._trouver_mem1(jeu)
        if self.base is None:
            raise SystemExit('memoire emulee introuvable (jeu en cours ?)')

    def _brut(self, adr, n):
        buf = ctypes.create_string_buffer(n)
        lu = ctypes.c_size_t(0)
        if not k32.ReadProcessMemory(self.h, ctypes.c_void_p(adr), buf, n, ctypes.byref(lu)) or lu.value != n:
            return None
        return buf.raw

    def _trouver_mem1(self, jeu):
        mbi = MBI()
        adr = 0
        candidats = []
        while k32.VirtualQueryEx(self.h, ctypes.c_void_p(adr), ctypes.byref(mbi), ctypes.sizeof(mbi)):
            taille = mbi.RegionSize
            if mbi.State == MEM_COMMIT and taille >= 0x1800000 and mbi.Protect in (0x04, 0x40, 0x02):
                tete = self._brut(mbi.BaseAddress, 8)
                if tete and tete[:6] == jeu:
                    candidats.append(mbi.BaseAddress)
            adr = (mbi.BaseAddress or 0) + taille
            if adr >= 0x7FFFFFFFFFFF:
                break
        return candidats[0] if candidats else None

    def lire(self, adr_gc, n):
        """adr_gc : adresse GameCube (0x80000000..0x817FFFFF)."""
        o = (adr_gc & 0x1FFFFFFF)
        if o + n > 0x1800000:
            raise ValueError('hors MEM1')
        b = self._brut(self.base + o, n)
        if b is None:
            raise IOError('lecture refusee %x' % adr_gc)
        return b

    def u32(self, adr):
        return struct.unpack('>I', self.lire(adr, 4))[0]

    def ecrire(self, adr_gc, data):
        o = (adr_gc & 0x1FFFFFFF)
        ec = ctypes.c_size_t(0)
        buf = ctypes.create_string_buffer(bytes(data), len(data))
        if not k32.WriteProcessMemory(self.h, ctypes.c_void_p(self.base + o), buf, len(data), ctypes.byref(ec)):
            raise IOError('ecriture refusee %x' % adr_gc)


if __name__ == '__main__':
    d = Dolphin()
    print('Dolphin pid %d, MEM1 emulee a %x, disque %s' % (d.pid, d.base, d.lire(0x80000000, 6)))
    pG = d.u32(0x80314BC8)
    print('pG = %08x' % pG)
    if 0x80000000 <= pG < 0x81800000:
        room = struct.unpack('>H', d.lire(pG + 0x4F9C, 2))[0]
        print('salle %03x' % room)
