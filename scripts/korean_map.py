import struct, re, io
root = r"C:\Users\woo31\Downloads\EVE burst error A"
out = io.StringIO()
def P(*a): print(*a, file=out)
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
P("sjis_ext chars:", len(chars))
P("first30:", ''.join(chr(c) for c in chars[:30]))

def sjis_user_index(lead, trail):
    # index within the user-defined area (leads 0xF0..0xF9, trail 0x40..0xFC with 0x7F skip)
    if trail >= 0x80:
        t = trail - 0x41
    else:
        t = trail - 0x40
    return (lead - 0xF0) * 188 + t

def try_map(data, base_code):
    res = []
    i = 0
    n = len(data)
    while i < n:
        b = data[i]
        if 0xF0 <= b <= 0xF9 and i+1 < n:
            idx = base_code(data, i)
            if 0 <= idx < len(chars):
                res.append(chr(chars[idx]))
            else:
                res.append('?')
            i += 2
        else:
            res.append(chr(b) if 32 <= b < 127 else '.')
            i += 1
    return ''.join(res)

for name in ["a000","kojiroh_1207","kyoutuu_1209","marina_1203"]:
    d = open(root + "\\" + name,"rb").read()
    # collect the 2-byte high codes
    codes = []
    i = 0
    while i < len(d)-1:
        b = d[i]
        if b >= 0x81 and b <= 0xFC:
            codes.append((b, d[i+1]))
            i += 2
        else:
            i += 1
    from collections import Counter
    cnt = Counter(codes)
    leads = sorted(set(b for b,_ in codes))
    P(f"\n=== {name} len={len(d)} 2byte codes={len(codes)} distinct={len(cnt)} lead range=0x{leads[0]:02X}..0x{leads[-1]:02X}" if leads else f"\n=== {name} no 2byte")
    P("sample codes:", ' '.join(f'{a:02X}{b:02X}' for a,b in codes[:24]))

# Hypothesis test on a000's text: map with user-area base 0xF040
d = open(root + r"\a000","rb").read()
P("\n=== a000 mapped via user-area 0xF040 ===")
P(try_map(d, lambda data,i: sjis_user_index(data[i], data[i+1])))

open(r"C:\Users\woo31\openbgi\scripts\korean_findings.txt","w",encoding="utf-8").write(out.getvalue())
print(out.getvalue())
