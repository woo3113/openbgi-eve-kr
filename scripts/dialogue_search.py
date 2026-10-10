import struct, re, io, os
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
N = len(chars)
def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x
def index_of(L, T):
    i1 = 64 if L < 0xA0 else 0
    edx = s32(T - 1 + (1 if T < 0x2D else 0))
    edx = s32(edx - 1 + (1 if (edx & 0xFFFFFFFF) < 0x21 else 0))
    ecx = 1 if edx >= 0x0E else 0; edx = s32(edx-ecx)
    ecx = 1 if edx >= 0x0B else 0; edx = s32(edx-ecx)
    ecx = 1 if edx >= 0x0A else 0
    edi = s32(s32((i1+L)*0x3A) + edx)
    return s32(s32(edi + (~ecx & 0xFFFFFFFF)) - 0x2B9A)
def is_custom(L,T): return (0x81<=L<=0x9F or 0xE0<=L<=0xEF) and 0x01<=T<=0x3F
def decode(data):
    out=[]; i=0
    while i < len(data):
        b=data[i]
        if i+1<len(data) and is_custom(b,data[i+1]):
            idx=index_of(b,data[i+1])
            out.append(chr(chars[idx]) if 0<=idx<N and chars[idx] else '\ufffd')
            i+=2; continue
        out.append(chr(b) if 32<=b<127 else '\u00b7'); i+=1
    return ''.join(out)
o=io.StringIO()
targets=["잠들","모양","뭐야"]
for name in os.listdir(root):
    p=os.path.join(root,name)
    if not os.path.isfile(p): continue
    try:
        d=open(p,"rb").read()
    except: continue
    if len(d) > 3_000_000: continue
    s=decode(d)
    for t in targets:
        if t in s:
            k=s.find(t)
            print(f"[{name}] {t}: ...{s[max(0,k-20):k+40]!r}", file=o)
            break
open(r"C:\Users\woo31\openbgi\scripts\dialogue_search.txt","w",encoding="utf-8").write(o.getvalue())
print("wrote", len(o.getvalue()))
