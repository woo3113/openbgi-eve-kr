import struct, re
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]

def ordinal(L, T):
    if 0x81 <= L <= 0x9F:
        t = T - (0x41 if T >= 0x80 else 0x40)
        return (L - 0x81) * 188 + t
    return None

def decode(data):
    out = []
    i = 0
    while i < len(data):
        b = data[i]
        if 0x81 <= b <= 0x9F and i+1 < len(data):
            o = ordinal(b, data[i+1])
            if o is not None and 0 <= o < len(chars):
                out.append(chr(chars[o]))
            else:
                out.append('~')
            i += 2
        else:
            out.append(chr(b) if 32 <= b < 127 else '\u00b7')
            i += 1
    return ''.join(out)

lines = []
for name in ["a000","a001","b001","marina_1203"]:
    d = open(root + "\\" + name,"rb").read()
    s = decode(d)
    runs = re.findall(r'[\uac00-\ud7a3\u3131-\u318e]{2,}', s)
    lines.append(f"=== {name}: {len(runs)} hangul runs ===")
    for r in runs[:15]:
        lines.append("   " + r)
open(r"C:\Users\woo31\openbgi\scripts\korean_decoded.txt","w",encoding="utf-8").write("\n".join(lines))
print("wrote")
