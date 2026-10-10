import struct, io, os, re
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
    e = e - (1 if e >= 0x0E else 0)
    e = e - (1 if e >= 0x0B else 0)
    last = 1 if e >= 0x0A else 0
    edi = s32((i1+L)*0x3A + e)
    return s32(edi + (~last & 0xFFFFFFFF) - 0x2B9A)
def custom(b): return (0x81<=b[0]<=0x9F or 0xE0<=b[0]<=0xEF) and 0x01<=b[1]<=0x3F
def dec(by):
    o=[]; i=0
    while i<len(by):
        if i+1<len(by) and custom(by[i:i+2]):
            k=idx(by[i],by[i+1]); o.append(chr(chars[k]) if 0<=k<N and chars[k] else '?'); i+=2
        else:
            o.append(chr(by[i]) if 32<=by[i]<127 else '\u00b7'); i+=1
    return ''.join(o)
o=io.StringIO()
hits=0
for name in os.listdir(root):
    p=os.path.join(root,name)
    if not os.path.isfile(p): continue
    try: d=open(p,"rb").read()
    except: continue
    if len(d)>3_000_000: continue
    i=0
    while i < len(d)-40:
        # try to match structure: c c 2C 20 c c c 20 c c c 20 c c c c 81 63 81 63
        p2=d[i:i+2]
        if custom(p2):
            j=i+2
            if d[j:j+2]==b'\x2c\x20':
                j+=2
                cnt=0
                while j+1<len(d) and custom(d[j:j+2]): j+=2; cnt+=1
                if cnt==3 and d[j:j+1]==b'\x20':
                    j+=1; cnt2=0
                    while j+1<len(d) and custom(d[j:j+2]): j+=2; cnt2+=1
                    if cnt2==3 and d[j:j+1]==b'\x20':
                        j+=1; cnt3=0
                        while j+1<len(d) and custom(d[j:j+2]): j+=2; cnt3+=1
                        if cnt3==4 and d[j:j+4]==b'\x81\x63\x81\x63':
                            seg=d[i:j+4]
                            print(f"[{name} @0x{i:x}] {dec(seg)!r}", file=o)
                            hits+=1
                            if hits>20: break
        i+=1
    if hits>20: break
print("hits", hits, file=o)
open(r"C:\Users\woo31\openbgi\scripts\dlg3.txt","w",encoding="utf-8").write(o.getvalue())
print("wrote", hits)
