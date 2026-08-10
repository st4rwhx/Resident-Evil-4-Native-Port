#!/usr/bin/env python3
"""Builds a plain (unpartitioned) GameCube disc image directly from an
extracted disc directory (sys/ + files/), placing every component at the
EXACT byte offset recorded in boot.bin/fst.bin. Used instead of `wit`
(which insists on wrapping output in Wii-style partitions) since we just
need a byte-faithful GC ISO with main.dol swapped for our own build of the
identical size - no re-layout needed at all.
"""
import struct
import sys
import os

SRC = sys.argv[1]       # e.g. orig/debug_disc1
DOL_OVERRIDE = sys.argv[2] if len(sys.argv) > 2 else None  # our reconstructed.dol
OUT = sys.argv[3]

GC_DISC_SIZE = 0x57058000  # standard single-layer GC disc capacity


def read(path):
    with open(path, "rb") as f:
        return f.read()


def parse_fst(fst_bytes):
    # root entry: type=1(dir), name_offset=0, parent=0, num_entries=total entry count
    root_type, root_name_off_hi, root_parent, num_entries = struct.unpack_from(">BBHII".replace("BB", "B"), fst_bytes, 0)
    # simpler manual parse of 12-byte entries
    def entry(i):
        off = i * 12
        b0 = fst_bytes[off]
        name_off = (fst_bytes[off + 1] << 16) | (fst_bytes[off + 2] << 8) | fst_bytes[off + 3]
        val1 = struct.unpack_from(">I", fst_bytes, off + 4)[0]
        val2 = struct.unpack_from(">I", fst_bytes, off + 8)[0]
        is_dir = b0 == 1
        return is_dir, name_off, val1, val2

    _, _, _, num_entries = entry(0)
    str_table_off = num_entries * 12

    def get_name(name_off):
        start = str_table_off + name_off
        end = fst_bytes.index(b"\0", start)
        return fst_bytes[start:end].decode("shift-jis", errors="replace")

    files = []  # (path, offset, length)
    # stack-based traversal using parent/next-index semantics
    path_stack = [""]
    i = 1
    # directory entries: val1=parent_index, val2=next_index (index of entry after this dir's last child)
    # file entries: val1=file_offset, val2=file_length
    dir_end_stack = [num_entries]
    cur_path = ""
    path_of = {0: ""}
    while i < num_entries:
        is_dir, name_off, val1, val2 = entry(i)
        name = get_name(name_off)
        parent_path = None
        # find parent path: parent is the most recent dir whose end > i
        # simplest: maintain explicit stack of (end_index, path)
        while dir_end_stack and dir_end_stack[-1] <= i:
            dir_end_stack.pop()
        parent_path = path_of.get(id(dir_end_stack), "")
        i += 1
    # The above generic approach is error-prone; use a simpler explicit stack algorithm instead.
    return None


def parse_fst_v2(fst_bytes):
    def entry(i):
        off = i * 12
        b0 = fst_bytes[off]
        name_off = (fst_bytes[off + 1] << 16) | (fst_bytes[off + 2] << 8) | fst_bytes[off + 3]
        val1 = struct.unpack_from(">I", fst_bytes, off + 4)[0]
        val2 = struct.unpack_from(">I", fst_bytes, off + 8)[0]
        is_dir = b0 == 1
        return is_dir, name_off, val1, val2

    _, _, _, num_entries = entry(0)
    str_table_off = num_entries * 12

    def get_name(name_off):
        start = str_table_off + name_off
        end = fst_bytes.index(b"\0", start)
        return fst_bytes[start:end].decode("shift-jis", errors="replace")

    files = []
    # stack of (end_index, dir_path)
    stack = [(num_entries, "")]
    i = 1
    while i < num_entries:
        while stack and i >= stack[-1][0]:
            stack.pop()
        parent_path = stack[-1][1] if stack else ""
        is_dir, name_off, val1, val2 = entry(i)
        name = get_name(name_off)
        full = (parent_path + "/" + name) if parent_path else name
        if is_dir:
            # val2 is the index of the entry AFTER this directory's subtree
            stack.append((val2, full))
            i += 1
        else:
            files.append((full, val1, val2))
            i += 1
    return files, num_entries


def main():
    sys_dir = os.path.join(SRC, "sys")
    files_dir = os.path.join(SRC, "files")

    boot = bytearray(read(os.path.join(sys_dir, "boot.bin")))
    bi2 = read(os.path.join(sys_dir, "bi2.bin"))
    appl = read(os.path.join(sys_dir, "apploader.img"))
    fst = read(os.path.join(sys_dir, "fst.bin"))
    dol = read(DOL_OVERRIDE) if DOL_OVERRIDE else read(os.path.join(sys_dir, "main.dol"))

    dol_offset = struct.unpack_from(">I", boot, 0x420)[0]
    fst_offset = struct.unpack_from(">I", boot, 0x424)[0]
    fst_size = struct.unpack_from(">I", boot, 0x428)[0]

    assert len(fst) == fst_size, f"fst.bin size {len(fst):#x} != boot.bin fst_size {fst_size:#x}"
    orig_dol_size = None
    orig_dol_path = os.path.join(sys_dir, "main.dol")
    if os.path.exists(orig_dol_path):
        orig_dol_size = os.path.getsize(orig_dol_path)
    if orig_dol_size is not None and len(dol) != orig_dol_size:
        print(f"WARNING: replacement DOL size {len(dol):#x} != original {orig_dol_size:#x} "
              f"- offsets after the DOL may need adjustment (not handled by this simple builder)")

    files, num_entries = parse_fst_v2(fst)
    print(f"Parsed {len(files)} files from FST ({num_entries} total entries)")

    max_end = 0
    for path, off, length in files:
        max_end = max(max_end, off + length)
    max_end = max(max_end, fst_offset + fst_size, dol_offset + len(dol))
    print(f"Highest offset+size needed: {max_end:#x}")

    total_size = GC_DISC_SIZE
    if max_end > total_size:
        print(f"WARNING: content extends past standard GC disc size, growing image to {max_end:#x}")
        total_size = max_end

    print(f"Allocating output image: {total_size:#x} bytes ({total_size/1024/1024/1024:.2f} GiB)")

    with open(OUT, "wb") as out:
        out.truncate(total_size)

        def write_at(offset, data):
            out.seek(offset)
            out.write(data)

        write_at(0, boot)
        write_at(0x440, bi2)
        write_at(0x2440, appl)
        write_at(dol_offset, dol)
        write_at(fst_offset, fst)

        missing = []
        written = 0
        for path, off, length in files:
            src_path = os.path.join(files_dir, path)
            if not os.path.exists(src_path):
                missing.append(path)
                continue
            actual_size = os.path.getsize(src_path)
            if actual_size != length:
                print(f"WARNING: {path} size {actual_size:#x} != FST length {length:#x}, using FST length")
            with open(src_path, "rb") as sf:
                data = sf.read(length)
            write_at(off, data)
            written += 1
            if written % 100 == 0:
                print(f"  ...{written}/{len(files)} files written")

        print(f"Wrote {written} files, {len(missing)} missing")
        if missing:
            print("Missing files (first 10):", missing[:10])

    print(f"Done: {OUT}")


if __name__ == "__main__":
    main()
