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

    target_labels = [
        ("sub_831CB460", 0x831CB460, 0x831CBE84, "loc_831CBE84"),
        ("sub_831CB460", 0x831CB460, 0x831CBEAC, "loc_831CBEAC"),
        ("sub_831CB460", 0x831CB460, 0x831CC24C, "loc_831CC24C"),
        ("sub_831CB460", 0x831CB460, 0x831CBECC, "loc_831CBECC"),
        ("sub_831CB460", 0x831CB460, 0x831CBEF0, "loc_831CBEF0"),
        ("sub_831CB460", 0x831CB460, 0x831CBF10, "loc_831CBF10"),
        ("sub_831CB460", 0x831CB460, 0x831CBF34, "loc_831CBF34"),
        ("sub_831CB460", 0x831CB460, 0x831CBF58, "loc_831CBF58"),
        ("sub_831CB460", 0x831CB460, 0x831CBF7C, "loc_831CBF7C"),
        ("sub_831CB460", 0x831CB460, 0x831CBFA0, "loc_831CBFA0"),
        ("sub_831CB460", 0x831CB460, 0x831CBFC4, "loc_831CBFC4"),
        ("sub_831CB460", 0x831CB460, 0x831CBFE8, "loc_831CBFE8"),
        ("sub_831CB460", 0x831CB460, 0x831CC070, "loc_831CC070"),
        ("sub_831CB460", 0x831CB460, 0x831CC114, "loc_831CC114"),
        ("sub_831CB460", 0x831CB460, 0x831CC1A0, "loc_831CC1A0"),
        ("sub_831CC260", 0x831CC260, 0x831CCC94, "loc_831CCC94"),
        ("sub_831CC260", 0x831CC260, 0x831CCCBC, "loc_831CCCBC"),
        ("sub_831CC260", 0x831CC260, 0x831CD05C, "loc_831CD05C"),
        ("sub_831CC260", 0x831CC260, 0x831CCCDC, "loc_831CCCDC"),
        ("sub_831CC260", 0x831CC260, 0x831CCD00, "loc_831CCD00"),
        ("sub_831CC260", 0x831CC260, 0x831CCD20, "loc_831CCD20"),
        ("sub_831CC260", 0x831CC260, 0x831CCD44, "loc_831CCD44"),
        ("sub_831CC260", 0x831CC260, 0x831CCD68, "loc_831CCD68"),
        ("sub_831CC260", 0x831CC260, 0x831CCD8C, "loc_831CCD8C"),
        ("sub_831CC260", 0x831CC260, 0x831CCDB0, "loc_831CCDB0"),
        ("sub_831CC260", 0x831CC260, 0x831CCDD4, "loc_831CCDD4"),
        ("sub_831CC260", 0x831CC260, 0x831CCDF8, "loc_831CCDF8"),
        ("sub_831CC260", 0x831CC260, 0x831CCE80, "loc_831CCE80"),
        ("sub_831CC260", 0x831CC260, 0x831CCF24, "loc_831CCF24"),
        ("sub_831CC260", 0x831CC260, 0x831CCFB0, "loc_831CCFB0"),
    ]

    insertions = []
    for function, function_address, target_address, label in target_labels:
        if f"{label}:\n" in lines:
            continue

        start_marker = f"PPC_FUNC_IMPL(__imp__{function}) {{"
        try:
            start = next(i for i, line in enumerate(lines) if start_marker in line)
        except StopIteration:
            print(f"Error: function {function} not found in {path}")
            return False

        instruction_index = (target_address - function_address) // 4
        if target_address < function_address or (target_address - function_address) % 4:
            print(f"Error: invalid target address 0x{target_address:08X}")
            return False

        current_index = 0
        target_line = None
        for i in range(start + 1, len(lines)):
            if lines[i].startswith("__attribute__((alias") and i > start + 1:
                break
            if lines[i].startswith("\t// "):
                if current_index == instruction_index:
                    target_line = i
                    break
                current_index += 1

        if target_line is None:
            print(f"Error: target 0x{target_address:08X} not found in {function}")
            return False
        insertions.append((target_line, f"{label}:\n"))

    for idx, label_line in sorted(insertions, reverse=True):
        lines.insert(idx, label_line)

    text = "".join(lines)
    text = text.replace(old_switch_1, new_switch_1, 1)
    text = text.replace(old_switch_2, new_switch_2, 1)

    with open(path, "w") as f:
        f.write(text)

    print(f"Patched XML parser switch tables in {path}")
    return True


def patch_variant_dispatch(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    old_dispatch = """\t// lis r12,-31966
\tctx.r12.s64 = -2094923776;
\t// rlwinm r0,r10,2,0,29
\tctx.r0.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0xFFFFFFFC;
\t// addi r12,r12,-11428
\tctx.r12.s64 = ctx.r12.s64 + -11428;
\t// lwzx r0,r12,r0
\tctx.r0.u64 = PPC_LOAD_U32(ctx.r12.u32 + ctx.r0.u32);
\t// mtctr r0
\tctr.u64 = ctx.r0.u64;
\t// bctr
\tPPC_CALL_INDIRECT_FUNC(ctr.u32);
\treturn;"""
    new_dispatch = """\tswitch (ctx.r10.u32) {
\tcase 0: goto loc_8321D398;
\tcase 1: goto loc_8321D3A0;
\tcase 2: goto loc_8321D3A8;
\tcase 3: goto loc_8321D3B0;
\tcase 4: goto loc_8321D3E8;
\tcase 5: goto loc_8321D3F0;
\tcase 6: goto loc_8321D3F8;
\tcase 7: goto loc_8321D400;
\tcase 8: goto loc_8321D40C;
\tcase 9: goto loc_8321D424;
\tcase 10: goto loc_8321D42C;
\tcase 11: goto loc_8321D434;
\tcase 12: goto loc_8321D44C;
\tcase 13: goto loc_8321D464;
\tcase 14: goto loc_8321D454;
\tdefault: goto loc_8321D464;
\t}"""

    if new_dispatch in text:
        print(f"Already patched variant dispatch: {path.name}")
        return False
    if old_dispatch not in text:
        print(f"Variant switch dispatch not found in: {path.name}")
        return False

    targets = (
        (0x8321D398, "loc_8321D398"),
        (0x8321D3A0, "loc_8321D3A0"),
        (0x8321D3A8, "loc_8321D3A8"),
        (0x8321D3B0, "loc_8321D3B0"),
        (0x8321D3E8, "loc_8321D3E8"),
        (0x8321D3F0, "loc_8321D3F0"),
        (0x8321D3F8, "loc_8321D3F8"),
        (0x8321D400, "loc_8321D400"),
        (0x8321D40C, "loc_8321D40C"),
        (0x8321D424, "loc_8321D424"),
        (0x8321D42C, "loc_8321D42C"),
        (0x8321D434, "loc_8321D434"),
        (0x8321D44C, "loc_8321D44C"),
        (0x8321D464, "loc_8321D464"),
        (0x8321D454, "loc_8321D454"),
    )
    lines = text.splitlines(keepends=True)
    start_marker = "PPC_FUNC_IMPL(__imp__sub_8321D020) {"
    try:
        start = next(i for i, line in enumerate(lines) if start_marker in line)
    except StopIteration:
        print(f"Function sub_8321D020 not found in {path.name}")
        return False

    insertions = []
    for target_address, label in targets:
        if any(line.strip() == f"{label}:" for line in lines):
            continue

        instruction_index = (target_address - 0x8321D020) // 4
        if target_address < 0x8321D020 or (target_address - 0x8321D020) % 4:
            print(f"Invalid variant target 0x{target_address:08X}")
            return False

        current_index = 0
        target_line = None
        for i in range(start + 1, len(lines)):
            if lines[i].startswith("__attribute__((alias") and i > start + 1:
                break
            if lines[i].startswith("\t// "):
                if current_index == instruction_index:
                    target_line = i
                    break
                current_index += 1
        if target_line is None:
            print(f"Variant target 0x{target_address:08X} not found in {path.name}")
            return False
        insertions.append((target_line, f"{label}:\n"))

    for idx, label_line in sorted(insertions, reverse=True):
        lines.insert(idx, label_line)

    text = "".join(lines).replace(old_dispatch, new_dispatch, 1)
    path.write_text(text, encoding="utf-8")
    print(f"Patched sub_8321D020 variant dispatch in {path.name}")
    return True


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <ppc output directory>", file=sys.stderr)
        return 1
    ppc_dir = pathlib.Path(sys.argv[1])
    path = ppc_dir / "ppc_recomp.263.cpp"
    if not path.exists():
        print(f"File not found: {path}")
        return 1
    patch_file(path)
    patch_variant_dispatch(ppc_dir / "ppc_recomp.269.cpp")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
