#!/usr/bin/env python3
"""Reapply runtime-critical fixes to generated PowerPC units."""

import pathlib
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

def patch_file(path: pathlib.Path, old_text: str, new_text: str) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False
    content = path.read_text(encoding="utf-8")
    if new_text in content:
        print(f"Already patched: {path.name}")
        return False
    if old_text not in content:
        print(f"Target pattern not found in: {path.name}")
        return False
    content = content.replace(old_text, new_text)
    path.write_text(content, encoding="utf-8")
    print(f"Successfully patched: {path.name}")
    return True


def patch_worker_list_updates(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    content = path.read_text(encoding="utf-8")
    old_text = "\t// lhzu r9,12(r10)\n\t// addi r11,r11,1"
    new_text = (
        "\t// lhzu r9,12(r10)\n"
        "\tctx.r10.s64 = ctx.r10.s64 + 12;\n"
        "\tctx.r9.u64 = PPC_LOAD_U16(ctx.r10.u32);\n"
        "\t// addi r11,r11,1"
    )
    if old_text not in content:
        if new_text in content:
            print(f"Already patched worker-list updates: {path.name}")
            return False
        print(f"Worker-list update pattern not found in: {path.name}")
        return False

    content = content.replace(old_text, new_text)
    path.write_text(content, encoding="utf-8")
    print(f"Patched worker-list updates: {path.name}")
    return True


def main():
    ppc_dir = REPO_ROOT / "FiveInvaderLib" / "ppc"
    patch_worker_list_updates(ppc_dir / "ppc_recomp.337.cpp")

    # Patch 269: sub_8321D9F8
    target_269_old = """\t// lwzx r3,r10,r11
\tctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r11.u32);
\t// lwz r9,0(r3)
\tctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
\t// lwz r8,8(r9)
\tctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + 8);
\t// mtctr r8
\tctr.u64 = ctx.r8.u64;
\t// bctrl 
\tPPC_CALL_INDIRECT_FUNC(ctr.u32);
\t// mr r29,r3
\tctx.r29.u64 = ctx.r3.u64;
\t// b 0x8321dad0
\tgoto loc_8321DAD0;"""

    target_269_new = """\t// lwzx r3,r10,r11
\tctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r11.u32);
\tif (ctx.r3.u32 == 0) ctx.r3.u32 = 0x83887168;
\tctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
\tctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + 8);
\tctr.u64 = ctx.r8.u64;
\tif (ctr.u32 >= 0x82000000 && ctr.u32 < 0x84000000) {
\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);
\t}
\t// mr r29,r3
\tctx.r29.u64 = ctx.r3.u64;
\t// b 0x8321dad0
\tgoto loc_8321DAD0;"""

    patch_file(ppc_dir / "ppc_recomp.269.cpp", target_269_old, target_269_new)

if __name__ == "__main__":
    main()
