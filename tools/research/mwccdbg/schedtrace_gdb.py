# (run via schedtrace.sh) gdb python: trace the scheduler's picks (cycle, pcode) for FUNC's pre-RA run, block by block
import gdb, struct
inf=gdb.selected_inferior
def rd(a,n): return bytes(inf().read_memory(a,n))
def u32(a): return struct.unpack('<I',rd(a,4))[0]
def s32(a): return struct.unpack('<i',rd(a,4))[0]
def u16(a): return struct.unpack('<H',rd(a,2))[0]
def s16(a): return struct.unpack('<h',rd(a,2))[0]
def cstr(a):
    out=b''
    while True:
        b=rd(a,1)
        if b==b'\0' or len(out)>200: return out.decode('latin-1')
        out+=b; a+=1
gdb.execute("set architecture i386"); gdb.execute("set osabi none"); gdb.execute("target remote localhost:9001")
def linkname(obj):
    gdb.execute(f"call ((void (*)(void*))0x4fe6a0)({obj:#x})")
    return cstr(u32(obj+0x32)+0xA)
gdb.execute("break *0x433492")
while True:
    gdb.execute("continue")
    if int(gdb.parse_and_eval("$pc"))!=0x433492: raise SystemExit
    if linkname(u32(0x5e9ec0))==FUNC: break
gdb.execute("break *0x507d80")   # schedule block entry: [esp+4] = block
gdb.execute("break *0x507e9b")   # after select: eax = node, [esp+0xc] = cycle
gdb.execute("break *0x433e0c")   # end of pre-RA run
out=open(OUT+'/trace1.txt','w')
while True:
    gdb.execute("continue")
    pc=int(gdb.parse_and_eval("$pc"))
    if pc==0x507d80:
        b=u32(int(gdb.parse_and_eval("$esp"))+4)
        out.write('BLOCK %d\n'%s32(b+0x1c))
    elif pc==0x507e9b:
        n=int(gdb.parse_and_eval("$eax"))&0xffffffff
        if n:
            sp=int(gdb.parse_and_eval("$esp"))
            out.write('c%d %08x h=%d dl=%d\n'%(u32(sp+0xc),u32(n+0xc),u16(n+0x16),u16(n+0x14)))
    else: break
out.close(); print('traced')
gdb.execute("kill")
