import struct
p = r"C:\Users\woo31\Downloads\EVE burst error A\BGI.gdb"
d = open(p,'rb').read()
key = struct.unpack_from('<H', d, 0x10)[0]
encLen = struct.unpack_from('<I', d, 0x14)[0]
outSize = struct.unpack_from('<I', d, 0x18)[0]
payload = d[0x20:0x20+encLen]
def mkhash(k):
    while True:
        lo = 20021*(k & 0xffff)
        hi = (20021*(k >> 16) + 346*k) & 0xffffffff
        hi = (hi + (lo >> 16)) & 0xffff
        k = ((hi << 16) + (lo & 0xffff) + 1) & 0xffffffff
        yield hi & 0x7fff
h = mkhash(key)
lz = bytes(((v - (next(h) & 0xff)) & 0xff) for v in payload)
out = bytearray(); i=0; n=len(lz)
while i<n:
    b=lz[i]
    if b & 0x80:
        length=((b>>3)&0xf)+2; dist=(((b&7)<<8)|lz[i+1])+2
        for _ in range(length): out.append(out[len(out)-dist])
        i+=2
    else:
        cnt=b+1; out+=lz[i+1:i+1+cnt]; i+=1+cnt
out=bytes(out)
print("decoded", len(out))
def hx(off, ln=64):
    print(f"@{off:#x}: " + ' '.join(f'{b:02x}' for b in out[off:off+ln]))
# header region
hx(0, 32)
# candidate name/flag bases
for base in (0x40408, 0x60408, 0x70408, 0x100408, 0x120408, 0x130408, 0x130000, 0x110408):
    print(f"--- base {base:#x} ---")
    hx(base, 64)
# scan for where 0xFF runs start
run=0; start=-1; runs=[]
for j,b in enumerate(out):
    if b==0xFF:
        if run==0: start=j
        run+=1
    else:
        if run>=1024: runs.append((start,run))
        run=0
if run>=1024: runs.append((start,run))
print("0xFF runs >=1024:", [(hex(a),hex(l)) for a,l in runs][:10])
# find printable ASCII name-like region: search for a run of >=4 strings of len>=3 separated by 0
import re
print("sample ASCII strings near 0x120000:", re.findall(rb'[\x20-\x7e]{3,}', out[0x118000:0x121000])[:20])
print("sample ASCII strings near 0x100000:", re.findall(rb'[\x20-\x7e]{3,}', out[0x100000:0x101000])[:20])
