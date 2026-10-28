// Find a candidate IntrLine3Box3::Test body: a medium function with the
// separating-axis pattern (many dot products, Cross, several fabs).
// @category FA
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class ScanIntr extends GhidraScript {
	@Override
	public void run() throws Exception {
		Listing lst = currentProgram.getListing();
		for (Function f : lst.getFunctions(true)) {
			if (f.isExternal() || f.isThunk()) continue;
			int n = (int) f.getBody().getNumAddresses();
			if (n < 100 || n > 700) continue;
			int mul = 0, add = 0, sub = 0, andps = 0, fabsCalls = 0, calls = 0, fmul = 0, fadd = 0, fabs = 0;
			InstructionIterator ii = lst.getInstructions(f.getBody(), true);
			while (ii.hasNext()) {
				Instruction in = ii.next();
				String m = in.getMnemonicString();
				if (m.equals("MULSS")) mul++;
				else if (m.equals("ADDSS")) add++;
				else if (m.equals("SUBSS")) sub++;
				else if (m.equals("ANDPS")) andps++;
				else if (m.equals("FMUL") || m.equals("FMULP")) fmul++;
				else if (m.equals("FADD") || m.equals("FADDP")) fadd++;
				else if (m.equals("FABS")) fabs++;
				else if (m.equals("CALL")) {
					calls++;
					for (Object o : in.getOpObjects(0)) {
						if (o instanceof ghidra.program.model.address.Address) {
							long v = ((ghidra.program.model.address.Address) o).getOffset();
							if (v == 0x0040DAB0L) fabsCalls++;
						}
					}
				}
			}
			if ((andps >= 5 && mul >= 6) || (fmul >= 6 && fadd >= 4 && fabs >= 3)) {
				println("I " + f.getEntryPoint() + " sz" + n + " mul=" + mul + " add=" + add
					+ " sub=" + sub + " andps=" + andps + " fabs=" + fabs + "/" + fabsCalls + " fmul=" + fmul + " fadd=" + fadd + " calls=" + calls);
			}
		}
		println("DONE");
	}
}
