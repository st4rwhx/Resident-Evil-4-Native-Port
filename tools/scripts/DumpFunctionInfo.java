// Dumps disassembly listing + raw hex bytes for one or more named functions,
// as the ground-truth reference for a matching-decomp attempt.
//
// Usage (headless): -postScript DumpFunctionInfo.java <out-file> <funcName1> [funcName2 ...]

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;

import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpFunctionInfo extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: DumpFunctionInfo.java <out-file> <funcName1> [funcName2 ...]");
            return;
        }

        FunctionManager fm = currentProgram.getFunctionManager();
        Listing listing = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (int i = 1; i < args.length; i++) {
                String wanted = args[i];
                Function target = null;
                for (Function f : fm.getFunctions(true)) {
                    if (f.getName().equals(wanted)) {
                        target = f;
                        break;
                    }
                }

                out.println("========================================");
                if (target == null) {
                    out.println("NOT FOUND: " + wanted);
                    out.println();
                    continue;
                }

                Address start = target.getEntryPoint();
                Address end = target.getBody().getMaxAddress();
                long size = end.subtract(start) + 1;

                out.println("Function: " + target.getName());
                out.println("Entry: " + start + "  End: " + end + "  Size: " + size + " bytes (0x" + Long.toHexString(size) + ")");
                out.println("Signature: " + target.getSignature());
                out.println("----------------------------------------");

                out.println("-- Disassembly --");
                for (Instruction instr : listing.getInstructions(target.getBody(), true)) {
                    byte[] b = instr.getBytes();
                    StringBuilder hex = new StringBuilder();
                    for (byte bb : b) hex.append(String.format("%02x", bb));
                    out.printf("%s  %-10s  %s%n", instr.getAddress(), hex.toString(), instr.toString());
                }

                out.println();
                out.println("-- Raw bytes --");
                byte[] all = new byte[(int) size];
                mem.getBytes(start, all);
                StringBuilder hexAll = new StringBuilder();
                for (int j = 0; j < all.length; j++) {
                    hexAll.append(String.format("%02x", all[j]));
                    if ((j + 1) % 16 == 0) hexAll.append('\n'); else hexAll.append(' ');
                }
                out.println(hexAll.toString());
                out.println();
            }
        }

        println("Wrote function info to " + args[0]);
    }
}
