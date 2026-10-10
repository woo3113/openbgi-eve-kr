import struct, io, os
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
N = len(chars)
def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x
def idx(L,T):
    i1 = 64 if L < 0xA0 else 0
    e = s32(T - 1 + (1 if T < 0x2D else 0))
    e = s32(e - 1 + (1 if (e & 0xFFFFFFFF) < 0x21 else 0))
    e = e - (1 if e >= 0x0E else 0); e = e - (1 if e >= 0x0B else 0)
    last = 1 if e >= 0x0A else 0
    return s32(s32((i1+L)*0x3A + e) + (~last & 0xFFFFFFFF) - 0x2B9A)
def cust(a,b): return (0x81<=a<=0x9F or 0xE0<=a<=0xEF) and 0x01<=b<=0x3F
# collect lines
lines=[]
for name in os.listdir(root):
    p=os.path.join(root,name)
    if not os.path.isfile(p): continue
    d=open(p,"rb").read()
    if len(d)>3_000_000: continue
    i=0
    while i<len(d)-40:
        if cust(d[i],d[i+1]) and cust(d[i+2],d[i+3]) and d[i+4:i+6]==b'\x2c\x20':
            j=i+6
            def take(j,n):
                for _ in range(n):
                    if j+1<len(d) and cust(d[j],d[j+1]): j+=2
                    else: return None
                return j
            j2=take(j,3)
            if j2 and d[j2:j2+1]==b'\x20':
                j3=take(j2+1,3)
                if j3 and d[j3:j3+1]==b'\x20':
                    j4=take(j3+1,4)
                    if j4 and d[j4:j4+4]==b'\x81\x63\x81\x63':
                        lines.append(d[i:j4])
        i+=1
common=set("이다는은이가를을에로고서있하지도만게아어요네니까한되된된거야군며면야뭐잠들버렸모양어머저걸뻔그")
def dec(by, off):
    o=[];i=0
    while i<len(by):
        if i+1<len(by) and cust(by[i],by[i+1]):
            k=idx(by[i],by[i+1])+off
            o.append(chr(chars[k]) if 0<=k<N and chars[k] else '?'); i+=2
        else: i+=1
    return ''.join(o)
o=io.StringIO()
best=[]
for off in range(-64,65):
    sc=0
    for l in lines:
        s=dec(l,off)
        sc += sum(1 for ch in s if ch in common)
    best.append((sc,off))
best.sort(reverse=True)
for sc,off in best[:8]:
    print(f"off={off:+d} score={sc}", file=o)
# show sample with best off
off=best[0][1]
print("--- sample with off", off, file=o)
for l in lines[:8]:
    print("  ", dec(l,off), file=o)
open(r"C:\Users\woo31\openbgi\scripts\off_search.txt","w",encoding="utf-8").write(o.getvalue())
print("lines",len(lines),"wrote")
