# Arret au changement d'etat de CamCtrl en r100, puis images une par une avec releve des cameras.
#   python -I pas_a_pas.py <appareil> <binaire> <images>
import sys, time, json, struct
sys.path.insert(0, r'C:/Users/Admin/Downloads/RE4 PROJECT/re4-decomp-upstream/port/outils')
import studio
dev, binaire, nimg = sys.argv[1], sys.argv[2], int(sys.argv[3])
s, f = studio.connect(dev)
t0 = time.time(); menu = False; armed = False
while time.time() - t0 < 300:
    e = studio.etat(f); bits = int(e['bits'], 16)
    if bits & (1 << 9) and e['ecran']['titre_alpha'] > 0.89 and not menu:
        studio.ask(f, 'pad 8 0 0 0 0 4'); time.sleep(0.6); studio.ask(f, 'pad 100 0 0 0 0 4'); menu = True
    elif e['mode'] == 6 and not (bits & (1 << 9)) and not menu:
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    elif menu and e['salle'] == '120' and (bits & 2):
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    if menu and e['salle'] == '100' and e['mode'] == 1 and not armed:
        print(studio.ask(f, 'surveiller %x 1 arret' % studio.adresse(f, 'CamCtrl+0x34', binaire))[0], flush=True)
        armed = True
    if armed and json.loads(studio.ask(f, 'ou')[0])['pause']:
        st = studio.lire(f, studio.adresse(f, 'CamCtrl+0x34', binaire), 1)[0]
        if st == 5:          # camera animee (cinematique) : c'est le moment cherche
            break
        studio.ask(f, 'reprendre')   # autre changement d'etat : on continue
    time.sleep(0.25)

def releve():
    o = json.loads(studio.ask(f, 'ou')[0])
    cc = studio.lire(f, studio.adresse(f, 'CamCtrl', binaire), 0x6A0)
    g = studio.lire(f, studio.adresse(f, '*pG+0x74', binaire), 0xF8)
    st, = struct.unpack_from('<B', cc, 0x34)
    x250, = struct.unpack_from('<I', cc, 0x250)
    extra, = struct.unpack_from('<I', cc, 0x68C)
    cp = struct.unpack_from('<8f', cc, 0x60 + 0xA4); cu = struct.unpack_from('<3f', cc, 0x60 + 0xC4)
    gp = struct.unpack_from('<8f', g, 0xA4); gu = struct.unpack_from('<3f', g, 0xC4); gd, = struct.unpack_from('<f', g, 0xF4)
    r = lambda t: tuple(round(v, 2) for v in t)
    print('img %d %-12s etat %d x250 %x extra %x | ctrl pos %s at %s rf %s up %s | jeu pos %s at %s rf %s up %s dist %.2f' % (
        o['image'], o['arret'] or o['dernier_point'], st, x250, extra, r(cp[0:3]), r(cp[3:6]), r(cp[6:8]), r(cu),
        r(gp[0:3]), r(gp[3:6]), r(gp[6:8]), r(gu), gd), flush=True)

releve()
studio.ask(f, 'image 1')
time.sleep(1.5)
cc = studio.lire(f, studio.adresse(f, 'CamCtrl', binaire), 0x6A0)
extra, = struct.unpack_from('<I', cc, 0x68C)
info = studio.lire(f, extra + 0x100, 0x48)
data, keytbl = struct.unpack_from('<II', info, 0)
print('extra %x data %x keytbl %x nParts %d maxFrame %g frame %g' % (extra, data, keytbl, info[0x30], struct.unpack_from('<f', info, 0x20)[0], struct.unpack_from('<f', info, 0x24)[0]), flush=True)
print('donnees', studio.lire(f, data, 96).hex(), flush=True)
ra = int.from_bytes(studio.lire(f, studio.adresse(f, '*pG+0x40', binaire), 4), 'little')
tab = studio.lire(f, ra, 0x10 + 4 * 64)
n = int.from_bytes(tab[0:4], 'little')
offs = [int.from_bytes(tab[0x10 + 4 * i:0x14 + 4 * i], 'little') for i in range(min(n, 60))]
rel = data - ra
idx = max([i for i, o in enumerate(offs) if o and o <= rel] or [-1])
print('archive salle %x n %d : donnees a +%x, entree %d (debut +%x)' % (ra, n, rel, idx, offs[idx] if idx >= 0 else 0), flush=True)
for k in range(0):
    studio.ask(f, 'image 1')
    for _ in range(100):
        time.sleep(0.05)
        if json.loads(studio.ask(f, 'ou')[0])['pause']:
            break
    releve()
for line in studio.ask(f, 'evenements')[1:]:
    if 'surveillance' in line or 'camera_' in line:
        print(line[:260], flush=True)
studio.ask(f, 'oublier'); studio.ask(f, 'reprendre')
s.close()
