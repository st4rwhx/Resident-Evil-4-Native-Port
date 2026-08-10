#!/usr/bin/env python3
"""DOL section-table helpers.

RE4's linker put .ctors and .dtors into one DOL data section. decomp-toolkit needs them as
separate sections (each is a null-terminated pointer list and the two lists interleave the
per-object link order), so:

  split  <in.dol> <out.dol> <addr>   split the data section containing <addr> at <addr>
  merge  <in.dol> <out.dol> <addr>   merge the two data sections that meet at <addr>

Both operations preserve the memory image byte-for-byte; only the header differs.
"""
import struct, sys

def read(path):
    d = bytearray(open(path, 'rb').read())
    offs = list(struct.unpack('>18I', d[0:72])); addrs = list(struct.unpack('>18I', d[72:144])); sizes = list(struct.unpack('>18I', d[144:216]))
    return d, offs, addrs, sizes

def write(path, d, offs, addrs, sizes):
    d[0:72] = struct.pack('>18I', *offs); d[72:144] = struct.pack('>18I', *addrs); d[144:216] = struct.pack('>18I', *sizes)
    open(path, 'wb').write(d)

def split(inp, out, addr):
    d, offs, addrs, sizes = read(inp)
    for i in range(7, 18):
        if sizes[i] and addrs[i] < addr < addrs[i] + sizes[i]:
            break
    else:
        sys.exit(f"no data section contains {addr:#x}")
    assert sizes[17] == 0, "no free data section slot"
    delta = addr - addrs[i]
    offs.insert(i + 1, offs[i] + delta); addrs.insert(i + 1, addr); sizes.insert(i + 1, sizes[i] - delta)
    del offs[17], addrs[17], sizes[17]
    sizes[i] = delta
    write(out, d, offs, addrs, sizes)
    print(f"split data section {i} at {addr:#x}: {sizes[i]:#x} + {sizes[i+1]:#x}")

def merge(inp, out, addr):
    d, offs, addrs, sizes = read(inp)
    for i in range(7, 17):
        if sizes[i] and sizes[i + 1] and addrs[i] + sizes[i] == addr == addrs[i + 1]:
            assert offs[i] + sizes[i] == offs[i + 1], "sections not file-contiguous"
            sizes[i] += sizes[i + 1]
            del offs[i + 1], addrs[i + 1], sizes[i + 1]
            offs.append(0); addrs.append(0); sizes.append(0)
            write(out, d, offs, addrs, sizes)
            return
    sys.exit(f"no section boundary at {addr:#x}")

if __name__ == '__main__':
    if sys.argv[1] == 'split':
        split(sys.argv[2], sys.argv[3], int(sys.argv[4], 16))
    elif sys.argv[1] == 'merge':
        merge(sys.argv[2], sys.argv[3], int(sys.argv[4], 16))
    else:
        sys.exit(__doc__)
