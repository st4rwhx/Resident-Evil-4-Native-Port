// Scans the whole program for the "or. rX,rX,rX" self-OR zero-test idiom
// (used to check how common/rare it is, to distinguish "this is a general
// compiler-flag/version characteristic of this build" from "this one
// function is unusual").
//
// Usage (headless): -postScript ScanSelfOrIdiom.java <out-file> [maxResults]

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.scalar.Scalar;

import java.io.FileWriter;
import java.io.PrintWriter;

public class ScanSelfOrIdiom extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: ScanSelfOrIdiom.java <out-file> [maxResults]");
            return;
        }
        int maxResults = args.length > 1 ? Integer.parseInt(args[1]) : 500;

        Listing listing = currentProgram.getListing();
        InstructionIterator it = listing.getInstructions(true);

        int total = 0;
        int found = 0;

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            while (it.hasNext() && found < maxResults) {
                Instruction instr = it.next();
                total++;
                String mnem = instr.getMnemonicString();
                if (!mnem.equals("or.")) continue;

                if (instr.getNumOperands() < 2) continue;
                String op0 = instr.getDefaultOperandRepresentation(0);
                String op1 = instr.getDefaultOperandRepresentation(1);
                String op2 = instr.getNumOperands() > 2 ? instr.getDefaultOperandRepresentation(2) : null;
                boolean selfOr = op0.equals(op1) && (op2 == null || op0.equals(op2));
                if (!selfOr) continue;

                found++;
                Address addr = instr.getAddress();
                Function func = getFunctionContaining(addr);
                String funcName = func != null ? func.getName() : "???";
                long offsetInFunc = func != null ? addr.subtract(func.getEntryPoint()) : -1;

                Instruction next = instr.getNext();
                String nextStr = next != null ? next.toString() : "?";

                out.printf("%s  in %s+0x%x  (next: %s)%n", addr, funcName, offsetInFunc, nextStr);
            }
        }

        println("Scanned " + total + " instructions, found " + found + " self-OR occurrences (capped at " + maxResults + ").");
    }
}
