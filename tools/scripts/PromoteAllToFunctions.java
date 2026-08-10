// For every symbol in the original re4sym-derived list that (a) falls in
// an executable memory block and (b) doesn't already have a Function
// there, attempts disassemble+createFunction. This fills the gap between
// "10,383 known symbol names" and "7,471 Ghidra Function objects" -
// most of that gap is real functions that just weren't auto-promoted the
// first time (ImportRE4Symbols only force-created functions where nothing
// existed yet at all); some of it is genuinely data and will just fail,
// which is fine - we skip and count those.
//
// Usage (headless): -postScript PromoteAllToFunctions.java <ghidra-txt-path>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class PromoteAllToFunctions extends GhidraScript {

    private static final Pattern LINE = Pattern.compile("^(.+?)\\s+0x([0-9A-Fa-f]+)\\s*$");

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: PromoteAllToFunctions.java <ghidra-txt-path>");
            return;
        }

        int promoted = 0, alreadyFn = 0, notExec = 0, failed = 0, total = 0;

        try (BufferedReader reader = new BufferedReader(new FileReader(args[0]))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (monitor.isCancelled()) break;
                line = line.trim();
                if (line.isEmpty()) continue;
                Matcher m = LINE.matcher(line);
                if (!m.matches()) continue;
                total++;

                long addrLong;
                try {
                    addrLong = Long.parseUnsignedLong(m.group(2), 16);
                } catch (NumberFormatException e) {
                    continue;
                }
                Address addr = toAddr(addrLong);
                if (addr == null) continue;

                if (getFunctionAt(addr) != null) {
                    alreadyFn++;
                    continue;
                }

                MemoryBlock block = currentProgram.getMemory().getBlock(addr);
                if (block == null || !block.isExecute()) {
                    notExec++;
                    continue;
                }

                try {
                    disassemble(addr);
                    Function f = createFunction(addr, null);
                    if (f != null) {
                        promoted++;
                    } else {
                        failed++;
                    }
                } catch (Exception e) {
                    failed++;
                }

                if (total % 2000 == 0) {
                    println("... " + total + " processed (promoted=" + promoted + " alreadyFn=" + alreadyFn
                        + " notExec=" + notExec + " failed=" + failed + ")");
                }
            }
        }

        println("Done: total=" + total + " promoted=" + promoted + " alreadyFn=" + alreadyFn
            + " notExec=" + notExec + " failed=" + failed);
    }
}
