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
    loop_patches = (
        (
            "sub_836B3A50",
            "\t// lhzu r9,12(r10)\n\t// li r7,255",
            "\t// lhzu r9,12(r10)\n"
            "\tctx.r10.s64 = ctx.r10.s64 + 12;\n"
            "\tctx.r9.u64 = PPC_LOAD_U16(ctx.r10.u32);\n"
            "\t// li r7,255",
            "goto loc_836B3B34;",
        ),
        (
            "sub_836B3B40",
            "\t// lhzu r9,12(r10)\n\t// addi r11,r11,1",
            "\t// lhzu r9,12(r10)\n"
            "\tctx.r10.s64 = ctx.r10.s64 + 12;\n"
            "\tctx.r9.u64 = PPC_LOAD_U16(ctx.r10.u32);\n"
            "\t// addi r11,r11,1",
            "goto loc_836B3BA8;",
        ),
    )
    changed = False
    for function_name, old_text, new_text, guard_target in loop_patches:
        start = content.find(f"PPC_FUNC_IMPL(__imp__{function_name})")
        end = content.find("\n__attribute__((alias(", start + 1)
        if start < 0 or end < 0:
            print(f"Could not isolate generated function {function_name}")
            return False
        function_text = content[start:end]

        if old_text in function_text:
            function_text = function_text.replace(old_text, new_text, 1)
            changed = True

        update_load = (
            "\tctx.r10.s64 = ctx.r10.s64 + 12;\n"
            "\tctx.r9.u64 = PPC_LOAD_U16(ctx.r10.u32);"
        )
        guarded_update_load = (
            "\tctx.r10.s64 = ctx.r10.s64 + 12;\n"
            f"\tif (ctx.r10.u64 > 0xFFFFFFFDull) {guard_target}\n"
            "\tctx.r9.u64 = PPC_LOAD_U16(ctx.r10.u32);"
        )
        if guarded_update_load not in function_text:
            old_guarded_update_load = (
                "\tctx.r10.s64 = ctx.r10.s64 + 12;\n"
                f"\tif (ctx.r10.u32 > 0xFFFFFFFDu) {guard_target}\n"
                "\tctx.r9.u64 = PPC_LOAD_U16(ctx.r10.u32);"
            )
            if old_guarded_update_load in function_text:
                function_text = function_text.replace(old_guarded_update_load, guarded_update_load, 1)
                changed = True
            elif update_load in function_text:
                function_text = function_text.replace(update_load, guarded_update_load, 1)
                changed = True
            else:
                print(f"Missing expected lhzu lowering in {function_name}")
                return False

        content = content[:start] + function_text + content[end:]

    if changed:
        path.write_text(content, encoding="utf-8")
        print(f"Patched worker-list updates with bounds checks: {path.name}")
    else:
        print(f"Worker-list updates already patched: {path.name}")
    return changed


def patch_indirect_call_bounds(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    content = path.read_text(encoding="utf-8")
    old_macro = "#define PPC_CALL_INDIRECT_FUNC(x) (PPC_LOOKUP_FUNC(base, x))(ctx, base)"
    new_macro = "#define PPC_CALL_INDIRECT_FUNC(x) PPCCallIndirectFunc(ctx, base, static_cast<uint32_t>(x))"
    old_declaration = "typedef void PPCFunc(struct PPCContext& __restrict__ ctx, uint8_t* base);\n"
    declaration = (
        "typedef void PPCFunc(struct PPCContext& __restrict__ ctx, uint8_t* base);\n"
        "struct PPCContext;\n"
        "void PPCCallIndirectFunc(PPCContext& ctx, uint8_t* base, uint32_t target);\n"
    )

    changed = False
    if new_macro not in content:
        if old_macro not in content:
            print(f"Indirect-call macro not found in: {path.name}")
            return False
        content = content.replace(old_macro, new_macro, 1)
        changed = True

    if "void PPCCallIndirectFunc(PPCContext& ctx, uint8_t* base, uint32_t target);" not in content:
        if old_declaration not in content:
            print(f"PPC function typedef not found in: {path.name}")
            return False
        content = content.replace(old_declaration, declaration, 1)
        changed = True

    if changed:
        path.write_text(content, encoding="utf-8")
        print(f"Patched indirect-call bounds checks: {path.name}")
    else:
        print(f"Indirect-call bounds checks already patched: {path.name}")
    return changed


def patch_failed_guest_allocation(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    content = path.read_text(encoding="utf-8")
    start = content.find("PPC_FUNC_IMPL(__imp__sub_8354EF98)")
    end = content.find("\n__attribute__((alias(", start + 1)
    if start < 0 or end < 0:
        print(f"Could not isolate generated function sub_8354EF98 in {path.name}")
        return False

    function_text = content[start:end]
    old_text = (
        "\t// beq cr6,0x8354f080\n"
        "\tif (cr6.eq) goto loc_8354F080;\n"
        "\t// lwz r4,12(r31)"
    )
    new_text = (
        "\t// beq cr6,0x8354f080\n"
        "\tif (cr6.eq || ctx.r3.u32 == 0xFFFFFFFFu) goto loc_8354F080;\n"
        "\t// lwz r4,12(r31)"
    )
    if new_text in function_text:
        print(f"Failed-allocation guard already patched: {path.name}")
        return False
    if old_text not in function_text:
        print(f"Failed-allocation branch not found in: {path.name}")
        return False

    function_text = function_text.replace(old_text, new_text, 1)
    content = content[:start] + function_text + content[end:]
    path.write_text(content, encoding="utf-8")
    print(f"Patched failed guest allocation handling: {path.name}")
    return True


def main():
    ppc_dir = REPO_ROOT / "FiveInvaderLib" / "ppc"
    patch_worker_list_updates(ppc_dir / "ppc_recomp.337.cpp")
    patch_indirect_call_bounds(ppc_dir / "ppc_context.h")
    patch_failed_guest_allocation(ppc_dir / "ppc_recomp.316.cpp")

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
\tctx.lr = 0x8321DAC4;
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
\tctx.lr = 0x8321DAC4;
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
