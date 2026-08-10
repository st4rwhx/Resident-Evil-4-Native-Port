# (run via scheddump.sh) gdb python: dump the PCode of FUNC's blocks right before/after both scheduler runs (GC/2.6 mwcceppc.exe)
import gdb, struct
inf=gdb.selected_inferior
def rd(a,n): return bytes(inf().read_memory(a,n))
def u32(a): return struct.unpack('<I',rd(a,4))[0]
def s32(a): return struct.unpack('<i',rd(a,4))[0]
def u16(a): return struct.unpack('<H',rd(a,2))[0]
def s16(a): return struct.unpack('<h',rd(a,2))[0]
def u8(a): return rd(a,1)[0]
def cstr(a):
    out=b''
    while True:
        b=rd(a,1)
        if b==b'\0' or len(out)>200: return out.decode('latin-1')
        out+=b; a+=1
gdb.execute("set architecture i386"); gdb.execute("set osabi none"); gdb.execute("target remote localhost:9001")
OPI=0x5c0fa8
names=[cstr(u32(OPI+i*0x12)) for i in range(471)]
def linkname(obj):
    gdb.execute(f"call ((void (*)(void*))0x4fe6a0)({obj:#x})")
    return cstr(u32(obj+0x32)+0xA)
def objname(obj):
    try: return cstr(u32(obj+0xA)+0xA)
    except Exception: return '?'
def dump_blocks(tag, out):
    b=u32(0x5ea748)
    nbits=u32(0x5e2fd8)
    while b:
        out.write('BLOCK %d flags=%04x count=%d weight=%d\n'%(s32(b+0x1c),u16(b+0x2a),s16(b+0x28),s32(b+0x24)))
        p=u32(b+0x14)
        while p:
            op=s16(p+0x20); argc=s16(p+0x22); fl=u32(p+0x14); al=u32(p+0x18)
            s='P %08x %s flags=%08x'%(p,names[op],fl)
            args=[]
            for i in range(argc):
                a=p+0x24+i*0xc; k=u8(a)
                if k==0: args.append('R%d:%d:%x'%(u8(a+1),s16(a+4),u16(a+2)))
                elif k==1: args.append('S%d'%u16(a+4))
                elif k==2: args.append('I%d'%s32(a+2))
                elif k==3: args.append('M%d:%s'%(s32(a+2),objname(u32(a+6))))
                elif k==4: args.append('L')
                else: args.append('K%d'%k)
            s+=' '+' '.join(args)
            if fl&0x60006 and not al: s+=' alias=NULL'
            if al and fl&0x60006:
                t=u8(al+0x2c)
                if t in (0,1):
                    o=u32(al+0x10)
                    s+=' alias=t%d@%08x obj=%08x(%s) off=%d size=%d idx=%d'%(t,al,o,objname(o) if o else '-',s32(al+0x14),s32(al+0x18),s32(al+0x28))
                else:
                    bs=u32(al+0x24); n=(nbits+31)//32
                    words=[u32(bs+4*i) for i in range(n)] if bs else []
                    s+=' alias=t2@%08x idx=%d bits=%s'%(al,s32(al+0x28),','.join('%08x'%w for w in words))
            out.write(s+'\n')
            p=u32(p)
        b=u32(b)
BPS={0x507d31:'pre',0x433e0c:'post1',0x434063:'post2'}   # 0x507d31 = after the scheduler's per-function alias init 0x511f20, before any block
gdb.execute("break *0x433492")
while True:
    gdb.execute("continue")
    if int(gdb.parse_and_eval("$pc"))!=0x433492: raise SystemExit
    if linkname(u32(0x5e9ec0))==FUNC: break
for a in BPS: gdb.execute(f"break *{a:#x}")
for i in range(4):
    gdb.execute("continue")
    pc=int(gdb.parse_and_eval("$pc"))
    if pc==0x433492: break
    name=BPS[pc]
    if name=='pre': name='pre1' if i==0 else 'pre2'
    with open(OUT+'/sched-'+name+'.txt','w') as f: dump_blocks(name,f)
    print('dumped',name)
gdb.execute("kill")
