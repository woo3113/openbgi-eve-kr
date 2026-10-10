import os, re, glob
dis = os.path.expandvars(r"%TEMP%\opencode\dis")
toks = ["sys81.0xd0","sys81.0xd1","sys81.0xd2","sys81.0xd3","sys81.0xd4","sys81.0xd5",
        "sys.0xcf","gfx0.0xce","gfx1.0x31","gfx1.0xdb","gfx2.0x9b","gfx2.0xf5","gfx2.0xf6",
        "snd.0x1c","ext0.0x06","ext1.0x06"]
pushre = re.compile(r'^\s*(push_i8|push_i16|push_i32|push_local_addr|push_local|push_global|push_global_addr|push_code_off|push_str|push_float|push_ext|push_sys)\b')
callre = re.compile(r'^\s*(call|.*\bcall)\b')
labelre = re.compile(r'^\s*(loc_[0-9a-f]+|sub_[0-9a-f]+):')

for f in sorted(glob.glob(os.path.join(dis,"*.s"))):
    lines = open(f, encoding='latin1').read().splitlines()
    for i,l in enumerate(lines):
        tok = l.strip()
        if tok in toks:
            # walk back
            j=i-1; operands=0; pushes=[]
            while j>=0:
                s=lines[j].strip()
                if labelre.match(s): break
                if pushre.match(s):
                    operands+=1; pushes.insert(0,s); j-=1; continue
                if re.match(r'^\s*\w[\w\.]*\s*$', s) and ('call' in s or s.startswith('sys.') or s.startswith('gfx')):
                    operands+=1; pushes.insert(0,'('+s+')'); j-=1; continue
                break
            print(f"{os.path.basename(f):22s} {tok:12s} operands={operands}  :: {' ; '.join(pushes)}")
