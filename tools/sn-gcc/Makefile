# Native Linux build of SN Systems' GCC 2.95.3 (SN BUILD v1.79) cc1 / cc1plus
# for the Nintendo GameCube (rs6000 sysv4 config).  Host is i386 via -m32 so
# HOST_WIDE_INT is 32 bits, as in the MSVC/Win32 build SN shipped.
#
# Invoked by build.sh; can be run by hand after build.sh has populated src/.

# No builtin rules: never let make "rebuild" the pregenerated bison parsers.
MAKEFLAGS += -r
.SUFFIXES:

SRC   := src/gcc
INC   := src/include
OBJ   ?= obj
BIN   ?= .
OPT   ?= -O2
MD    := $(SRC)/config/rs6000/rs6000.md

CC      := gcc
# -DIN_GCC -DHAIFA -DCROSS_COMPILE -DMULTIBYTE_CHARS : exactly what SN's
# dolphin.bat passes to cl.exe (SNPSX is unused by the sources).
DEFS    := -DIN_GCC -DHAIFA -DCROSS_COMPILE -DMULTIBYTE_CHARS
INCS    := -I$(OBJ) -I$(SRC) -I$(SRC)/config -I$(INC)
# gnu89 semantics for 1999-era C; -fpermissive turns GCC>=14 hard errors for
# K&R-isms back into warnings; -w silences the flood.
CFLAGS  := -m32 $(OPT) -std=gnu89 -fpermissive -w -fcommon -fno-strict-aliasing \
           -fwrapv -fno-delete-null-pointer-checks $(DEFS) $(INCS)
LDFLAGS := -m32 -static

BACKEND := toplev version tree print-tree stor-layout fold-const function \
 stmt except expr calls expmed explow optabs intl varasm rtl print-rtl rtlanal \
 emit-rtl genrtl real dbxout sdbout dwarfout dwarf2out xcoffout bitmap alias \
 gcse integrate jump cse loop unroll flow stupid combine varray regclass \
 regmove local-alloc global reload reload1 caller-save insn-peep reorg \
 haifa-sched final recog reg-stack insn-opinit insn-recog insn-extract \
 insn-output insn-emit lcm profile insn-attrtab getpwd convert mbchar \
 dyn-string splay-tree graph sbitmap resource hash obstack SNMisc

C_OBJS   := c-parse c-lang c-lex c-pragma c-decl c-typeck c-convert \
            c-aux-info c-common c-iterate
CXX_OBJS := c-common c-pragma cp/call cp/decl cp/errfn cp/expr cp/pt cp/sig \
 cp/typeck2 cp/class cp/decl2 cp/error cp/lex cp/parse cp/ptree cp/rtti \
 cp/spew cp/typeck cp/cvt cp/except cp/friend cp/init cp/method cp/search \
 cp/semantics cp/tree cp/xref cp/repo

BACKEND_O   := $(addprefix $(OBJ)/,$(addsuffix .o,$(BACKEND)))
C_O         := $(addprefix $(OBJ)/,$(addsuffix .o,$(C_OBJS)))
CXX_O       := $(addprefix $(OBJ)/,$(addsuffix .o,$(CXX_OBJS)))

GEN_RTL_O := $(OBJ)/rtl.o $(OBJ)/bitmap.o $(OBJ)/print-rtl.o $(OBJ)/obstack.o

GENERATED := $(OBJ)/tree-check.h $(OBJ)/genrtl.h $(OBJ)/genrtl.c \
 $(OBJ)/insn-attr.h $(OBJ)/insn-codes.h $(OBJ)/insn-config.h \
 $(OBJ)/insn-flags.h $(OBJ)/insn-peep.c $(OBJ)/insn-opinit.c \
 $(OBJ)/insn-recog.c $(OBJ)/insn-extract.c $(OBJ)/insn-output.c \
 $(OBJ)/insn-emit.c $(OBJ)/insn-attrtab.c

all: $(BIN)/cc1plus $(BIN)/cc1

# rs6000.c includes cp/cp-tree.h or c-tree.h depending on SN_CPP_BUILD
# (SN's vsgcc.dsp, the cc1plus project, defines SN_CPP_BUILD).
$(BIN)/cc1plus: $(BACKEND_O) $(OBJ)/rs6000-cxx.o $(CXX_O)
	$(CC) $(LDFLAGS) -o $@ $^

$(BIN)/cc1: $(BACKEND_O) $(OBJ)/rs6000.o $(C_O)
	$(CC) $(LDFLAGS) -o $@ $^

# ---- generated headers must exist before any compiler-proper object ----
# The rtl reader objects (also linked into the gen* tools) only need genrtl.h.
$(GEN_RTL_O) $(OBJ)/rtlanal.o: | $(OBJ)/genrtl.h
$(filter-out $(GEN_RTL_O) $(OBJ)/rtlanal.o,$(BACKEND_O)) $(C_O) $(CXX_O): | $(GENERATED)

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/toplev.o: $(SRC)/toplev.c
	$(CC) $(CFLAGS) -DTARGET_NAME=\"Dolphin\" -c -o $@ $<

$(OBJ)/intl.o: $(SRC)/intl.c
	$(CC) $(CFLAGS) -DLOCALEDIR=\"//locale\" -c -o $@ $<

$(OBJ)/rs6000.o: $(SRC)/config/rs6000/rs6000.c | $(GENERATED)
	$(CC) $(CFLAGS) -c -o $@ $<
$(OBJ)/rs6000-cxx.o: $(SRC)/config/rs6000/rs6000.c | $(GENERATED)
	$(CC) $(CFLAGS) -DSN_CPP_BUILD -c -o $@ $<

$(OBJ)/%.o: $(OBJ)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- generator programs ----
# The gen* programs only need hconfig.h (= config.h here) and the rtl reader.
# rtl.o/bitmap.o/print-rtl.o/obstack.o don't depend on generated headers,
# so they double as the "host" objects.
$(OBJ)/rtl.o $(OBJ)/bitmap.o $(OBJ)/print-rtl.o $(OBJ)/obstack.o $(OBJ)/rtlanal.o: | $(OBJ)/.dir
$(OBJ)/.dir:
	@mkdir -p $(OBJ) $(OBJ)/cp && touch $@

$(OBJ)/gencheck: $(SRC)/gencheck.c | $(OBJ)/.dir
	$(CC) $(CFLAGS) -o $@ $<
$(OBJ)/gengenrtl: $(SRC)/gengenrtl.c | $(OBJ)/.dir
	$(CC) $(CFLAGS) -DNO_GENRTL_H -o $@ $<
GENTOOLS := genattr gencodes genconfig genflags genpeep genopinit genrecog \
            genextract genoutput genemit
$(addprefix $(OBJ)/,$(GENTOOLS)): $(OBJ)/%: $(SRC)/%.c $(GEN_RTL_O)
	$(CC) $(CFLAGS) -o $@ $< $(GEN_RTL_O)
$(OBJ)/genattrtab: $(SRC)/genattrtab.c $(GEN_RTL_O) $(OBJ)/rtlanal.o
	$(CC) $(CFLAGS) -o $@ $< $(GEN_RTL_O) $(OBJ)/rtlanal.o
$(OBJ)/genextract: | $(OBJ)/insn-config.h
$(OBJ)/genattrtab: | $(OBJ)/insn-attr.h

$(OBJ)/tree-check.h: $(OBJ)/gencheck
	$< > $@.tmp && mv $@.tmp $@
$(OBJ)/genrtl.h $(OBJ)/genrtl.c: $(OBJ)/gengenrtl
	$< $(OBJ)/genrtl.h $(OBJ)/genrtl.c
$(OBJ)/insn-attr.h: $(OBJ)/genattr $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-codes.h: $(OBJ)/gencodes $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-config.h: $(OBJ)/genconfig $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-flags.h: $(OBJ)/genflags $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-peep.c: $(OBJ)/genpeep $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-opinit.c: $(OBJ)/genopinit $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-recog.c: $(OBJ)/genrecog $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-extract.c: $(OBJ)/genextract $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-output.c: $(OBJ)/genoutput $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-emit.c: $(OBJ)/genemit $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@
$(OBJ)/insn-attrtab.c: $(OBJ)/genattrtab $(MD)
	$< $(MD) > $@.tmp && mv $@.tmp $@

clean:
	rm -rf $(OBJ) $(BIN)/cc1 $(BIN)/cc1plus

.PHONY: all clean
.SECONDARY:
