// Imports re4sym-generated "Name 0xADDRESS" symbol lists (Bio4.*.ghidra.txt)
// into the current program: creates a label at each address, and where the
// address falls inside an executable memory block, also disassembles and
// creates a function there.
//
// Usage (headless): -postScript ImportRE4Symbols.java <path-to-ghidra.txt>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ImportRE4Symbols extends GhidraScript {

    private static final Pattern LINE = Pattern.compile("^(.+?)\\s+0x([0-9A-Fa-f]+)\\s*$");

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: ImportRE4Symbols.java <path-to-ghidra.txt>");
            return;
        }

        String path = args[0];
        SymbolTable symbolTable = currentProgram.getSymbolTable();

        int labeled = 0;
        int functions = 0;
        int skipped = 0;
        int lineNum = 0;

        try (BufferedReader reader = new BufferedReader(new FileReader(path))) {
            String line;
            while ((line = reader.readLine()) != null) {
                lineNum++;
                if (monitor.isCancelled()) break;
                line = line.trim();
                if (line.isEmpty()) continue;

                Matcher m = LINE.matcher(line);
                if (!m.matches()) {
                    skipped++;
                    continue;
                }

                String name = m.group(1).trim();
                long addrLong;
                try {
                    addrLong = Long.parseUnsignedLong(m.group(2), 16);
                } catch (NumberFormatException e) {
                    skipped++;
                    continue;
                }

                Address addr = toAddr(addrLong);
                if (addr == null) {
                    skipped++;
                    continue;
                }

                // Ghidra symbol names can't contain some characters CodeWarrior
                // mangling produces (e.g. spaces from operator names); sanitize.
                String safeName = name.replaceAll("[^A-Za-z0-9_:<>~,.\\-]", "_");
                if (safeName.isEmpty()) {
                    skipped++;
                    continue;
                }

                try {
                    symbolTable.createLabel(addr, safeName, SourceType.IMPORTED);
                    labeled++;
                } catch (Exception e) {
                    skipped++;
                    continue;
                }

                MemoryBlock block = currentProgram.getMemory().getBlock(addr);
                if (block != null && block.isExecute()) {
                    Function existing = getFunctionAt(addr);
                    if (existing == null) {
                        try {
                            disassemble(addr);
                            Function f = createFunction(addr, safeName);
                            if (f != null) functions++;
                        } catch (Exception e) {
                            // best-effort; leave as a label if function creation fails
                        }
                    }
                }

                if (lineNum % 1000 == 0) {
                    println("... " + lineNum + " lines processed (" + labeled + " labeled, " + functions + " functions)");
                }
            }
        }

        println("Done: " + labeled + " labels, " + functions + " functions, " + skipped + " skipped, from " + lineNum + " lines.");
    }
}
