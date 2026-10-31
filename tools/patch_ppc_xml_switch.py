#!/usr/bin/env python3
"""Resolve switch table bctr dispatches in XML token parser (ppc_recomp.263.cpp).

In GTA V's XML parser (sub_831CB460 and sub_831CC260), switch tables at
0x831CB4A8 and 0x831CC2A8 index 17 jump targets via CTR indirect branch.
XenonRecomp translated these as PPC_CALL_INDIRECT_FUNC and disassembled
the table entries as fake lwz instructions, causing the parser to return
early without clearing the pending token flag (1239(r30)), leading to an
infinite spinlock in sub_831DAF68.

This script patches both functions to use native C++ switch statements
branching directly to the corresponding case labels.
"""

import pathlib
import sys

def patch_file(path: pathlib.Path) -> bool:
    with open(path) as f:
        lines = f.readlines()

    # Verify if already patched
    text = "".join(lines)
    if "loc_831CB4EC:" in text and "loc_831CC2EC:" in text:
        print(f"File {path} is already patched.")
        return False

    # Check markers
    if "// lis r12,-31971" not in lines[11414] or "// li r26,0" not in lines[11461]:
        print(f"Warning: Markers not matching expected line numbers in {path}")
        return False

    insertions = [
        # sub_831CC260 labels (0-based indices)
        (15362 - 1, "loc_831CCFB0:\n"),
        (15266 - 1, "loc_831CCF24:\n"),
        (15153 - 1, "loc_831CCE80:\n"),
        (15061 - 1, "loc_831CCDF8:\n"),
        (15041 - 1, "loc_831CCDD4:\n"),
        (15021 - 1, "loc_831CCDB0:\n"),
        (15001 - 1, "loc_831CCD8C:\n"),
        (14981 - 1, "loc_831CCD68:\n"),
        (14961 - 1, "loc_831CCD44:\n"),
        (14941 - 1, "loc_831CCD20:\n"),
        (14923 - 1, "loc_831CCD00:\n"),
        (14903 - 1, "loc_831CCCDC:\n"),
        (14885 - 1, "loc_831CCCBC:\n"),
        (14863 - 1, "loc_831CCC94:\n"),

        # sub_831CB460 labels (0-based indices)
        (13300 - 1, "loc_831CC1A0:\n"),
        (13204 - 1, "loc_831CC114:\n"),
        (13091 - 1, "loc_831CC070:\n"),
        (12999 - 1, "loc_831CBFE8:\n"),
        (12979 - 1, "loc_831CBFC4:\n"),
        (12959 - 1, "loc_831CBFA0:\n"),
        (12939 - 1, "loc_831CBF7C:\n"),
        (12919 - 1, "loc_831CBF58:\n"),
        (12899 - 1, "loc_831CBF34:\n"),
        (12879 - 1, "loc_831CBF10:\n"),
        (12861 - 1, "loc_831CBEF0:\n"),
        (12841 - 1, "loc_831CBECC:\n"),
        (12823 - 1, "loc_831CBEAC:\n"),
        (12801 - 1, "loc_831CBE84:\n"),
    ]

    for idx, label_line in insertions:
        lines.insert(idx, label_line)

    text = "".join(lines)

    old_switch_1 = """\t// lis r12,-31971
\tctx.r12.s64 = -2095251456;
\t// rlwinm r0,r6,2,0,29
\tctx.r0.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;
\t// addi r12,r12,-19288
\tctx.r12.s64 = ctx.r12.s64 + -19288;
\t// lwzx r0,r12,r0
\tctx.r0.u64 = PPC_LOAD_U32(ctx.r12.u32 + ctx.r0.u32);
\t// mtctr r0
\tctr.u64 = ctx.r0.u64;
\t// bctr 
\tPPC_CALL_INDIRECT_FUNC(ctr.u32);
\treturn;
\t// lwz r24,-19220(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -19220);
\t// lwz r24,-19220(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -19220);
\t// lwz r24,-16764(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16764);
\t// lwz r24,-16724(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16724);
\t// lwz r24,-15796(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -15796);
\t// lwz r24,-16692(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16692);
\t// lwz r24,-16656(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16656);
\t// lwz r24,-16624(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16624);
\t// lwz r24,-16588(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16588);
\t// lwz r24,-16552(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16552);
\t// lwz r24,-16516(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16516);
\t// lwz r24,-16480(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16480);
\t// lwz r24,-16444(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16444);
\t// lwz r24,-16408(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16408);
\t// lwz r24,-16272(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16272);
\t// lwz r24,-16108(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -16108);
\t// lwz r24,-15968(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -15968);"""

    new_switch_1 = """\tswitch (ctx.r6.u32) {
\tcase 0:
\tcase 1: goto loc_831CB4EC;
\tcase 2: goto loc_831CBE84;
\tcase 3: goto loc_831CBEAC;
\tcase 4: goto loc_831CC24C;
\tcase 5: goto loc_831CBECC;
\tcase 6: goto loc_831CBEF0;
\tcase 7: goto loc_831CBF10;
\tcase 8: goto loc_831CBF34;
\tcase 9: goto loc_831CBF58;
\tcase 10: goto loc_831CBF7C;
\tcase 11: goto loc_831CBFA0;
\tcase 12: goto loc_831CBFC4;
\tcase 13: goto loc_831CBFE8;
\tcase 14: goto loc_831CC070;
\tcase 15: goto loc_831CC114;
\tcase 16: goto loc_831CC1A0;
\tdefault: goto loc_831CC24C;
\t}
loc_831CB4EC:"""

    old_switch_2 = """\t// lis r12,-31971
\tctx.r12.s64 = -2095251456;
\t// rlwinm r0,r6,2,0,29
\tctx.r0.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;
\t// addi r12,r12,-15704
\tctx.r12.s64 = ctx.r12.s64 + -15704;
\t// lwzx r0,r12,r0
\tctx.r0.u64 = PPC_LOAD_U32(ctx.r12.u32 + ctx.r0.u32);
\t// mtctr r0
\tctr.u64 = ctx.r0.u64;
\t// bctr 
\tPPC_CALL_INDIRECT_FUNC(ctr.u32);
\treturn;
\t// lwz r24,-15636(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -15636);
\t// lwz r24,-15636(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -15636);
\t// lwz r24,-13164(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -13164);
\t// lwz r24,-13124(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -13124);
\t// lwz r24,-12196(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12196);
\t// lwz r24,-13092(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -13092);
\t// lwz r24,-13056(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -13056);
\t// lwz r24,-13024(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -13024);
\t// lwz r24,-12988(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12988);
\t// lwz r24,-12952(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12952);
\t// lwz r24,-12916(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12916);
\t// lwz r24,-12880(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12880);
\t// lwz r24,-12844(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12844);
\t// lwz r24,-12808(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12808);
\t// lwz r24,-12672(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12672);
\t// lwz r24,-12508(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12508);
\t// lwz r24,-12368(r28)
\tctx.r24.u64 = PPC_LOAD_U32(ctx.r28.u32 + -12368);"""

    new_switch_2 = """\tswitch (ctx.r6.u32) {
\tcase 0:
\tcase 1: goto loc_831CC2EC;
\tcase 2: goto loc_831CCC94;
\tcase 3: goto loc_831CCCBC;
\tcase 4: goto loc_831CD05C;
\tcase 5: goto loc_831CCCDC;
\tcase 6: goto loc_831CCD00;
\tcase 7: goto loc_831CCD20;
\tcase 8: goto loc_831CCD44;
\tcase 9: goto loc_831CCD68;
\tcase 10: goto loc_831CCD8C;
\tcase 11: goto loc_831CCDB0;
\tcase 12: goto loc_831CCDD4;
\tcase 13: goto loc_831CCDF8;
\tcase 14: goto loc_831CCE80;
\tcase 15: goto loc_831CCF24;
\tcase 16: goto loc_831CCFB0;
\tdefault: goto loc_831CD05C;
\t}
loc_831CC2EC:"""

    if old_switch_1 not in text or old_switch_2 not in text:
        print("Error: switch blocks not found in text.")
        return False

    text = text.replace(old_switch_1, new_switch_1, 1)
    text = text.replace(old_switch_2, new_switch_2, 1)

    with open(path, "w") as f:
        f.write(text)

    print(f"Patched XML parser switch tables in {path}")
    return True

def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <ppc output directory>", file=sys.stderr)
        return 1
    ppc_dir = pathlib.Path(sys.argv[1])
    path = ppc_dir / "ppc_recomp.263.cpp"
    if not path.exists():
        return 0
    patch_file(path)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
