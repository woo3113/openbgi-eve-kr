// dump_addr.java - decompile functions at given entry addresses in dinput8_kr.dll
//@category OpenBGI
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;

public class dump_addr extends GhidraScript {
    private String dec(Address e) {
        Function f = getFunctionAt(e);
        if (f == null) { f = getFunctionContaining(e); }
        if (f == null) return "(no function at " + e + ")";
        try {
            DecompInterface d = new DecompInterface();
            d.openProgram(currentProgram);
            DecompileResults res = d.decompileFunction(f, 180, monitor);
            if (res != null && res.decompileCompleted())
                return "// " + f.getName() + " @ " + e + "\n" + res.getDecompiledFunction().getC();
        } catch (Exception ex) { return "(err " + ex + ")"; }
        return "(incomplete)";
    }
    public void run() throws Exception {
        String[] addrs = { "10001470", "100014b0", "10001660", "100015f0", "10001270", "10001410" };
        for (String a : addrs) {
            Address ad = toAddr(a);
            println("\n\n################################ " + a + " ################################");
            println(dec(ad));
        }
    }
}
