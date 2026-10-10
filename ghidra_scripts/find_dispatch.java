// find_dispatch.java - locate the $80%02X / $81%02X "undefined instruction"
// message strings, the functions referencing them (the family dispatchers),
// and decompile those functions.
//@category OpenBGI
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashSet;

public class find_dispatch extends GhidraScript {
    private String decompile(Function f) {
        try {
            DecompInterface d = new DecompInterface();
            d.openProgram(currentProgram);
            DecompileResults res = d.decompileFunction(f, 180, monitor);
            if (res != null && res.decompileCompleted())
                return res.getDecompiledFunction().getC();
            return "  (decompile incomplete)";
        } catch (Exception e) {
            return "  (decompile error: " + e + ")";
        }
    }

    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        String[] needles = { "$81%02X", "$80%02X", "BGIError.txt", "BURIKO GDB 3.00", "BGI.gdb", "BGI.hvl" };
        LinkedHashSet<Address> funcs = new LinkedHashSet<Address>();
        for (String s : needles) {
            Address a = mem.findBytes(currentProgram.getMinAddress(), s.getBytes(StandardCharsets.ISO_8859_1), null, true, monitor);
            println("=== string \"" + s + "\" -> " + a + " ===");
            if (a == null) continue;
            Reference[] refs = getReferencesTo(a);
            println("    xrefs: " + refs.length);
            for (Reference r : refs) {
                Address frm = r.getFromAddress();
                Function f = getFunctionContaining(frm);
                println("    from " + frm + "  func " + (f != null ? f.getName() + " @" + f.getEntryPoint() : "?"));
                if (f != null) funcs.add(f.getEntryPoint());
            }
        }
        for (Address e : funcs) {
            Function f = getFunctionAt(e);
            println("\n\n########## FUNCTION " + f.getName() + " @ " + e + " ##########");
            println(decompile(f));
        }
    }
}
