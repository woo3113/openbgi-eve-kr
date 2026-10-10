import struct
p = r"C:\Users\woo31\Downloads\EVE burst error A\EVE_BLACK.ttf"
d = open(p,'rb').read()
print("size", len(d))
ver, ntables = struct.unpack_from('>IH', d, 0)
print("sfnt", hex(ver), "tables", ntables)
tables = {}
for i in range(ntables):
    off = 12 + i*16
    tag = d[off:off+4].decode('latin1')
    cks, o, l = struct.unpack_from('>III', d, off+4)
    tables[tag] = (o, l)
print("tables:", ', '.join(sorted(tables)))
co, cl = tables.get('cmap', (0,0))
n = struct.unpack_from('>H', d, co+2)[0]
subs = []
for i in range(n):
    pid, eid, so = struct.unpack_from('>HHI', d, co+4+i*8)
    subs.append((pid, eid, so))
print("cmap subtables:", subs)
# pick a format 4 subtable
def parse_fmt4(base):
    fmt = struct.unpack_from('>H', d, base)[0]
    if fmt != 4: return None
    length, lang, segX2 = struct.unpack_from('>HHH', d, base+2)
    seg = segX2//2
    endo = base+14
    end = struct.unpack_from('>%dH'%seg, d, endo)
    starto = endo + segX2 + 2
    start = struct.unpack_from('>%dH'%seg, d, starto)
    ido = starto + segX2
    idelta = struct.unpack_from('>%dh'%seg, d, ido)
    idro = ido + segX2
    idrange = struct.unpack_from('>%dH'%seg, d, idro)
    cov = set()
    for i in range(seg):
        for c in range(start[i], end[i]+1):
            if c == 0xFFFF: continue
            if idrange[i] == 0:
                g = (c + idelta[i]) & 0xFFFF
            else:
                gi = idro + i*2 + idrange[i] + (c-start[i])*2
                g = struct.unpack_from('>H', d, gi)[0]
                if g: g = (g + idelta[i]) & 0xFFFF
            if g: cov.add(c)
    return cov
cov = None
for pid,eid,so in subs:
    c = parse_fmt4(co+so)
    if c: cov = c; break
print("codepoints covered:", len(cov) if cov else None)
if cov:
    for cp in (0x5B9A, 0x82B3, 0x82EA, 0x3042, 0xAC00, 0xC544):
        print(hex(cp), cp in cov)
    hangul = [c for c in cov if 0xAC00 <= c <= 0xD7A3]
    print("hangul codepoints in font:", len(hangul))
    kana = [c for c in cov if 0x3040 <= c <= 0x30FF]
    print("kana in font:", len(kana))
    kanji = [c for c in cov if 0x4E00 <= c <= 0x9FFF]
    print("kanji in font:", len(kanji))
