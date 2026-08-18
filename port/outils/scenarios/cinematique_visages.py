# Essai visages : titre -> NEW GAME, puis pendant la cinematique d'ouverture : evenements forme_* + captures.
#   python -I cine_faces.py <appareil> <duree_s> <dossier_sortie>
import json, os, subprocess, sys, time
sys.path.insert(0, r'./port/outils')
import studio

dev, duree, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
os.makedirs(out, exist_ok=True)
s, f = studio.connect(dev)
for c in sys.argv[4:]:
    print(c, studio.ask(f, c), flush=True)
log = open(os.path.join(out, 'evenements.txt'), 'w', encoding='utf-8')
dumps = {}
t0 = time.time(); menu = False; cine_t = None; last_cap = 0; ncap = 0
while time.time() - t0 < duree:
    e = studio.etat(f)
    bits = int(e['bits'], 16); ec = e['ecran']
    for line in studio.ask(f, 'evenements')[1:]:
        log.write(line + '\n')
        if 'forme_lancee' in line and 'data=0x' in line:
            a = int(line.split('data=0x')[1].split()[0], 16)
            if a not in dumps:
                dumps[a] = b''.join(bytes.fromhex(json.loads(studio.ask(f, 'memoire %x 2048' % (a + k))[0])['octets']) for k in range(0, 8192, 2048))
                open(os.path.join(out, 'forme_%08x.bin' % a), 'wb').write(dumps[a])
                print('donnees de visage lues', hex(a), flush=True)
    log.flush()
    if bits & (1 << 9) and ec['titre_alpha'] > 0.89 and not menu:
        studio.ask(f, 'pad 8 0 0 0 0 4'); time.sleep(0.6); studio.ask(f, 'pad 100 0 0 0 0 4'); menu = True
        print('NEW GAME', flush=True)
    elif e['mode'] == 6 and not (bits & (1 << 9)):
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    elif bits & (1 << 2):
        studio.ask(f, 'pad 100 0 0 0 0 4')
    if menu and e['salle'] == '120':
        if cine_t is None:
            cine_t = time.time(); print('cinematique salle', e['salle'], flush=True)
        if time.time() - last_cap > 6 and ncap < 14:
            studio.ask(f, 'capture'); last_cap = time.time(); ncap += 1
            print('capture %d image %d salle %s' % (ncap, e['image'], e['salle']), flush=True)
    time.sleep(1.0)
s.close()
print('fini', time.time() - t0, flush=True)
