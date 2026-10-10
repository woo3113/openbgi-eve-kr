import struct
p = r"C:\Users\woo31\openbgi\EVE_engine.exe"
d = open(p,'rb').read()
pe = struct.unpack_from('<I', d, 0x3C)[0]
nsec = struct.unpack_from('<H', d, pe+6)[0]
optsz = struct.unpack_from('<H', d, pe+0x14)[0]
imgbase = struct.unpack_from('<I', d, pe+0x34)[0]
sh = pe+0x18+optsz
secs = []
for i in range(nsec):
    o = sh+i*40
    name = d[o:o+8].rstrip(b'\0').decode('latin1')
    vs,va,rs,raw = struct.unpack_from('<IIII', d, o+8)
    secs.append((name, va, vs, raw, rs))
print("imagebase", hex(imgbase))
for s in secs: print(" ", s)

# text VA range
tvd = [s for s in secs if s[0]=='.text'][0]
text_lo = imgbase + tvd[1]
text_hi = imgbase + tvd[1] + tvd[2]
print("text range", hex(text_lo), hex(text_hi))

def va2off(va):
    rva = va - imgbase
    for name, sv, vs, raw, rs in secs:
        if sv <= rva < sv+max(vs,rs):
            return raw + (rva - sv)
    return None

# scan every 4-aligned offset for a run of pointers into .text
best = []
i = 0
N = len(d)
run_start = None
run_len = 0
while i+4 <= N:
    v = struct.unpack_from('<I', d, i)[0]
    if text_lo <= v < text_hi:
        if run_start is None:
            run_start = i
        run_len += 1
    else:
        if run_len >= 64:
            best.append((run_start, run_len))
        run_start = None; run_len = 0
    i += 4
if run_len >= 64: best.append((run_start, run_len))
best.sort(key=lambda x:-x[1])
print("\ncandidate pointer runs (>=64 dwords into .text):")
for off,ln in best[:20]:
    va = None
    for name, sv, vs, raw, rs in secs:
        if raw <= off < raw+rs:
            va = imgbase+sv+(off-raw)
    print(f"  fileoff=0x{off:X} va={hex(va) if va else '?'} len={ln} dwords")

# For the largest, dump first entries as function targets
if best:
    off,ln = best[0]
    print("\nfirst 32 entries of the largest run:")
    for k in range(32):
        v = struct.unpack_from('<I', d, off+k*4)[0]
        print(f"   [{k:02X}] {hex(v)}")
