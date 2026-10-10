import struct
p = r"C:\Users\woo31\openbgi\EVE_engine.exe"
d = open(p,'rb').read()
pe = struct.unpack_from('<I', d, 0x3C)[0]
nsec = struct.unpack_from('<H', d, pe+6)[0]
optsz = struct.unpack_from('<H', d, pe+0x14)[0]
imgbase = struct.unpack_from('<I', d, pe+0x34)[0]
sh = pe+0x18+optsz
secs=[]
for i in range(nsec):
    o=sh+i*40; name=d[o:o+8].rstrip(b'\0').decode('latin1'); vs,va,rs,raw=struct.unpack_from('<IIII',d,o+8); secs.append((name,va,vs,raw,rs))
tvd=[s for s in secs if s[0]=='.text'][0]; rva_lo=tvd[1]; rva_hi=tvd[1]+tvd[2]
print("text RVA", hex(rva_lo), hex(rva_hi))
def off2va(off):
    for name,sv,vs,raw,rs in secs:
        if raw<=off<raw+rs: return imgbase+sv+(off-raw)
    return None
# scan dword runs whose values are RVAs in .text
runs=[]; i=0; N=len(d); rs=None; rl=0
while i+4<=N:
    v=struct.unpack_from('<I',d,i)[0]
    if rva_lo<=v<rva_hi:
        if rs is None: rs=i
        rl+=1
    else:
        if rl>=8: runs.append((rs,rl))
        rs=None; rl=0
    i+=4
if rl>=8: runs.append((rs,rl))
runs.sort(key=lambda x:-x[1])
print("runs of RVA-in-text dwords (>=8):", len(runs))
for off,ln in runs[:15]:
    print(f"  off=0x{off:X} va={hex(off2va(off))} len={ln}")
    vals=[struct.unpack_from('<I',d,off+k*4)[0] for k in range(min(ln,12))]
    print("     ", ' '.join(hex(v) for v in vals))
