#!/usr/bin/env python3
"""Fixes regen.sh applies to the lifted code (python3 tools/patch_gen.py <gen dir>).

Mechanism from MiddleDucky's port/tools/patch_gen.py (MIT; see docs/upstream-patches/README.md): a function whose first
instruction is a conditional jump on its CALLER's flags is lifted with a fresh `_flags = 0`, so the branch is never
taken. Each fix finds its line by exact text and fails loudly if it is gone; running twice changes nothing.
No fixes yet for this executable (phase 2: look at the upstream patch_gen.py of hp3-recompiled, then re-find them in the
lifted code of this lifter; some are labels inside the caller here and need none).
"""
import glob, sys

FIXES = [
    # (function, exact line in the lifted code, replacement), e.g.
    # ("sub_000C0001", "    if (_flags /* je: equal / zero */) goto loc_000C000E;",
    #  "    if (eax == 0 /* je: the caller's test eax, eax */) goto loc_000C000E;"),
]

def main(gen):
    files = sorted(glob.glob(f"{gen}/recomp_*.c"))
    for func, old, new in FIXES:
        head = f"void {func}(void)\n"
        for p in files:
            s = open(p).read()
            i = s.find(head)
            if i < 0:
                continue
            j = s.find("\nvoid sub_", i + len(head))
            body = s[i:j if j > 0 else len(s)]
            if new in body:
                print(f"patch_gen: {func} already fixed"); break
            if old not in body:
                sys.exit(f"patch_gen: {func}: line not found (lifter changed?)")
            s = s[:i] + body.replace(old, new, 1) + s[i + len(body):]
            open(p, "w").write(s)
            print(f"patch_gen: {func} fixed"); break
        else:
            sys.exit(f"patch_gen: {func} not in {gen}")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "gen")
