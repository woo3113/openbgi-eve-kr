import struct, re, collections, io
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
N = len(chars)
def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x
def index_of(L, T):
    a=L&0xFF; b=T&0xFF
    i1 = 64 if a < 0xA0 else 0
    edx = s32(b - 1 + (1 if b < 0x2D else 0))
    edx = s32(edx - 1 + (1 if (edx & 0xFFFFFFFF) < 0x21 else 0))
    ecx = 1 if edx >= 0x0E else 0; edx = s32(edx-ecx)
    ecx = 1 if edx >= 0x0B else 0; edx = s32(edx-ecx)
    ecx = 1 if edx >= 0x0A else 0
    edi = s32(s32((i1+a)*0x3A) + edx)
    ecx = s32(edi + (~ecx & 0xFFFFFFFF))
    return s32(ecx - 0x2B9A)
def decode(data):
    out=[]; i=0
    while i < len(data):
        b=data[i]
        if (0x81<=b<=0x9F or 0xE0<=b<=0xEF) and i+1<len(data):
            T=data[i+1]
            if (0x40<=T<=0x7E) or (0x80<=T<=0xFC):
                idx=index_of(b,T)
                out.append(chr(chars[idx]) if 0<=idx<N and chars[idx] else '?')
                i+=2; continue
        out.append(chr(b) if 32<=b<127 else ' ')
        i+=1
    return ''.join(out)
d=open(root+r"\a001","rb").read()
s=decode(d)
cnt=collections.Counter(c for c in s if 0xAC00<=ord(c)<=0xD7A3)
o=io.StringIO()
print("total hangul:", sum(cnt.values()), "distinct:", len(cnt), file=o)
print("top 30:", ''.join(f"{c}:{n} " for c,n in cnt.most_common(30)), file=o)
# sample sentences
runs=re.findall(r'[\uac00-\ud7a3]{6,}', s)
print("sample runs:", file=o)
for r in runs[:15]: print("  ", r, file=o)
open(r"C:\Users\woo31\openbgi\scripts\korean_freq.txt","w",encoding="utf-8").write(o.getvalue())
print("wrote")
