import struct, re
root = r"C:\Users\woo31\Downloads\EVE burst error A"
d = open(root + r"\a001","rb").read()
# find long runs of bytes that look like SJIS text (lead 0x81-0x9F/0xE0-0xEF with valid trail), hiragana/kanji
def islead(b): return 0x81 <= b <= 0x9F or 0xE0 <= b <= 0xEF
i = 0; runs = []
cur = bytearray(); curstart = 0
while i < len(d):
    b = d[i]
    if islead(b) and i+1 < len(d):
        T = d[i+1]
        if (0x40 <= T <= 0x7E) or (0x80 <= T <= 0xFC):
            if not cur: curstart = i
            cur += bytes([b, T]); i += 2; continue
    if 0x20 <= b < 0x7F:
        if not cur: curstart = i
        cur += bytes([b]); i += 1; continue
    if len(cur) >= 20:
        runs.append((curstart, bytes(cur)))
    cur = bytearray(); i += 1
if len(cur) >= 20: runs.append((curstart, bytes(cur)))
runs.sort(key=lambda x: -len(x[1]))
print("a001 text runs found:", len(runs))
for off, by in runs[:6]:
    try:
        jp = by.decode('cp932')
    except Exception as e:
        jp = "?"
    print(f"@{off:#x} len={len(by)} cp932: {jp[:80]!r}")
