"""blkflags.py DUMPDIR: per basic block the `:{xxxx}` flag word after every backend pass >= 11
(0x8 = "scheduled": set by the pre-RA scheduler, cleared by any pass that rewrites the block --
peephole-forward sinks, the RA deleting a dead def / coalesced copy, the post-RA peephole record-form merge;
the post-RA scheduler re-runs only blocks whose 0x8 is clear). CRI pass 38/41/46."""
import sys,re,os,glob
d=sys.argv[1]
files=sorted(glob.glob(os.path.join(d,'backend-*')))
res={}
order=[]
for f in files:
    n=os.path.basename(f)[8:10]
    lines=open(f).read().split('\n')
    flag=None
    for i,l in enumerate(lines):
        m=re.match(r':\{([0-9a-fA-F]+)\}',l)
        if m: flag=m.group(1); continue
        m=re.match(r'(B\d+):',l)
        if m and flag is not None:
            b=m.group(1)
            if b not in res: res[b]={}; order.append(b)
            res[b][n]=flag
            flag=None
passes=sorted({p for b in res for p in res[b]})
sel=[p for p in passes if int(p)>=11]
print('blk  '+' '.join(sel))
for b in order:
    print(b.ljust(4),' '.join(res[b].get(p,'----') for p in sel))
