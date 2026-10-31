#!/usr/bin/env python3
"""Inline shared switch-case tails that XenonRecomp emits as cross-function gotos.

GTA V's compiler places some switch case blocks in a different function chunk
than the dispatch block. XenonRecomp's BDZ/BDNZ translation emits a plain
``goto loc_XXXX`` even when that label belongs to another recompiled function,
which is invalid C++.

This script patches the known GTA V case tails in ``ppc_recomp.219.cpp``. Each
tail is a two-instruction constant load followed by a branch to the shared
epilogue, so inlining it at the dispatch site preserves the original behavior.
Run it after every XenonRecomp regeneration.
"""

import pathlib
import sys

PATCHES = {
    "loc_82F96068": ("-2113339392", "12328"),
    "loc_82F960E0": ("-2113339392", "12196"),
}


def patch_file(path: pathlib.Path) -> bool:
    text = path.read_text()
    changed = False
    for label, (value, offset) in PATCHES.items():
        for prefix in ("ctx.ctr", "ctr"):
            old = f"\tif ({prefix}.u32 == 0) goto {label};"
            new = (
                f"\tif ({prefix}.u32 == 0)\n"
                "\t{\n"
                f"\t\tctx.r11.s64 = {value};\n"
                f"\t\tctx.r11.s64 = ctx.r11.s64 + {offset};\n"
                "\t\tgoto loc_82F96254;\n"
                "\t}"
            )
            if old in text:
                text = text.replace(old, new)
                changed = True
    if changed:
        path.write_text(text)
    return changed


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <ppc output directory>", file=sys.stderr)
        return 1
    ppc_dir = pathlib.Path(sys.argv[1])
    path = ppc_dir / "ppc_recomp.219.cpp"
    if not path.exists():
        return 0
    if patch_file(path):
        print(f"Patched shared case tails in {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
