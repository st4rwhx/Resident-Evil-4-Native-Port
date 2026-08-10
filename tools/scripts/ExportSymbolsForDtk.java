// Exports every function Ghidra knows about (from our own successful
// auto-analysis + imported SYM names) in decomp-toolkit's symbols.txt
// format, to seed dtk's own analysis and avoid the overlapping-function
// errors its from-scratch analyzer hits on raw/unusual code (interrupt
// vectors, hand-tuned asm, etc.).
//
// Format: name = .text:0xADDRESS; // type:function size:0xSIZE scope:global align:4
//
// Usage (headless): -postScript ExportSymbolsForDtk.java <out-file>

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class ExportSymbolsForDtk extends GhidraScript {
    private static final class Entry {
        long addr;
        long size;
        String name;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: ExportSymbolsForDtk.java <out-file>");
            return;
        }

        FunctionManager fm = currentProgram.getFunctionManager();
        List<Entry> entries = new ArrayList<>();

        for (Function f : fm.getFunctions(true)) {
            long addr = f.getEntryPoint().getOffset();
            long size = f.getBody().getNumAddresses();
            if (size <= 0) continue;
            String name = f.getName();
            String safeName = name.replaceAll("[^A-Za-z0-9_:<>~,.\\-]", "_");
            if (safeName.isEmpty()) continue;
            Entry e = new Entry();
            e.addr = addr;
            e.size = size;
            e.name = safeName;
            entries.add(e);
        }

        // Sort by address, then clamp each entry's size so it never extends
        // past the start of the next entry. Ghidra's own function "body" can
        // occasionally overlap a neighbor (disjoint address sets from switch
        // jump tables, shared tails, etc.) - clamping keeps the exported
        // symbol table strictly non-overlapping, which dtk requires.
        entries.sort(Comparator.comparingLong(e -> e.addr));
        int clamped = 0;
        for (int i = 0; i < entries.size() - 1; i++) {
            Entry cur = entries.get(i);
            Entry next = entries.get(i + 1);
            long maxSize = next.addr - cur.addr;
            if (maxSize > 0 && cur.size > maxSize) {
                cur.size = maxSize;
                clamped++;
            }
        }
        // PowerPC instructions are always 4 bytes - any function whose size
        // isn't a multiple of 4 is a Ghidra analysis artifact (e.g. a
        // "noreturn" stub where flow analysis stopped early). Round up to
        // the next 4-byte boundary, but never past the next symbol.
        int rounded = 0;
        for (int i = 0; i < entries.size(); i++) {
            Entry cur = entries.get(i);
            if (cur.size % 4 != 0) {
                long newSize = (cur.size + 3) & ~3L;
                if (i + 1 < entries.size()) {
                    long maxSize = entries.get(i + 1).addr - cur.addr;
                    if (newSize > maxSize) newSize = maxSize;
                }
                if (newSize > cur.size) {
                    cur.size = newSize;
                    rounded++;
                }
            }
        }

        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (Entry e : entries) {
                out.printf("%s = .text:0x%08X; // type:function size:0x%X scope:global align:4%n",
                    e.name, e.addr, e.size);
            }
        }

        println("Exported " + entries.size() + " function symbols (" + clamped + " clamped, " + rounded + " rounded to 4-byte) to " + args[0]);
    }
}
