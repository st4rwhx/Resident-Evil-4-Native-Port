"""Classe un profil simpleperf (sortie --sort dso,symbol) par famille de cout. Usage : classe_profil.py <fichier>..."""
import sys

FAM = [
    ('pilote GL', lambda d, s: 'adreno' in d or 'GLES' in d or 'libgsl' in d or 'EGL' in d),
    ('noyau', lambda d, s: 'kallsyms' in d),
    ('son', lambda d, s: 'audio' in d.lower() or 'wilhelm' in d or 'ax_hle' in s or 'Snd' in s or 'AX' in s[:3]),
    ('memoire (alloc/free)', lambda d, s: any(k in s for k in ('je_', 'malloc', 'calloc', 'free', 'operator new', 'operator delete',
                                                                'arena_', 'tcache', 'ifree', 'huge_', 'chunk_', 'extent_'))),
    ('memcpy/memset/memcmp', lambda d, s: s.startswith(('memcpy', 'memset', 'memcmp', '__wrap_memset', 'memmove', '__memcpy'))),
    ('vie des GxOp (copie, destruction, compteurs)', lambda d, s: any(k in s for k in ('GxOp::~GxOp', 'GxOp::GxOp', 'construct_at<nj::runtime::gx::GxOp',
                                                                                    'destroy_at<nj::runtime::gx::GxOp', 'RawDraw::~', '__shared_ptr', 'shared_ptr',
                                                                                    'make_pooled', '__release_shared', 'vector<nj::runtime::gx::GxOp'))),
    ('emission GX du jeu (gx_sdk put/fifo)', lambda d, s: s.startswith(('GX', '__GX', '(anonymous namespace)::put', 'gx_', 'Fifo')) and 'nj::' not in s),
    ('lecture du flux (parse, listes)', lambda d, s: any(k in s for k in ('parse_', 'read_vector', 'expand_list', 'DLContentHash', 'list_state_hash',
                                                                       'ReplayCachedCommand', 'list_snap_apply', 'ParseFifo', 'GX_DL_Fetch', 'take(',
                                                                       'reg_command', 'shadow_matrix', 'current_bank', 'xf_array'))),
    ('decodage / tirage', lambda d, s: any(k in s for k in ('DecodeDraw', 'ncp::', 'deccache', 'decode_color', 'pull_capture', 'ResolveArrayPtr',
                                                          'LiveFingerprint', 'entry_valid'))),
    ('rendu (draw_frame, lots, etats, textures)', lambda d, s: any(k in s for k in ('GlRenderer', 'draw_prim', 'push_prim', 'submit_pending', 'bind_draw_state',
                                                                                  'put_vertex', 'resident::', 'pull::', 'SelectProgram', 'rprof', 'TexCache',
                                                                                  'Upload', 'set_blend', 'set_depth', 'set_cull', 'upload_', 'cu_', 'Efb', 'efb',
                                                                                  'njgfx', 'bpprof', 'texture'))),
    ('outils dev (knob, journal, printf)', lambda d, s: any(k in s for k in ('knob', 'printf', 'vfprintf', 'NJ_LOG', 'log', 'getenv', 'strcmp', 'strlen'))),
    ('code du jeu (natif)', lambda d, s: 'libnjcore' in d and 'nj::' not in s and 'std::' not in s and not s.startswith(('@plt', '_Z'))),
]


def classify(path):
    tot = {}
    for line in open(path, encoding='utf-8', errors='replace'):
        p = line.split(None, 2)
        if len(p) < 3 or not p[0].endswith('%'):
            continue
        pct = float(p[0][:-1])
        d, s = p[1], p[2].strip()
        # nom seul : les parametres (vector<GxOp>...) faussaient le classement
        depth, cut = 0, len(s)
        for i, ch in enumerate(s):
            if ch == '<': depth += 1
            elif ch == '>': depth -= 1
            elif ch == '(' and depth == 0 and i > 0 and not s.startswith('(anonymous', i):
                cut = i
                break
        s = s[:cut] + ('(' if cut < len(s) else '')
        fam = 'autre'
        for name, f in FAM:
            if f(d, s):
                fam = name
                break
        tot[fam] = tot.get(fam, 0) + pct
    return tot


res = {f: classify(f) for f in sys.argv[1:]}
fams = sorted({k for r in res.values() for k in r}, key=lambda k: -max(r.get(k, 0) for r in res.values()))
print('%-46s' % 'famille' + ''.join('%10s' % f.split('_')[0] for f in res))
for k in fams:
    print('%-46s' % k + ''.join('%9.1f%%' % res[f].get(k, 0) for f in res))
