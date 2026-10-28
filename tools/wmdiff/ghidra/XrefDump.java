// Print callers (calling function + instruction) for each target address.
// @category FA
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class XrefDump extends GhidraScript {
	@Override
	public void run() throws Exception {
		Listing lst = currentProgram.getListing();
		for (String h : getScriptArgs()) {
			Address a = toAddr(h);
			Function tf = getFunctionAt(a);
			println("=== TARGET " + h + "  " + (tf == null ? "no func" : tf.getName()));
			for (Reference r : getReferencesTo(a)) {
				Address from = r.getFromAddress();
				Function cf = getFunctionContaining(from);
				Instruction ins = lst.getInstructionAt(from);
				println("  ref from " + from + " in " + (cf == null ? "?" : cf.getName() + "@" + cf.getEntryPoint()
						+ " sz" + cf.getBody().getNumAddresses()) + "   " + (ins == null ? "" : ins.toString()));
			}
		}
	}
}
