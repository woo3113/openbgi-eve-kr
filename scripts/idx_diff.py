import struct, io
root = r"C:\Users\woo31\Downloads\EVE burst error A"
ext = open(root + r"\sjis_ext.bin","rb").read()
chars = [struct.unpack_from('<H', ext, i)[0] for i in range(0, len(ext), 2)]
inv = {}
for i,c in enumerate(chars):
    inv.setdefault(chr(c), i)
actual  = "퉁트응좋대태을싄춘칠그뮈"
correct = "뭐야잠들어버렸던모양이군"
o=io.StringIO()
print("actual -> idx  | correct -> idx", file=o)
for a,c in zip(actual, correct):
    ia = inv.get(a, -1); ic = inv.get(c, -1)
    print(f"{a} {ia:5d} | {c} {ic:5d}   d={ic-ia}", file=o)
# also verify name
for s in ["코지로"]:
    print(s, [inv.get(ch,-1) for ch in s], file=o)
open(r"C:\Users\woo31\openbgi\scripts\idx_diff.txt","w",encoding="utf-8").write(o.getvalue())
print("wrote")
