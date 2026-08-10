#!/usr/bin/env python3
"""RE4 .drs archives (files/em/*.drs: enemies, players, weapons): list, extract, repack, REL extraction.

usage: drs.py list <file.drs>
       drs.py rel <file.drs> <out.rel>            # the embedded REL, exact size (error if none)
       drs.py extract <file.drs> <out dir>        # manifest.json + every entry as a file
       drs.py pack <dir> <out.drs>                # inverse of extract (byte-identical)
       drs.py rebuild <file.drs> <new.rel> <out.drs>   # the archive with its REL replaced
       drs.py roundtrip <file.drs>...             # extract+pack in memory and compare

Format ("ハカセのアホーーーーーーー！！！" container, all big-endian, everything 0x20-aligned):

  0x000  char sig[0x20]      free text; the em/wep archives and 6 pl ones read "ハカセのアホーーーーーーー！！！"
                             (Shift-JIS), 11 pl ones repeat the half-width "ﾊｶｾ " (also the sound bank's)
  0x020  record[]            32 bytes each: u32 type, size, 0, offset, p0, p1, 0, 0; terminated by type
                             0xFFFFFFFF; the table area is zero up to 0x400 where the first record's data starts
         type 0              the body (below); its size is 0x20-rounded (the packer's buffer)
         type 4              the sound bank appended after the body (p0 = p1 = 0), or
         type 0xFFFFFFFE     no sound bank: size 0, offset = file size (the slot where it would go)
  body   u32 count, rel_offset, 0, 0; u32 offsets[count]; char tags[count][4]; zero pad to 0x20
         the entries in offset order (offsets relative to the body, 0x20-aligned, tags "BIN" (model),
         "TPL" (texture), "FCV"/"SEQ" (animation), "EFF", or zero for an empty entry with the next
         one's offset); no sizes: an entry runs to the next offset, the packer padded it to 0x20 with
         0xCD (uninitialised-buffer fill), the tools keep those bytes with the entry.
         rel_offset != 0: the REL module at body+rel_offset, padded to 0x20 with 0xCD (its own size
         is the end of its last relocation list, see relfile.py); the body ends there.
  sound bank                 the same container again: sig "ﾊｶｾ " x8, records type 1 and 2 (p0/p1 are
                             the same in both, bank parameters) whose data has exact sizes and zero padding.

extract writes <dir>/manifest.json, <dir>/body/NNN.<tag>, <dir>/body/<name>.rel and <dir>/snd/<type>.bin.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import relfile  # noqa: E402

TABLE_SIZE = 0x400
ALIGN = 0x20
FILL = b'\xcd'
END = 0xFFFFFFFF
EMPTY = 0xFFFFFFFE


def align(n, a=ALIGN):
    return (n + a - 1) & ~(a - 1)


def tag_name(tag):
    return tag.rstrip(b'\0').decode('ascii')


def read_records(d, base=0):
    sig = d[base:base + 0x20]
    recs = []
    o = base + 0x20
    while True:
        r = struct.unpack('>8I', d[o:o + 32])
        o += 32
        if r[0] == END:
            break
        recs.append(r)
    assert d[o:base + TABLE_SIZE] == b'\0' * (base + TABLE_SIZE - o), 'record table not zero-padded'
    return sig, recs


def write_records(sig, recs):
    out = bytearray(sig)
    for r in recs:
        out += struct.pack('>8I', *r)
    out += struct.pack('>8I', END, 0, 0, 0, 0, 0, 0, 0)
    assert len(out) <= TABLE_SIZE, 'too many records'
    return out + b'\0' * (TABLE_SIZE - len(out))


def rel_size(d):
    """Size of the REL at the start of d (end of its last relocation list)."""
    imp_offset, imp_size = struct.unpack('>2I', d[0x28:0x30])
    end = 0
    for i in range(imp_size // 8):
        _, p = struct.unpack('>2I', d[imp_offset + 8 * i:][:8])
        while d[p + 2] != relfile.R_DOLPHIN_END:
            p += 8
        end = max(end, p + 8)
    return end


class Drs:
    def __init__(self, d: bytes):
        self.sig, recs = read_records(d)
        assert len(recs) == 2 and recs[0][:4] == (0, recs[0][1], 0, TABLE_SIZE) and recs[0][4:] == (0,) * 4, recs
        body = d[TABLE_SIZE:TABLE_SIZE + recs[0][1]]
        snd = recs[1]
        if snd[0] == EMPTY:
            assert snd == (EMPTY, 0, 0, len(d), 0, 0, 0, 0), snd
            self.snd = None
        else:
            assert snd[0] == 4 and snd[2] == 0 and snd[3] == TABLE_SIZE + recs[0][1] and snd[4:] == (0,) * 4, snd
            assert snd[3] + snd[1] == len(d), 'sound bank does not end the file'
            self.snd = SoundBank(d[snd[3]:])
        # body
        count, rel_offset = struct.unpack('>2I', body[:8])
        assert body[8:16] == b'\0' * 8
        offsets = struct.unpack(f'>{count}I', body[16:16 + 4 * count])
        tags = [body[16 + 4 * count + 4 * i:][:4] for i in range(count)]
        hdr_end = 16 + 8 * count
        assert offsets[0] == align(hdr_end) and body[hdr_end:offsets[0]] == b'\0' * (offsets[0] - hdr_end)
        body_end = rel_offset or len(body)
        self.entries = []  # (tag, bytes incl. padding)
        for i in range(count):
            end = offsets[i + 1] if i + 1 < count else body_end
            assert offsets[i] % ALIGN == 0 and end >= offsets[i], (i, offsets[i], end)
            self.entries.append((tags[i], body[offsets[i]:end]))
        self.rel = None
        if rel_offset:
            n = rel_size(body[rel_offset:])
            self.rel = body[rel_offset:rel_offset + n]
            assert body[rel_offset + n:] == FILL * (len(body) - rel_offset - n), 'REL padding is not 0xCD'
        else:
            assert len(body) == align(body_end), 'body without REL not 0x20-aligned'

    def pack_body(self):
        count = len(self.entries)
        hdr_end = 16 + 8 * count
        out = bytearray()
        off = align(hdr_end)
        offsets = []
        for tag, data in self.entries:
            offsets.append(off)
            off += len(data)
        rel_offset = off if self.rel is not None else 0
        out += struct.pack('>4I', count, rel_offset, 0, 0)
        out += struct.pack(f'>{count}I', *offsets)
        for tag, _ in self.entries:
            out += tag
        out += b'\0' * (offsets[0] - len(out))
        for _, data in self.entries:
            out += data
        if self.rel is not None:
            out += self.rel
            out += FILL * (align(len(out)) - len(out))
        return out

    def pack(self):
        body = self.pack_body()
        if self.snd is None:
            snd_rec = (EMPTY, 0, 0, TABLE_SIZE + len(body), 0, 0, 0, 0)
            snd = b''
        else:
            snd = self.snd.pack()
            snd_rec = (4, len(snd), 0, TABLE_SIZE + len(body), 0, 0, 0, 0)
        return bytes(write_records(self.sig, [(0, len(body), 0, TABLE_SIZE, 0, 0, 0, 0), snd_rec]) + body + snd)


class SoundBank:
    def __init__(self, d: bytes):
        self.sig, recs = read_records(d)
        self.records = []  # (type, p0, p1, data)
        off = TABLE_SIZE
        for r in recs:
            assert r[2] == 0 and r[3] == off and r[6:] == (0, 0), r
            end = align(r[3] + r[1])
            assert d[r[3] + r[1]:end] == b'\0' * (end - r[3] - r[1]), 'sound bank padding not zero'
            self.records.append((r[0], r[4], r[5], d[r[3]:r[3] + r[1]]))
            off = end
        assert off == len(d), (off, len(d))

    def pack(self):
        recs = []
        data = bytearray()
        off = TABLE_SIZE
        for ty, p0, p1, blob in self.records:
            recs.append((ty, len(blob), 0, off, p0, p1, 0, 0))
            data += blob
            data += b'\0' * (align(len(blob)) - len(blob))
            off += align(len(blob))
        return bytes(write_records(self.sig, recs) + data)


def load(path):
    return Drs(open(path, 'rb').read())


def cmd_list(path):
    a = load(path)
    print(f'{path}: sig {a.sig.decode("shift_jis", "replace")!r}')
    off = align(16 + 8 * len(a.entries))
    for i, (tag, data) in enumerate(a.entries):
        print(f'  [{i:3}] {tag_name(tag) or "-":4} body+{off:#08x} {len(data):#8x}')
        off += len(data)
    if a.rel is not None:
        print(f'  REL   module {struct.unpack(">I", a.rel[:4])[0]:3} body+{off:#08x} {len(a.rel):#8x}')
    if a.snd is not None:
        print(f'  sound bank {sum(align(len(r[3])) for r in a.snd.records) + TABLE_SIZE:#x} bytes:')
        for ty, p0, p1, blob in a.snd.records:
            print(f'    type {ty} p0 {p0} p1 {p1} {len(blob):#x}')


def cmd_rel(path, out):
    a = load(path)
    if a.rel is None:
        sys.exit(f'{path}: no REL')
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(a.rel)
    print(f'{out}: module {struct.unpack(">I", a.rel[:4])[0]}, {len(a.rel):#x} bytes')


def cmd_extract(path, out_dir):
    a = load(path)
    name = os.path.splitext(os.path.basename(path))[0]
    os.makedirs(os.path.join(out_dir, 'body'), exist_ok=True)
    manifest = {'signature': a.sig.hex(), 'entries': [], 'rel': None, 'sound': None}
    for i, (tag, data) in enumerate(a.entries):
        fn = f'body/{i:03}.{tag_name(tag) or "empty"}'
        manifest['entries'].append({'tag': tag.hex(), 'file': fn})
        with open(os.path.join(out_dir, fn), 'wb') as f:
            f.write(data)
    if a.rel is not None:
        manifest['rel'] = f'body/{name}.rel'
        with open(os.path.join(out_dir, manifest['rel']), 'wb') as f:
            f.write(a.rel)
    if a.snd is not None:
        os.makedirs(os.path.join(out_dir, 'snd'), exist_ok=True)
        manifest['sound'] = {'signature': a.snd.sig.hex(), 'records': []}
        for ty, p0, p1, blob in a.snd.records:
            fn = f'snd/{ty}.bin'
            manifest['sound']['records'].append({'type': ty, 'p0': p0, 'p1': p1, 'file': fn})
            with open(os.path.join(out_dir, fn), 'wb') as f:
                f.write(blob)
    with open(os.path.join(out_dir, 'manifest.json'), 'w') as f:
        json.dump(manifest, f, indent=1)
        f.write('\n')


def from_dir(in_dir):
    m = json.load(open(os.path.join(in_dir, 'manifest.json')))
    a = Drs.__new__(Drs)
    a.sig = bytes.fromhex(m['signature'])
    a.entries = [(bytes.fromhex(e['tag']), open(os.path.join(in_dir, e['file']), 'rb').read()) for e in m['entries']]
    a.rel = open(os.path.join(in_dir, m['rel']), 'rb').read() if m['rel'] else None
    a.snd = None
    if m['sound']:
        a.snd = SoundBank.__new__(SoundBank)
        a.snd.sig = bytes.fromhex(m['sound']['signature'])
        a.snd.records = [(r['type'], r['p0'], r['p1'], open(os.path.join(in_dir, r['file']), 'rb').read())
                         for r in m['sound']['records']]
    return a


def cmd_pack(in_dir, out):
    with open(out, 'wb') as f:
        f.write(from_dir(in_dir).pack())


def cmd_rebuild(path, rel, out):
    a = load(path)
    if a.rel is None:
        sys.exit(f'{path}: no REL to replace')
    a.rel = open(rel, 'rb').read()
    assert rel_size(a.rel) == len(a.rel), f'{rel}: not a REL (or trailing bytes)'
    with open(out, 'wb') as f:
        f.write(a.pack())


def cmd_roundtrip(paths):
    bad = 0
    for p in paths:
        d = open(p, 'rb').read()
        out = Drs(d).pack()
        if out == d:
            print(f'{p}: OK')
        else:
            bad += 1
            n = next((i for i in range(min(len(d), len(out))) if d[i] != out[i]), min(len(d), len(out)))
            print(f'{p}: MISMATCH at {n:#x} (sizes {len(d):#x} vs {len(out):#x})')
    sys.exit(1 if bad else 0)


def main():
    cmd, *args = sys.argv[1:]
    if cmd == 'list':
        cmd_list(*args)
    elif cmd == 'rel':
        cmd_rel(*args)
    elif cmd == 'extract':
        cmd_extract(*args)
    elif cmd == 'pack':
        cmd_pack(*args)
    elif cmd == 'rebuild':
        cmd_rebuild(*args)
    elif cmd == 'roundtrip':
        cmd_roundtrip(args)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
