// dump_hook.java - find the TextOutW / TextOutA hook in dinput8.dll and decompile it
//@category OpenBGI
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import java.util.LinkedHashSet;

public class dump_hook extends GhidraScript {
    private String decompile(Function f) {
        try {
            DecompInterface d = new DecompInterface();
            d.openProgram(currentProgram);
            DecompileResults res = d.decompileFunction(f, 180, monitor);
            if (res != null && res.decompileCompleted())
                return res.getDecompiledFunction().getC();
            return "(incomplete)";
        } catch (Exception e) { return "(err " + e + ")"; }
    }
    public void run() throws Exception {
        String[] names = { "TextOutW", "TextOutA", "CreateFontW", "CreateFontA", "AddFontResourceExW", "GetTextExtentPoint32W" };
        LinkedHashSet<Address> funcs = new LinkedHashSet<Address>();
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        while (it.hasNext()) {
            Symbol s = it.next();
            String n = s.getName();
            for (String want : names) {
                if (n.equals(want)) {
                    Reference[] refs = getReferencesTo(s.getAddress());
                    println("symbol " + n + " @" + s.getAddress() + " refs=" + refs.length);
                    for (Reference r : refs) {
                        Function f = getFunctionContaining(r.getFromAddress());
                        if (f != null) { println("   from " + r.getFromAddress() + " in " + f.getName() + "@" + f.getEntryPoint()); funcs.add(f.getEntryPoint()); }
                    }
                }
            }
        }
        for (Address e : funcs) {
            Function f = getFunctionAt(e);
            println("\n########## " + f.getName() + " @ " + e + " ##########");
            println(decompile(f));
        }
    }
}
