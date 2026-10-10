import struct, io, os, re
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
o=io.StringIO()
for name in os.listdir(root):
    p=os.path.join(root,name)
    if not os.path.isfile(p): continue
    try: d=open(p,"rb").read()
    except: continue
    if len(d)>3_000_000: continue
    # find custom-code positions and decode to text with byte offsets
    i=0; out=[]
    while i<len(d):
        b=d[i]
        if i+1<len(d) and is_custom(b,d[i+1]):
            idx=index_of(b,d[i+1])
            out.append((i, chr(chars[idx]) if 0<=idx<N and chars[idx] else '?', (b,d[i+1]), idx))
            i+=2
        else:
            out.append((i, chr(b) if 32<=b<127 else '.', (b,), -1)); i+=1
    s=''.join(x[1] for x in out)
    for t in ["춘칠","태을싄","응좋대"]:
        k=s.find(t)
        if k>=0:
            # map char index to byte offset
            boff=out[k][0]
            print(f"[{name}] '{t}' at char {k} bytes@0x{boff:x}", file=o)
            seg=out[max(0,k-6):k+10]
            print("   codes:", ' '.join(f"{c[0]:02X}{c[1] if len(c[2])==2 else 0:02X}/idx{c[3]}" if len(c[2])==2 else f"'{c[1]}'/{c[2][0]:02X}" for c in seg), file=o)
            break
open(r"C:\Users\woo31\openbgi\scripts\dialogue_find.txt","w",encoding="utf-8").write(o.getvalue())
print("wrote")
