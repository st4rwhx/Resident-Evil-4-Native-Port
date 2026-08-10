# -*- coding: utf-8 -*-
"""Voir l'ecran du telephone sur le PC, et le commander a la souris et au clavier.

Ne telecharge rien et n'installe rien sur le telephone : tout passe par `adb`, deja present.
L'image vient de `screencap`, les clics de `input tap`, les glissements de `input swipe`, les
touches de `input keyevent`.

**Ce que cela vaut, dit franchement.** `screencap` recapture tout l'ecran a chaque image : on
obtient quelques images par seconde, pas soixante. C'est largement assez pour regarder un
lancement, verifier un affichage ou appuyer sur un bouton, et insuffisant pour juger la
fluidite d'un jeu. Pour cela il existe [scrcpy](https://github.com/Genymobile/scrcpy), qui
encode en H.264 sur le telephone et decode sur le PC - meme principe, un cran de plus en
ingenierie. Ce script ne cherche pas a le remplacer, il evite juste d'avoir a l'installer.

usage :
    python port/phone_mirror.py [--adb <chemin>] [--scale 0.5] [--fps 4]

commandes :
    clic gauche        appui
    glisser            balayage
    molette            balayage vertical court
    fleches / entree   touches directionnelles et validation
    Echap              retour
    Home               accueil
    s                  enregistre l'image courante en PNG
    q                  quitter
"""
import argparse
import io
import os
import subprocess
import sys
import time
import tkinter as tk


class Phone:
    def __init__(self, adb):
        self.adb = adb
        self.size = self._size()

    def _run(self, args, binary=False):
        env = dict(os.environ, MSYS_NO_PATHCONV='1')
        return subprocess.run([self.adb] + args, capture_output=True, env=env,
                              check=False).stdout if binary else \
            subprocess.run([self.adb] + args, capture_output=True, text=True,
                           env=env, check=False).stdout

    def _size(self):
        out = self._run(['shell', 'wm', 'size'])
        for tok in out.split():
            if 'x' in tok and tok.replace('x', '').isdigit():
                w, h = tok.split('x')
                return int(w), int(h)
        return 1080, 1920

    def grab(self):
        # `exec-out` evite la traduction CRLF que `shell` applique au flux binaire : sans lui,
        # une image sur deux est corrompue et le decodeur PNG refuse - piege classique d'adb.
        return self._run(['exec-out', 'screencap', '-p'], binary=True)

    def tap(self, x, y):
        self._run(['shell', 'input', 'tap', str(int(x)), str(int(y))])

    def swipe(self, x0, y0, x1, y1, ms=120):
        self._run(['shell', 'input', 'swipe', str(int(x0)), str(int(y0)),
                   str(int(x1)), str(int(y1)), str(int(ms))])

    def key(self, name):
        self._run(['shell', 'input', 'keyevent', name])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--adb', default='adb')
    ap.add_argument('--scale', type=float, default=0.45)
    ap.add_argument('--fps', type=float, default=4.0)
    args = ap.parse_args()

    try:
        from PIL import Image, ImageTk
    except ImportError:
        sys.exit("il faut Pillow : python -m pip install pillow")

    phone = Phone(args.adb)
    pw, ph = phone.size
    vw, vh = int(pw * args.scale), int(ph * args.scale)

    root = tk.Tk()
    root.title('telephone %dx%d' % (pw, ph))
    label = tk.Label(root)
    label.pack()
    status = tk.Label(root, text='', anchor='w')
    status.pack(fill='x')

    state = {'img': None, 'photo': None, 'drag': None, 'n': 0}

    def to_phone(ev):
        return ev.x / args.scale, ev.y / args.scale

    def on_press(ev):
        state['drag'] = to_phone(ev)

    def on_release(ev):
        if not state['drag']:
            return
        x0, y0 = state['drag']
        x1, y1 = to_phone(ev)
        state['drag'] = None
        if abs(x1 - x0) < 12 and abs(y1 - y0) < 12:
            phone.tap(x0, y0)
            status.config(text='appui (%d, %d)' % (x0, y0))
        else:
            phone.swipe(x0, y0, x1, y1)
            status.config(text='balayage (%d, %d) -> (%d, %d)' % (x0, y0, x1, y1))

    def on_wheel(ev):
        x, y = to_phone(ev)
        dy = 300 if ev.delta > 0 else -300
        phone.swipe(x, y, x, y + dy)

    KEYS = {'Up': 'DPAD_UP', 'Down': 'DPAD_DOWN', 'Left': 'DPAD_LEFT',
            'Right': 'DPAD_RIGHT', 'Return': 'ENTER', 'Escape': 'BACK',
            'Home': 'HOME', 'BackSpace': 'DEL'}

    def on_key(ev):
        if ev.keysym == 'q':
            root.destroy()
            return
        if ev.keysym == 's' and state['img']:
            name = 'ecran_%03d.png' % state['n']
            state['n'] += 1
            state['img'].save(name)
            status.config(text='enregistre : ' + name)
            return
        if ev.keysym in KEYS:
            phone.key(KEYS[ev.keysym])

    label.bind('<ButtonPress-1>', on_press)
    label.bind('<ButtonRelease-1>', on_release)
    label.bind('<MouseWheel>', on_wheel)
    root.bind('<Key>', on_key)

    period = max(1, int(1000.0 / args.fps))

    def tick():
        raw = phone.grab()
        if raw and raw[:4] == b'\x89PNG':
            try:
                img = Image.open(io.BytesIO(raw)).convert('RGB')
                state['img'] = img
                state['photo'] = ImageTk.PhotoImage(img.resize((vw, vh)))
                label.config(image=state['photo'])
            except Exception:
                pass
        root.after(period, tick)

    tick()
    root.mainloop()


if __name__ == '__main__':
    main()
