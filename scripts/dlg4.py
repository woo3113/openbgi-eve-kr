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
def dec(by):
    o=[];i=0
    while i<len(by):
        if i+1<len(by) and cust(by[i],by[i+1]):
            k=idx(by[i],by[i+1]); o.append(chr(chars[k]) if 0<=k<N and chars[k] else '?'); i+=2
        else: o.append(chr(by[i]) if 32<=by[i]<127 else '\u00b7'); i+=1
    return ''.join(o)
o=io.StringIO()
for name in os.listdir(root):
    p=os.path.join(root,name)
    if not os.path.isfile(p): continue
    d=open(p,"rb").read() if os.path.isfile(p) else b""
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
                        seg=d[i:j4+4]
                        print(f"[{name} @0x{i:x}] {dec(seg)!r}   bytes={seg.hex()}", file=o)
        i+=1
open(r"C:\Users\woo31\openbgi\scripts\dlg4.txt","w",encoding="utf-8").write(o.getvalue())
print("wrote", len(o.getvalue()))
