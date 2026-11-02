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

    old_switch_1 = old_switch_1.replace("\t// bctr\n", "\t// bctr" + " \n")
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

    old_switch_2 = old_switch_2.replace("\t// bctr\n", "\t// bctr" + " \n")
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
    old_dispatch = old_dispatch.replace("\t// bctr\n", "\t// bctr" + " \n")
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


def patch_mid_function_indirect_target(
    path: pathlib.Path,
    function: str,
    function_address: int,
    target_address: int,
    ctr_register: str,
) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)
    start_marker = f"PPC_FUNC_IMPL(__imp__{function}) {{"
    try:
        start = next(i for i, line in enumerate(lines) if start_marker in line)
    except StopIteration:
        print(f"Function {function} not found in {path.name}")
        return False

    end = next(
        (i for i in range(start + 1, len(lines)) if lines[i].startswith("__attribute__((alias")),
        len(lines),
    )
    label = f"loc_{target_address:08X}"
    if not any(line.strip() == f"{label}:" for line in lines[start:end]):
        instruction_index = (target_address - function_address) // 4
        if target_address < function_address or (target_address - function_address) % 4:
            print(f"Invalid target 0x{target_address:08X}")
            return False

        current_index = 0
        target_line = None
        for i in range(start + 1, end):
            if lines[i].startswith("\t// "):
                if current_index == instruction_index:
                    target_line = i
                    break
                current_index += 1
        if target_line is None:
            print(f"Target 0x{target_address:08X} not found in {function}")
            return False
        lines.insert(target_line, f"{label}:\n")
        end += 1

    guard = f"\tif (ctr.u32 == 0x{target_address:08X}) goto {label};\n"
    dispatch_start = next(
        (
            i for i in range(start + 1, end)
            if lines[i].strip() == f"ctr.u64 = ctx.{ctr_register}.u64;"
            and i + 1 < end
            and lines[i + 1].strip().startswith("// bctr")
            and any(
                lines[j].strip() == "PPC_CALL_INDIRECT_FUNC(ctr.u32);"
                for j in range(i + 2, min(i + 8, end))
            )
        ),
        None,
    )
    if dispatch_start is None:
        if guard in lines[start:end]:
            return True
        print(f"Indirect bctr using {ctr_register} not found in {function}")
        return False

    call_line = next(
        i for i in range(dispatch_start + 2, min(dispatch_start + 8, end))
        if lines[i].strip() == "PPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    if guard not in lines[start:end]:
        lines.insert(call_line, guard)

    path.write_text("".join(lines), encoding="utf-8")
    print(f"Patched indirect target 0x{target_address:08X} in {path.name}")
    return True


def patch_mid_function_indirect_range(
    path: pathlib.Path,
    function: str,
    function_address: int,
    first_target: int,
    target_end: int,
    ctr_register: str,
) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    lines = text.splitlines(keepends=True)
    start_marker = f"PPC_FUNC_IMPL(__imp__{function}) {{"
    try:
        start = next(i for i, line in enumerate(lines) if start_marker in line)
    except StopIteration:
        print(f"Function {function} not found in {path.name}")
        return False

    end = next(
        (i for i in range(start + 1, len(lines)) if lines[i].startswith("__attribute__((alias")),
        len(lines),
    )
    if (
        first_target < function_address
        or target_end <= first_target
        or (first_target - function_address) % 4
        or (target_end - function_address) % 4
    ):
        print(f"Invalid target range 0x{first_target:08X}-0x{target_end:08X}")
        return False

    targets = range(first_target, target_end, 4)
    labels = {line.strip() for line in lines[start:end] if line.strip().startswith("loc_")}
    target_indices = {
        (address - function_address) // 4: address
        for address in targets
    }

    instruction_index = 0
    i = start + 1
    while i < end:
        if lines[i].startswith("\t// "):
            address = target_indices.get(instruction_index)
            if address is not None:
                label = f"loc_{address:08X}:"
                if label not in labels:
                    lines.insert(i, f"{label}\n")
                    labels.add(label)
                    end += 1
                    i += 1
            instruction_index += 1
        i += 1

    switch_start = "\tswitch (ctr.u32) {\n"
    if switch_start in lines[start:end]:
        path.write_text("".join(lines), encoding="utf-8")
        print(f"Already patched indirect target range in {path.name}")
        return True

    dispatch_start = next(
        (
            i for i in range(start + 1, end)
            if lines[i].strip() == f"ctr.u64 = ctx.{ctr_register}.u64;"
            and i + 1 < end
            and lines[i + 1].strip().startswith("// bctr")
            and any(
                lines[j].strip() == "PPC_CALL_INDIRECT_FUNC(ctr.u32);"
                for j in range(i + 2, min(i + 8, end))
            )
        ),
        None,
    )
    if dispatch_start is None:
        print(f"Indirect bctr using {ctr_register} not found in {function}")
        return False

    call_line = next(
        i for i in range(dispatch_start + 2, min(dispatch_start + 8, end))
        if lines[i].strip() == "PPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    dispatch = [switch_start]
    dispatch.extend(
        f"\tcase 0x{address:08X}: goto loc_{address:08X};\n"
        for address in targets
    )
    dispatch.extend(["\tdefault: break;\n", "\t}\n"])
    lines[call_line:call_line] = dispatch

    path.write_text("".join(lines), encoding="utf-8")
    print(
        f"Patched indirect target range 0x{first_target:08X}-"
        f"0x{target_end - 4:08X} in {path.name}"
    )
    return True


def patch_invalid_resource_stream_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    start_marker = "PPC_FUNC_IMPL(__imp__sub_8321CB58) {"
    start = text.find(start_marker)
    if start < 0:
        print(f"Function sub_8321CB58 not found in {path.name}")
        return False
    end = text.find("__attribute__((alias", start + len(start_marker))
    if end < 0:
        end = len(text)

    function_text = text[start:end]
    changed = False
    stream_result_guard = (
        "\tif (ctx.r3.u32 < 0x40000000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 884) {\n"
        "\t\tPPC_STORE_U32(ctx.r27.u32 + 16, 0);\n"
        "\t\tgoto loc_8321CDEC;\n"
        "\t}\n"
    )
    stream_result_point = (
        "\tsub_836B45B8(ctx, base);\n"
        "\t// stw r3,16(r27)\n"
    )
    if stream_result_guard not in function_text:
        if stream_result_point not in function_text:
            print(f"Invalid stream result point not found in {path.name}")
            return False
        function_text = function_text.replace(
            stream_result_point,
            "\tsub_836B45B8(ctx, base);\n" + stream_result_guard + "\t// stw r3,16(r27)\n",
            1,
        )
        changed = True

    guard = (
        "\tif (ctx.r29.u32 < 0x40000000 || ctx.r29.u32 >= PPC_MEMORY_SIZE - 884) {\n"
        "\t\tctx.r28.u64 = 0;\n"
        "\t\tctx.r29.u64 = 0;\n"
        "\t\tgoto loc_8321CCF0;\n"
        "\t}\n"
    )
    if guard not in function_text:
        insertion_point = (
            "\tif (cr6.eq) goto loc_8321CCF0;\n"
            "\t// addi r5,r1,92\n"
        )
        if insertion_point not in function_text:
            print(f"Invalid stream lookup point not found in {path.name}")
            return False
        function_text = function_text.replace(
            insertion_point,
            "\tif (cr6.eq) goto loc_8321CCF0;\n" + guard + "\t// addi r5,r1,92\n",
            1,
        )
        changed = True

    allocation_start = text.find("PPC_FUNC_IMPL(__imp__sub_8321CE00) {")
    if allocation_start < 0:
        print(f"Function sub_8321CE00 not found in {path.name}")
        return False
    allocation_end = text.find("__attribute__((alias", allocation_start)
    if allocation_end < 0:
        allocation_end = len(text)
    allocation_text = text[allocation_start:allocation_end]
    allocation_changed = False
    allocation_guards = (
        (
            "\tsub_8255DC60(ctx, base);\n"
            "\t// mr r27,r3\n",
            "\tif (ctx.r3.u32 == 0) goto loc_8321D00C;\n",
            "\t// mr r27,r3\n",
        ),
        (
            "\tsub_8255DC60(ctx, base);\n"
            "\t// mr r26,r3\n",
            "\tif (ctx.r3.u32 == 0) {\n"
            "\t\tctx.r3.u64 = ctx.r27.u64;\n"
            "\t\tsub_8255DCC0(ctx, base);\n"
            "\t\tgoto loc_8321D00C;\n"
            "\t}\n",
            "\t// mr r26,r3\n",
        ),
    )
    for insertion_point, guard, trailing_point in allocation_guards:
        if guard in allocation_text:
            guard_index = allocation_text.find(guard)
            preceding = allocation_text[:guard_index]
            if not preceding.endswith("\tsub_8255DC60(ctx, base);\n"):
                allocation_text = (
                    preceding + "\tsub_8255DC60(ctx, base);\n"
                    + allocation_text[guard_index:]
                )
                allocation_changed = True
            continue
        if insertion_point not in allocation_text:
            print(f"Invalid allocation result point not found in {path.name}")
            return False
        guarded_point = (
            "\tsub_8255DC60(ctx, base);\n" + guard + trailing_point
        )
        allocation_text = allocation_text.replace(insertion_point, guarded_point, 1)
        allocation_changed = True

    if changed:
        text = text[:start] + function_text + text[end:]
    if allocation_changed:
        allocation_start = text.find("PPC_FUNC_IMPL(__imp__sub_8321CE00) {")
        allocation_end = text.find("__attribute__((alias", allocation_start)
        if allocation_end < 0:
            allocation_end = len(text)
        text = text[:allocation_start] + allocation_text + text[allocation_end:]
        changed = True
    if changed:
        path.write_text(text, encoding="utf-8")
        print(f"Patched invalid resource stream guards in {path.name}")
    return True


def patch_invalid_lookup_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    start_marker = "PPC_FUNC_IMPL(__imp__sub_83215B40) {"
    start = text.find(start_marker)
    if start < 0:
        print(f"Function sub_83215B40 not found in {path.name}")
        return False
    end = text.find("__attribute__((alias", start + len(start_marker))
    if end < 0:
        end = len(text)
    function_text = text[start:end]

    object_guard = (
        "\tif (ctx.r3.u32 < 0x40000000u || ctx.r3.u32 > PPC_MEMORY_SIZE - 16 ||\n"
        "\t\t(ctx.r5.u32 != 0 && (ctx.r5.u32 < 0x10000u || ctx.r5.u32 > PPC_MEMORY_SIZE - 4))) {\n"
        "\t\tctx.r3.u64 = 0;\n"
        "\t\treturn;\n"
        "\t}\n"
    )
    prologue = "\tPPC_FUNC_PROLOGUE();\n"
    if prologue not in function_text:
        print(f"Function prologue not found in sub_83215B40 ({path.name})")
        return False
    if object_guard not in function_text:
        function_text = function_text.replace(prologue, prologue + object_guard, 1)

    array_guard = (
        "\tif (ctx.r9.u32 != 0 && (ctx.r8.u32 < 0x40000000u ||\n"
        "\t\tstatic_cast<uint64_t>(ctx.r8.u32) + static_cast<uint64_t>(ctx.r9.u32) * 4 > PPC_MEMORY_SIZE)) {\n"
        "\t\tctx.r3.u64 = 0;\n"
        "\t\treturn;\n"
        "\t}\n"
    )
    array_pointer_load = "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 8);\n"
    if array_guard not in function_text:
        if array_pointer_load not in function_text:
            print(f"Array pointer load not found in sub_83215B40 ({path.name})")
            return False
        function_text = function_text.replace(
            array_pointer_load, array_pointer_load + array_guard, 1
        )

    if function_text != text[start:end]:
        path.write_text(text[:start] + function_text + text[end:], encoding="utf-8")
        print(f"Patched invalid lookup pointers in {path.name}")
    return True


def patch_allocator_free_list_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    if "#include <cstdio>" not in text:
        text = text.replace(
            '#include "ppc_recomp_shared.h"\n',
            '#include "ppc_recomp_shared.h"\n\n#include <cstdio>\n',
            1,
        )
    guard = (
        "\tif (++freeListIterations > 4096) {\n"
        "\t\tstd::fprintf(stderr, \"[ALLOC] Aborting corrupt free-list walk \"\n"
        "\t\t                    \"allocator=0x%08X node=0x%08X sentinel=0x%08X \"\n"
        "\t\t                    \"request=0x%08X lr=0x%08X\\n\",\n"
        "\t\t             ctx.r30.u32, ctx.r11.u32, ctx.r10.u32, ctx.r5.u32,\n"
        "\t\t             static_cast<uint32_t>(ctx.lr));\n"
        "\t\tPPC_STORE_U32(ctx.r31.u32 + 88, 0);\n"
        "\t\tgoto loc_8364080C;\n"
        "\t}\n"
    )
    changed = False
    for function_name in ("sub_836405A0", "sub_836405A8"):
        start_marker = f"PPC_FUNC_IMPL(__imp__{function_name}) {{"
        start = text.find(start_marker)
        if start < 0:
            print(f"Function {function_name} not found in {path.name}")
            return False
        end = text.find("__attribute__((alias", start + len(start_marker))
        if end < 0:
            end = len(text)

        function_text = text[start:end]
        declaration = "\tuint32_t freeListIterations = 0;\n"
        if declaration not in function_text:
            prologue = "\tPPC_FUNC_PROLOGUE();\n"
            if prologue not in function_text:
                print(f"Allocator prologue not found in {path.name}")
                return False
            function_text = function_text.replace(prologue, prologue + declaration, 1)
        if guard not in function_text:
            old_guard_start = function_text.find(
                "\tif (++freeListIterations > 4096) {"
            )
            if old_guard_start >= 0:
                old_guard_end = function_text.find("\n\t}\n", old_guard_start)
                if old_guard_end < 0:
                    print(f"Malformed allocator guard in {function_name}")
                    return False
                old_guard_end += len("\n\t}\n")
                function_text = (
                    function_text[:old_guard_start]
                    + guard
                    + function_text[old_guard_end:]
                )
        if guard not in function_text:
            function_lines = function_text.splitlines(keepends=True)
            loop_label = next(
                (i for i, line in enumerate(function_lines)
                 if line.strip() == "loc_83640740:"),
                None,
            )
            if loop_label is None:
                print(f"Allocator free-list loop not found in {function_name}")
                return False
            function_lines.insert(loop_label + 1, guard)
            function_text = "".join(function_lines)

        if function_text != text[start:end]:
            text = text[:start] + function_text + text[end:]
            changed = True
    if changed:
        path.write_text(text, encoding="utf-8")
        print(f"Patched allocator free-list guards in {path.name}")
    return True


def patch_allocator_large_free_list_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        print(f"File not found: {path}")
        return False

    text = path.read_text(encoding="utf-8")
    if "#include <cstdio>" not in text:
        text = text.replace(
            '#include "ppc_recomp_shared.h"\n',
            '#include "ppc_recomp_shared.h"\n\n#include <cstdio>\n',
            1,
        )

    start_marker = "PPC_FUNC_IMPL(__imp__sub_8363E870) {"
    start = text.find(start_marker)
    if start < 0:
        print(f"Function sub_8363E870 not found in {path.name}")
        return False
    end = text.find("__attribute__((alias", start + len(start_marker))
    if end < 0:
        end = len(text)
    function_text = text[start:end]

    declaration = "\tuint32_t largeFreeListIterations = 0;\n"
    if declaration not in function_text:
        prologue = "\tPPC_FUNC_PROLOGUE();\n"
        if prologue not in function_text:
            print(f"Allocator prologue not found in sub_8363E870")
            return False
        function_text = function_text.replace(prologue, prologue + declaration, 1)

    guard = (
        "\tif (++largeFreeListIterations > 4096) {\n"
        "\t\tstd::fprintf(stderr, \"[ALLOC] Aborting corrupt large free-list walk \"\n"
        "\t\t                    \"allocator=0x%08X node=0x%08X sentinel=0x%08X \"\n"
        "\t\t                    \"request=0x%08X lr=0x%08X\\n\",\n"
        "\t\t             ctx.r3.u32, ctx.r11.u32, ctx.r9.u32, ctx.r5.u32,\n"
        "\t\t             static_cast<uint32_t>(ctx.lr));\n"
        "\t\tctx.r3.u64 = 0;\n"
        "\t\tgoto loc_8363E9A0;\n"
        "\t}\n"
    )
    if guard not in function_text:
        function_lines = function_text.splitlines(keepends=True)
        loop_label = next(
            (i for i, line in enumerate(function_lines)
             if line.strip() == "loc_8363E940:"),
            None,
        )
        if loop_label is None:
            print(f"Allocator large free-list loop not found in {path.name}")
            return False
        function_lines.insert(loop_label + 1, guard)
        function_text = "".join(function_lines)

    if function_text != text[start:end]:
        path.write_text(text[:start] + function_text + text[end:], encoding="utf-8")
        print(f"Patched allocator large free-list guard in {path.name}")
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
    patch_invalid_resource_stream_guard(ppc_dir / "ppc_recomp.269.cpp")
    patch_invalid_lookup_guard(ppc_dir / "ppc_recomp.268.cpp")
    patch_mid_function_indirect_range(
        ppc_dir / "ppc_recomp.269.cpp",
        "sub_83223C88",
        0x83223C88,
        0x83223EDC,
        0x83224154,
        "r0",
    )
    patch_mid_function_indirect_target(
        ppc_dir / "ppc_recomp.328.cpp",
        "sub_8362B5E0",
        0x8362B5E0,
        0x8362BE00,
        "r12",
    )
    patch_allocator_free_list_guard(ppc_dir / "ppc_recomp.330.cpp")
    patch_allocator_large_free_list_guard(ppc_dir / "ppc_recomp.330.cpp")
    patch_xml_unlink_loop_guard(ppc_dir / "ppc_recomp.263.cpp")
    patch_atmap_insert_guard(ppc_dir / "ppc_recomp.79.cpp")
    patch_recomp297_indirect_guard(ppc_dir / "ppc_recomp.297.cpp")
    patch_recomp263_indirect_guard(ppc_dir / "ppc_recomp.263.cpp")
    patch_recomp263_xml_indirect_guard(ppc_dir / "ppc_recomp.263.cpp")
    patch_recomp269_array_guard(ppc_dir / "ppc_recomp.269.cpp")
    patch_recomp265_stream_guard(ppc_dir / "ppc_recomp.265.cpp")
    return 0

def patch_atmap_insert_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "loc_827E201C:\n"
        "\t// lwz r10,0(r31)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\t// stwx r11,r30,r10\n"
        "\tPPC_STORE_U32(ctx.r30.u32 + ctx.r10.u32, ctx.r11.u32);\n"
        "\t// lwz r9,0(r31)\n"
        "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\t// lwzx r3,r30,r9\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r9.u32);"
    )
    new = (
        "loc_827E201C:\n"
        "\t// lwz r10,0(r31)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\tif (ctx.r10.u32 >= 0x10000 && ctx.r10.u32 < PPC_MEMORY_SIZE - 4) {\n"
        "\t\t// stwx r11,r30,r10\n"
        "\t\tPPC_STORE_U32(ctx.r30.u32 + ctx.r10.u32, ctx.r11.u32);\n"
        "\t}\n"
        "\t// lwz r9,0(r31)\n"
        "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\tif (ctx.r9.u32 >= 0x10000 && ctx.r9.u32 < PPC_MEMORY_SIZE - 4) {\n"
        "\t\t// lwzx r3,r30,r9\n"
        "\t\tctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r9.u32);\n"
        "\t} else {\n"
        "\t\tctx.r3.u64 = ctx.r11.u64;\n"
        "\t}"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched atMap insert guard in {path}")
        return True
    return False

def patch_recomp297_indirect_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// mtctr r8\n"
        "\tctr.u64 = ctx.r8.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x8344D19C;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    new = (
        "\t// mtctr r8\n"
        "\tctr.u64 = ctx.r8.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x8344D19C;\n"
        "\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {\n"
        "\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t}"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.297 indirect guard in {path}")
        return True
    return False

def patch_recomp263_indirect_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// mtctr r10\n"
        "\tctr.u64 = ctx.r10.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x831D0B28;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    new = (
        "\t// mtctr r10\n"
        "\tctr.u64 = ctx.r10.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x831D0B28;\n"
        "\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {\n"
        "\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t}"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.263 indirect guard in {path}")
        return True
    return False

def patch_recomp263_xml_indirect_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// beq cr6,0x831ced8c\n"
        "\tif (cr6.eq) goto loc_831CED8C;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x831CED88;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t// b 0x831ced90\n"
        "\tgoto loc_831CED90;\n"
        "loc_831CED8C:\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x831CED90;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    new = (
        "\t// beq cr6,0x831ced8c\n"
        "\tif (cr6.eq) goto loc_831CED8C;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x831CED88;\n"
        "\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {\n"
        "\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t} else {\n"
        "\t\tctx.r3.u64 = 0;\n"
        "\t}\n"
        "\t// b 0x831ced90\n"
        "\tgoto loc_831CED90;\n"
        "loc_831CED8C:\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x831CED90;\n"
        "\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {\n"
        "\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t} else {\n"
        "\t\tctx.r3.u64 = 0;\n"
        "\t}"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.263 XML indirect guard in {path}")
        return True
    return False

def patch_xml_unlink_loop_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "loc_831D0C60:\n"
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// addi r11,r11,20\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 20;\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r10,r31\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, ctx.r31.u32, ctx.xer);\n"
        "\t// bne cr6,0x831d0c60\n"
        "\tif (!cr6.eq) goto loc_831D0C60;"
    )
    new = (
        "loc_831D0C60:\n"
        "\t// lwz r11,0(r11)\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_831D0C7C;\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 24) goto loc_831D0C7C;\n"
        "\t// addi r11,r11,20\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 20;\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\tif (ctx.r10.u32 == 0) goto loc_831D0C7C;\n"
        "\t// cmplw cr6,r10,r31\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, ctx.r31.u32, ctx.xer);\n"
        "\t// bne cr6,0x831d0c60\n"
        "\tif (!cr6.eq) goto loc_831D0C60;"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched xml unlink loop guard in {path}")
        return True
    return False

def patch_recomp269_array_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// stb r3,3(r30)\n"
        "\tPPC_STORE_U8(ctx.r30.u32 + 3, ctx.r3.u8);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x8321d13c\n"
        "\tif (cr6.eq) goto loc_8321D13C;\n"
        "\t// lwz r11,-4(r10)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + -4);"
    )
    new = (
        "\t// stb r3,3(r30)\n"
        "\tPPC_STORE_U8(ctx.r30.u32 + 3, ctx.r3.u8);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x8321d13c\n"
        "\tif (cr6.eq || ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 4) goto loc_8321D13C;\n"
        "\t// lwz r11,-4(r10)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + -4);"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.269 array guard in {path}")
        return True
    return False

def patch_recomp265_stream_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "loc_831E2F1C:\n"
        "\t// lwz r10,24(r31)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24);\n"
        "\t// subf r11,r10,r11\n"
        "\tctx.r11.s64 = ctx.r11.s64 - ctx.r10.s64;\n"
        "\t// cmpw cr6,r30,r11\n"
        "\tcr6.compare<int32_t>(ctx.r30.s32, ctx.r11.s32, ctx.xer);\n"
        "\t// ble cr6,0x831e2f30\n"
        "\tif (!cr6.gt) goto loc_831E2F30;\n"
        "\t// mr r30,r11\n"
        "\tctx.r30.u64 = ctx.r11.u64;"
    )
    new = (
        "loc_831E2F1C:\n"
        "\t// lwz r10,24(r31)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 24);\n"
        "\t// subf r11,r10,r11\n"
        "\tctx.r11.s64 = ctx.r11.s64 - ctx.r10.s64;\n"
        "\tif (ctx.r11.s32 <= 0) {\n"
        "\t\tctx.r30.u64 = 0;\n"
        "\t\tgoto loc_831E2F30;\n"
        "\t}\n"
        "\t// cmpw cr6,r30,r11\n"
        "\tcr6.compare<int32_t>(ctx.r30.s32, ctx.r11.s32, ctx.xer);\n"
        "\t// ble cr6,0x831e2f30\n"
        "\tif (!cr6.gt) goto loc_831E2F30;\n"
        "\t// mr r30,r11\n"
        "\tctx.r30.u64 = ctx.r11.u64;"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.265 stream guard in {path}")
        return True
    return False

if __name__ == "__main__":
    raise SystemExit(main())
