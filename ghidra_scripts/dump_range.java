// dump_range.java - disassemble a byte range of dinput8_kr.dll
//@category OpenBGI
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class dump_range extends GhidraScript {
    public void run() throws Exception {
        long start = 0x10001400L, end = 0x10001680L;
        Address a = toAddr(start);
        Address b = toAddr(end);
        disassemble(a, new ghidra.program.model.address.AddressSet(a, b));
        InstructionIterator it = currentProgram.getListing().getInstructions(a, true);
        while (it.hasNext()) {
            Instruction in = it.next();
            if (in.getAddress().getOffset() > end) break;
            println(in.getAddress() + "  " + in.toString());
        }
        // also dump the sjis table load area / data globals sizes
        println("\n-- data refs --");
        println("DAT_10031030 @ " + toAddr(0x10031030) + ", DAT_10033030 @ " + toAddr(0x10033030));
    }
}
