// Dump one function's instructions (Ghidra 12.1 safe API).
// @category FA
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;

public class DumpFunc extends GhidraScript {

	private String[] targets = new String[0];
	private int maxIns = 600;

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length > 0) {
			targets = args;
		}
		for (String t : targets) {
			dump(t);
		}
	}

	private void dump(String hex) throws Exception {
		Address a = toAddr(hex);
		Function f = getFunctionAt(a);
		println("");
		println("================================================================");
		if (f == null) {
			// maybe it is the middle of a function
			f = getFunctionContaining(a);
			println(hex + "  no function starts here; containing=" + (f == null ? "none"
				: f.getName() + "@" + f.getEntryPoint()));
			if (f == null) {
				return;
			}
		}
		println(String.format("%s  %s   entry=%s  params=%d", hex, f.getName(), f.getEntryPoint(),
			f.getParameterCount()));
		AddressSetView body = f.getBody();
		Listing lst = currentProgram.getListing();
		Address cur = body.getMinAddress();
		Address max = body.getMaxAddress();
		int n = 0;
		while (cur != null && cur.compareTo(max) <= 0 && n < maxIns) {
			Instruction ins = lst.getInstructionAt(cur);
			if (ins == null) {
				cur = cur.next();
				continue;
			}
			println(String.format("  %s  %-40s%s", ins.getAddress(), ins.toString(), extra(ins)));
			cur = ins.getMaxAddress().next();
			n++;
		}
	}

	private String extra(Instruction ins) {
		StringBuilder sb = new StringBuilder();
		try {
			for (int k = 0; k < ins.getNumOperands(); k++) {
				for (Object o : ins.getOpObjects(k)) {
					if (o instanceof Address) {
						String v = readAsFloat((Address) o);
						if (v != null) {
							sb.append("  ; ").append(v);
						}
					}
				}
			}
		}
		catch (Exception e) {
			sb.append("  ; err ").append(e.getMessage());
		}
		return sb.toString();
	}

	private String readAsFloat(Address addr) {
		try {
			Memory mem = currentProgram.getMemory();
			if (!mem.contains(addr)) {
				return null;
			}
			byte[] b8 = new byte[8];
			if (mem.getBytes(addr, b8, 0, 8) != 8) {
				return null;
			}
			long bits = 0;
			for (int i = 0; i < 8; i++) {
				bits |= ((long) (b8[i] & 0xFF)) << (8 * i);
			}
			double d = Double.longBitsToDouble(bits);
			if (Double.isNaN(d) || Double.isInfinite(d) || Math.abs(d) > 1e5) {
				return null;
			}
			int f4 = ((b8[3] & 0xFF) << 24) | ((b8[2] & 0xFF) << 16) | ((b8[1] & 0xFF) << 8)
				| (b8[0] & 0xFF);
			float ff = Float.intBitsToFloat(f4);
			boolean ffOk = !Float.isNaN(ff) && !Float.isInfinite(ff) && Math.abs(ff) < 1e5f;
			if (d == 0.0 && ff == 0.0f) {
				return "ZERO";
			}
			if (ffOk && d != 0.0 && ff != 0.0f) {
				return String.format("f64=%.10g | f32=%.9g", d, ff);
			}
			if (ffOk) {
				return String.format("f32=%.9g", ff);
			}
			return String.format("f64=%.10g", d);
		}
		catch (Exception e) {
			return null;
		}
	}
}
