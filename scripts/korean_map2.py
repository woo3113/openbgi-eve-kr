import struct
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
N = len(chars)

def u32(x): return x & 0xFFFFFFFF
def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x

def index_of(L, T):
    a = L & 0xFF
    b = T & 0xFF
    i1 = 64 if a < 0xA0 else 0
    edx = s32(b - 1 + (1 if b < 0x2D else 0))
    edx = s32(edx - 1 + (1 if (edx & 0xFFFFFFFF) < 0x21 else 0))
    ecx = 1 if edx >= 0x0E else 0
    edx = s32(edx - ecx)
    ecx = 1 if edx >= 0x0B else 0
    edx = s32(edx - ecx)
    ecx = 1 if edx >= 0x0A else 0
    edi = (i1 + a) * 0x3A
    edi = s32(edi + edx)
    ecx = u32(~ecx)
    ecx = s32(edi + ecx)
    ecx = s32(ecx + (-0x2B9A))
    return ecx

def decode(data, verbose=False):
    out = []
    i = 0
    while i < len(data):
        b = data[i]
        if (0x81 <= b <= 0x9F or 0xE0 <= b <= 0xEF) and i+1 < len(data):
            T = data[i+1]
            if (0x40 <= T <= 0x7E) or (0x80 <= T <= 0xFC):
                idx = index_of(b, T)
                if 0 <= idx < N and chars[idx]:
                    out.append(chr(chars[idx]))
                else:
                    out.append('?')
                i += 2
                continue
        out.append(chr(b) if 32 <= b < 127 else '.')
        i += 1
    return ''.join(out)

import re, io
out = io.StringIO()
# a000 text at 0x155
d = open(root + r"\a000","rb").read()
print("a000@0x155:", repr(decode(d[0x155:0x17f])), file=out)
d2 = open(root + r"\a001","rb").read()
print("a001@0x2e59b:", repr(decode(d2[0x2e59b:0x2e59b+40])), file=out)
s = decode(d2)
runs = re.findall(r'[\uac00-\ud7a3]{4,}', s)
print("a001 hangul runs:", len(runs), file=out)
for r in runs[:15]:
    print("   ", r, file=out)
open(r"C:\Users\woo31\openbgi\scripts\korean_map2_out.txt","w",encoding="utf-8").write(out.getvalue())
print("wrote")

