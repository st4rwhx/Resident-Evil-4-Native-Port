# Ecran beige : arreter le jeu au moment exact ou pG->Cam.up devient nul, puis inspecter (sans compiler).
#   python -I enquete_beige.py <appareil> <binaire_non_epure>
import sys, time, json
sys.path.insert(0, r'C:/Users/Admin/Downloads/RE4 PROJECT/re4-decomp-upstream/port/outils')
import studio
dev, binaire = sys.argv[1], sys.argv[2]
s, f = studio.connect(dev)
t0 = time.time(); menu = False; armed = False
while time.time() - t0 < 300:
    e = studio.etat(f)
    bits = int(e['bits'], 16)
    if bits & (1 << 9) and e['ecran']['titre_alpha'] > 0.89 and not menu:
        studio.ask(f, 'pad 8 0 0 0 0 4'); time.sleep(0.6); studio.ask(f, 'pad 100 0 0 0 0 4'); menu = True
    elif e['mode'] == 6 and not (bits & (1 << 9)) and not menu:
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    elif menu and e['salle'] == '120' and (bits & 2):
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    if menu and e['salle'] == '100' and e['mode'] == 1 and not armed:
        print('r100 en jeu, image', e['image'], ': surveillance de pG->Cam.up', flush=True)
        print(studio.ask(f, 'surveiller %x 12 arret zero' % studio.adresse(f, '*pG+0x138', binaire))[0], flush=True)
        armed = True
    if armed:
        o = json.loads(studio.ask(f, 'ou')[0])
        if o['pause']:
            print('ARRET', o, flush=True)
            break
    time.sleep(0.3)
for line in studio.ask(f, 'evenements')[1:]:
    if 'surveillance' in line or 'camera' in line:
        print(line[:300], flush=True)
# inspection a l'arret
for expr, typ, n in (('*pG+0x74+0xA4', 'f32', 8), ('*pG+0x74+0xC4', 'vec3', 3), ('*pG+0x74+0xF4', 'f32', 1),
                     ('*pG+0x74', 'f32', 12)):
    print(expr, studio.voir(f, expr, typ, n, binaire), flush=True)
print(json.dumps(studio.etat(f))[:400], flush=True)
input_wait = 0
s.close()
