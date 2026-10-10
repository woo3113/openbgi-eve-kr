# find_dispatch.py - locate the $80%02X / $81%02X "undefined instruction"
# message strings and the functions that reference them (the family
# dispatchers), then decompile those functions.
#@category OpenBGI
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.app.decompiler import DecompInterface

def find_str(s):
    mon = ConsoleTaskMonitor()
    a = currentProgram.getMinAddress()
    return currentProgram.getMemory().findBytes(a, s, None, True, mon)

def decompile(func):
    try:
        d = DecompInterface()
        d.openProgram(currentProgram)
        res = d.decompileFunction(func, 120, ConsoleTaskMonitor())
        if res and res.decompileCompleted():
            return res.getDecompiledFunction().getC()
    except Exception as e:
        return "decompile error: %s" % e
    return None

targets = []
for s in ["$81%02X", "$80%02X"]:
    addr = find_str(s)
    print("=== string %r -> %s ===" % (s, addr))
    if addr is None:
        continue
    refs = getReferencesTo(addr)
    print("   xrefs: %d" % len(refs))
    for r in refs:
        frm = r.getFromAddress()
        f = getFunctionContaining(frm)
        print("   from %s  func %s" % (frm, f))
        if f is not None:
            targets.append(f)

seen = set()
for f in targets:
    e = f.getEntryPoint()
    if e in seen:
        continue
    seen.add(e)
    print("\n\n########## FUNCTION %s @ %s ##########" % (f.getName(), e))
    c = decompile(f)
    if c:
        print(c)
