#!/usr/bin/env python3
"""Parse RE4 GC debug-build .sym files. Format (big-endian):
header: num_funcs, funcinfo_off, strtab_off, funcname_off
0x10..funcinfo_off: u32 offsets into strtab (source file names / section names)
funcinfo: {addr, unk4, unk8, name_off}[num_funcs], name_off relative to funcname_off
"""
import struct, sys
def parse(path):
    d=open(path,'rb').read()
    n,fi,st,fn=struct.unpack('>4I',d[:16])
    names=[]
    for o in range(0x10,fi,4):
        off=struct.unpack('>I',d[o:o+4])[0]
        names.append(d[st+off:d.index(b'\0',st+off)].decode('latin1'))
    funcs=[]
    for i in range(n):
        a,u4,u8,no=struct.unpack('>4I',d[fi+16*i:fi+16*i+16])
        s=fn+no; funcs.append((a,u4,u8,d[s:d.index(b'\0',s)].decode('latin1')))
    return names,funcs
if __name__=='__main__':
    names,funcs=parse(sys.argv[1])
    print(len(names),'strtab names;',len(funcs),'funcs')
    print('names sample:',names[:20])
    for f in funcs[:15]: print(hex(f[0]),hex(f[1]),hex(f[2]),f[3])
    print('...'); 
    for f in funcs[-5:]: print(hex(f[0]),hex(f[1]),hex(f[2]),f[3])
    import collections
    print('unk4 distinct:',len(set(f[1] for f in funcs)),'unk8 distinct:',len(set(f[2] for f in funcs)))
