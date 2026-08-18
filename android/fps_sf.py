#!/usr/bin/env python3
"""Images par seconde et memoire d'un build Android **de diffusion** (sans journal), vues de
l'exterieur : horodatages de presentation de SurfaceFlinger (--latency, 127 dernieres images) et
PSS de dumpsys meminfo, toutes les N secondes.

  python android/fps_sf.py [secondes=120] [pas=5]
"""
import subprocess
import sys
import time

ADB = 'C:/android/sdk/platform-tools/adb.exe'
PKG = 'com.ayano.nightjar'
LAYER = 'SurfaceView - com.ayano.nightjar/org.nightjar.re4.RE4Activity#0'


def sh(cmd):
    return subprocess.run([ADB, 'shell', cmd], capture_output=True, text=True, errors='replace').stdout


def main():
    total = int(sys.argv[1]) if len(sys.argv) > 1 else 120
    pas = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    vus = set()
    t0 = time.time()
    while time.time() - t0 < total:
        lines = sh(f"dumpsys SurfaceFlinger --latency '{LAYER}'").split('\n')[1:]
        stamps = []
        for l in lines:
            p = l.split()
            if len(p) == 3 and p[1] != '0' and int(p[1]) < 2 ** 62:
                stamps.append(int(p[1]))
        new = [s for s in stamps if s not in vus]
        vus.update(stamps)
        win = [s for s in stamps if s >= max(stamps) - pas * 10 ** 9] if stamps else []
        fps = (len(win) - 1) / ((win[-1] - win[0]) / 1e9) if len(win) > 1 and win[-1] > win[0] else 0.0
        pss = 0
        for l in sh(f'dumpsys meminfo {PKG}').split('\n'):
            if l.strip().startswith('TOTAL') and pss == 0:
                try:
                    pss = int(l.split()[1]) // 1024
                except (IndexError, ValueError):
                    pass
        print('%4.0f s  %5.1f i/s  (%d nouvelles)  memoire %d Mo' % (time.time() - t0, fps, len(new), pss),
              flush=True)
        time.sleep(pas)


if __name__ == '__main__':
    main()
