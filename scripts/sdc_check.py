import struct, sys
p = r"C:\Users\woo31\Downloads\EVE burst error A\BGI.gdb"
d = open(p,'rb').read()
assert d[:16] == b"SDC FORMAT 1.00\0"
key = struct.unpack_from('<H', d, 0x10)[0]
encLen = struct.unpack_from('<I', d, 0x14)[0]
outSize = struct.unpack_from('<I', d, 0x18)[0]
payload = d[0x20:0x20+encLen]
print("key",key,"encLen",encLen,"outSize",outSize,"payload",len(payload))

def mkhash(k):
    while True:
        lo = 20021*(k & 0xffff)
        hi = (20021*(k >> 16) + 346*k) & 0xffffffff
        hi = (hi + (lo >> 16)) & 0xffff
        k = ((hi << 16) + (lo & 0xffff) + 1) & 0xffffffff
        yield hi & 0x7fff

h = mkhash(key)
lz = bytes(((v - (next(h) & 0xff)) & 0xff) for v in payload)

out = bytearray()
i = 0
n = len(lz)
bad = 0
while i < n:
    b = lz[i]
    if b & 0x80:
        length = ((b >> 3) & 0xf) + 2
        dist = (((b & 7) << 8) | lz[i+1]) + 2
        if dist > len(out):
            bad += 1
            if bad < 5: print("BAD backref at", i, "dist", dist, "outpos", len(out))
        for _ in range(length):
            out.append(out[len(out)-dist])
        i += 2
    else:
        cnt = b + 1
        out += lz[i+1:i+1+cnt]
        i += 1 + cnt
print("decoded", len(out), "expected outSize", outSize, "match", len(out)==outSize, "badrefs", bad)
