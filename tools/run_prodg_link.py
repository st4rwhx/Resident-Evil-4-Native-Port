#!/usr/bin/env python3

import os
import shlex
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 7:
        print(
            "usage: run_prodg_link.py <linker> <wrapper> <sn_ngc_path> <out> <rsp> <ldflags>",
            file=sys.stderr,
        )
        return 2

    linker, wrapper, sn_ngc_path, out_path, rsp_path, ldflags = sys.argv[1:]

    with open(rsp_path, "r", encoding="utf-8") as f:
        inputs = [line.strip() for line in f if line.strip()]

    split_posix = os.name != "nt"
    flags = shlex.split(ldflags, posix=split_posix) if ldflags else []

    cmd = []
    if wrapper:
        cmd.append(wrapper)
    cmd.append(linker)
    cmd.extend(flags)
    cmd.extend(["-o", out_path])
    cmd.extend(inputs)

    env = os.environ.copy()
    env["SN_NGC_PATH"] = sn_ngc_path

    return subprocess.run(cmd, env=env, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
