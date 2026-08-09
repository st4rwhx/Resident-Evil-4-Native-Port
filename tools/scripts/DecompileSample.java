// Decompiles a handful of named functions to pseudo-C and writes them to a
// report file, as a sanity check that symbol import + analysis produced
// something real and usable.
//
// Usage (headless): -postScript DecompileSample.java <out-file> <funcName1> [funcName2 ...]

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

import java.io.FileWriter;
import java.io.PrintWriter;

public class DecompileSample extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: DecompileSample.java <out-file> <funcName1> [funcName2 ...]");
            return;
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        FunctionManager fm = currentProgram.getFunctionManager();

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

                out.println("Function: " + target.getName() + "  @ " + target.getEntryPoint());
                out.println("----------------------------------------");
                DecompileResults res = decompiler.decompileFunction(target, 30, monitor);
                if (res != null && res.decompileCompleted()) {
                    out.println(res.getDecompiledFunction().getC());
                } else {
                    out.println("DECOMPILE FAILED: " + (res != null ? res.getErrorMessage() : "null result"));
                }
                out.println();
            }
        }

        decompiler.dispose();
        println("Wrote report to " + args[0]);
    }
}
