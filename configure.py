#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import importlib.util
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "G4BE08",  # 0: Resident Evil 4 (USA) Debug Disc 1
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
parser.add_argument(
    "--prodg-driver",
    choices=["native", "ngccc"],
    default="native",
    help="how ProDG units are compiled: 'native' = SN cpp/ngcas via wibo + native cc1plus built "
    "from the SN GCC 2.95.3 v1.79 source (tools/ngccc.py, default); 'ngccc' = ngccc.exe v1.76 via wibo",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-1"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.3"
config.objdiff_tag = "v3.6.1"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
# ProDG ngcld does not accept Metrowerks linker flags.
config.ldflags = []

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = [Path("config") / config.version / "objects.py"]

# Optional numeric ID for decomp.me preset
config.scratch_preset_id = None

# The game was built with SN Systems ProDG (GCC 2.95.x). Version to be confirmed
# once the first game function is matched; 3.9.3 (gcc 2.95.3) is the newest.
PRODG_VERSION = "ProDG/3.9.3"
config.linker_version = PRODG_VERSION
# cc1/cc1plus built natively from SN's GPL source drop ("2.95.3 SN BUILD v1.79"): the shipped
# ngccc.exe pack is v1.76, whose rs6000.md shares one fpmem-address unspec between the GQR
# fast-cast conversions and the classic double-trick ones (dead `mr` copies the original never
# has). Built by tools/sn-gcc/build.sh (see docs/matching.md "Compiler").
PRODG_NATIVE_DIR = Path("build") / "compilers" / (PRODG_VERSION + "-v1.79")
if args.prodg_driver == "native":
    if is_windows():
        sys.exit("--prodg-driver native needs Linux (native cc1plus); use --prodg-driver ngccc")
    for exe in ("cc1plus", "cc1"):
        if not (PRODG_NATIVE_DIR / exe).exists():
            sys.exit(
                f"{PRODG_NATIVE_DIR / exe} missing: build it with tools/sn-gcc/build.sh "
                "(or configure with --prodg-driver ngccc)"
            )
    config.prodg_native_dir = PRODG_NATIVE_DIR
config.prodg_ldscript = Path("config") / config.version / "ldscript.ld"
# REL modules (config.yml `modules:`): ngcld -r with rel_ldscript.ld, then tools/make_rel.py rebuilds the
# REL from the ELF, config/G4BE08/modules/<mod>/rel.json and the DOL symbols (docs/matching.md "REL modules").
config.rel_ldscript = Path("config") / config.version / "rel_ldscript.ld"
config.rel_config_dir = Path("config") / config.version / "modules"
config.rel_dol_symbols = Path("config") / config.version / "symbols.txt"
config.prodg_sda_base = 0x8031BEE0
config.prodg_sda2_base = 0x8032BEE0
config.prodg_stack_end = 0x80316CE0  # _stack_end; _stack_addr (stack top) is 0x8031ACE0 (see ldscript.ld)
# .init has no symbols in Bio4.sym; __start is defined in the linker script
config.entry_override = "__start"

project_root = Path(__file__).resolve().parent
include_dirs = [
    project_root / "include",
    project_root / "src",
    project_root / "build" / config.version / "include",
]

# Base flags for game code (ProDG / GCC 2.95).
# -mfast-cast (not -mps-float): the original saves callee-saved FPRs in 8 bytes each and converts
# s8/u8 to float through a stack byte + psq_l (qr2/qr4). -mps-float reserves 16 bytes per saved FPR,
# which does not match any function in game/cam_ctrl that saves f31.
cflags_game = [
    "-O2",
    "-mfast-cast",
    *[f"-I {d.as_posix()}" for d in include_dirs],
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]
if args.debug:
    cflags_game.extend(["-g2", "-DDEBUG=1"])
else:
    cflags_game.append("-DNDEBUG=1")

# Nintendo Dolphin SDK libraries were prebuilt by Nintendo with CodeWarrior. The DOL carries
# "<< Dolphin SDK - OS release build: May 21 2004 (0x2301) >>" (OS = SDK 2004 patch 1, i.e.
# SDK_REVISION 1; the other libs are the Apr 5-7 2004 base builds, identical for revision 0/1).
# Sources come from doldecomp/dolsdk2004 (src/lib/<Name>.c, headers in include/dolphin/).
MWCC_SDK_VERSION = "GC/1.2.5n"
cflags_mw_sdk = [
    "-nodefaults",
    "-proc gekko",
    "-fp hard",
    "-Cpp_exceptions off",
    "-enum int",
    "-char unsigned",
    "-warn pragmas",
    "-requireprotos",
    "-pragma 'cats off'",
    "-O4,p",
    "-inline auto",
    "-I-",
    f"-i {(project_root / 'include').as_posix()}",
    f"-i {(project_root / 'include' / 'libc').as_posix()}",
    f"-i {(project_root / 'src' / 'lib').as_posix()}",
    "-D__GEKKO__",
    "-DSDK_REVISION=1",
]

# SDK objects, grouped like the SDK archives they came from (os.a, gx.a, ...).
# Runtime objects compiled with ProDG (libgcc, crt, SN debugger stub, __start,
# __ppc_eabi_init, tors) are NOT listed here and stay in the ProDG "lib" library.
SDK_LIBS: Dict[str, List[str]] = {
    "os": [
        "OS", "OSAlarm", "OSAlloc", "OSArena", "OSAudioSystem", "OSCache", "OSContext",
        "OSError", "OSExec", "OSFatal", "OSFont", "OSInterrupt", "OSLink", "OSMemory",
        "OSMutex", "OSReboot", "OSReset", "OSResetSW", "OSRtc", "OSSemaphore",
        "OSStopwatch", "OSSync", "OSThread", "OSTime",
    ],
    "base": ["PPCArch"],
    "exi": ["EXIBios", "EXIUart"],
    "si": ["SIBios", "SISamplingRate"],
    "db": ["db"],
    "mtx": ["mtx", "mtxvec", "mtx44", "mtx44vec", "vec", "quat", "psmtx"],
    "dvd": ["dvdlow", "dvdfs", "dvd", "dvdqueue", "dvderror", "dvdidutils", "dvdFatal", "fstload"],
    "vi": ["vi"],
    "pad": ["Pad", "Padclamp"],
    "ai": ["ai"],
    "ar": ["ar", "arq"],
    "ax": ["AX", "AXAlloc", "AXAux", "AXCL", "AXOut", "AXSPB", "AXVPB", "AXProf", "AXComp", "DSPCode"],
    "axfx": ["axfx", "reverb_hi", "reverb_std", "chorus", "delay", "reverb_hi_4ch"],
    "mix": ["mix"],
    "axart": ["axart", "axartsound", "axartcents", "axartenv", "axartlfo", "axart3d", "axartlpf"],
    "syn": ["syn", "synctrl", "synenv", "synlfo", "synmix", "synpitch", "synsample", "synvoice", "synwt"],
    "seq": ["seq"],
    "dsp": ["dsp", "dsp_debug", "dsp_task"],
    "card": [
        "CARDBios", "CARDBlock", "CARDDir", "CARDCheck", "CARDMount", "CARDFormat", "CARDOpen",
        "CARDCreate", "CARDRead", "CARDWrite", "CARDDelete", "CARDStat", "CARDNet", "CARDUnlock",
        "CARDRdwr",
    ],
    "gx": [
        "GXAttr", "GXGeometry", "GXLight", "GXTexture", "GXBump", "GXTev", "GXPixel", "GXTransform",
        "GXInit", "GXFifo", "GXMisc", "GXFrameBuf", "GXPerf", "GXDraw", "GXDisplayList",
    ],
    "texPalette": ["texPalette"],
    "fileCache": ["fileCache"],
    "amcstubs": ["AmcExi2Stubs"],
    # Metrowerks EABI runtime pieces the ProDG link pulled in (unoptimized CodeWarrior code)
    "runtime": ["__ppc_eabi_init"],
    "odemustubs": ["DebuggerDriver"],
}
SDK_UNIT_LIB: Dict[str, str] = {
    f"lib/{name}.c": lib for lib, names in SDK_LIBS.items() for name in names
}
# Per-object deviations from cflags_mw_sdk (same as in doldecomp/dolsdk2004's Makefile). These
# replace the base flag: MWCC keeps the first -O level it sees, so appending "-O3,p" has no effect.
SDK_CFLAG_OVERRIDES: Dict[str, Dict[str, str]] = {
    **{f"lib/{name}.c": {"-char unsigned": "-char signed"} for name in SDK_LIBS["dvd"]},
    "lib/mtx.c": {"-char unsigned": "-char signed"},
    "lib/mtx44.c": {"-char unsigned": "-char signed"},
    "lib/CARDOpen.c": {"-char unsigned": "-char signed"},
    "lib/EXIBios.c": {"-O4,p": "-O3,p"},
    "lib/__ppc_eabi_init.c": {"-O4,p": "-O4,p -opt nopeephole"},
}


def sdk_cflags(unit: str) -> List[str]:
    repl = SDK_CFLAG_OVERRIDES.get(unit)
    if not repl:
        return cflags_mw_sdk
    return [repl.get(flag, flag) for flag in cflags_mw_sdk]


def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": MWCC_SDK_VERSION,
        "cflags": cflags_mw_sdk,
        "mwcc_depflag": "-MD",  # with -I- the default -MMD records no header dependencies
        "progress_category": "sdk",
        "objects": objects,
    }

# CRI Middleware (ADX audio, Sofdec video, the GCCI/MFCI file interfaces and CRI's UTY_* helpers)
# was prebuilt by CRI with CodeWarrior >= 1.3.2 (`stwu/mflr/stw` 16-byte-frame prologue, `__div2i`
# runtime calls, `mr.` tests) at -O4,p with no small data at all (every 4-byte global is addressed
# `lis/addi`, float constants sit in .rodata): -sdata 0 -sdata2 0; string literals are in .rodata
# (-str readonly) and callee-saved registers are saved with stmw/lmw (-use_lmw_stmw on). Headers live
# in src/lib/cri/.
MWCC_CRI_VERSION = "GC/2.7"  # 2.4.7 build 108: the originals carry "MW2407 GC20Apr2004Patch1"; build >= 105 matches the struct-copy tail (sfd_tst, mwsfdcre)
cflags_mw_cri = [
    "-nodefaults",
    "-proc gekko",
    "-fp hard",
    # fused multiply-add (dct_ac fmadd chains, adx_dcd's fnmsub Newton steps); `-fp fmadd` alone does
    # not contract in 2.4.7
    "-fp_contract on",
    "-Cpp_exceptions off",
    "-enum int",
    "-char signed",
    "-warn pragmas",
    "-pragma 'cats off'",
    "-O4,p",
    "-inline auto",
    "-sdata 0",
    "-sdata2 0",
    "-str readonly",
    "-use_lmw_stmw on",
    "-I-",
    f"-i {(project_root / 'include').as_posix()}",
    f"-i {(project_root / 'include' / 'libc').as_posix()}",
    f"-i {(project_root / 'src' / 'lib').as_posix()}",
    f"-i {(project_root / 'src' / 'lib' / 'cri').as_posix()}",
]
CRI_LIBS: Dict[str, List[str]] = {
    # libadxgc.a (ADX streaming/decoding, stream jobs, load scheduler, CVFS, AX renderer)
    "adx": [
        "adx_tlk", "adx_tsvr", "adx_rnaa", "lsc", "lsc_ini", "lsc_svr", "sj_rbf", "sj_utl", "sj_err",
        "svm", "ax_rna", "rna_res", "adx_inis", "adx_sjd", "adx_bahx", "adx_bsc", "adx_bsps",
        "adx_amp", "adx_crs", "adx_dcd", "adx_errs", "adx_fsvr", "adx_stmc", "adx_xpnd", "adx_mgc",
        "adx_dcd5", "adx_fini", "adx_fs", "cri_cvfs", "lsc_err", "lsc_crs", "sj_crs", "sj_mem",
        "sj_uni", "rna_err", "rna_crs", "adx_bwav", "adx_baif", "adx_bau", "adx_fcch", "adx_insh",
        "adx_sfa", "adx_sje", "adx_sugc", "adx_gc", "adx_dcd3",
    ],
    # libsfdgc.a / libmwsfdgc.a (Sofdec MPEG video, MW player front end, UTY helpers)
    "sfd": [
        "mwsfdfrm", "mwsfdsvm", "mwsfdsfx", "mwsfdcre", "mwsfdlib", "mwsfdply", "mwsfdset",
        "mwsfdsvr", "mwsfdsst", "mwsfx_ARGB8888PLN", "mwsfx_Y84C44", "sud_lib", "sfx_set", "sfx_cnv",
        "sfx_lib", "sfx_alp", "sfx_zmv", "sfx_sud", "sfx_cnv_to_ARGB8888PLN", "sfx_cnv_to_Y84C44",
        "sfx_YCC420PLN_to_ARGB8888PLN", "sfx_YCC420PLN_to_Y84C44", "sfd_adxt", "sfd_aoap", "sfd_buf",
        "sfd_con", "sfd_cre", "sfd_hds", "sfd_lib", "sfd_mem", "sfd_mps", "sfd_mpv", "sfd_mpvf",
        "sfd_pl2", "sfd_ply", "sfd_pts", "sfd_see", "sfd_set", "sfd_tim", "sfd_trn", "sfd_uo",
        "sfd_vom", "sfd_tmr", "sfd_tst", "sfh_main", "cmptime", "memsetd", "muldiv", "muldivr",
        "uty_tmr", "mwsfdrna", "mwsfdrsc", "mwsfdsl", "mwstm", "mwsfdsee", "cftyp422_ppc", "cftfx",
        "cftcoladj", "sfx_inf", "mcp_not", "mps_dec", "mps_del", "mps_get", "mps_lib", "mpv_deli",
        "mpv_emp", "mpv_err", "mpv_frm", "mpv_get", "mpv_hdec", "mpv_lib", "mpv_vlc", "mpv_umc",
        "mpv_mcy", "mpv_m2v", "mpvabdec", "memcpyd", "uty_ppc", "dct_fsri", "mpv_bdec", "mpv_cdec",
        "mpv_cmc", "mpv_dec", "mpv_mc", "dct_ac", "dct_ver", "pl2link", "pl2enc", "cft_common",
    ],
    # CRI's GameCube/MemoryFile CVFS interfaces
    "gcci": ["mfci", "gcci", "gcci_sub"],
}
CRI_UNIT_LIB: Dict[str, str] = {
    f"lib/{name}.c": lib for lib, names in CRI_LIBS.items() for name in names
}
CRI_CFLAG_OVERRIDES: Dict[str, Dict[str, str]] = {
    # deferred inlining: accessors inline MWSFD_IsEnableHndl/mwPlyGetSfdHn defined at the file top,
    # and the functions are emitted in reverse source order (mwcc 2.4.7 always reverses under
    # `deferred`; the DOL's .text/.rodata order is the reverse of the natural file order)
    "lib/mwsfdset.c": {"-inline auto": "-inline auto,deferred"},
    "lib/adx_fs.c": {"-inline auto": "-inline auto,deferred"},
    # the VLC table generators: MPVVLC_Init (2nd in the file) calls the generators defined after it,
    # .text is the reverse of the call order and .bss follows the reversed codegen order
    "lib/mpv_vlc.c": {"-inline auto": "-inline auto,deferred"},
    "lib/cftfx.c": {"-inline auto": "-inline auto,deferred"},
    # the public accessors are inlined into ADXT_SetOutputMono/ADXT_GetTime/ADXT_DiscardSmpl defined
    # before them; .text is the reverse of the source order
    "lib/adx_tlk.c": {"-inline auto": "-inline auto,deferred"},
    # the Sofdec video driver inlines SFD_CalcYccPlane / SFD_SetMpvCond defined after their users;
    # .text is the reverse of the source order and .bss the reverse of the declaration order
    "lib/sfd_mpv.c": {"-inline auto": "-inline auto,deferred"},
}


def cri_cflags(unit: str) -> List[str]:
    repl = CRI_CFLAG_OVERRIDES.get(unit)
    if not repl:
        return cflags_mw_cri
    return [repl.get(flag, flag) for flag in cflags_mw_cri]


def CriLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": MWCC_CRI_VERSION,
        "cflags": cflags_mw_cri,
        "mwcc_depflag": "-MD",
        "progress_category": "sdk",
        "objects": objects,
    }

# newlib 1.8.2 libm (fdlibm) as shipped in SN ProDG's libm.a. The fdlibm sources (src/lib/fdlibm/)
# were compiled as C++ through an #include wrapper (src/lib/<unit>.cpp): g++ 2.95 drops folded
# static consts defined in an included file and keeps []-declared tables in small data. Literal
# pools live in .sdata (-msafe-sda), tables up to the 792-byte two_over_pi are small data (-G),
# fabs() stays a real call (-fno-builtin) and -mstrict-align gives the indexed float loads and the
# y[1] addressing of the float trig wrappers.
LIBM_UNITS = {
    f"lib/{name}.c"
    for name in [
        "e_pow", "e_sqrt", "s_cos", "ef_pow", "k_cos", "k_sin", "e_atan2", "e_log10", "s_atan",
        "s_fabs", "s_tan", "ef_acos", "ef_asin", "ef_atan2", "ef_sqrt", "sf_atan", "sf_cos",
        "sf_fabs", "sf_sin", "sf_tan", "k_tan", "e_log", "e_rem_pio2", "kf_cos", "kf_sin", "kf_tan",
        "ef_rem_pio2", "k_rem_pio2", "s_floor", "kf_rem_pio2", "sf_floor", "s_scalbn", "sf_scalbn",
        "s_copysign", "sf_copysign",
    ]
}
cflags_libm = [*cflags_game, "-msafe-sda", "-G 1024", "-fno-builtin", "-mstrict-align"]

# SN Systems libsn (ProDG runtime: stdio stubs, debugger stub, fp/64-bit helpers): GCC 2.95 -O2 with
# no small data (`first.183` of dummy.c sits in .data) and no common symbols (FSasync's uninitialised
# globals sit in the unit's own .bss in declaration order).
LIBSN_UNITS = {f"lib/{name}.c" for name in ["dummy", "tealeaf", "FSasync", "sndvd", "fileserver", "crt0", "crtbegin", "builtin-delete"]}
cflags_libsn = [*cflags_game, "-G 0", "-fno-common"]
# libsn/crt units that were hand-written assembly in the original (no C form exists): the split
# name stays lib/<name>.c, the source is src/lib/<name>.s, assembled by the template's `as` rule
# (binutils powerpc-eabi-as with config.asflags, then `dtk elf fixup`).
ASM_UNITS = {f"lib/{name}.c": f"lib/{name}.s" for name in ["proview", "ppcdown", "fileserver", "eabi", "__start", "tealeaf"]}

# gcc 2.95.3 libgcc2.c, one L_* section per unit (src/lib/libgcc2/ holds the verbatim sources plus
# a tconfig.h shim). __clz_tab (256 bytes) sits in .sdata2, so -G is large here as well; functions
# nobody referenced (__do_global_dtors, most of the exception runtime) were dropped by the linker.
LIBGCC_UNITS = {
    f"lib/{name}.c"
    for name in [
        "_ashldi3", "_ashrdi3", "_lshrdi3", "_divdi3", "_moddi3", "_udivdi3", "_umoddi3", "_pure",
        "_eh", "__main", "_exit", "tors",
    ]
}
cflags_libgcc = [*cflags_game, f"-I {(project_root / 'src' / 'lib' / 'libgcc2').as_posix()}", "-G 1024"]
# crt-style objects (__main, _eh, tors): no small data at all (__terminate_func, _ctors_data in .data)
cflags_crt = [*cflags_game, f"-I {(project_root / 'src' / 'lib' / 'libgcc2').as_posix()}", "-G 0"]
LIBGCC_CFLAGS = {"lib/__main.c": cflags_crt, "lib/_eh.c": cflags_crt, "lib/tors.c": cflags_crt}

Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


def MatchingFor(*versions):
    return config.version in versions


# Unit list generated from the debug build's Bio4.sym (see tools/gen_config.py).
spec = importlib.util.spec_from_file_location("objects", Path("config") / config.version / "objects.py")
objects_mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(objects_mod)
UNITS: List[str] = objects_mod.UNITS
NON_MATCHING = set(getattr(objects_mod, "NON_MATCHING", ()))
STRIP_UNUSED = set(getattr(objects_mod, "STRIP_UNUSED", ()))
# game/ units whose flags deviate from cflags_game (flag -> replacement, like SDK_CFLAG_OVERRIDES;
# the sound library is -O0).
UNIT_CFLAG_OVERRIDES: Dict[str, Dict[str, str]] = getattr(objects_mod, "UNIT_CFLAG_OVERRIDES", {})

# Split unit names come from splits.txt (game/ units are all named *.cpp there). A UNITS entry
# may instead name a *.c source (newlib units): the Object keeps the split name, the source is the .c.
split_units = set()
with open(Path("config") / config.version / "splits.txt") as _f:
    for _line in _f:
        _m = re.match(r"^(\S+):\s*$", _line)
        if _m:
            split_units.add(_m.group(1))

game_objects: List[Object] = []
lib_objects: List[Object] = []
sdk_objects: Dict[str, List[Object]] = {lib: [] for lib in SDK_LIBS}
cri_objects: Dict[str, List[Object]] = {lib: [] for lib in CRI_LIBS}
for unit in UNITS:
    status = NonMatching if unit in NON_MATCHING else Matching
    name = unit
    if unit not in split_units:
        alt = str(Path(unit).with_suffix(".cpp"))
        if alt in split_units:
            name = alt
    if unit in ASM_UNITS:
        lib_objects.append(Object(status, name, source=ASM_UNITS[unit]))
    elif unit.startswith("game/"):
        # GCC 2.95 linkonce sections (vtables, template instantiations, out-of-line inlines) are
        # folded into .rodata / dropped the way the original link laid them out (see the tool).
        # The newlib C units (game/*.c) came out of SN's libc.a: the linker dropped the functions
        # nobody referenced (their .rodata strings stayed) and unreferenced statics; strip them too.
        post_build = [f"$python tools/fold_linkonce.py --unit {unit} {{out}}"]
        post_build_implicit = [Path("tools/fold_linkonce.py"), Path("config") / config.version / "sym_map.tsv"]
        cflags = cflags_game
        if unit in UNIT_CFLAG_OVERRIDES:
            repl = UNIT_CFLAG_OVERRIDES[unit]
            cflags = [repl.get(flag, flag) for flag in cflags_game if repl.get(flag, flag)]
        if unit.endswith(".c"):
            post_build.insert(0, f"$python tools/strip_unused.py --gcc --unit {name} {{out}}")
            post_build_implicit.append(Path("tools/strip_unused.py"))
            # newlib proper was built with -fno-common (fopen's _sn_iobf sits in its own .bss);
            # errno.c's `int errno' is a common symbol the linker put at the end of .sbss.
            if unit != "game/errno.c":
                cflags = [*cflags_game, "-fno-common"]
        elif unit in STRIP_UNUSED:
            # C++ units whose original had functions the linker dead-stripped (their constant
            # pools and statics stayed): drop the bodies after the linkonce fold.
            post_build.append(f"$python tools/strip_unused.py --gcc --unit {name} {{out}}")
            post_build_implicit.append(Path("tools/strip_unused.py"))
        game_objects.append(
            Object(
                status,
                name,
                source=unit,
                cflags=cflags,
                post_build=post_build,
                post_build_implicit=post_build_implicit,
            )
        )
    elif unit in SDK_UNIT_LIB:
        # The ProDG linker dead-stripped unreferenced SDK functions; drop them from our object too.
        sdk_objects[SDK_UNIT_LIB[unit]].append(
            Object(
                status,
                name,
                source=unit,
                cflags=sdk_cflags(unit),
                post_build=[f"$python tools/strip_unused.py --unit {unit} {{out}}"],
                post_build_implicit=[Path("tools/strip_unused.py"), Path("config") / config.version / "sym_map.tsv"],
            )
        )
    elif unit in CRI_UNIT_LIB:
        # Same linker dead-strip as the SDK archives (CRI's libs are MWCC objects as well).
        cri_objects[CRI_UNIT_LIB[unit]].append(
            Object(
                status,
                name,
                source=unit,
                cflags=cri_cflags(unit),
                post_build=[f"$python tools/strip_unused.py --unit {unit} {{out}}"],
                post_build_implicit=[Path("tools/strip_unused.py"), Path("config") / config.version / "sym_map.tsv"],
            )
        )
    elif unit in LIBGCC_UNITS:
        lib_objects.append(
            Object(
                status,
                name,
                source=unit,
                cflags=LIBGCC_CFLAGS.get(unit, cflags_libgcc),
                post_build=[f"$python tools/strip_unused.py --gcc --unit {unit} {{out}}"],
                post_build_implicit=[Path("tools/strip_unused.py"), Path("config") / config.version / "sym_map.tsv"],
            )
        )
    elif unit in LIBSN_UNITS:
        lib_objects.append(
            Object(
                status,
                name,
                source=unit,
                cflags=cflags_libsn,
                post_build=[f"$python tools/strip_unused.py --gcc --unit {unit} {{out}}"],
                post_build_implicit=[Path("tools/strip_unused.py"), Path("config") / config.version / "sym_map.tsv"],
            )
        )
    elif unit in LIBM_UNITS:
        lib_objects.append(
            Object(
                status,
                name,
                source=str(Path(unit).with_suffix(".cpp")),
                cflags=cflags_libm,
            )
        )
    else:
        lib_objects.append(Object(status, name, source=unit))

# REL module units: config/G4BE08/modules.py (UNITS boundaries, NON_MATCHING) plus one default unit
# "<mod>/<mod>.cpp" per module of config.yml. REL code has no small data (every DOL global goes
# through lis/addi: -G 0), so cflags_game minus small data.
cflags_rel = [*cflags_game, "-G 0"]
spec = importlib.util.spec_from_file_location("modules", Path("config") / config.version / "modules.py")
modules_mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(modules_mod)
REL_UNITS: Dict[str, List] = getattr(modules_mod, "UNITS", {})
REL_NON_MATCHING = set(getattr(modules_mod, "NON_MATCHING", ()))
# module units the original REL link dead-stripped at function level (stage em_wrap.cpp/cSceObj.cpp):
# tools/strip_unused.py --module keeps what the module's sym_map.tsv names for the unit
REL_STRIP_UNUSED = set(getattr(modules_mod, "STRIP_UNUSED", ()))
# extra compiler flags of a module (config/G4BE08/modules.py CFLAGS: Sscrn's -fno-implement-inlines)
REL_CFLAGS: Dict[str, List[str]] = getattr(modules_mod, "CFLAGS", {})
# per-unit additions appended after the module's (modules.py UNIT_CFLAGS: Tools/db_toolbase.cpp was
# built with the default -fimplement-inlines, its in-class cDbgWindow::Init is in the object)
REL_UNIT_CFLAGS: Dict[str, List[str]] = getattr(modules_mod, "UNIT_CFLAGS", {})
rel_objects: List[Object] = []
with open(config.config_path) as _f:
    _config_yml = _f.read()
_module_names = re.findall(r"^\s+name:\s*(\S+)\s*$", _config_yml, re.M)
# The original RELs (loose files/Rel/*.rel, or inside the files/em/*.drs archives) and the
# Bio4.<mod>.sym files come from the disc; tools/extract_orig.py pulls the missing ones out of an
# image placed in orig/<ver>/ (or run it by hand with an extracted disc directory).
_orig_dir = Path("orig") / config.version
_missing = [o for o in re.findall(r"^- object:\s*(\S+)\s*$", _config_yml, re.M) if not (_orig_dir / o).exists()]
if _missing and args.mode == "configure":
    _images = sorted(_orig_dir.glob("*.iso")) + sorted(_orig_dir.glob("*.gcm"))
    if not _images:
        sys.exit(
            f"{len(_missing)} original REL(s) missing under {_orig_dir} ({_missing[0]}, ...): put the disc 1 (and disc 2 for st3_*) "
            f"image in {_orig_dir}/ or run python3 tools/extract_orig.py {config.config_path} <image or extracted disc>"
        )
    # disc 1 carries the DOL and 110 RELs, disc 2 adds st3_0..st3_3: extract from every image present
    for _img in _images:
        subprocess.check_call([sys.executable, "tools/extract_orig.py", str(config.config_path), str(_img)])
for _mod in _module_names:
    for unit, _first, *_src in REL_UNITS.get(_mod, [(f"{_mod}/{_mod}.cpp", None)]):
        # a third element names a source shared by several modules (st2/st2.cpp ends every st2_* REL);
        # a fourth (data section starts) is gen_rel_config.py's.
        # Linkonce sections are folded into .text/.rodata the way the module link kept them (every
        # object's copies stay, see tools/fold_linkonce.py --module).
        _post = [f"$python tools/fold_linkonce.py --module {_mod} --unit {unit} {{out}}"]
        _post_implicit = [Path("tools/fold_linkonce.py"), config.rel_config_dir / _mod / "sym_map.tsv"]
        if unit in REL_STRIP_UNUSED:
            _post.insert(0, f"$python tools/strip_unused.py --gcc --module {_mod} --unit {unit} {{out}}")
            _post_implicit.append(Path("tools/strip_unused.py"))
        # REL_MODULE names the module a (possibly shared) source is compiled for: the stage rooms use it
        # to define the module's COMMON block placeholder (include/st_room.h, `common_<mod>`).
        rel_objects.append(
            Object(
                NonMatching if unit in REL_NON_MATCHING else Matching,
                unit,
                source=_src[0] if _src and _src[0] else unit,
                cflags=[*cflags_rel, f"-DREL_MODULE={_mod}", *REL_CFLAGS.get(_mod, []), *REL_UNIT_CFLAGS.get(unit, [])],
                post_build=_post,
                post_build_implicit=_post_implicit,
            )
        )
config.reconfig_deps.append(Path("config") / config.version / "modules.py")
config.reconfig_deps.extend(config.rel_config_dir / _mod / "rel.json" for _mod in _module_names)

config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "Bio4",
        "mw_version": PRODG_VERSION,
        "cflags": cflags_game,
        "progress_category": "game",
        "objects": game_objects,
    },
    {
        # ProDG-compiled runtime: libgcc (_ashldi3, _divdi3, _eh, ...), crt (__start,
        # __ppc_eabi_init, __main, tors, crtbegin), SN debugger stub (ppcdown, fileserver,
        # proview, tealeaf), newlib libm, and third-party libs (CRI ADX/Sofdec) not yet identified.
        "lib": "lib",
        "mw_version": PRODG_VERSION,
        "cflags": cflags_game,
        "progress_category": "sdk",
        "objects": lib_objects,
    },
    *[DolphinLib(lib, objs) for lib, objs in sdk_objects.items() if objs],
    *[CriLib(lib, objs) for lib, objs in cri_objects.items() if objs],
    {
        "lib": "Rel",
        "mw_version": PRODG_VERSION,
        "cflags": cflags_rel,
        "progress_category": "rel",
        "objects": rel_objects,
    },
]

config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("sdk", "SDK/Runtime Code"),
    ProgressCategory("rel", "REL Modules"),
]
config.progress_each_module = args.verbose
config.progress_report_args = []

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
