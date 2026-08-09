// Lists all real functions (not data symbols) under a given byte size,
// to find good next matching-decomp candidates: small, likely simple.
//
// Usage (headless): -postScript ListTinyFunctions.java <out-file> <maxSize>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class ListTinyFunctions extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("Usage: ListTinyFunctions.java <out-file> <maxSize>");
            return;
        }
        long maxSize = Long.parseLong(args[1]);

        FunctionManager fm = currentProgram.getFunctionManager();
        List<String> lines = new ArrayList<>();

        for (Function f : fm.getFunctions(true)) {
            long size = f.getBody().getNumAddresses();
            if (size > 0 && size <= maxSize) {
                lines.add(String.format("%08x  %6d  %s", f.getEntryPoint().getOffset(), size, f.getName()));
            }
        }

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (String l : lines) out.println(l);
        }

        println("Wrote " + lines.size() + " tiny functions to " + args[0]);
    }
}
