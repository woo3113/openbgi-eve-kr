import struct, re, sys
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
print("sjis_ext chars:", len(chars), "first:", ''.join(chr(c) for c in chars[:30]))
print("last:", ''.join(chr(c) for c in chars[-10:]))

a = open(root + r"\a000","rb").read()
print("a000 len", len(a))
# header
print("magic:", a[:27])
# find runs that decode to hiragana/katakana/kanji or that use high bytes
# Print all maximal runs of bytes that are printable cp932 (including 2-byte)
s = a.decode('cp932', errors='replace')
runs = re.findall(r'[\u3000-\u9fff\uff00-\uffef\uac00-\ud7a3A-Za-z0-9!?.,:;()\[\]<>/\\\'"~=+\-*%&$#@ \u2015\u2026\u2018\u2019\u201c\u201d]{3,}', s)
print("candidate text runs:", len(runs))
for r in runs[:60]:
    print(repr(r))
