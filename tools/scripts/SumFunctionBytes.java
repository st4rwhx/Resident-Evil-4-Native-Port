// Sums the total byte size of every function's body in the program, for
// an honest "matched bytes / total code bytes" progress metric (function
// COUNT alone is misleading - one 1000-byte function outweighs 80 tiny
// ones).
//
// Usage (headless): -postScript SumFunctionBytes.java <out-file>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

import java.io.FileWriter;
import java.io.PrintWriter;

public class SumFunctionBytes extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        FunctionManager fm = currentProgram.getFunctionManager();

        long totalBytes = 0;
        long count = 0;
        for (Function f : fm.getFunctions(true)) {
            totalBytes += f.getBody().getNumAddresses();
            count++;
        }

        String result = "Functions: " + count + "  Total function body bytes: " + totalBytes;
        println(result);
        if (args.length > 0) {
            try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
                out.println(result);
            }
        }
    }
}
