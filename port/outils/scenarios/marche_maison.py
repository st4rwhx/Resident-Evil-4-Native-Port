# Nouvelle partie sans rien passer (comme cine_faces) : en r100, camera relevee et captures pendant la marche.
#   python -I marche.py <appareil> <duree_s> <dossier>
import os, sys, time, json
sys.path.insert(0, r'./port/outils')
import studio
dev, duree, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
os.makedirs(out, exist_ok=True)
s, f = studio.connect(dev)
ev = open(os.path.join(out, 'evenements.txt'), 'w', encoding='utf-8')
cam = open(os.path.join(out, 'camera.txt'), 'w', encoding='utf-8')
t0 = time.time(); menu = False; last_cap = 0; ncap = 0
while time.time() - t0 < duree:
    e = studio.etat(f)
    bits = int(e['bits'], 16); ec = e['ecran']
    for line in studio.ask(f, 'evenements')[1:]:
        ev.write(line + '\n')
    if bits & (1 << 9) and ec['titre_alpha'] > 0.89 and not menu:
        studio.ask(f, 'pad 8 0 0 0 0 4'); time.sleep(0.6); studio.ask(f, 'pad 100 0 0 0 0 4'); menu = True
    elif e['mode'] == 6 and not (bits & (1 << 9)) and not menu:
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    elif menu and e['salle'] == '120' and (bits & 2):
        studio.ask(f, 'pad 1000 0 0 0 0 4')   # cinematique de la voiture passee
    if menu and e['salle'] == '100':
        c = e['camera']
        cam.write('%d mode %d pos %s vise %s haut %s leon %.0f %.0f\n' % (e['image'], e['mode'], c['pos'], c['vise'], c['haut'], e['leon']['x'], e['leon']['z']))
        if e['mode'] == 2 and time.time() - last_cap > 1.2 and ncap < 16:
            studio.ask(f, 'capture'); last_cap = time.time(); ncap += 1
            print('capture %d image %d' % (ncap, e['image']), flush=True)
    time.sleep(0.4)
ev.flush(); cam.flush()
s.close()
print('fini', flush=True)
