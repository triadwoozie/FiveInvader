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
    patch_recomp246_task_dispatch(ppc_dir / "ppc_recomp.246.cpp")
    patch_recomp6_atmap_find(ppc_dir / "ppc_recomp.6.cpp")
    patch_recomp269_data_switches(ppc_dir / "ppc_recomp.269.cpp")
    patch_recomp268_schema_guards(ppc_dir / "ppc_recomp.268.cpp")
    patch_recomp77_atmap_guards(ppc_dir / "ppc_recomp.77.cpp")
    patch_recomp47_switch_dispatch(ppc_dir / "ppc_recomp.47.cpp")
    patch_recomp246_event_count_guard(ppc_dir / "ppc_recomp.246.cpp")
    patch_recomp372_handler_lookup_guard(ppc_dir / "ppc_recomp.372.cpp")
    patch_recomp28_pointer_guard(ppc_dir / "ppc_recomp.28.cpp")
    patch_recomp28_switch_dispatch(ppc_dir / "ppc_recomp.28.cpp")
    patch_recomp15_null_guard(ppc_dir / "ppc_recomp.15.cpp")
    patch_recomp245_poll_guard(ppc_dir / "ppc_recomp.245.cpp")
    patch_recomp246_case2_guard(ppc_dir / "ppc_recomp.246.cpp")
    patch_recomp36_refcount_guards(ppc_dir / "ppc_recomp.36.cpp")
    patch_refcount_release_guards(ppc_dir)
    patch_recomp269_array_guards(ppc_dir / "ppc_recomp.269.cpp")
    patch_recomp288_buffer_guard(ppc_dir / "ppc_recomp.288.cpp")
    patch_recomp77_find_guard(ppc_dir / "ppc_recomp.77.cpp")
    patch_recomp330_thread_callback_guard(ppc_dir / "ppc_recomp.330.cpp")
    patch_recomp330_free_list_insert_guards(ppc_dir / "ppc_recomp.330.cpp")
    patch_recomp311_alloc_chunk_loop(ppc_dir / "ppc_recomp.311.cpp")
    patch_recomp265_shader_lookup_guard(ppc_dir / "ppc_recomp.265.cpp")
    patch_recomp18_bugcheck_guards(ppc_dir / "ppc_recomp.18.cpp")
    patch_recomp338_worker_thread_guards(ppc_dir / "ppc_recomp.338.cpp")
    patch_recomp17_queue_guard(ppc_dir / "ppc_recomp.17.cpp")
    patch_recomp209_stream_wait_loop(ppc_dir / "ppc_recomp.209.cpp")
    patch_recomp42_lang_loop(ppc_dir / "ppc_recomp.42.cpp")
    patch_recomp104_packfile_loop(ppc_dir / "ppc_recomp.104.cpp")
    patch_recomp108_packfile_loop(ppc_dir / "ppc_recomp.108.cpp")
    patch_recomp328_lock10_recursion(ppc_dir / "ppc_recomp.328.cpp")
    patch_recomp268_atmap_loop(ppc_dir / "ppc_recomp.268.cpp")
    patch_recomp260_pool_init(ppc_dir / "ppc_recomp.260.cpp")
    patch_recomp27_statemachine_loop(ppc_dir / "ppc_recomp.27.cpp")
    patch_recomp9_queue_guard(ppc_dir / "ppc_recomp.9.cpp")
    patch_recomp3_sub_822438F8_switch(ppc_dir / "ppc_recomp.3.cpp")
    patch_recomp33_sub_8252E5A0_switch(ppc_dir / "ppc_recomp.33.cpp")
    patch_recomp101_sub_8294C6C0_switch(ppc_dir / "ppc_recomp.101.cpp")
    patch_recomp10_event_loop_guard(ppc_dir / "ppc_recomp.10.cpp")
    patch_recomp27_event_loop_guard(ppc_dir / "ppc_recomp.27.cpp")
    patch_recomp29_node_loop_guard(ppc_dir / "ppc_recomp.29.cpp")
    patch_recomp33_sub_8251ED60(ppc_dir / "ppc_recomp.33.cpp")
    patch_recomp8_session_loop_guard(ppc_dir / "ppc_recomp.8.cpp")
    patch_recomp47_sub_826060C0_loop(ppc_dir / "ppc_recomp.47.cpp")
    patch_recomp38_stage_loop_guards(ppc_dir / "ppc_recomp.38.cpp")
    patch_recomp39_stage8_loop_guard(ppc_dir / "ppc_recomp.39.cpp")
    patch_recomp212_sub_82F0F078_loop(ppc_dir / "ppc_recomp.212.cpp")
    patch_recomp51_sub_82626FE8_guard(ppc_dir / "ppc_recomp.51.cpp")
    patch_recomp118_sub_82A13988_guard(ppc_dir / "ppc_recomp.118.cpp")
    patch_recomp31_sub_824F8A48_guard(ppc_dir / "ppc_recomp.31.cpp")
    patch_recomp20_sub_823DF6A8_guard(ppc_dir / "ppc_recomp.20.cpp")
    patch_recomp106_sub_8299BD70_loop(ppc_dir / "ppc_recomp.106.cpp")
    patch_recomp159_sub_82C78900_guard(ppc_dir / "ppc_recomp.159.cpp")
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

def patch_recomp246_task_dispatch(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// lwz r11,244(r1)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 244);\n"
        "\t// cmplwi cr6,r11,4\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 4, ctx.xer);\n"
        "\t// bgt cr6,0x830e0794\n"
        "\tif (cr6.gt) goto loc_830E0794;\n"
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// cmpwi cr6,r11,0\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x830e0878\n"
        "\t// bdzf 4*cr6+eq,0x830e0934\n"
        "\t// bdzf 4*cr6+eq,0x830e0948\n"
        "\t// bne cr6,0x830e0954\n"
        "\tif (!cr6.eq) goto loc_830E0954;"
    )
    new = (
        "\t// lwz r11,244(r1)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 244);\n"
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0:\n"
        "\t\tbreak;\n"
        "\tcase 1:\n"
        "\t\tgoto loc_830E0878;\n"
        "\tcase 2:\n"
        "\t\tgoto loc_830E0934;\n"
        "\tcase 3:\n"
        "\t\tgoto loc_830E0948;\n"
        "\tcase 4:\n"
        "\t\tgoto loc_830E0954;\n"
        "\tdefault:\n"
        "\t\tgoto loc_830E0794;\n"
        "\t}"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.246 task dispatch in {path}")
        return True
    return False

def patch_recomp6_atmap_find(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "PPC_FUNC_IMPL(__imp__sub_82273368) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\t// lhz r11,8(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 8);\n"
        "\t// mr r10,r3\n"
        "\tctx.r10.u64 = ctx.r3.u64;\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x822733b0\n"
        "\tif (cr6.eq) goto loc_822733B0;\n"
        "\t// divwu r9,r4,r11\n"
        "\tif (ctx.r11.u32 != 0) ctx.r9.u32 = ctx.r4.u32 / ctx.r11.u32;\n"
        "\t// lwz r8,4(r3)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);\n"
        "\t// mullw r7,r9,r11\n"
        "\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n"
        "\t// subf r6,r7,r4\n"
        "\tctx.r6.s64 = ctx.r4.s64 - ctx.r7.s64;\n"
        "\t// rlwinm r3,r6,2,0,29\n"
        "\tctx.r3.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r11,r3,r8\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + ctx.r8.u32);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x822733b0\n"
        "\tif (cr6.eq) goto loc_822733B0;\n"
        "loc_82273398:\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r4,r10\n"
        "\tcr6.compare<uint32_t>(ctx.r4.u32, ctx.r10.u32, ctx.xer);\n"
        "\t// beq cr6,0x822733b8\n"
        "\tif (cr6.eq) goto loc_822733B8;\n"
        "\t// lwz r11,8(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x82273398\n"
        "\tif (!cr6.eq) goto loc_82273398;\n"
        "loc_822733B0:\n"
        "\t// mr r3,r5\n"
        "\tctx.r3.u64 = ctx.r5.u64;\n"
        "\t// blr \n"
        "\treturn;\n"
        "loc_822733B8:\n"
        "\t// addi r11,r11,4\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 4;\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x822733b0\n"
        "\tif (cr6.eq) goto loc_822733B0;\n"
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);"
    )
    new = (
        "PPC_FUNC_IMPL(__imp__sub_82273368) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\tuint32_t loop_guard = 0;\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 12) {\n"
        "\t\tctx.r3.u64 = ctx.r5.u64;\n"
        "\t\treturn;\n"
        "\t}\n"
        "\t// lhz r11,8(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 8);\n"
        "\t// mr r10,r3\n"
        "\tctx.r10.u64 = ctx.r3.u64;\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x822733b0\n"
        "\tif (cr6.eq) goto loc_822733B0;\n"
        "\t// divwu r9,r4,r11\n"
        "\tif (ctx.r11.u32 != 0) ctx.r9.u32 = ctx.r4.u32 / ctx.r11.u32;\n"
        "\t// lwz r8,4(r3)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);\n"
        "\t// mullw r7,r9,r11\n"
        "\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n"
        "\t// subf r6,r7,r4\n"
        "\tctx.r6.s64 = ctx.r4.s64 - ctx.r7.s64;\n"
        "\t// rlwinm r3,r6,2,0,29\n"
        "\tctx.r3.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\tif (ctx.r8.u32 < 0x10000 || (ctx.r3.u32 + ctx.r8.u32) >= PPC_MEMORY_SIZE - 4) goto loc_822733B0;\n"
        "\t// lwzx r11,r3,r8\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + ctx.r8.u32);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x822733b0\n"
        "\tif (cr6.eq) goto loc_822733B0;\n"
        "loc_82273398:\n"
        "\tif (++loop_guard > 4096) goto loc_822733B0;\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 12) goto loc_822733B0;\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r4,r10\n"
        "\tcr6.compare<uint32_t>(ctx.r4.u32, ctx.r10.u32, ctx.xer);\n"
        "\t// beq cr6,0x822733b8\n"
        "\tif (cr6.eq) goto loc_822733B8;\n"
        "\t// lwz r11,8(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x82273398\n"
        "\tif (!cr6.eq) goto loc_82273398;\n"
        "loc_822733B0:\n"
        "\t// mr r3,r5\n"
        "\tctx.r3.u64 = ctx.r5.u64;\n"
        "\t// blr \n"
        "\treturn;\n"
        "loc_822733B8:\n"
        "\t// addi r11,r11,4\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 4;\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_822733B0;\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x822733b0\n"
        "\tif (cr6.eq) goto loc_822733B0;\n"
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);"
    )
    if old in text:
        path.write_text(text.replace(old, new))
        print(f"Patched recomp.6 atMap find in {path}")
        return True
    return False

def patch_recomp269_data_switches(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_loop_guard = (
        "loc_83221538:\n"
        "\t// addi r10,r29,-2\n"
    )
    new_loop_guard = (
        "loc_83221538:\n"
        "\tif (ctx.r30.s32 <= 0) goto loc_832217EC;\n"
        "\t// addi r10,r29,-2\n"
    )
    if old_loop_guard in text:
        text = text.replace(old_loop_guard, new_loop_guard)
        patched = True

    old1 = (
        "\t// mtctr r10\n"
        "\tctr.u64 = ctx.r10.u64;\n"
        "\t// cmpwi cr6,r10,0\n"
        "\tcr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x832215a0\n"
        "\t// bdzf 4*cr6+eq,0x832215f0\n"
        "\t// bdzf 4*cr6+eq,0x83221658\n"
        "\t// bdzf 4*cr6+eq,0x832217e4\n"
        "\t// bdzf 4*cr6+eq,0x832217e4\n"
        "\t// bdzf 4*cr6+eq,0x832216d8\n"
        "\t// bne cr6,0x83221760\n"
        "\tif (!cr6.eq) goto loc_83221760;"
    )
    new1 = (
        "\tswitch (ctx.r10.u32) {\n"
        "\tcase 0:\n"
        "\t\tbreak;\n"
        "\tcase 1:\n"
        "\t\tgoto loc_832215A0;\n"
        "\tcase 2:\n"
        "\t\tgoto loc_832215F0;\n"
        "\tcase 3:\n"
        "\t\tgoto loc_83221658;\n"
        "\tcase 4:\n"
        "\tcase 5:\n"
        "\t\tgoto loc_832217E4;\n"
        "\tcase 6:\n"
        "\t\tgoto loc_832216D8;\n"
        "\tcase 7:\n"
        "\t\tgoto loc_83221760;\n"
        "\tdefault:\n"
        "\t\tgoto loc_832217E4;\n"
        "\t}"
    )
    if old1 in text:
        text = text.replace(old1, new1)
        patched = True

    old2 = (
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// cmpwi cr6,r11,0\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x8322709c\n"
        "\t// bdzf 4*cr6+eq,0x8322709c\n"
        "\t// bdzf 4*cr6+eq,0x832270ac\n"
        "\t// bne cr6,0x832270ac\n"
        "\tif (!cr6.eq) goto loc_832270AC;"
    )
    new2 = (
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0:\n"
        "\t\tbreak;\n"
        "\tcase 1:\n"
        "\tcase 2:\n"
        "\t\tgoto loc_8322709C;\n"
        "\tcase 3:\n"
        "\tcase 4:\n"
        "\t\tgoto loc_832270AC;\n"
        "\tdefault:\n"
        "\t\tgoto loc_832270BC;\n"
        "\t}"
    )
    if old2 in text:
        text = text.replace(old2, new2)
        patched = True

    old3 = (
        "\t// mtctr r10\n"
        "\tctr.u64 = ctx.r10.u64;\n"
        "\t// cmpwi cr6,r10,0\n"
        "\tcr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x8322718c\n"
        "\t// bdzf 4*cr6+eq,0x832271e8\n"
        "\t// bdzf 4*cr6+eq,0x83227268\n"
        "\t// bne cr6,0x832272c4\n"
        "\tif (!cr6.eq) goto loc_832272C4;"
    )
    new3 = (
        "\tswitch (ctx.r10.u32) {\n"
        "\tcase 0:\n"
        "\t\tbreak;\n"
        "\tcase 1:\n"
        "\t\tgoto loc_8322718C;\n"
        "\tcase 2:\n"
        "\t\tgoto loc_832271E8;\n"
        "\tcase 3:\n"
        "\t\tgoto loc_83227268;\n"
        "\tcase 4:\n"
        "\t\tgoto loc_832272C4;\n"
        "\tdefault:\n"
        "\t\tgoto loc_83227344;\n"
        "\t}"
    )
    if old3 in text:
        text = text.replace(old3, new3)
        patched = True

    old4 = (
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// cmpwi cr6,r11,0\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x8322d5c8\n"
        "\t// bdzf 4*cr6+eq,0x8322d5f8\n"
        "\t// bne cr6,0x8322d5e4\n"
        "\tif (!cr6.eq) goto loc_8322D5E4;"
    )
    new4 = (
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0:\n"
        "\t\tbreak;\n"
        "\tcase 1:\n"
        "\t\tgoto loc_8322D5C8;\n"
        "\tcase 2:\n"
        "\t\tgoto loc_8322D5F8;\n"
        "\tcase 3:\n"
        "\t\tgoto loc_8322D5E4;\n"
        "\tdefault:\n"
        "\t\tgoto loc_8322D744;\n"
        "\t}"
    )
    if old4 in text:
        text = text.replace(old4, new4)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.269 data switches in {path}")
        return True
    return False

def patch_recomp268_schema_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    t1 = "PPC_FUNC_IMPL(__imp__sub_832193E8) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCCRRegister cr6{};\n"
    r1 = "PPC_FUNC_IMPL(__imp__sub_832193E8) {\n\tPPC_FUNC_PROLOGUE();\n\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 16) return;\n\tPPCCRRegister cr6{};\n"
    if t1 in text:
        text = text.replace(t1, r1)
        patched = True

    t2 = "\t// beq cr6,0x832194c8\n\tif (cr6.eq) goto loc_832194C8;\n\t// addi r4,r3,8"
    r2 = "\t// beq cr6,0x832194c8\n\tif (cr6.eq || PPC_LOAD_U32(ctx.r3.u32 + 24) < 0x10000) goto loc_832194C8;\n\t// addi r4,r3,8"
    if t2 in text:
        text = text.replace(t2, r2)
        patched = True

    t3 = "\t// beq cr6,0x83219500\n\tif (cr6.eq) goto loc_83219500;\n\t// addi r4,r31,8"
    r3 = "\t// beq cr6,0x83219500\n\tif (cr6.eq || PPC_LOAD_U32(ctx.r31.u32 + 32) < 0x10000) goto loc_83219500;\n\t// addi r4,r31,8"
    if t3 in text:
        text = text.replace(t3, r3)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.268 schema guards in {path}")
        return True
    return False

def patch_recomp77_atmap_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old1 = (
        "PPC_FUNC_IMPL(__imp__sub_827BE448) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\t// lis r11,-31802\n"
        "\tctx.r11.s64 = -2084175872;\n"
        "\t// mr r3,r5\n"
        "\tctx.r3.u64 = ctx.r5.u64;\n"
        "\t// lwz r11,-11772(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -11772);\n"
        "\t// lwz r10,16(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);\n"
        "\t// lhz r11,468(r10)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 468);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// divwu r9,r4,r11\n"
        "\tctx.r9.u32 = ctx.r4.u32 / ctx.r11.u32;\n"
        "\t// lwz r8,464(r10)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + 464);\n"
        "\t// mullw r7,r9,r11\n"
        "\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n"
        "\t// subf r6,r7,r4\n"
        "\tctx.r6.s64 = ctx.r4.s64 - ctx.r7.s64;\n"
        "\t// rlwinm r5,r6,2,0,29\n"
        "\tctx.r5.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r11,r5,r8\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r5.u32 + ctx.r8.u32);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "loc_827BE484:\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r4,r10\n"
        "\tcr6.compare<uint32_t>(ctx.r4.u32, ctx.r10.u32, ctx.xer);\n"
        "\t// beq cr6,0x827be4a0\n"
        "\tif (cr6.eq) goto loc_827BE4A0;\n"
        "\t// lwz r11,12(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x827be484\n"
        "\tif (!cr6.eq) goto loc_827BE484;\n"
        "\t// blr \n"
        "\treturn;"
    )
    new1 = (
        "PPC_FUNC_IMPL(__imp__sub_827BE448) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\tuint32_t loop_guard = 0;\n"
        "\t// lis r11,-31802\n"
        "\tctx.r11.s64 = -2084175872;\n"
        "\t// mr r3,r5\n"
        "\tctx.r3.u64 = ctx.r5.u64;\n"
        "\tif ((ctx.r11.u32 + -11772) >= PPC_MEMORY_SIZE - 4) return;\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -11772);\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 20) return;\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);\n"
        "\tif (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 472) return;\n"
        "\t// lhz r11,468(r10)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 468);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// divwu r9,r4,r11\n"
        "\tif (ctx.r11.u32 != 0) ctx.r9.u32 = ctx.r4.u32 / ctx.r11.u32;\n"
        "\t// lwz r8,464(r10)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + 464);\n"
        "\tif (ctx.r8.u32 < 0x10000 || ctx.r8.u32 >= PPC_MEMORY_SIZE - 4) return;\n"
        "\t// mullw r7,r9,r11\n"
        "\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n"
        "\t// subf r6,r7,r4\n"
        "\tctx.r6.s64 = ctx.r4.s64 - ctx.r7.s64;\n"
        "\t// rlwinm r5,r6,2,0,29\n"
        "\tctx.r5.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\tif ((ctx.r5.u32 + ctx.r8.u32) >= PPC_MEMORY_SIZE - 4) return;\n"
        "\t// lwzx r11,r5,r8\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r5.u32 + ctx.r8.u32);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "loc_827BE484:\n"
        "\tif (++loop_guard > 4096) return;\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 16) return;\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r4,r10\n"
        "\tcr6.compare<uint32_t>(ctx.r4.u32, ctx.r10.u32, ctx.xer);\n"
        "\t// beq cr6,0x827be4a0\n"
        "\tif (cr6.eq) goto loc_827BE4A0;\n"
        "\t// lwz r11,12(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x827be484\n"
        "\tif (!cr6.eq) goto loc_827BE484;\n"
        "\t// blr \n"
        "\treturn;"
    )
    if old1 in text:
        text = text.replace(old1, new1)
        patched = True

    old2 = (
        "PPC_FUNC_IMPL(__imp__sub_827BE4C0) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\t// lis r11,-31802\n"
        "\tctx.r11.s64 = -2084175872;\n"
        "\t// lwz r11,-11772(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -11772);\n"
        "\t// lwz r10,16(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);\n"
        "\t// lhz r11,468(r10)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 468);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// divwu r9,r3,r11\n"
        "\tctx.r9.u32 = ctx.r3.u32 / ctx.r11.u32;\n"
        "\t// lwz r8,464(r10)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + 464);\n"
        "\t// mullw r7,r9,r11\n"
        "\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n"
        "\t// subf r6,r7,r3\n"
        "\tctx.r6.s64 = ctx.r3.s64 - ctx.r7.s64;\n"
        "\t// rlwinm r11,r6,2,0,29\n"
        "\tctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r11,r11,r8\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "loc_827BE4F8:\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r3,r10\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, ctx.r10.u32, ctx.xer);\n"
        "\t// beq cr6,0x827be514\n"
        "\tif (cr6.eq) goto loc_827BE514;\n"
        "\t// lwz r11,12(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x827be4f8\n"
        "\tif (!cr6.eq) goto loc_827BE4F8;\n"
        "\t// blr \n"
        "\treturn;"
    )
    new2 = (
        "PPC_FUNC_IMPL(__imp__sub_827BE4C0) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\tuint32_t loop_guard = 0;\n"
        "\t// lis r11,-31802\n"
        "\tctx.r11.s64 = -2084175872;\n"
        "\tif ((ctx.r11.u32 + -11772) >= PPC_MEMORY_SIZE - 4) return;\n"
        "\t// lwz r11,-11772(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -11772);\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 20) return;\n"
        "\t// lwz r10,16(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);\n"
        "\tif (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 472) return;\n"
        "\t// lhz r11,468(r10)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 468);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// divwu r9,r3,r11\n"
        "\tif (ctx.r11.u32 != 0) ctx.r9.u32 = ctx.r3.u32 / ctx.r11.u32;\n"
        "\t// lwz r8,464(r10)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + 464);\n"
        "\tif (ctx.r8.u32 < 0x10000 || ctx.r8.u32 >= PPC_MEMORY_SIZE - 4) return;\n"
        "\t// mullw r7,r9,r11\n"
        "\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n"
        "\t// subf r6,r7,r3\n"
        "\tctx.r6.s64 = ctx.r3.s64 - ctx.r7.s64;\n"
        "\t// rlwinm r11,r6,2,0,29\n"
        "\tctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\tif ((ctx.r11.u32 + ctx.r8.u32) >= PPC_MEMORY_SIZE - 4) return;\n"
        "\t// lwzx r11,r11,r8\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "loc_827BE4F8:\n"
        "\tif (++loop_guard > 4096) return;\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 16) return;\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplw cr6,r3,r10\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, ctx.r10.u32, ctx.xer);\n"
        "\t// beq cr6,0x827be514\n"
        "\tif (cr6.eq) goto loc_827BE514;\n"
        "\t// lwz r11,12(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x827be4f8\n"
        "\tif (!cr6.eq) goto loc_827BE4F8;\n"
        "\t// blr \n"
        "\treturn;"
    )
    if old2 in text:
        text = text.replace(old2, new2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.77 atMap guards in {path}")
        return True
    return False

def patch_recomp47_switch_dispatch(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// cmpwi cr6,r11,0\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x825fea00\n"
        "\t// bdzf 4*cr6+eq,0x825fe994\n"
        "\t// bdzf 4*cr6+eq,0x825fe994\n"
        "\t// bdzf 4*cr6+eq,0x825fea00\n"
        "\t// bdzf 4*cr6+eq,0x825fea00\n"
        "\t// bne cr6,0x825fe9c8\n"
        "\tif (!cr6.eq) goto loc_825FE9C8;"
    )
    new = (
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0:\n"
        "\t\tbreak;\n"
        "\tcase 1:\n"
        "\t\tgoto loc_825FEA00;\n"
        "\tcase 2:\n"
        "\tcase 3:\n"
        "\t\tgoto loc_825FE994;\n"
        "\tcase 4:\n"
        "\tcase 5:\n"
        "\t\tgoto loc_825FEA00;\n"
        "\tdefault:\n"
        "\t\tgoto loc_825FE9C8;\n"
        "\t}"
    )
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.47 switch dispatch in {path}")
        return True
    return False

def patch_recomp246_event_count_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// lwz r30,56(r31)\n"
        "\tctx.r30.u64 = PPC_LOAD_U32(ctx.r31.u32 + 56);\n"
        "\t// mr r29,r3\n"
        "\tctx.r29.u64 = ctx.r3.u64;\n"
        "\t// li r31,0\n"
        "\tctx.r31.s64 = 0;\n"
        "\t// cmplwi cr6,r30,0\n"
        "\tcr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x830db3c8\n"
        "\tif (cr6.eq) goto loc_830DB3C8;"
    )
    new = (
        "\t// lwz r30,56(r31)\n"
        "\tctx.r30.u64 = PPC_LOAD_U32(ctx.r31.u32 + 56);\n"
        "\tif (ctx.r30.u32 > 4096) goto loc_830DB3C8;\n"
        "\t// mr r29,r3\n"
        "\tctx.r29.u64 = ctx.r3.u64;\n"
        "\t// li r31,0\n"
        "\tctx.r31.s64 = 0;\n"
        "\t// cmplwi cr6,r30,0\n"
        "\tcr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x830db3c8\n"
        "\tif (cr6.eq) goto loc_830DB3C8;"
    )
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.246 event count guard in {path}")
        return True
    return False

def patch_recomp372_handler_lookup_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "PPC_FUNC_IMPL(__imp__sub_83757BF0) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCRegister ctr{};\n"
        "\tPPCCRRegister cr6{};\n"
        "\t// mr r6,r4\n"
        "\tctx.r6.u64 = ctx.r4.u64;\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// lwz r7,296(r3)\n"
        "\tctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 296);\n"
        "\t// cmplwi cr6,r7,0\n"
        "\tcr6.compare<uint32_t>(ctx.r7.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// li r10,0\n"
        "\tctx.r10.s64 = 0;\n"
        "\t// cmpwi cr6,r7,0\n"
        "\tcr6.compare<int32_t>(ctx.r7.s32, 0, ctx.xer);\n"
        "\t// blelr cr6\n"
        "\tif (!cr6.gt) return;\n"
        "\t// lwz r9,252(r3)\n"
        "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 252);\n"
        "\t// li r11,0\n"
        "\tctx.r11.s64 = 0;\n"
        "\t// lwz r8,376(r3)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 376);\n"
        "loc_83757C20:\n"
        "\t// lwzx r4,r11,r9\n"
        "\tctx.r4.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r9.u32);\n"
        "\t// rlwinm r4,r4,2,0,29\n"
        "\tctx.r4.u64 = __builtin_rotateleft64(ctx.r4.u32 | (ctx.r4.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r4,r4,r8\n"
        "\tctx.r4.u64 = PPC_LOAD_U32(ctx.r4.u32 + ctx.r8.u32);\n"
        "\t// cmpw cr6,r4,r6\n"
        "\tcr6.compare<int32_t>(ctx.r4.s32, ctx.r6.s32, ctx.xer);\n"
        "\t// beq cr6,0x83757c48\n"
        "\tif (cr6.eq) goto loc_83757C48;\n"
        "\t// addi r10,r10,1\n"
        "\tctx.r10.s64 = ctx.r10.s64 + 1;\n"
        "\t// addi r11,r11,4\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 4;\n"
        "\t// cmpw cr6,r10,r7\n"
        "\tcr6.compare<int32_t>(ctx.r10.s32, ctx.r7.s32, ctx.xer);\n"
        "\t// blt cr6,0x83757c20\n"
        "\tif (cr6.lt) goto loc_83757C20;\n"
        "\t// blr \n"
        "\treturn;"
    )
    new = (
        "PPC_FUNC_IMPL(__imp__sub_83757BF0) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCRegister ctr{};\n"
        "\tPPCCRRegister cr6{};\n"
        "\tuint32_t loop_guard = 0;\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 380) return;\n"
        "\t// mr r6,r4\n"
        "\tctx.r6.u64 = ctx.r4.u64;\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// lwz r7,296(r3)\n"
        "\tctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 296);\n"
        "\tif (ctx.r7.u32 > 4096) return;\n"
        "\t// cmplwi cr6,r7,0\n"
        "\tcr6.compare<uint32_t>(ctx.r7.u32, 0, ctx.xer);\n"
        "\t// beqlr cr6\n"
        "\tif (cr6.eq) return;\n"
        "\t// li r10,0\n"
        "\tctx.r10.s64 = 0;\n"
        "\t// cmpwi cr6,r7,0\n"
        "\tcr6.compare<int32_t>(ctx.r7.s32, 0, ctx.xer);\n"
        "\t// blelr cr6\n"
        "\tif (!cr6.gt) return;\n"
        "\t// lwz r9,252(r3)\n"
        "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 252);\n"
        "\tif (ctx.r9.u32 < 0x10000 || ctx.r9.u32 >= PPC_MEMORY_SIZE - 4) return;\n"
        "\t// li r11,0\n"
        "\tctx.r11.s64 = 0;\n"
        "\t// lwz r8,376(r3)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r3.u32 + 376);\n"
        "\tif (ctx.r8.u32 < 0x10000 || ctx.r8.u32 >= PPC_MEMORY_SIZE - 4) return;\n"
        "loc_83757C20:\n"
        "\tif (++loop_guard > 4096) return;\n"
        "\t// lwzx r4,r11,r9\n"
        "\tctx.r4.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r9.u32);\n"
        "\t// rlwinm r4,r4,2,0,29\n"
        "\tctx.r4.u64 = __builtin_rotateleft64(ctx.r4.u32 | (ctx.r4.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r4,r4,r8\n"
        "\tctx.r4.u64 = PPC_LOAD_U32(ctx.r4.u32 + ctx.r8.u32);\n"
        "\t// cmpw cr6,r4,r6\n"
        "\tcr6.compare<int32_t>(ctx.r4.s32, ctx.r6.s32, ctx.xer);\n"
        "\t// beq cr6,0x83757c48\n"
        "\tif (cr6.eq) goto loc_83757C48;\n"
        "\t// addi r10,r10,1\n"
        "\tctx.r10.s64 = ctx.r10.s64 + 1;\n"
        "\t// addi r11,r11,4\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 4;\n"
        "\t// cmpw cr6,r10,r7\n"
        "\tcr6.compare<int32_t>(ctx.r10.s32, ctx.r7.s32, ctx.xer);\n"
        "\t// blt cr6,0x83757c20\n"
        "\tif (cr6.lt) goto loc_83757C20;\n"
        "\t// blr \n"
        "\treturn;"
    )
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.372 handler lookup guard in {path}")
        return True
    return False

def patch_recomp28_pointer_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False
    old = (
        "\t// mr r29,r3\n"
        "\tctx.r29.u64 = ctx.r3.u64;\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x824c7aa4\n"
        "\tif (cr6.eq) goto loc_824C7AA4;"
    )
    new = (
        "\t// mr r29,r3\n"
        "\tctx.r29.u64 = ctx.r3.u64;\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x824c7aa4\n"
        "\tif (cr6.eq || ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 8) goto loc_824C7AA4;"
    )
    if old in text:
        text = text.replace(old, new)
        patched = True

    old_listener = (
        '\t// lwz r3,4(r31)\n'
        '\tctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);\n'
        '\t// mr r4,r26\n'
        '\tctx.r4.u64 = ctx.r26.u64;\n'
        '\t// lwz r10,0(r3)\n'
        '\tctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n'
        '\t// lwz r9,12(r10)\n'
        '\tctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 12);\n'
        '\t// mtctr r9\n'
        '\tctr.u64 = ctx.r9.u64;\n'
        '\t// bctrl \n'
        '\tctx.lr = 0x824C2B34;\n'
        '\tPPC_CALL_INDIRECT_FUNC(ctr.u32);'
    )
    new_listener = (
        '\t// lwz r3,4(r31)\n'
        '\tctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);\n'
        '\t// mr r4,r26\n'
        '\tctx.r4.u64 = ctx.r26.u64;\n'
        '\tif (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 4) {\n'
        '\t\t// lwz r10,0(r3)\n'
        '\t\tctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n'
        '\t\tif (ctx.r10.u32 >= 0x10000 && ctx.r10.u32 < PPC_MEMORY_SIZE - 16) {\n'
        '\t\t\t// lwz r9,12(r10)\n'
        '\t\t\tctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 12);\n'
        '\t\t\t// mtctr r9\n'
        '\t\t\tctr.u64 = ctx.r9.u64;\n'
        '\t\t\t// bctrl \n'
        '\t\t\tctx.lr = 0x824C2B34;\n'
        '\t\t\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < PPC_CODE_BASE + PPC_CODE_SIZE) {\n'
        '\t\t\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n'
        '\t\t\t}\n'
        '\t\t}\n'
        '\t}'
    )
    if old_listener in text:
        text = text.replace(old_listener, new_listener)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.28 pointer guard in {path}")
        return True
    return False

def patch_recomp28_switch_dispatch(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "\t// lis r12,-32180\n"
        "\tctx.r12.s64 = -2108948480;\n"
        "\t// rlwinm r0,r11,2,0,29\n"
        "\tctx.r0.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// addi r12,r12,31268\n"
        "\tctx.r12.s64 = ctx.r12.s64 + 31268;\n"
        "\t// lwzx r0,r12,r0\n"
        "\tctx.r0.u64 = PPC_LOAD_U32(ctx.r12.u32 + ctx.r0.u32);\n"
        "\t// mtctr r0\n"
        "\tctr.u64 = ctx.r0.u64;\n"
        "\t// bctr \n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\treturn;\n"
        "\t// lwz r18,31320(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31320);\n"
        "\t// lwz r18,31332(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31332);\n"
        "\t// lwz r18,31340(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31340);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31348(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31348);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31368(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31368);\n"
        "\t// lwz r18,31356(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31356);\n"
        "\t// lwz r18,31364(r12)\n"
        "\tctx.r18.u64 = PPC_LOAD_U32(ctx.r12.u32 + 31364);\n"
        "\t// li r4,1\n"
        "\tctx.r4.s64 = 1;\n"
        "\t// bl 0x82556bd0\n"
        "\tctx.lr = 0x824C7A60;\n"
        "\tsub_82556BD0(ctx, base);\n"
        "\t// b 0x824c7a88\n"
        "\tgoto loc_824C7A88;\n"
        "\t// bl 0x83333598\n"
        "\tctx.lr = 0x824C7A68;\n"
        "\tsub_83333598(ctx, base);\n"
        "\t// b 0x824c7a88\n"
        "\tgoto loc_824C7A88;\n"
        "\t// bl 0x82ac3fb0\n"
        "\tctx.lr = 0x824C7A70;\n"
        "\tsub_82AC3FB0(ctx, base);\n"
        "\t// b 0x824c7a88\n"
        "\tgoto loc_824C7A88;\n"
        "\t// bl 0x82966730\n"
        "\tctx.lr = 0x824C7A78;\n"
        "\tsub_82966730(ctx, base);\n"
        "\t// b 0x824c7a88\n"
        "\tgoto loc_824C7A88;\n"
        "\t// bl 0x82ac3fb0\n"
        "\tctx.lr = 0x824C7A80;\n"
        "\tsub_82AC3FB0(ctx, base);\n"
        "\t// b 0x824c7a88\n"
        "\tgoto loc_824C7A88;\n"
        "\t// bl 0x822509b0\n"
        "\tctx.lr = 0x824C7A88;\n"
        "\tsub_822509B0(ctx, base);"
    )
    new = (
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0:\n"
        "\t\t// li r4,1\n"
        "\t\tctx.r4.s64 = 1;\n"
        "\t\t// bl 0x82556bd0\n"
        "\t\tctx.lr = 0x824C7A60;\n"
        "\t\tsub_82556BD0(ctx, base);\n"
        "\t\tgoto loc_824C7A88;\n"
        "\tcase 1:\n"
        "\t\t// bl 0x83333598\n"
        "\t\tctx.lr = 0x824C7A68;\n"
        "\t\tsub_83333598(ctx, base);\n"
        "\t\tgoto loc_824C7A88;\n"
        "\tcase 2:\n"
        "\t\t// bl 0x82ac3fb0\n"
        "\t\tctx.lr = 0x824C7A70;\n"
        "\t\tsub_82AC3FB0(ctx, base);\n"
        "\t\tgoto loc_824C7A88;\n"
        "\tcase 4:\n"
        "\t\t// bl 0x82966730\n"
        "\t\tctx.lr = 0x824C7A78;\n"
        "\t\tsub_82966730(ctx, base);\n"
        "\t\tgoto loc_824C7A88;\n"
        "\tcase 11:\n"
        "\t\t// bl 0x82ac3fb0\n"
        "\t\tctx.lr = 0x824C7A80;\n"
        "\t\tsub_82AC3FB0(ctx, base);\n"
        "\t\tgoto loc_824C7A88;\n"
        "\tcase 12:\n"
        "\t\t// bl 0x822509b0\n"
        "\t\tctx.lr = 0x824C7A88;\n"
        "\t\tsub_822509B0(ctx, base);\n"
        "\t\tgoto loc_824C7A88;\n"
        "\tdefault:\n"
        "\t\tgoto loc_824C7A88;\n"
        "\t}"
    )
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.28 switch dispatch in {path}")
        return True
    return False

def patch_recomp15_null_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "__attribute__((alias(\"__imp__sub_82360250\"))) PPC_WEAK_FUNC(sub_82360250);\n"
        "PPC_FUNC_IMPL(__imp__sub_82360250) {\n"
        "\tPPC_FUNC_PROLOGUE();"
    )
    new = (
        "__attribute__((alias(\"__imp__sub_82360250\"))) PPC_WEAK_FUNC(sub_82360250);\n"
        "PPC_FUNC_IMPL(__imp__sub_82360250) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 32) return;"
    )
    patched = False
    if old in text:
        text = text.replace(old, new)
        patched = True

    old_loop = (
        "\t// mr r11,r10\n"
        "\tctx.r11.u64 = ctx.r10.u64;\n"
        "\t// lwz r10,0(r10)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r10.u32 + 0);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x8236029c\n"
        "\tif (!cr6.eq) goto loc_8236029C;\n"
        "loc_823602D8:\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82360394\n"
        "\tif (cr6.eq) goto loc_82360394;\n"
        "loc_823602E4:\n"
        "\t// lwz r30,0(r11)\n"
        "\tctx.r30.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);"
    )
    new_loop = (
        "\t// mr r11,r10\n"
        "\tctx.r11.u64 = ctx.r10.u64;\n"
        "\tif (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 4) goto loc_82360394;\n"
        "\t// lwz r10,0(r10)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r10.u32 + 0);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x8236029c\n"
        "\tif (!cr6.eq) goto loc_8236029C;\n"
        "loc_823602D8:\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_82360394;\n"
        "\t// lwz r10,0(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82360394\n"
        "\tif (cr6.eq) goto loc_82360394;\n"
        "loc_823602E4:\n"
        "\t// lwz r30,0(r11)\n"
        "\tctx.r30.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\tif (ctx.r30.u32 < 0x10000 || ctx.r30.u32 >= PPC_MEMORY_SIZE - 4) goto loc_82360394;"
    )
    if old_loop in text:
        text = text.replace(old_loop, new_loop)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.15 null guard in {path}")
        return True
    return False

def patch_recomp245_poll_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "loc_830D6CF4:\n"
        "\t// li r3,10\n"
        "\tctx.r3.s64 = 10;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x830D6CFC;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// lbz r11,100(r31)\n"
        "\tctx.r11.u64 = PPC_LOAD_U8(ctx.r31.u32 + 100);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x830d6cf4\n"
        "\tif (cr6.eq) goto loc_830D6CF4;"
    )
    prev_new = (
        "loc_830D6CF4:\n"
        "\t{\n"
        "\t\tstatic int s_pollCount = 0;\n"
        "\t\tif (++s_pollCount <= 5 || s_pollCount % 50 == 0) {\n"
        "\t\t\tfprintf(stderr, \"[sub_830D6CD0] Polling *(0x%08X + 100) == %u (poll #%d)\\n\", ctx.r31.u32, PPC_LOAD_U8(ctx.r31.u32 + 100), s_pollCount);\n"
        "\t\t}\n"
        "\t\t// li r3,10\n"
        "\t\tctx.r3.s64 = 10;\n"
        "\t\t// bl 0x82548de0\n"
        "\t\tctx.lr = 0x830D6CFC;\n"
        "\t\tsub_82548DE0(ctx, base);\n"
        "\t\t// lbz r11,100(r31)\n"
        "\t\tctx.r11.u64 = PPC_LOAD_U8(ctx.r31.u32 + 100);\n"
        "\t\tif (s_pollCount >= 10 && ctx.r11.u32 == 0) {\n"
        "\t\t\tfprintf(stderr, \"[sub_830D6CD0] Worker task 2 timeout after %d polls! Executing sub_83757A28 fallback on main thread!\\n\", s_pollCount);\n"
        "\t\t\tPPCContext subCtx = ctx;\n"
        "\t\t\tsubCtx.r4.s64 = 0;\n"
        "\t\t\tsubCtx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\t\t\tsub_83757A28(subCtx, base);\n"
        "\t\t\tPPC_STORE_U8(ctx.r31.u32 + 100, 1);\n"
        "\t\t\tctx.r11.u64 = 1;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x830d6cf4\n"
        "\tif (cr6.eq) goto loc_830D6CF4;"
    )
    clean_new = (
        "loc_830D6CF4:\n"
        "\t{\n"
        "\t\tstatic int s_pollCount = 0;\n"
        "\t\tif (++s_pollCount <= 5 || s_pollCount % 100 == 0) {\n"
        "\t\t\tfprintf(stderr, \"[sub_830D6CD0] Polling *(0x%08X + 100) == %u (poll #%d)\\n\", ctx.r31.u32, PPC_LOAD_U8(ctx.r31.u32 + 100), s_pollCount);\n"
        "\t\t}\n"
        "\t\t// li r3,10\n"
        "\t\tctx.r3.s64 = 10;\n"
        "\t\t// bl 0x82548de0\n"
        "\t\tctx.lr = 0x830D6CFC;\n"
        "\t\tsub_82548DE0(ctx, base);\n"
        "\t\t// lbz r11,100(r31)\n"
        "\t\tctx.r11.u64 = PPC_LOAD_U8(ctx.r31.u32 + 100);\n"
        "\t}\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x830d6cf4\n"
        "\tif (cr6.eq) goto loc_830D6CF4;"
    )
    if prev_new in text:
        text = text.replace(prev_new, clean_new)
        path.write_text(text)
        print(f"Updated recomp.245 poll guard (clean poll) in {path}")
        return True
    elif old in text:
        text = text.replace(old, clean_new)
        path.write_text(text)
        print(f"Patched recomp.245 poll guard in {path}")
        return True
    return False

def patch_recomp246_case2_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        "loc_830E0934:\n"
        "\t// li r4,0\n"
        "\tctx.r4.s64 = 0;\n"
        "\t// lwz r3,0(r31)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\t// bl 0x83757a28\n"
        "\tctx.lr = 0x830E0940;\n"
        "\tsub_83757A28(ctx, base);\n"
        "\t// stb r29,100(r31)\n"
        "\tPPC_STORE_U8(ctx.r31.u32 + 100, ctx.r29.u8);\n"
        "\t// b 0x830e0794\n"
        "\tgoto loc_830E0794;"
    )
    new = (
        "loc_830E0934:\n"
        "\tif (PPC_LOAD_U8(ctx.r31.u32 + 100) != 0) goto loc_830E0794;\n"
        "\t// li r4,0\n"
        "\tctx.r4.s64 = 0;\n"
        "\t// lwz r3,0(r31)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\t// bl 0x83757a28\n"
        "\tctx.lr = 0x830E0940;\n"
        "\tsub_83757A28(ctx, base);\n"
        "\t// stb r29,100(r31)\n"
        "\tPPC_STORE_U8(ctx.r31.u32 + 100, ctx.r29.u8);\n"
        "\t// b 0x830e0794\n"
        "\tgoto loc_830E0794;"
    )
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.246 case 2 guard in {path}")
        return True
    return False

def patch_recomp36_refcount_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_bd0 = (
        "__attribute__((alias(\"__imp__sub_82556BD0\"))) PPC_WEAK_FUNC(sub_82556BD0);\n"
        "PPC_FUNC_IMPL(__imp__sub_82556BD0) {\n"
        "\tPPC_FUNC_PROLOGUE();"
    )
    new_bd0 = (
        "__attribute__((alias(\"__imp__sub_82556BD0\"))) PPC_WEAK_FUNC(sub_82556BD0);\n"
        "PPC_FUNC_IMPL(__imp__sub_82556BD0) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 40) return;"
    )
    if old_bd0 in text:
        text = text.replace(old_bd0, new_bd0)
        patched = True

    old_c40 = (
        "loc_82556C40:\n"
        "\t// cmplwi cr6,r30,0\n"
        "\tcr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82556c50\n"
        "\tif (cr6.eq) goto loc_82556C50;"
    )
    new_c40 = (
        "loc_82556C40:\n"
        "\t// cmplwi cr6,r30,0\n"
        "\tcr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82556c50\n"
        "\tif (cr6.eq || ctx.r30.u32 < 0x10000 || ctx.r30.u32 >= PPC_MEMORY_SIZE - 16) goto loc_82556C50;"
    )
    if old_c40 in text:
        text = text.replace(old_c40, new_c40)
        patched = True

    old_c70 = (
        "__attribute__((alias(\"__imp__sub_82556C70\"))) PPC_WEAK_FUNC(sub_82556C70);\n"
        "PPC_FUNC_IMPL(__imp__sub_82556C70) {\n"
        "\tPPC_FUNC_PROLOGUE();"
    )
    new_c70 = (
        "__attribute__((alias(\"__imp__sub_82556C70\"))) PPC_WEAK_FUNC(sub_82556C70);\n"
        "PPC_FUNC_IMPL(__imp__sub_82556C70) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 16) return;"
    )
    if old_c70 in text:
        text = text.replace(old_c70, new_c70)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.36 refcount guards in {path}")
        return True
    return False

def patch_refcount_release_guards(ppc_dir: pathlib.Path) -> bool:
    # Also patch recomp.28 switch guard directly if present
    recomp28 = ppc_dir / "ppc_recomp.28.cpp"
    if recomp28.exists():
        r28_text = recomp28.read_text()
        old_sw = "\tswitch (ctx.r11.u32) {"
        new_sw = "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 256) goto loc_824C7A88;\n\tswitch (ctx.r11.u32) {"
        if old_sw in r28_text and new_sw not in r28_text:
            r28_text = r28_text.replace(old_sw, new_sw, 1)

        old_entry = "\tif (cr6.eq || ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 8) goto loc_824C7AA4;"
        new_entry = "\tif (cr6.eq || ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= 0xE0000000) goto loc_824C7AA4;"
        if old_entry in r28_text:
            r28_text = r28_text.replace(old_entry, new_entry)

        old_loop = "loc_824C79C8:\n\t// lwz r3,0(r31)"
        new_loop = "loc_824C79C8:\n\tif (ctx.r31.u32 < 0x10000 || ctx.r31.u32 >= 0xE0000000) goto loc_824C7AA4;\n\t// lwz r3,0(r31)"
        if old_loop in r28_text:
            r28_text = r28_text.replace(old_loop, new_loop)

        old_8c0 = "PPC_FUNC_IMPL(__imp__sub_824C78C0) {\n\tPPC_FUNC_PROLOGUE();\n\tPPCCRRegister cr0{};"
        new_8c0 = "PPC_FUNC_IMPL(__imp__sub_824C78C0) {\n\tPPC_FUNC_PROLOGUE();\n\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= 0xE0000000) return;\n\tPPCCRRegister cr0{};"
        if old_8c0 in r28_text:
            r28_text = r28_text.replace(old_8c0, new_8c0)

        old_904 = "loc_824C7904:\n\t// lwzu r3,4(r29)"
        new_904 = "loc_824C7904:\n\tif (ctx.r29.u32 < 0x10000 || ctx.r29.u32 >= 0xE0000000 - 4 || ctx.r31.u32 > 1024) goto loc_824C7914;\n\t// lwzu r3,4(r29)"
        if old_904 in r28_text:
            r28_text = r28_text.replace(old_904, new_904)

        old_888 = "loc_824C7888:\n\t// lwzu r3,4(r30)"
        new_888 = "loc_824C7888:\n\tif (ctx.r30.u32 < 0x10000 || ctx.r30.u32 >= 0xE0000000 - 4 || ctx.r31.u32 > 1024) goto loc_824C7898;\n\t// lwzu r3,4(r30)"
        if old_888 in r28_text:
            r28_text = r28_text.replace(old_888, new_888)

        recomp28.write_text(r28_text)
        print("Patched switch r3 bounds guard in ppc_recomp.28.cpp")

    targets = [
        (ppc_dir / "ppc_recomp.129.cpp", "sub_82AC3FB0"),
        (ppc_dir / "ppc_recomp.286.cpp", "sub_83333598"),
        (ppc_dir / "ppc_recomp.102.cpp", "sub_82966730"),
        (ppc_dir / "ppc_recomp.4.cpp", "sub_822509B0"),
    ]
    for path, func_name in targets:
        if not path.exists():
            continue
        text = path.read_text()
        old = (
            f"__attribute__((alias(\"__imp__{func_name}\"))) PPC_WEAK_FUNC({func_name});\n"
            f"PPC_FUNC_IMPL(__imp__{func_name}) {{\n"
            f"\tPPC_FUNC_PROLOGUE();"
        )
        new = (
            f"__attribute__((alias(\"__imp__{func_name}\"))) PPC_WEAK_FUNC({func_name});\n"
            f"PPC_FUNC_IMPL(__imp__{func_name}) {{\n"
            f"\tPPC_FUNC_PROLOGUE();\n"
            f"\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 256) {{ ctx.r3.s64 = 0; return; }}"
        )
        if old in text:
            text = text.replace(old, new)
            path.write_text(text)
            print(f"Patched refcount release guard in {func_name} ({path.name})")
    return True

def patch_recomp269_array_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_db78 = (
        "\t// lwz r8,0(r31)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\t// li r11,0\n"
        "\tctx.r11.s64 = 0;\n"
        "loc_8321DBB0:"
    )
    new_db78 = (
        "\t// lwz r8,0(r31)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\tif (ctx.r8.u32 < 0x10000 || ctx.r8.u32 >= PPC_MEMORY_SIZE - 16) goto loc_8321DBCC;\n"
        "\t// li r11,0\n"
        "\tctx.r11.s64 = 0;\n"
        "loc_8321DBB0:"
    )
    if old_db78 in text:
        text = text.replace(old_db78, new_db78)
        patched = True

    old_db3c = (
        "loc_8321DB3C:\n"
        "\t// lwz r8,0(r31)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);"
    )
    new_db3c = (
        "loc_8321DB3C:\n"
        "\t// lwz r8,0(r31)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);\n"
        "\tif (ctx.r8.u32 < 0x10000 || ctx.r8.u32 >= PPC_MEMORY_SIZE - 16) goto loc_8321DB6C;"
    )
    if old_db3c in text:
        text = text.replace(old_db3c, new_db3c)
        patched = True

    old_loop = (
        "loc_8321DBB0:\n"
        "\t// lwzx r7,r11,r8\n"
        "\tctx.r7.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);"
    )
    new_loop = (
        "loc_8321DBB0:\n"
        "\tif ((uint64_t)ctx.r11.u32 + (uint64_t)ctx.r8.u32 >= PPC_MEMORY_SIZE - 4 || ctx.r10.s32 >= 4096) goto loc_8321DBCC;\n"
        "\t// lwzx r7,r11,r8\n"
        "\tctx.r7.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);"
    )
    if old_loop in text:
        text = text.replace(old_loop, new_loop)
        patched = True

    old_loop3c = (
        "\t// add r8,r11,r8\n"
        "\tctx.r8.u64 = ctx.r11.u64 + ctx.r8.u64;\n"
        "\t// add r6,r11,r9\n"
        "\tctx.r6.u64 = ctx.r11.u64 + ctx.r9.u64;\n"
        "\t// addi r11,r11,8\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 8;\n"
        "\t// lwz r9,4(r8)\n"
    )
    new_loop3c = (
        "\tif ((uint64_t)ctx.r11.u32 + (uint64_t)ctx.r8.u32 >= PPC_MEMORY_SIZE - 8 || (uint64_t)ctx.r11.u32 + (uint64_t)ctx.r9.u32 >= PPC_MEMORY_SIZE - 8 || ctx.r10.s32 >= 4096) goto loc_8321DB6C;\n"
        "\t// add r8,r11,r8\n"
        "\tctx.r8.u64 = ctx.r11.u64 + ctx.r8.u64;\n"
        "\t// add r6,r11,r9\n"
        "\tctx.r6.u64 = ctx.r11.u64 + ctx.r9.u64;\n"
        "\t// addi r11,r11,8\n"
        "\tctx.r11.s64 = ctx.r11.s64 + 8;\n"
        "\t// lwz r9,4(r8)\n"
    )
    if old_loop3c in text:
        text = text.replace(old_loop3c, new_loop3c)
        patched = True

    old_div = (
        '\t// beq cr6,0x8321d610\n'
        '\tif (cr6.eq) goto loc_8321D610;\n'
        '\t// lis r9,16383\n'
        '\tctx.r9.s64 = 1073676288;\n'
        '\t// divw r26,r10,r11\n'
        '\tctx.r26.s32 = ctx.r10.s32 / ctx.r11.s32;\n'
        '\t// ori r8,r9,65535\n'
        '\tctx.r8.u64 = ctx.r9.u64 | 65535;\n'
        '\t// divw r27,r28,r11\n'
        '\tctx.r27.s32 = ctx.r28.s32 / ctx.r11.s32;'
    )
    new_div = (
        '\t// beq cr6,0x8321d610\n'
        '\tif (cr6.eq || ctx.r11.u32 == 0) goto loc_8321D610;\n'
        '\t// lis r9,16383\n'
        '\tctx.r9.s64 = 1073676288;\n'
        '\t// divw r26,r10,r11\n'
        '\tctx.r26.s32 = ctx.r11.s32 != 0 ? (ctx.r10.s32 / ctx.r11.s32) : 0;\n'
        '\t// ori r8,r9,65535\n'
        '\tctx.r8.u64 = ctx.r9.u64 | 65535;\n'
        '\t// divw r27,r28,r11\n'
        '\tctx.r27.s32 = ctx.r11.s32 != 0 ? (ctx.r28.s32 / ctx.r11.s32) : 0;'
    )
    if old_div in text:
        text = text.replace(old_div, new_div)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.269 array guards in {path}")
        return True
    return False

def patch_recomp288_buffer_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_func = (
        '__attribute__((alias("__imp__sub_83349068"))) PPC_WEAK_FUNC(sub_83349068);\n'
        'PPC_FUNC_IMPL(__imp__sub_83349068) {\n'
        '\tPPC_FUNC_PROLOGUE();\n'
        '\tPPCCRRegister cr6{};'
    )
    new_func = (
        '__attribute__((alias("__imp__sub_83349068"))) PPC_WEAK_FUNC(sub_83349068);\n'
        'PPC_FUNC_IMPL(__imp__sub_83349068) {\n'
        '\tPPC_FUNC_PROLOGUE();\n'
        '\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 2048) { ctx.r3.s64 = 0; return; }\n'
        '\tPPCCRRegister cr6{};'
    )
    if old_func in text:
        text = text.replace(old_func, new_func)
        patched = True

    old_store = (
        '\t// stwx r30,r10,r31\n'
        '\tPPC_STORE_U32(ctx.r10.u32 + ctx.r31.u32, ctx.r30.u32);'
    )
    new_store = (
        '\t// stwx r30,r10,r31\n'
        '\tif ((uint64_t)ctx.r10.u32 + (uint64_t)ctx.r31.u32 < PPC_MEMORY_SIZE - 4)\n'
        '\t\tPPC_STORE_U32(ctx.r10.u32 + ctx.r31.u32, ctx.r30.u32);'
    )
    if old_store in text:
        text = text.replace(old_store, new_store)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.288 buffer guard in {path}")
        return True
    return False

def patch_recomp77_find_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_entry = (
        '\t// lwz r11,-11772(r11)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -11772);\n'
        '\t// lwz r10,16(r11)\n'
        '\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);\n'
        '\t// lhz r11,468(r10)\n'
        '\tctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 468);\n'
        '\t// cmplwi cr6,r11,0\n'
        '\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n'
        '\t// beqlr cr6\n'
        '\tif (cr6.eq) return;\n'
        '\t// divwu r9,r3,r11\n'
        '\tif (ctx.r11.u32 != 0) ctx.r9.u32 = ctx.r3.u32 / ctx.r11.u32;\n'
        '\t// lwz r8,464(r10)\n'
        '\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + 464);\n'
        '\t// mullw r7,r9,r11\n'
        '\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n'
        '\t// subf r6,r7,r3\n'
        '\tctx.r6.s64 = ctx.r3.s64 - ctx.r7.s64;\n'
        '\t// rlwinm r11,r6,2,0,29\n'
        '\tctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n'
        '\t// lwzx r11,r11,r8\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);\n'
        '\t// cmplwi cr6,r11,0\n'
        '\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n'
        '\t// beqlr cr6\n'
        '\tif (cr6.eq) return;\n'
        'loc_827BE4F8:\n'
        '\t// lwz r10,0(r11)'
    )
    new_entry = (
        '\t// lwz r11,-11772(r11)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + -11772);\n'
        '\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 32) return;\n'
        '\t// lwz r10,16(r11)\n'
        '\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);\n'
        '\tif (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 512) return;\n'
        '\t// lhz r11,468(r10)\n'
        '\tctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 468);\n'
        '\t// cmplwi cr6,r11,0\n'
        '\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n'
        '\t// beqlr cr6\n'
        '\tif (cr6.eq) return;\n'
        '\t// divwu r9,r3,r11\n'
        '\tif (ctx.r11.u32 != 0) ctx.r9.u32 = ctx.r3.u32 / ctx.r11.u32;\n'
        '\t// lwz r8,464(r10)\n'
        '\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + 464);\n'
        '\tif (ctx.r8.u32 < 0x10000 || ctx.r8.u32 >= PPC_MEMORY_SIZE - 4) return;\n'
        '\t// mullw r7,r9,r11\n'
        '\tctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r11.s32);\n'
        '\t// subf r6,r7,r3\n'
        '\tctx.r6.s64 = ctx.r3.s64 - ctx.r7.s64;\n'
        '\t// rlwinm r11,r6,2,0,29\n'
        '\tctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 2) & 0xFFFFFFFC;\n'
        '\tif ((uint64_t)ctx.r11.u32 + (uint64_t)ctx.r8.u32 >= PPC_MEMORY_SIZE - 4) return;\n'
        '\t// lwzx r11,r11,r8\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);\n'
        '\t// cmplwi cr6,r11,0\n'
        '\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n'
        '\t// beqlr cr6\n'
        '\tif (cr6.eq) return;\n'
        '\tuint32_t loopCount = 0;\n'
        'loc_827BE4F8:\n'
        '\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 16 || ++loopCount > 512) return;\n'
        '\t// lwz r10,0(r11)'
    )
    if old_entry in text:
        text = text.replace(old_entry, new_entry)
        patched = True

    old_exit = (
        'loc_827BE514:\n'
        '\t// addi r10,r11,4\n'
        '\tctx.r10.s64 = ctx.r11.s64 + 4;\n'
        '\t// cmplwi cr6,r10,0'
    )
    new_exit = (
        'loc_827BE514:\n'
        '\t// addi r10,r11,4\n'
        '\tctx.r10.s64 = ctx.r11.s64 + 4;\n'
        '\tif (ctx.r10.u32 < 0x10000 || ctx.r10.u32 >= PPC_MEMORY_SIZE - 8) return;\n'
        '\tif (ctx.r4.u32 < 0x10000 || ctx.r4.u32 >= PPC_MEMORY_SIZE - 4) return;\n'
        '\tif (ctx.r5.u32 < 0x10000 || ctx.r5.u32 >= PPC_MEMORY_SIZE - 4) return;\n'
        '\t// cmplwi cr6,r10,0'
    )
    if old_exit in text:
        text = text.replace(old_exit, new_exit)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.77 find guard in {path}")
        return True
    return False

def patch_recomp330_thread_callback_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        '\t// beq cr6,0x836411e8\n'
        '\tif (cr6.eq) goto loc_836411E8;\n'
        'loc_836411C8:\n'
        '\t// mr r11,r30\n'
        '\tctx.r11.u64 = ctx.r30.u64;\n'
        '\t// lwz r30,0(r30)\n'
        '\tctx.r30.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);\n'
        '\t// mr r3,r28\n'
        '\tctx.r3.u64 = ctx.r28.u64;\n'
        '\t// lwz r11,8(r11)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);\n'
        '\t// mtctr r11\n'
        '\tctr.u64 = ctx.r11.u64;\n'
        '\t// bctrl \n'
        '\tctx.lr = 0x836411E0;\n'
        '\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n'
        '\t// cmplw cr6,r30,r31\n'
        '\tcr6.compare<uint32_t>(ctx.r30.u32, ctx.r31.u32, ctx.xer);\n'
        '\t// bne cr6,0x836411c8\n'
        '\tif (!cr6.eq) goto loc_836411C8;\n'
        'loc_836411E8:\n'
        '\t// mr r3,r29\n'
        '\tctx.r3.u64 = ctx.r29.u64;\n'
        '\t// bl 0x83752f4c\n'
        '\tctx.lr = 0x836411F0;\n'
        '\t__imp__RtlLeaveCriticalSection(ctx, base);'
    )
    new = (
        '\t// beq cr6,0x836411e8\n'
        '\tif (cr6.eq) goto loc_836411E8;\n'
        '\t{\n'
        '\t\tconst uint32_t lockAddr_8364 = 0x83878764;\n'
        '\t\tconst uint32_t headSentinel_8364 = 0x83878780;\n'
        '\t\tuint32_t loopGuard_8364 = 0;\n'
        'loc_836411C8:\n'
        '\t\tif (++loopGuard_8364 > 64) goto loc_836411E8;\n'
        '\t\tif (ctx.r30.u32 < 0x10000 || ctx.r30.u32 >= PPC_MEMORY_SIZE - 16) goto loc_836411E8;\n'
        '\t\t// mr r11,r30\n'
        '\t\tctx.r11.u64 = ctx.r30.u64;\n'
        '\t\t// lwz r30,0(r30)\n'
        '\t\tctx.r30.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);\n'
        '\t\tuint32_t nextNode_8364 = ctx.r30.u32;\n'
        '\t\tuint32_t saved_r28 = ctx.r28.u32;\n'
        '\t\t// mr r3,r28\n'
        '\t\tctx.r3.u64 = saved_r28;\n'
        '\t\t// lwz r11,8(r11)\n'
        '\t\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 16) goto loc_836411E8;\n'
        '\t\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);\n'
        '\t\t// mtctr r11\n'
        '\t\tctr.u64 = ctx.r11.u64;\n'
        '\t\t// bctrl \n'
        '\t\tctx.lr = 0x836411E0;\n'
        '\t\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {\n'
        '\t\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n'
        '\t\t}\n'
        '\t\tctx.r28.u64 = saved_r28;\n'
        '\t\tctx.r29.u64 = lockAddr_8364;\n'
        '\t\tctx.r31.u64 = headSentinel_8364;\n'
        '\t\tctx.r30.u64 = nextNode_8364;\n'
        '\t\t// cmplw cr6,r30,r31\n'
        '\t\tcr6.compare<uint32_t>(ctx.r30.u32, ctx.r31.u32, ctx.xer);\n'
        '\t\t// bne cr6,0x836411c8\n'
        '\t\tif (!cr6.eq) goto loc_836411C8;\n'
        '\t}\n'
        'loc_836411E8:\n'
        '\t// mr r3,r29\n'
        '\tctx.r3.u64 = 0x83878764;\n'
        '\t// bl 0x83752f4c\n'
        '\tctx.lr = 0x836411F0;\n'
        '\t__imp__RtlLeaveCriticalSection(ctx, base);'
    )
    # Also support replacing already patched version
    mid = (
        '\t// beq cr6,0x836411e8\n'
        '\tif (cr6.eq) goto loc_836411E8;\n'
        '\tconst uint32_t lockAddr_8364 = 0x83878764;\n'
        '\tconst uint32_t headSentinel_8364 = 0x83878780;\n'
        '\tuint32_t loopGuard_8364 = 0;\n'
        'loc_836411C8:\n'
        '\tif (++loopGuard_8364 > 64) goto loc_836411E8;\n'
        '\tif (ctx.r30.u32 < 0x10000 || ctx.r30.u32 >= PPC_MEMORY_SIZE - 16) goto loc_836411E8;\n'
        '\t// mr r11,r30\n'
        '\tctx.r11.u64 = ctx.r30.u64;\n'
        '\t// lwz r30,0(r30)\n'
        '\tctx.r30.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);\n'
        '\tuint32_t nextNode_8364 = ctx.r30.u32;\n'
        '\tuint32_t saved_r28 = ctx.r28.u32;\n'
        '\t// mr r3,r28\n'
        '\tctx.r3.u64 = saved_r28;\n'
        '\t// lwz r11,8(r11)\n'
        '\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 16) goto loc_836411E8;\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);\n'
        '\t// mtctr r11\n'
        '\tctr.u64 = ctx.r11.u64;\n'
        '\t// bctrl \n'
        '\tctx.lr = 0x836411E0;\n'
        '\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < (PPC_CODE_BASE + PPC_CODE_SIZE)) {\n'
        '\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n'
        '\t}\n'
        '\tctx.r28.u64 = saved_r28;\n'
        '\tctx.r29.u64 = lockAddr_8364;\n'
        '\tctx.r31.u64 = headSentinel_8364;\n'
        '\tctx.r30.u64 = nextNode_8364;\n'
        '\t// cmplw cr6,r30,r31\n'
        '\tcr6.compare<uint32_t>(ctx.r30.u32, ctx.r31.u32, ctx.xer);\n'
        '\t// bne cr6,0x836411c8\n'
        '\tif (!cr6.eq) goto loc_836411C8;\n'
        'loc_836411E8:\n'
        '\t// mr r3,r29\n'
        '\tctx.r3.u64 = lockAddr_8364;\n'
        '\t// bl 0x83752f4c\n'
        '\tctx.lr = 0x836411F0;\n'
        '\t__imp__RtlLeaveCriticalSection(ctx, base);'
    )
    if mid in text:
        text = text.replace(mid, new)
        path.write_text(text)
        print(f"Patched recomp.330 thread callback guard (updated block scope) in {path}")
        return True
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.330 thread callback guard in {path}")
        return True
    return False

def patch_recomp311_alloc_chunk_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    old = (
        '\t// bge cr6,0x83508c14\n'
        '\tif (!cr6.lt) goto loc_83508C14;\n'
        '\t// lwz r8,4(r29)\n'
        '\tctx.r8.u64 = PPC_LOAD_U32(ctx.r29.u32 + 4);\n'
        'loc_83508BEC:\n'
        '\t// add r4,r8,r4\n'
        '\tctx.r4.u64 = ctx.r8.u64 + ctx.r4.u64;\n'
        '\t// add r11,r10,r4\n'
        '\tctx.r11.u64 = ctx.r10.u64 + ctx.r4.u64;\n'
        '\t// srw r11,r11,r9\n'
        '\tctx.r11.u64 = ctx.r9.u8 & 0x20 ? 0 : (ctx.r11.u32 >> (ctx.r9.u8 & 0x3F));\n'
        '\t// addi r7,r11,31\n'
        '\tctx.r7.s64 = ctx.r11.s64 + 31;\n'
        '\t// rlwinm r11,r7,29,3,29\n'
        '\tctx.r11.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 29) & 0x1FFFFFFC;\n'
        '\t// addi r6,r11,47\n'
        '\tctx.r6.s64 = ctx.r11.s64 + 47;\n'
        '\t// rlwinm r11,r6,0,0,27\n'
        '\tctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 0) & 0xFFFFFFF0;\n'
        '\t// add r5,r11,r31\n'
        '\tctx.r5.u64 = ctx.r11.u64 + ctx.r31.u64;\n'
        '\t// cmplw cr6,r4,r5\n'
        '\tcr6.compare<uint32_t>(ctx.r4.u32, ctx.r5.u32, ctx.xer);\n'
        '\t// blt cr6,0x83508bec\n'
        '\tif (cr6.lt) goto loc_83508BEC;\n'
        'loc_83508C14:'
    )
    new = (
        '\t// bge cr6,0x83508c14\n'
        '\tif (!cr6.lt) goto loc_83508C14;\n'
        '\t// lwz r8,4(r29)\n'
        '\tctx.r8.u64 = PPC_LOAD_U32(ctx.r29.u32 + 4);\n'
        '\t{\n'
        '\t\tuint32_t loopGuard_83508 = 0;\n'
        'loc_83508BEC:\n'
        '\t\tif (++loopGuard_83508 > 512 || ctx.r8.u32 == 0) goto loc_83508C14;\n'
        '\t\t// add r4,r8,r4\n'
        '\t\tctx.r4.u64 = ctx.r8.u64 + ctx.r4.u64;\n'
        '\t\t// add r11,r10,r4\n'
        '\t\tctx.r11.u64 = ctx.r10.u64 + ctx.r4.u64;\n'
        '\t\t// srw r11,r11,r9\n'
        '\t\tctx.r11.u64 = ctx.r9.u8 & 0x20 ? 0 : (ctx.r11.u32 >> (ctx.r9.u8 & 0x3F));\n'
        '\t\t// addi r7,r11,31\n'
        '\t\tctx.r7.s64 = ctx.r11.s64 + 31;\n'
        '\t\t// rlwinm r11,r7,29,3,29\n'
        '\t\tctx.r11.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 29) & 0x1FFFFFFC;\n'
        '\t\t// addi r6,r11,47\n'
        '\t\tctx.r6.s64 = ctx.r11.s64 + 47;\n'
        '\t\t// rlwinm r11,r6,0,0,27\n'
        '\t\tctx.r11.u64 = __builtin_rotateleft64(ctx.r6.u32 | (ctx.r6.u64 << 32), 0) & 0xFFFFFFF0;\n'
        '\t\t// add r5,r11,r31\n'
        '\t\tctx.r5.u64 = ctx.r11.u64 + ctx.r31.u64;\n'
        '\t\t// cmplw cr6,r4,r5\n'
        '\t\tcr6.compare<uint32_t>(ctx.r4.u32, ctx.r5.u32, ctx.xer);\n'
        '\t\t// blt cr6,0x83508bec\n'
        '\t\tif (cr6.lt) goto loc_83508BEC;\n'
        '\t}\n'
        'loc_83508C14:'
    )
    if old in text:
        text = text.replace(old, new)
        path.write_text(text)
        print(f"Patched recomp.311 alloc chunk loop in {path}")
        return True
    return False

def patch_recomp265_shader_lookup_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_lookup = (
        '\t// stwu r1,-384(r1)\n'
        '\tea = -384 + ctx.r1.u32;\n'
        '\tPPC_STORE_U32(ea, ctx.r1.u32);\n'
        '\tctx.r1.u32 = ea;\n'
        '\t// lwz r11,3076(r3)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 3076);\n'
        '\t// mr r31,r3\n'
        '\tctx.r31.u64 = ctx.r3.u64;\n'
        '\t// mr r28,r4\n'
        '\tctx.r28.u64 = ctx.r4.u64;\n'
        '\t// mr r27,r5\n'
        '\tctx.r27.u64 = ctx.r5.u64;\n'
        '\t// mr r29,r7\n'
        '\tctx.r29.u64 = ctx.r7.u64;\n'
        '\t// li r3,0\n'
        '\tctx.r3.s64 = 0;\n'
        '\t// li r30,0\n'
        '\tctx.r30.s64 = 0;\n'
        '\t// cmpwi cr6,r11,0\n'
        '\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n'
        '\t// ble cr6,0x831edbfc\n'
        '\tif (!cr6.gt) goto loc_831EDBFC;'
    )
    new_lookup = (
        '\t// stwu r1,-384(r1)\n'
        '\tea = -384 + ctx.r1.u32;\n'
        '\tPPC_STORE_U32(ea, ctx.r1.u32);\n'
        '\tctx.r1.u32 = ea;\n'
        '\t// mr r28,r4\n'
        '\tctx.r28.u64 = ctx.r4.u64;\n'
        '\t// li r3,0\n'
        '\tctx.r3.s64 = 0;\n'
        '\tif (ctx.r4.u32 < 0x10000 || ctx.r4.u32 >= PPC_MEMORY_SIZE) goto loc_831EDBFC;\n'
        '\t{\n'
        '\t\tuint8_t first_char_831e = PPC_LOAD_U8(ctx.r4.u32);\n'
        '\t\tif (first_char_831e < 32 || first_char_831e > 126) goto loc_831EDBFC;\n'
        '\t}\n'
        '\t// lwz r11,3076(r3)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 3076);\n'
        '\tif (ctx.r11.u32 > 350) ctx.r11.u64 = 0;\n'
        '\t// mr r31,r3\n'
        '\tctx.r31.u64 = ctx.r3.u64;\n'
        '\t// mr r27,r5\n'
        '\tctx.r27.u64 = ctx.r5.u64;\n'
        '\t// mr r29,r7\n'
        '\tctx.r29.u64 = ctx.r7.u64;\n'
        '\t// li r30,0\n'
        '\tctx.r30.s64 = 0;\n'
        '\t// cmpwi cr6,r11,0\n'
        '\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n'
        '\t// ble cr6,0x831edbfc\n'
        '\tif (!cr6.gt) goto loc_831EDBFC;'
    )
    fixed_lookup = (
        '\t// stwu r1,-384(r1)\n'
        '\tea = -384 + ctx.r1.u32;\n'
        '\tPPC_STORE_U32(ea, ctx.r1.u32);\n'
        '\tctx.r1.u32 = ea;\n'
        '\t// mr r31,r3\n'
        '\tctx.r31.u64 = ctx.r3.u64;\n'
        '\t// mr r28,r4\n'
        '\tctx.r28.u64 = ctx.r4.u64;\n'
        '\t// mr r27,r5\n'
        '\tctx.r27.u64 = ctx.r5.u64;\n'
        '\t// mr r29,r7\n'
        '\tctx.r29.u64 = ctx.r7.u64;\n'
        '\t// li r3,0\n'
        '\tctx.r3.s64 = 0;\n'
        '\tif (ctx.r31.u32 < 0x10000 || ctx.r31.u32 >= PPC_MEMORY_SIZE - 4096) goto loc_831EDBFC;\n'
        '\tif (ctx.r28.u32 < 0x10000 || ctx.r28.u32 >= PPC_MEMORY_SIZE) goto loc_831EDBFC;\n'
        '\t{\n'
        '\t\tuint8_t first_char_831e = PPC_LOAD_U8(ctx.r28.u32);\n'
        '\t\tif (first_char_831e < 32 || first_char_831e > 126) goto loc_831EDBFC;\n'
        '\t}\n'
        '\t// lwz r11,3076(r31)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 3076);\n'
        '\tif (ctx.r11.u32 > 350) ctx.r11.u64 = 0;\n'
        '\t// li r30,0\n'
        '\tctx.r30.s64 = 0;\n'
        '\t// cmpwi cr6,r11,0\n'
        '\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n'
        '\t// ble cr6,0x831edbfc\n'
        '\tif (!cr6.gt) goto loc_831EDBFC;'
    )
    if new_lookup in text:
        text = text.replace(new_lookup, fixed_lookup)
        patched = True
    elif old_lookup in text:
        text = text.replace(old_lookup, fixed_lookup)
        patched = True

    old_loop_step = (
        'loc_831EDBEC:\n'
        '\t// lwz r11,3076(r31)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 3076);\n'
        '\t// addi r30,r30,1\n'
        '\tctx.r30.s64 = ctx.r30.s64 + 1;\n'
        '\t// cmpw cr6,r30,r11\n'
        '\tcr6.compare<int32_t>(ctx.r30.s32, ctx.r11.s32, ctx.xer);\n'
        '\t// blt cr6,0x831edb98\n'
        '\tif (cr6.lt) goto loc_831EDB98;'
    )
    new_loop_step = (
        'loc_831EDBEC:\n'
        '\t// lwz r11,3076(r31)\n'
        '\tctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 3076);\n'
        '\tif (ctx.r11.u32 > 350) ctx.r11.u64 = 0;\n'
        '\t// addi r30,r30,1\n'
        '\tctx.r30.s64 = ctx.r30.s64 + 1;\n'
        '\t// cmpw cr6,r30,r11\n'
        '\tcr6.compare<int32_t>(ctx.r30.s32, ctx.r11.s32, ctx.xer);\n'
        '\t// blt cr6,0x831edb98\n'
        '\tif (cr6.lt) goto loc_831EDB98;'
    )
    if old_loop_step in text:
        text = text.replace(old_loop_step, new_loop_step)
        patched = True

    old_copy1 = (
        'loc_831E2608:\n'
        '\t// cmplwi cr6,r8,0\n'
        '\tcr6.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e262c\n'
        '\tif (cr6.eq) goto loc_831E262C;\n'
        '\t// lbz r9,0(r10)\n'
        '\tctx.r9.u64 = PPC_LOAD_U8(ctx.r10.u32 + 0);\n'
        '\t// cmplwi cr6,r9,0\n'
        '\tcr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e262c\n'
        '\tif (cr6.eq) goto loc_831E262C;\n'
        '\t// stb r9,0(r11)\n'
        '\tPPC_STORE_U8(ctx.r11.u32 + 0, ctx.r9.u8);'
    )
    new_copy1 = (
        'loc_831E2608:\n'
        '\t// cmplwi cr6,r8,0\n'
        '\tcr6.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e262c\n'
        '\tif (cr6.eq) goto loc_831E262C;\n'
        '\t// lbz r9,0(r10)\n'
        '\tctx.r9.u64 = PPC_LOAD_U8(ctx.r10.u32 + 0);\n'
        '\t// cmplwi cr6,r9,0\n'
        '\tcr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e262c\n'
        '\tif (cr6.eq) goto loc_831E262C;\n'
        '\tif (ctx.r9.u8 < 32 || ctx.r9.u8 > 126) goto loc_831E262C;\n'
        '\t// stb r9,0(r11)\n'
        '\tPPC_STORE_U8(ctx.r11.u32 + 0, ctx.r9.u8);'
    )
    if old_copy1 in text:
        text = text.replace(old_copy1, new_copy1)
        patched = True

    old_copy2 = (
        'loc_831E2694:\n'
        '\t// cmplwi cr6,r8,0\n'
        '\tcr6.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e26b8\n'
        '\tif (cr6.eq) goto loc_831E26B8;\n'
        '\t// lbz r9,0(r10)\n'
        '\tctx.r9.u64 = PPC_LOAD_U8(ctx.r10.u32 + 0);\n'
        '\t// cmplwi cr6,r9,0\n'
        '\tcr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e26b8\n'
        '\tif (cr6.eq) goto loc_831E26B8;\n'
        '\t// stb r9,0(r11)\n'
        '\tPPC_STORE_U8(ctx.r11.u32 + 0, ctx.r9.u8);'
    )
    new_copy2 = (
        'loc_831E2694:\n'
        '\t// cmplwi cr6,r8,0\n'
        '\tcr6.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e26b8\n'
        '\tif (cr6.eq) goto loc_831E26B8;\n'
        '\t// lbz r9,0(r10)\n'
        '\tctx.r9.u64 = PPC_LOAD_U8(ctx.r10.u32 + 0);\n'
        '\t// cmplwi cr6,r9,0\n'
        '\tcr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e26b8\n'
        '\tif (cr6.eq) goto loc_831E26B8;\n'
        '\tif (ctx.r9.u8 < 32 || ctx.r9.u8 > 126) goto loc_831E26B8;\n'
        '\t// stb r9,0(r11)\n'
        '\tPPC_STORE_U8(ctx.r11.u32 + 0, ctx.r9.u8);'
    )
    if old_copy2 in text:
        text = text.replace(old_copy2, new_copy2)
        patched = True

    old_copy3 = (
        'loc_831E24FC:\n'
        '\t// lbz r10,0(r31)\n'
        '\tctx.r10.u64 = PPC_LOAD_U8(ctx.r31.u32 + 0);\n'
        '\t// mr r11,r31\n'
        '\tctx.r11.u64 = ctx.r31.u64;\n'
        '\t// mr r9,r31\n'
        '\tctx.r9.u64 = ctx.r31.u64;\n'
        '\t// cmplwi cr6,r10,0\n'
        '\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e2560\n'
        '\tif (cr6.eq) goto loc_831E2560;\n'
        '\t// addi r8,r31,2'
    )
    new_copy3 = (
        'loc_831E24FC:\n'
        '\t// lbz r10,0(r31)\n'
        '\tctx.r10.u64 = PPC_LOAD_U8(ctx.r31.u32 + 0);\n'
        '\t// mr r11,r31\n'
        '\tctx.r11.u64 = ctx.r31.u64;\n'
        '\t// mr r9,r31\n'
        '\tctx.r9.u64 = ctx.r31.u64;\n'
        '\t// cmplwi cr6,r10,0\n'
        '\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n'
        '\t// beq cr6,0x831e2560\n'
        '\tif (cr6.eq) goto loc_831E2560;\n'
        '\tif (ctx.r10.u8 < 32 || ctx.r10.u8 > 126) goto loc_831E2560;\n'
        '\t// addi r8,r31,2'
    )
    if old_copy3 in text:
        text = text.replace(old_copy3, new_copy3)
        patched = True

    old_copy4 = (
        'loc_831E2518:\n'
        '\t// lbz r10,0(r9)\n'
        '\tctx.r10.u64 = PPC_LOAD_U8(ctx.r9.u32 + 0);\n'
        '\t// cmplwi cr6,r10,92\n'
        '\tcr6.compare<uint32_t>(ctx.r10.u32, 92, ctx.xer);'
    )
    new_copy4 = (
        'loc_831E2518:\n'
        '\t// lbz r10,0(r9)\n'
        '\tctx.r10.u64 = PPC_LOAD_U8(ctx.r9.u32 + 0);\n'
        '\tif (ctx.r10.u8 < 32 || ctx.r10.u8 > 126) goto loc_831E2560;\n'
        '\t// cmplwi cr6,r10,92\n'
        '\tcr6.compare<uint32_t>(ctx.r10.u32, 92, ctx.xer);'
    )
    if old_copy4 in text:
        text = text.replace(old_copy4, new_copy4)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.265 shader lookup guard in {path}")
        return True
    return False

def patch_recomp330_free_list_insert_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    for reg, has_addi_first in [("r30", False), ("r31", True), ("r4", True), ("r29", True)]:
        if not has_addi_first:
            old = (
                "\t// lwz r9,4(r11)\n"
                "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);\n"
                f"\t// stw r11,8({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 8, ctx.r11.u32);\n"
                f"\t// stw r9,12({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 12, ctx.r9.u32);\n"
                f"\t// addi r10,{reg},8\n"
                f"\tctx.r10.s64 = ctx.{reg}.s64 + 8;\n"
                "\t// stw r10,0(r9)\n"
                "\tPPC_STORE_U32(ctx.r9.u32 + 0, ctx.r10.u32);\n"
                "\t// stw r10,4(r11)\n"
                "\tPPC_STORE_U32(ctx.r11.u32 + 4, ctx.r10.u32);"
            )
            new = (
                "\t// lwz r9,4(r11)\n"
                "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);\n"
                "\tif (ctx.r9.u32 < 0x10000 || ctx.r9.u32 >= PPC_MEMORY_SIZE - 4) ctx.r9.u64 = ctx.r11.u64;\n"
                f"\t// stw r11,8({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 8, ctx.r11.u32);\n"
                f"\t// stw r9,12({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 12, ctx.r9.u32);\n"
                f"\t// addi r10,{reg},8\n"
                f"\tctx.r10.s64 = ctx.{reg}.s64 + 8;\n"
                "\t// stw r10,0(r9)\n"
                "\tif (ctx.r9.u32 >= 0x10000 && ctx.r9.u32 < PPC_MEMORY_SIZE - 4) {\n"
                "\t\tPPC_STORE_U32(ctx.r9.u32 + 0, ctx.r10.u32);\n"
                "\t}\n"
                "\t// stw r10,4(r11)\n"
                "\tif (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 4) {\n"
                "\t\tPPC_STORE_U32(ctx.r11.u32 + 4, ctx.r10.u32);\n"
                "\t}"
            )
        else:
            old = (
                "\t// lwz r9,4(r11)\n"
                "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);\n"
                f"\t// addi r10,{reg},8\n"
                f"\tctx.r10.s64 = ctx.{reg}.s64 + 8;\n"
                f"\t// stw r11,8({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 8, ctx.r11.u32);\n"
                f"\t// stw r9,12({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 12, ctx.r9.u32);\n"
                "\t// stw r10,0(r9)\n"
                "\tPPC_STORE_U32(ctx.r9.u32 + 0, ctx.r10.u32);\n"
                "\t// stw r10,4(r11)\n"
                "\tPPC_STORE_U32(ctx.r11.u32 + 4, ctx.r10.u32);"
            )
            new = (
                "\t// lwz r9,4(r11)\n"
                "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);\n"
                "\tif (ctx.r9.u32 < 0x10000 || ctx.r9.u32 >= PPC_MEMORY_SIZE - 4) ctx.r9.u64 = ctx.r11.u64;\n"
                f"\t// addi r10,{reg},8\n"
                f"\tctx.r10.s64 = ctx.{reg}.s64 + 8;\n"
                f"\t// stw r11,8({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 8, ctx.r11.u32);\n"
                f"\t// stw r9,12({reg})\n"
                f"\tPPC_STORE_U32(ctx.{reg}.u32 + 12, ctx.r9.u32);\n"
                "\t// stw r10,0(r9)\n"
                "\tif (ctx.r9.u32 >= 0x10000 && ctx.r9.u32 < PPC_MEMORY_SIZE - 4) {\n"
                "\t\tPPC_STORE_U32(ctx.r9.u32 + 0, ctx.r10.u32);\n"
                "\t}\n"
                "\t// stw r10,4(r11)\n"
                "\tif (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 4) {\n"
                "\t\tPPC_STORE_U32(ctx.r11.u32 + 4, ctx.r10.u32);\n"
                "\t}"
            )
        if old in text:
            text = text.replace(old, new)
            patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.330 free list insert guards in {path}")
        return True
    return False

def patch_recomp18_bugcheck_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    target1_imp = (
        "\t// li r3,245\n"
        "\tctx.r3.s64 = 245;\n"
        "\t// bl 0x8375339c\n"
        "\tctx.lr = 0x823BAC30;\n"
        "\t__imp__KeBugCheck(ctx, base);"
    )
    target1_goto = (
        "\t// li r3,245\n"
        "\tctx.r3.s64 = 245;\n"
        "\t// bl 0x8375339c\n"
        "\tctx.lr = 0x823BAC30;\n"
        "\t// KeBugCheck(245) bypassed: resume at loc_823BADC4\n"
        "\tgoto loc_823BADC4;"
    )
    target1_prev = (
        "\t// li r3,245\n"
        "\tctx.r3.s64 = 245;\n"
        "\t// bl 0x8375339c\n"
        "\tctx.lr = 0x823BAC30;\n"
        "\t// KeBugCheck(245) bypassed: ensure command buffer recording active and resume at loc_823BAC30\n"
        "\tif (ctx.r31.u32 != 0 && PPC_LOAD_U32(ctx.r31.u32 + 13904) == 0) {\n"
        "\t\tPPC_STORE_U32(ctx.r31.u32 + 13904, 1);\n"
        "\t}\n"
        "\tgoto loc_823BAC30;"
    )
    new_site1 = (
        "\t// li r3,245\n"
        "\tctx.r3.s64 = 245;\n"
        "\t// bl 0x8375339c\n"
        "\tctx.lr = 0x823BAC30;\n"
        "\t// KeBugCheck(245) bypassed: ensure command buffer recording active and resume at loc_823BADC4\n"
        "\tif (ctx.r31.u32 != 0 && PPC_LOAD_U32(ctx.r31.u32 + 13904) == 0) {\n"
        "\t\tPPC_STORE_U32(ctx.r31.u32 + 13904, 1);\n"
        "\t}\n"
        "\tgoto loc_823BADC4;"
    )
    if target1_imp in text:
        text = text.replace(target1_imp, new_site1)
        patched = True
    elif target1_goto in text:
        text = text.replace(target1_goto, new_site1)
        patched = True
    elif target1_prev in text:
        text = text.replace(target1_prev, new_site1)
        patched = True

    old_site2 = (
        "\t// li r3,245\n"
        "\tctx.r3.s64 = 245;\n"
        "\t// bl 0x8375339c\n"
        "\tctx.lr = 0x823BADC4;\n"
        "\t__imp__KeBugCheck(ctx, base);"
    )
    new_site2 = (
        "\t// li r3,245\n"
        "\tctx.r3.s64 = 245;\n"
        "\t// bl 0x8375339c\n"
        "\tctx.lr = 0x823BADC4;\n"
        "\t// KeBugCheck(245) bypassed: resume at loc_823BADC4\n"
        "\tgoto loc_823BADC4;"
    )
    if old_site2 in text:
        text = text.replace(old_site2, new_site2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.18 bugcheck guards in {path}")
        return True
    return False

def patch_recomp338_worker_thread_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_bebe0 = (
        "\t// stwu r1,-128(r1)\n"
        "\tea = -128 + ctx.r1.u32;\n"
        "\tPPC_STORE_U32(ea, ctx.r1.u32);\n"
        "\tctx.r1.u32 = ea;\n"
        "\t// lwz r31,0(r3)\n"
        "\tctx.r31.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t// mr r30,r3\n"
        "\tctx.r30.u64 = ctx.r3.u64;"
    )
    new_bebe0 = (
        "\t// stwu r1,-128(r1)\n"
        "\tea = -128 + ctx.r1.u32;\n"
        "\tPPC_STORE_U32(ea, ctx.r1.u32);\n"
        "\tctx.r1.u32 = ea;\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 4) goto loc_836BEC7C;\n"
        "\t// lwz r31,0(r3)\n"
        "\tctx.r31.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t// mr r30,r3\n"
        "\tctx.r30.u64 = ctx.r3.u64;\n"
        "\tif (ctx.r31.u32 < 0x10000 || ctx.r31.u32 >= PPC_MEMORY_SIZE - 400) goto loc_836BEC7C;"
    )
    if old_bebe0 in text:
        text = text.replace(old_bebe0, new_bebe0)
        patched = True

    old_beea0 = (
        "loc_836BEEC4:\n"
        "\t// lis r11,-5\n"
        "\tctx.r11.s64 = -327680;\n"
        "\t// lwz r25,0(r28)\n"
        "\tctx.r25.u64 = PPC_LOAD_U32(ctx.r28.u32 + 0);"
    )
    new_beea0 = (
        "loc_836BEEC4:\n"
        "\t// lis r11,-5\n"
        "\tctx.r11.s64 = -327680;\n"
        "\tif (ctx.r28.u32 < 0x10000 || ctx.r28.u32 >= PPC_MEMORY_SIZE - 4) goto loc_836BF034;\n"
        "\t// lwz r25,0(r28)\n"
        "\tctx.r25.u64 = PPC_LOAD_U32(ctx.r28.u32 + 0);\n"
        "\tif (ctx.r25.u32 < 0x10000 || ctx.r25.u32 >= PPC_MEMORY_SIZE - 400) goto loc_836BF034;"
    )
    if old_beea0 in text:
        text = text.replace(old_beea0, new_beea0)
        patched = True

    target_present = (
        "\t// lbz r11,11070(r31)\n"
        "\tctx.r11.u64 = PPC_LOAD_U8(ctx.r31.u32 + 11070);\n"
        "\t// rlwinm. r11,r11,0,30,30\n"
        "\tctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x2;\n"
        "\tcr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// beq 0x836befb4\n"
        "\tif (cr0.eq) goto loc_836BEFB4;"
    )
    repl_present = (
        "\t// lbz r11,11070(r31)\n"
        "\tctx.r11.u64 = PPC_LOAD_U8(ctx.r31.u32 + 11070);\n"
        "\t// rlwinm. r11,r11,0,30,30\n"
        "\tctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x2;\n"
        "\tcr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// Present scene dirty check bypassed: always invoke sub_822D41E8 to execute VdSwap and BeginScene\n"
        "\tif (ctx.r31.u32 < 0x10000 || ctx.r31.u32 >= PPC_MEMORY_SIZE - 25000) goto loc_836BEFB4;"
    )
    if target_present in text:
        text = text.replace(target_present, repl_present)
        patched = True

    # Ensure loc_836BF00C branches to loc_836BEF5C on timeout (STATUS_TIMEOUT=258)
    # so sub_822D41E8 (VdSwap) is invoked on initial present wait (callerLR 0x836BEF3C)
    target_unblock = (
        "loc_836BF00C:\n"
        "\t// cmplwi cr6,r3,258\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 258, ctx.xer);\n"
        "\t// Present timeout loop unblocked: fall through to loc_836BF014 so Present() returns to game\n"
        "\t// if (cr6.eq) goto loc_836BEF5C;"
    )
    repl_unblock = (
        "loc_836BF00C:\n"
        "\t// cmplwi cr6,r3,258\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 258, ctx.xer);\n"
        "\t// beq cr6,0x836bef5c\n"
        "\tif (cr6.eq) goto loc_836BEF5C;"
    )
    if target_unblock in text:
        text = text.replace(target_unblock, repl_unblock)
        patched = True

    # In loc_836BF014, acknowledge swapchain flip and return STATUS_SUCCESS (D3D_OK=0)
    target_f014 = (
        "loc_836BF014:\n"
        "\t// addi r3,r28,60\n"
        "\tctx.r3.s64 = ctx.r28.s64 + 60;\n"
        "\t// bl 0x8375366c\n"
        "\tctx.lr = 0x836BF01C;\n"
        "\t__imp__KeResetEvent(ctx, base);\n"
        "\t// lwz r11,4(r25)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r25.u32 + 4);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x836bf034\n"
        "\tif (cr6.eq) goto loc_836BF034;\n"
        "\t// mr r3,r28\n"
        "\tctx.r3.u64 = ctx.r28.u64;\n"
        "\t// bl 0x836bebe0\n"
        "\tctx.lr = 0x836BF030;\n"
        "\tsub_836BEBE0(ctx, base);\n"
        "\t// b 0x836beec4\n"
        "\tgoto loc_836BEEC4;"
    )
    repl_f014 = (
        "loc_836BF014:\n"
        "\t// addi r3,r28,60\n"
        "\tctx.r3.s64 = ctx.r28.s64 + 60;\n"
        "\t// bl 0x8375366c\n"
        "\tctx.lr = 0x836BF01C;\n"
        "\t__imp__KeResetEvent(ctx, base);\n"
        "\t// Acknowledge swapchain flip so D3D Present returns cleanly to game loop\n"
        "\tif (ctx.r28.u32 >= 0x10000 && ctx.r28.u32 < PPC_MEMORY_SIZE - 92) {\n"
        "\t\tPPC_STORE_U32(ctx.r28.u32 + 84, PPC_LOAD_U32(ctx.r28.u32 + 88));\n"
        "\t}\n"
        "\tif (ctx.r25.u32 >= 0x10000 && ctx.r25.u32 < PPC_MEMORY_SIZE - 8) {\n"
        "\t\tPPC_STORE_U32(ctx.r25.u32 + 4, 0);\n"
        "\t}\n"
        "\tgoto loc_836BF034;"
    )
    if target_f014 in text:
        text = text.replace(target_f014, repl_f014)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.338 worker thread guards in {path}")
        return True
    return False

def patch_recomp17_queue_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    old_chk = (
        "\t// bl 0x823c0578\n"
        "\tctx.lr = 0x8238BDD4;\n"
        "\tsub_823C0578(ctx, base);\n"
        "\t// cmplwi cr6,r31,0\n"
        "\tcr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x8238be64\n"
        "\tif (cr6.eq) goto loc_8238BE64;"
    )
    new_chk = (
        "\t// bl 0x823c0578\n"
        "\tctx.lr = 0x8238BDD4;\n"
        "\tsub_823C0578(ctx, base);\n"
        "\t// cmplwi cr6,r31,0\n"
        "\tcr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x8238be64\n"
        "\tif (cr6.eq || ctx.r31.u32 < 0x10000 || ctx.r31.u32 >= PPC_MEMORY_SIZE - 400) goto loc_8238BE64;"
    )
    if old_chk in text:
        text = text.replace(old_chk, new_chk)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.17 queue guard in {path}")
        return True
    return False

def patch_recomp209_stream_wait_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    target1 = (
        "loc_82EF5D94:\n"
        "\t// li r3,10\n"
        "\tctx.r3.s64 = 10;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82EF5D9C;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// li r8,0\n"
        "\tctx.r8.s64 = 0;\n"
        "\t// mr r7,r31\n"
        "\tctx.r7.u64 = ctx.r31.u64;\n"
        "\t// fmr f1,f31\n"
        "\tctx.fpscr.disableFlushMode();\n"
        "\tctx.f1.f64 = ctx.f31.f64;\n"
        "\t// li r6,0\n"
        "\tctx.r6.s64 = 0;\n"
        "\t// li r5,1\n"
        "\tctx.r5.s64 = 1;\n"
        "\t// addi r4,r1,104\n"
        "\tctx.r4.s64 = ctx.r1.s64 + 104;\n"
        "\t// mr r3,r30\n"
        "\tctx.r3.u64 = ctx.r30.u64;\n"
        "\t// bl 0x8319b1f0\n"
        "\tctx.lr = 0x82EF5DBC;\n"
        "\tsub_8319B1F0(ctx, base);\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82ef5d94\n"
        "\tif (cr6.eq) goto loc_82EF5D94;"
    )

    repl1 = (
        "loc_82EF5D94:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount5D94 = 0;\n"
        "\t\tif (++s_loopCount5D94 > 5) {\n"
        "\t\t\ts_loopCount5D94 = 0;\n"
        "\t\t\tif (ctx.r31.u32 != 0) {\n"
        "\t\t\t\tctx.r3.u64 = ctx.r31.u64;\n"
        "\t\t\t\tsub_831921F8(ctx, base);\n"
        "\t\t\t}\n"
        "\t\t\tgoto loc_82EF5E6C;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// li r3,10\n"
        "\tctx.r3.s64 = 10;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82EF5D9C;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// li r8,0\n"
        "\tctx.r8.s64 = 0;\n"
        "\t// mr r7,r31\n"
        "\tctx.r7.u64 = ctx.r31.u64;\n"
        "\t// fmr f1,f31\n"
        "\tctx.fpscr.disableFlushMode();\n"
        "\tctx.f1.f64 = ctx.f31.f64;\n"
        "\t// li r6,0\n"
        "\tctx.r6.s64 = 0;\n"
        "\t// li r5,1\n"
        "\tctx.r5.s64 = 1;\n"
        "\t// addi r4,r1,104\n"
        "\tctx.r4.s64 = ctx.r1.s64 + 104;\n"
        "\t// mr r3,r30\n"
        "\tctx.r3.u64 = ctx.r30.u64;\n"
        "\t// bl 0x8319b1f0\n"
        "\tctx.lr = 0x82EF5DBC;\n"
        "\tsub_8319B1F0(ctx, base);\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82ef5d94\n"
        "\tif (cr6.eq) goto loc_82EF5D94;"
    )

    target2 = (
        "loc_82EF5E30:\n"
        "\t// li r3,10\n"
        "\tctx.r3.s64 = 10;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82EF5E38;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// lis r11,-32017\n"
        "\tctx.r11.s64 = -2098266112;\n"
        "\t// li r10,0\n"
        "\tctx.r10.s64 = 0;\n"
        "\t// fmr f1,f31\n"
        "\tctx.fpscr.disableFlushMode();\n"
        "\tctx.f1.f64 = ctx.f31.f64;\n"
        "\t// li r9,0\n"
        "\tctx.r9.s64 = 0;\n"
        "\t// mr r8,r28\n"
        "\tctx.r8.u64 = ctx.r28.u64;\n"
        "\t// addi r7,r11,8440\n"
        "\tctx.r7.s64 = ctx.r11.s64 + 8440;\n"
        "\t// li r6,0\n"
        "\tctx.r6.s64 = 0;\n"
        "\t// li r5,1\n"
        "\tctx.r5.s64 = 1;\n"
        "\t// addi r4,r1,104\n"
        "\tctx.r4.s64 = ctx.r1.s64 + 104;\n"
        "\t// mr r3,r30\n"
        "\tctx.r3.u64 = ctx.r30.u64;\n"
        "\t// bl 0x8319a4c8\n"
        "\tctx.lr = 0x82EF5E64;\n"
        "\tsub_8319A4C8(ctx, base);\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82ef5e30\n"
        "\tif (cr6.eq) goto loc_82EF5E30;"
    )

    repl2 = (
        "loc_82EF5E30:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount5E30 = 0;\n"
        "\t\tif (++s_loopCount5E30 > 5) {\n"
        "\t\t\ts_loopCount5E30 = 0;\n"
        "\t\t\tgoto loc_82EF5E6C;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// li r3,10\n"
        "\tctx.r3.s64 = 10;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82EF5E38;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// lis r11,-32017\n"
        "\tctx.r11.s64 = -2098266112;\n"
        "\t// li r10,0\n"
        "\tctx.r10.s64 = 0;\n"
        "\t// fmr f1,f31\n"
        "\tctx.fpscr.disableFlushMode();\n"
        "\tctx.f1.f64 = ctx.f31.f64;\n"
        "\t// li r9,0\n"
        "\tctx.r9.s64 = 0;\n"
        "\t// mr r8,r28\n"
        "\tctx.r8.u64 = ctx.r28.u64;\n"
        "\t// addi r7,r11,8440\n"
        "\tctx.r7.s64 = ctx.r11.s64 + 8440;\n"
        "\t// li r6,0\n"
        "\tctx.r6.s64 = 0;\n"
        "\t// li r5,1\n"
        "\tctx.r5.s64 = 1;\n"
        "\t// addi r4,r1,104\n"
        "\tctx.r4.s64 = ctx.r1.s64 + 104;\n"
        "\t// mr r3,r30\n"
        "\tctx.r3.u64 = ctx.r30.u64;\n"
        "\t// bl 0x8319a4c8\n"
        "\tctx.lr = 0x82EF5E64;\n"
        "\tsub_8319A4C8(ctx, base);\n"
        "\t// cmplwi cr6,r3,0\n"
        "\tcr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82ef5e30\n"
        "\tif (cr6.eq) goto loc_82EF5E30;"
    )

    if target1 in text:
        text = text.replace(target1, repl1)
        patched = True
    if target2 in text:
        text = text.replace(target2, repl2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.209 stream wait loop in {path}")
        return True
    return False

def patch_recomp42_lang_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_82591D08:\n"
        "\t// mr r4,r29\n"
        "\tctx.r4.u64 = ctx.r29.u64;\n"
        "\t// mr r3,r31\n"
        "\tctx.r3.u64 = ctx.r31.u64;\n"
        "\t// bl 0x82985458\n"
        "\tctx.lr = 0x82591D14;\n"
        "\tsub_82985458(ctx, base);"
    )
    repl = (
        "loc_82591D08:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount1D08 = 0;\n"
        "\t\tif (++s_loopCount1D08 > 5) {\n"
        "\t\t\ts_loopCount1D08 = 0;\n"
        "\t\t\tgoto loc_82591D94;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// mr r4,r29\n"
        "\tctx.r4.u64 = ctx.r29.u64;\n"
        "\t// mr r3,r31\n"
        "\tctx.r3.u64 = ctx.r31.u64;\n"
        "\t// bl 0x82985458\n"
        "\tctx.lr = 0x82591D14;\n"
        "\tsub_82985458(ctx, base);"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.42 lang loop in {path}")
        return True
    return False

def patch_recomp104_packfile_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_8298B9D8:\n"
        "\t// mr r3,r30\n"
        "\tctx.r3.u64 = ctx.r30.u64;\n"
        "\t// bl 0x831dccc0\n"
        "\tctx.lr = 0x8298B9E0;\n"
        "\tsub_831DCCC0(ctx, base);"
    )
    repl = (
        "loc_8298B9D8:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountB9D8 = 0;\n"
        "\t\tif (++s_loopCountB9D8 > 5) {\n"
        "\t\t\ts_loopCountB9D8 = 0;\n"
        "\t\t\tgoto loc_8298BB3C;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// mr r3,r30\n"
        "\tctx.r3.u64 = ctx.r30.u64;\n"
        "\t// bl 0x831dccc0\n"
        "\tctx.lr = 0x8298B9E0;\n"
        "\tsub_831DCCC0(ctx, base);"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.104 packfile loop in {path}")
        return True
    return False

def patch_recomp108_packfile_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_829B4BF4:\n"
        "\t// mr r3,r27\n"
        "\tctx.r3.u64 = ctx.r27.u64;\n"
        "\t// bl 0x8344fcd8\n"
        "\tctx.lr = 0x829B4BFC;\n"
        "\tsub_8344FCD8(ctx, base);"
    )
    repl = (
        "loc_829B4BF4:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount4BF4 = 0;\n"
        "\t\tif (++s_loopCount4BF4 > 5) {\n"
        "\t\t\ts_loopCount4BF4 = 0;\n"
        "\t\t\tgoto loc_829B4CA8;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// mr r3,r27\n"
        "\tctx.r3.u64 = ctx.r27.u64;\n"
        "\t// bl 0x8344fcd8\n"
        "\tctx.lr = 0x829B4BFC;\n"
        "\tsub_8344FCD8(ctx, base);"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.108 packfile loop in {path}")
        return True
    return False

def patch_recomp328_lock10_recursion(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target1 = (
        "loc_8362D5D4:\n"
        "\t// li r3,10\n"
        "\tctx.r3.s64 = 10;\n"
        "\t// bl 0x8362d690\n"
        "\tctx.lr = 0x8362D5DC;\n"
        "\tsub_8362D690(ctx, base);"
    )
    old_repl1 = (
        "loc_8362D5D4:\n"
        "\tif (ctx.r30.u32 != 10) {\n"
        "\t\t// li r3,10\n"
        "\t\tctx.r3.s64 = 10;\n"
        "\t\t// bl 0x8362d690\n"
        "\t\tctx.lr = 0x8362D5DC;\n"
        "\t\tsub_8362D690(ctx, base);\n"
        "\t}"
    )
    repl1 = (
        "loc_8362D5D4:\n"
        "\tif (ctx.r29.u32 != 80) {\n"
        "\t\t// li r3,10\n"
        "\t\tctx.r3.s64 = 10;\n"
        "\t\t// bl 0x8362d690\n"
        "\t\tctx.lr = 0x8362D5DC;\n"
        "\t\tsub_8362D690(ctx, base);\n"
        "\t}"
    )

    target2 = (
        "loc_8362D62C:\n"
        "\t// mr r8,r8\n"
        "\tctx.r8.u64 = ctx.r8.u64;\n"
        "\t// addi r12,r31,128\n"
        "\tctx.r12.s64 = ctx.r31.s64 + 128;\n"
        "\t// bl 0x8362d660\n"
        "\tctx.lr = 0x8362D638;\n"
        "\tsub_8362D660(ctx, base);"
    )
    old_repl2 = (
        "loc_8362D62C:\n"
        "\t// mr r8,r8\n"
        "\tctx.r8.u64 = ctx.r8.u64;\n"
        "\tif (ctx.r30.u32 != 10) {\n"
        "\t\t// addi r12,r31,128\n"
        "\t\tctx.r12.s64 = ctx.r31.s64 + 128;\n"
        "\t\t// bl 0x8362d660\n"
        "\t\tctx.lr = 0x8362D638;\n"
        "\t\tsub_8362D660(ctx, base);\n"
        "\t}"
    )
    repl2 = (
        "loc_8362D62C:\n"
        "\t// mr r8,r8\n"
        "\tctx.r8.u64 = ctx.r8.u64;\n"
        "\tif (ctx.r29.u32 != 80) {\n"
        "\t\t// addi r12,r31,128\n"
        "\t\tctx.r12.s64 = ctx.r31.s64 + 128;\n"
        "\t\t// bl 0x8362d660\n"
        "\t\tctx.lr = 0x8362D638;\n"
        "\t\tsub_8362D660(ctx, base);\n"
        "\t}"
    )

    patched = False
    if old_repl1 in text:
        text = text.replace(old_repl1, repl1)
        patched = True
    elif target1 in text:
        text = text.replace(target1, repl1)
        patched = True

    if old_repl2 in text:
        text = text.replace(old_repl2, repl2)
        patched = True
    elif target2 in text:
        text = text.replace(target2, repl2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.328 lock 10 recursion in {path}")
        return True
    return False

def patch_recomp268_atmap_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target1 = (
        "loc_832170E4:\n"
        "\t// addi r11,r9,1464\n"
        "\tctx.r11.s64 = ctx.r9.s64 + 1464;\n"
        "\t// rlwinm r10,r11,2,0,29\n"
        "\tctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r8,r10,r31\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r31.u32);"
    )
    repl1 = (
        "loc_832170E4:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount70E4 = 0;\n"
        "\t\tif (++s_loopCount70E4 > 64) {\n"
        "\t\t\ts_loopCount70E4 = 0;\n"
        "\t\t\tgoto loc_83217154;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// addi r11,r9,1464\n"
        "\tctx.r11.s64 = ctx.r9.s64 + 1464;\n"
        "\t// rlwinm r10,r11,2,0,29\n"
        "\tctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r8,r10,r31\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r31.u32);"
    )

    target2 = (
        "loc_83216E7C:\n"
        "\t// addi r11,r9,864\n"
        "\tctx.r11.s64 = ctx.r9.s64 + 864;\n"
        "\t// rlwinm r10,r11,2,0,29\n"
        "\tctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r8,r10,r31\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r31.u32);"
    )
    repl2 = (
        "loc_83216E7C:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount6E7C = 0;\n"
        "\t\tif (++s_loopCount6E7C > 64) {\n"
        "\t\t\ts_loopCount6E7C = 0;\n"
        "\t\t\tgoto loc_83216EEC;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// addi r11,r9,864\n"
        "\tctx.r11.s64 = ctx.r9.s64 + 864;\n"
        "\t// rlwinm r10,r11,2,0,29\n"
        "\tctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// lwzx r8,r10,r31\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r31.u32);"
    )

    patched = False
    if target1 in text:
        text = text.replace(target1, repl1)
        patched = True
    if target2 in text:
        text = text.replace(target2, repl2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.268 atmap loops in {path}")
        return True
    return False

def patch_recomp260_pool_init(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target1 = (
        "\t// stw r9,8(r3)\n"
        "\tPPC_STORE_U32(ctx.r3.u32 + 8, ctx.r9.u32);\n"
        "\t// beq 0x831ab12c\n"
        "\tif (cr0.eq) goto loc_831AB12C;"
    )
    repl1 = (
        "\t// stw r9,8(r3)\n"
        "\tPPC_STORE_U32(ctx.r3.u32 + 8, ctx.r9.u32);\n"
        "\t// beq 0x831ab12c\n"
        "\tif (cr0.eq || int32_t(ctx.r10.s32) < 0) goto loc_831AB12C;"
    )

    target2 = (
        "\t// stw r9,8(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + 8, ctx.r9.u32);\n"
        "\t// beq 0x831aed60\n"
        "\tif (cr0.eq) goto loc_831AED60;"
    )
    repl2 = (
        "\t// stw r9,8(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + 8, ctx.r9.u32);\n"
        "\t// beq 0x831aed60\n"
        "\tif (cr0.eq || int32_t(ctx.r10.s32) < 0) goto loc_831AED60;"
    )

    target3 = (
        "\t// stw r7,8(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + 8, ctx.r7.u32);\n"
        "\t// beq 0x831aedfc\n"
        "\tif (cr0.eq) goto loc_831AEDFC;"
    )
    repl3 = (
        "\t// stw r7,8(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + 8, ctx.r7.u32);\n"
        "\t// beq 0x831aedfc\n"
        "\tif (cr0.eq || int32_t(ctx.r9.s32) < 0) goto loc_831AEDFC;"
    )

    loop_target1 = "loc_831AB118:\n\t// add r10,r10,r11\n"
    loop_repl1 = (
        "loc_831AB118:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountB118 = 0;\n"
        "\t\tif (++s_loopCountB118 > 100000) {\n"
        "\t\t\ts_loopCountB118 = 0;\n"
        "\t\t\tgoto loc_831AB12C;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// add r10,r10,r11\n"
    )

    loop_target2 = "loc_831AED4C:\n\t// add r10,r10,r11\n"
    loop_repl2 = (
        "loc_831AED4C:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountED4C = 0;\n"
        "\t\tif (++s_loopCountED4C > 100000) {\n"
        "\t\t\ts_loopCountED4C = 0;\n"
        "\t\t\tgoto loc_831AED60;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// add r10,r10,r11\n"
    )

    loop_target3 = "loc_831AEDE8:\n\t// add r10,r10,r11\n"
    loop_repl3 = (
        "loc_831AEDE8:\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountEDE8 = 0;\n"
        "\t\tif (++s_loopCountEDE8 > 100000) {\n"
        "\t\t\ts_loopCountEDE8 = 0;\n"
        "\t\t\tgoto loc_831AEDFC;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// add r10,r10,r11\n"
    )

    patched = False
    for t, r in [(target1, repl1), (target2, repl2), (target3, repl3),
                 (loop_target1, loop_repl1), (loop_target2, loop_repl2), (loop_target3, loop_repl3)]:
        if t in text:
            text = text.replace(t, r)
            patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.260 pool init underflow in {path}")
        return True
    return False

def patch_recomp27_statemachine_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_824B9D94:\n"
        "\t// cmpwi cr6,r3,0\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x824b9c74\n"
        "\tif (cr6.eq) goto loc_824B9C74;"
    )
    repl = (
        "loc_824B9D94:\n"
        "\t// cmpwi cr6,r3,0\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x824b9c74\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount9D94 = 0;\n"
        "\t\tif (++s_loopCount9D94 > 16) {\n"
        "\t\t\ts_loopCount9D94 = 0;\n"
        "\t\t\tctx.r3.s64 = 1;\n"
        "\t\t} else if (cr6.eq) {\n"
        "\t\t\tgoto loc_824B9C74;\n"
        "\t\t} else {\n"
        "\t\t\ts_loopCount9D94 = 0;\n"
        "\t\t}\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.27 statemachine loop in {path}")
        return True
    return False

def patch_recomp9_queue_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target1 = (
        "\t// mr r31,r3\n"
        "\tctx.r31.u64 = ctx.r3.u64;\n"
        "\t// lwz r3,4(r3)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);\n"
        "\t// lwz r11,0(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t// lwz r10,4(r11)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);\n"
        "\t// mtctr r10\n"
        "\tctr.u64 = ctx.r10.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x822B65D4;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    repl1 = (
        "\t// mr r31,r3\n"
        "\tctx.r31.u64 = ctx.r3.u64;\n"
        "\t// lwz r3,4(r3)\n"
        "\tctx.r3.u64 = (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 8) ? PPC_LOAD_U32(ctx.r3.u32 + 4) : 0;\n"
        "\tif (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 8) {\n"
        "\t\t// lwz r11,0(r3)\n"
        "\t\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t\tif (ctx.r11.u32 >= 0x10000 && ctx.r11.u32 < PPC_MEMORY_SIZE - 8) {\n"
        "\t\t\t// lwz r10,4(r11)\n"
        "\t\t\tctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 4);\n"
        "\t\t\t// mtctr r10\n"
        "\t\t\tctr.u64 = ctx.r10.u64;\n"
        "\t\t\t// bctrl \n"
        "\t\t\tctx.lr = 0x822B65D4;\n"
        "\t\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t\t}\n"
        "\t}"
    )

    target2 = (
        "\t// lwz r3,4(r31)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);\n"
        "\t// lwz r9,0(r3)\n"
        "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t// lwz r8,8(r9)\n"
        "\tctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + 8);\n"
        "\t// mtctr r8\n"
        "\tctr.u64 = ctx.r8.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x822B65F4;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    repl2 = (
        "\t// lwz r3,4(r31)\n"
        "\tctx.r3.u64 = (ctx.r31.u32 >= 0x10000 && ctx.r31.u32 < PPC_MEMORY_SIZE - 8) ? PPC_LOAD_U32(ctx.r31.u32 + 4) : 0;\n"
        "\tif (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 8) {\n"
        "\t\t// lwz r9,0(r3)\n"
        "\t\tctx.r9.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t\tif (ctx.r9.u32 >= 0x10000 && ctx.r9.u32 < PPC_MEMORY_SIZE - 8) {\n"
        "\t\t\t// lwz r8,8(r9)\n"
        "\t\t\tctx.r8.u64 = PPC_LOAD_U32(ctx.r9.u32 + 8);\n"
        "\t\t\t// mtctr r8\n"
        "\t\t\tctr.u64 = ctx.r8.u64;\n"
        "\t\t\t// bctrl \n"
        "\t\t\tctx.lr = 0x822B65F4;\n"
        "\t\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t\t}\n"
        "\t}"
    )

    patched = False
    if target1 in text:
        text = text.replace(target1, repl1)
        patched = True
    if target2 in text:
        text = text.replace(target2, repl2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.9 queue guard in {path}")
        return True
    return False

def patch_recomp3_sub_822438F8_switch(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    
    target_dispatch = (
        "\t// cmplwi cr6,r10,15\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 15, ctx.xer);\n"
        "\t// bgt cr6,0x82243a40\n"
        "\tif (cr6.gt) goto loc_82243A40;\n"
        "\t// lis r12,-32220\n"
        "\tctx.r12.s64 = -2111569920;\n"
        "\t// rlwinm r0,r11,2,0,29\n"
        "\tctx.r0.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;\n"
        "\t// addi r12,r12,14676\n"
        "\tctx.r12.s64 = ctx.r12.s64 + 14676;\n"
        "\t// lwzx r0,r12,r0\n"
        "\tctx.r0.u64 = PPC_LOAD_U32(ctx.r12.u32 + ctx.r0.u32);\n"
        "\t// mtctr r0\n"
        "\tctr.u64 = ctx.r0.u64;\n"
        "\t// bctr \n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\treturn;\n"
        "\t// lwz r17,14740(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14740);\n"
        "\t// lwz r17,14764(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14764);\n"
        "\t// lwz r17,14824(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14824);\n"
        "\t// lwz r17,14772(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14772);\n"
        "\t// lwz r17,14780(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14780);\n"
        "\t// lwz r17,14800(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14800);\n"
        "\t// lwz r17,14816(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14816);\n"
        "\t// lwz r17,14852(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14852);\n"
        "\t// lwz r17,14860(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14860);\n"
        "\t// lwz r17,14868(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14868);\n"
        "\t// lwz r17,14876(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14876);\n"
        "\t// lwz r17,14928(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14928);\n"
        "\t// lwz r17,14884(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14884);\n"
        "\t// lwz r17,14892(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14892);\n"
        "\t// lwz r17,14904(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14904);\n"
        "\t// lwz r17,14808(r4)\n"
        "\tctx.r17.u64 = PPC_LOAD_U32(ctx.r4.u32 + 14808);"
    )

    repl_dispatch = (
        "\tswitch (ctx.r10.u32) {\n"
        "\tcase 0: goto loc_82243994;\n"
        "\tcase 1: goto loc_822439AC;\n"
        "\tcase 2: goto loc_822439E8;\n"
        "\tcase 3: goto loc_822439B4;\n"
        "\tcase 4: goto loc_822439BC;\n"
        "\tcase 5: goto loc_822439D0;\n"
        "\tcase 6: goto loc_822439E0;\n"
        "\tcase 7: goto loc_82243A04;\n"
        "\tcase 8: goto loc_82243A0C;\n"
        "\tcase 9: goto loc_82243A14;\n"
        "\tcase 10: goto loc_82243A1C;\n"
        "\tcase 11: goto loc_82243A50;\n"
        "\tcase 12: goto loc_82243A24;\n"
        "\tcase 13: goto loc_82243A2C;\n"
        "\tcase 14: goto loc_82243A38;\n"
        "\tcase 15: goto loc_822439D8;\n"
        "\tdefault: goto loc_82243A40;\n"
        "\t}"
    )

    target_body = (
        "\t// lbz r10,-14809(r30)\n"
        "\tctx.r10.u64 = PPC_LOAD_U8(ctx.r30.u32 + -14809);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82243a50\n"
        "\tif (cr6.eq) goto loc_82243A50;\n"
        "\t// bl 0x829458a0\n"
        "\tctx.lr = 0x822439A4;\n"
        "\tsub_829458A0(ctx, base);\n"
        "\t// stb r27,-14809(r30)\n"
        "\tPPC_STORE_U8(ctx.r30.u32 + -14809, ctx.r27.u8);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// stw r22,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r22.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x82567ec8\n"
        "\tctx.lr = 0x822439B8;\n"
        "\tsub_82567EC8(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// li r4,1\n"
        "\tctx.r4.s64 = 1;\n"
        "\t// lwz r3,23808(r26)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r26.u32 + 23808);\n"
        "\t// bl 0x82525480\n"
        "\tctx.lr = 0x822439C8;\n"
        "\tsub_82525480(ctx, base);\n"
        "\t// stw r24,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r24.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x8256fc30\n"
        "\tctx.lr = 0x822439D4;\n"
        "\tsub_8256FC30(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x82562720\n"
        "\tctx.lr = 0x822439DC;\n"
        "\tsub_82562720(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x82567f78\n"
        "\tctx.lr = 0x822439E4;\n"
        "\tsub_82567F78(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// li r10,1\n"
        "\tctx.r10.s64 = 1;\n"
        "\t// stw r27,22940(r29)\n"
        "\tPPC_STORE_U32(ctx.r29.u32 + 22940, ctx.r27.u32);\n"
        "\t// mr r11,r27\n"
        "\tctx.r11.u64 = ctx.r27.u64;\n"
        "\t// stb r10,-14809(r30)\n"
        "\tPPC_STORE_U8(ctx.r30.u32 + -14809, ctx.r10.u8);\n"
        "\t// bl 0x8294c5f0\n"
        "\tctx.lr = 0x822439FC;\n"
        "\tsub_8294C5F0(ctx, base);\n"
        "\t// stw r28,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r28.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x82568000\n"
        "\tctx.lr = 0x82243A08;\n"
        "\tsub_82568000(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x8256dc20\n"
        "\tctx.lr = 0x82243A10;\n"
        "\tsub_8256DC20(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x8256bf90\n"
        "\tctx.lr = 0x82243A18;\n"
        "\tsub_8256BF90(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// stw r23,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r23.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x8256c048\n"
        "\tctx.lr = 0x82243A28;\n"
        "\tsub_8256C048(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// li r3,0\n"
        "\tctx.r3.s64 = 0;\n"
        "\t// bl 0x8256c230\n"
        "\tctx.lr = 0x82243A34;\n"
        "\tsub_8256C230(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "\t// bl 0x8256c190\n"
        "\tctx.lr = 0x82243A3C;\n"
        "\tsub_8256C190(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;"
    )

    repl_body = (
        "loc_82243994:\n"
        "\t// lbz r10,-14809(r30)\n"
        "\tctx.r10.u64 = PPC_LOAD_U8(ctx.r30.u32 + -14809);\n"
        "\t// cmplwi cr6,r10,0\n"
        "\tcr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82243a50\n"
        "\tif (cr6.eq) goto loc_82243A50;\n"
        "\t// bl 0x829458a0\n"
        "\tctx.lr = 0x822439A4;\n"
        "\tsub_829458A0(ctx, base);\n"
        "\t// stb r27,-14809(r30)\n"
        "\tPPC_STORE_U8(ctx.r30.u32 + -14809, ctx.r27.u8);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439AC:\n"
        "\t// stw r22,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r22.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439B4:\n"
        "\t// bl 0x82567ec8\n"
        "\tctx.lr = 0x822439B8;\n"
        "\tsub_82567EC8(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439BC:\n"
        "\t// li r4,1\n"
        "\tctx.r4.s64 = 1;\n"
        "\t// lwz r3,23808(r26)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r26.u32 + 23808);\n"
        "\t// bl 0x82525480\n"
        "\tctx.lr = 0x822439C8;\n"
        "\tsub_82525480(ctx, base);\n"
        "\t// stw r24,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r24.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439D0:\n"
        "\t// bl 0x8256fc30\n"
        "\tctx.lr = 0x822439D4;\n"
        "\tsub_8256FC30(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439D8:\n"
        "\t// bl 0x82562720\n"
        "\tctx.lr = 0x822439DC;\n"
        "\tsub_82562720(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439E0:\n"
        "\t// bl 0x82567f78\n"
        "\tctx.lr = 0x822439E4;\n"
        "\tsub_82567F78(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_822439E8:\n"
        "\t// li r10,1\n"
        "\tctx.r10.s64 = 1;\n"
        "\t// stw r27,22940(r29)\n"
        "\tPPC_STORE_U32(ctx.r29.u32 + 22940, ctx.r27.u32);\n"
        "\t// mr r11,r27\n"
        "\tctx.r11.u64 = ctx.r27.u64;\n"
        "\t// stb r10,-14809(r30)\n"
        "\tPPC_STORE_U8(ctx.r30.u32 + -14809, ctx.r10.u8);\n"
        "\t// bl 0x8294c5f0\n"
        "\tctx.lr = 0x822439FC;\n"
        "\tsub_8294C5F0(ctx, base);\n"
        "\t// stw r28,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r28.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A04:\n"
        "\t// bl 0x82568000\n"
        "\tctx.lr = 0x82243A08;\n"
        "\tsub_82568000(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A0C:\n"
        "\t// bl 0x8256dc20\n"
        "\tctx.lr = 0x82243A10;\n"
        "\tsub_8256DC20(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A14:\n"
        "\t// bl 0x8256bf90\n"
        "\tctx.lr = 0x82243A18;\n"
        "\tsub_8256BF90(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A1C:\n"
        "\t// stw r23,-14788(r31)\n"
        "\tPPC_STORE_U32(ctx.r31.u32 + -14788, ctx.r23.u32);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A24:\n"
        "\t// bl 0x8256c048\n"
        "\tctx.lr = 0x82243A28;\n"
        "\tsub_8256C048(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A2C:\n"
        "\t// li r3,0\n"
        "\tctx.r3.s64 = 0;\n"
        "\t// bl 0x8256c230\n"
        "\tctx.lr = 0x82243A34;\n"
        "\tsub_8256C230(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
        "loc_82243A38:\n"
        "\t// bl 0x8256c190\n"
        "\tctx.lr = 0x82243A3C;\n"
        "\tsub_8256C190(ctx, base);\n"
        "\t// b 0x82243a44\n"
        "\tgoto loc_82243A44;\n"
    )

    patched = False
    if target_dispatch in text:
        text = text.replace(target_dispatch, repl_dispatch)
        patched = True
    if target_body in text:
        text = text.replace(target_body, repl_body)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.3 sub_822438F8 switch in {path}")
        return True
    return False

def patch_recomp33_sub_8252E5A0_switch(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// cmpwi cr6,r11,0\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x8252e640\n"
        "\t// bdzf 4*cr6+eq,0x8252e79c\n"
        "\t// bdzf 4*cr6+eq,0x8252e64c\n"
        "\t// bdzf 4*cr6+eq,0x8252e6c4\n"
        "\t// bdzf 4*cr6+eq,0x8252e6f4\n"
        "\t// bdzf 4*cr6+eq,0x8252e75c\n"
        "\t// bne cr6,0x8252e794\n"
        "\tif (!cr6.eq) goto loc_8252E794;"
    )

    repl = (
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0: break;\n"
        "\tcase 1: goto loc_8252E640;\n"
        "\tcase 2: goto loc_8252E79C;\n"
        "\tcase 3: goto loc_8252E64C;\n"
        "\tcase 4: goto loc_8252E6C4;\n"
        "\tcase 5: goto loc_8252E6F4;\n"
        "\tcase 6: goto loc_8252E75C;\n"
        "\tdefault: goto loc_8252E794;\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.33 sub_8252E5A0 switch in {path}")
        return True
    return False

def patch_recomp101_sub_8294C6C0_switch(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// cmpwi cr6,r11,0\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x8294c7d0\n"
        "\t// bdzf 4*cr6+eq,0x8294c858\n"
        "\t// bdzf 4*cr6+eq,0x8294c8b4\n"
        "\t// bdzf 4*cr6+eq,0x8294c8ec\n"
        "\t// bdzf 4*cr6+eq,0x8294c930\n"
        "\t// bne cr6,0x8294c994\n"
        "\tif (!cr6.eq) goto loc_8294C994;"
    )

    repl = (
        "\tswitch (ctx.r11.u32) {\n"
        "\tcase 0: break;\n"
        "\tcase 1: goto loc_8294C7D0;\n"
        "\tcase 2: goto loc_8294C858;\n"
        "\tcase 3: goto loc_8294C8B4;\n"
        "\tcase 4: goto loc_8294C8EC;\n"
        "\tcase 5: goto loc_8294C930;\n"
        "\tdefault: goto loc_8294C994;\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.101 sub_8294C6C0 switch in {path}")
        return True
    return False

def patch_recomp10_event_loop_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// mr r31,r29\n"
        "\tctx.r31.u64 = ctx.r29.u64;\n"
        "\t// cmplw cr6,r30,r29\n"
        "\tcr6.compare<uint32_t>(ctx.r30.u32, ctx.r29.u32, ctx.xer);\n"
        "\t// bne cr6,0x822da7b8\n"
        "\tif (!cr6.eq) goto loc_822DA7B8;"
    )
    repl = (
        "\t// mr r31,r29\n"
        "\tctx.r31.u64 = ctx.r29.u64;\n"
        "\t// cmplw cr6,r30,r29\n"
        "\tcr6.compare<uint32_t>(ctx.r30.u32, ctx.r29.u32, ctx.xer);\n"
        "\t// bne cr6,0x822da7b8 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount2DA770 = 0;\n"
        "\t\tif (++s_loopCount2DA770 > 64) {\n"
        "\t\t\ts_loopCount2DA770 = 0;\n"
        "\t\t\tgoto loc_822DA834;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_822DA7B8;\n"
        "\t\ts_loopCount2DA770 = 0;\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.10 event loop guard in {path}")
        return True
    return False

def patch_recomp27_event_loop_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// bl 0x82fc9798\n"
        "\tctx.lr = 0x824BBC18;\n"
        "\tsub_82FC9798(ctx, base);\n"
        "\t// b 0x824bbb70\n"
        "\tgoto loc_824BBB70;"
    )
    repl = (
        "\t// bl 0x82fc9798\n"
        "\tctx.lr = 0x824BBC18;\n"
        "\tsub_82FC9798(ctx, base);\n"
        "\t// b 0x824bbb70 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount4BBB30 = 0;\n"
        "\t\tif (++s_loopCount4BBB30 > 64) {\n"
        "\t\t\ts_loopCount4BBB30 = 0;\n"
        "\t\t\tgoto loc_824BBC1C;\n"
        "\t\t}\n"
        "\t\tgoto loc_824BBB70;\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.27 event loop guard in {path}")
        return True
    return False

def patch_recomp29_node_loop_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// lwz r31,4(r31)\n"
        "\tctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);\n"
        "\t// cmplw cr6,r27,r31\n"
        "\tcr6.compare<uint32_t>(ctx.r27.u32, ctx.r31.u32, ctx.xer);\n"
        "\t// bne cr6,0x824dbe18\n"
        "\tif (!cr6.eq) goto loc_824DBE18;"
    )
    repl = (
        "\t// lwz r31,4(r31)\n"
        "\tctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);\n"
        "\t// cmplw cr6,r27,r31\n"
        "\tcr6.compare<uint32_t>(ctx.r27.u32, ctx.r31.u32, ctx.xer);\n"
        "\t// bne cr6,0x824dbe18 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountDBE18 = 0;\n"
        "\t\tif (++s_loopCountDBE18 > 64) {\n"
        "\t\t\ts_loopCountDBE18 = 0;\n"
        "\t\t\tgoto loc_824DBEC8;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_824DBE18;\n"
        "\t\ts_loopCountDBE18 = 0;\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.29 node loop guard in {path}")
        return True
    return False

def patch_recomp33_sub_8251ED60(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target_loop = (
        "\t// lwz r30,0(r30)\n"
        "\tctx.r30.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);\n"
        "\t// bne cr6,0x8251ed9c\n"
        "\tif (!cr6.eq) goto loc_8251ED9C;"
    )
    repl_loop = (
        "\t// lwz r30,0(r30)\n"
        "\tctx.r30.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);\n"
        "\t// bne cr6,0x8251ed9c with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountED9C = 0;\n"
        "\t\tif (++s_loopCountED9C > 64) {\n"
        "\t\t\ts_loopCountED9C = 0;\n"
        "\t\t\tgoto loc_8251EDB4;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_8251ED9C;\n"
        "\t\ts_loopCountED9C = 0;\n"
        "\t}"
    )

    target_switch = (
        "\t// mtctr r10\n"
        "\tctr.u64 = ctx.r10.u64;\n"
        "\t// cmpwi cr6,r10,0\n"
        "\tcr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);\n"
        "\t// bdzf 4*cr6+eq,0x8251ee14\n"
        "\t// bdzf 4*cr6+eq,0x8251ee4c\n"
        "\t// bdzf 4*cr6+eq,0x8251ee98\n"
        "\t// bdzf 4*cr6+eq,0x8251eebc\n"
        "\t// bne cr6,0x8251eefc\n"
        "\tif (!cr6.eq) goto loc_8251EEFC;"
    )
    repl_switch = (
        "\tswitch (ctx.r10.u32) {\n"
        "\tcase 0: break;\n"
        "\tcase 1: goto loc_8251EE14;\n"
        "\tcase 2: goto loc_8251EE4C;\n"
        "\tcase 3: goto loc_8251EE98;\n"
        "\tcase 4: goto loc_8251EEBC;\n"
        "\tcase 5: goto loc_8251EEFC;\n"
        "\tdefault: goto loc_8251EF84;\n"
        "\t}"
    )

    patched = False
    if target_loop in text:
        text = text.replace(target_loop, repl_loop)
        patched = True
    if target_switch in text:
        text = text.replace(target_switch, repl_switch)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.33 sub_8251ED60 loop and switch in {path}")
        return True
    return False

def patch_recomp8_session_loop_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_822996A0:\n"
        "\t// mr r31,r26\n"
        "\tctx.r31.u64 = ctx.r26.u64;\n"
        "\t// lwz r26,60(r26)\n"
        "\tctx.r26.u64 = PPC_LOAD_U32(ctx.r26.u32 + 60);\n"
        "\t// cmplw cr6,r29,r31\n"
        "\tcr6.compare<uint32_t>(ctx.r29.u32, ctx.r31.u32, ctx.xer);\n"
        "\t// bne cr6,0x82299610\n"
        "\tif (!cr6.eq) goto loc_82299610;"
    )
    repl = (
        "loc_822996A0:\n"
        "\t// mr r31,r26\n"
        "\tctx.r31.u64 = ctx.r26.u64;\n"
        "\t// lwz r26,60(r26)\n"
        "\tctx.r26.u64 = PPC_LOAD_U32(ctx.r26.u32 + 60);\n"
        "\t// cmplw cr6,r29,r31\n"
        "\tcr6.compare<uint32_t>(ctx.r29.u32, ctx.r31.u32, ctx.xer);\n"
        "\t// bne cr6,0x82299610 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount299610 = 0;\n"
        "\t\tif (++s_loopCount299610 > 64) {\n"
        "\t\t\ts_loopCount299610 = 0;\n"
        "\t\t\tgoto loc_8229982C;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_82299610;\n"
        "\t\ts_loopCount299610 = 0;\n"
        "\t}"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.8 session loop guard in {path}")
        return True
    return False

def patch_recomp47_sub_826060C0_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// bl 0x825fc2d8\n"
        "\tctx.lr = 0x826060E8;\n"
        "\tsub_825FC2D8(ctx, base);\n"
        "\t// clrlwi r29,r3,24\n"
        "\tctx.r29.u64 = ctx.r3.u32 & 0xFF;\n"
        "\t// cmplwi cr6,r29,0\n"
        "\tcr6.compare<uint32_t>(ctx.r29.u32, 0, ctx.xer);"
    )
    repl = (
        "\t// bl 0x825fc2d8\n"
        "\tctx.lr = 0x826060E8;\n"
        "\tsub_825FC2D8(ctx, base);\n"
        "\t// clrlwi r29,r3,24\n"
        "\tctx.r29.u64 = ctx.r3.u32 & 0xFF;\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount6060DC = 0;\n"
        "\t\tif (++s_loopCount6060DC > 5) {\n"
        "\t\t\ts_loopCount6060DC = 0;\n"
        "\t\t\tctx.r29.u64 = 1;\n"
        "\t\t}\n"
        "\t}\n"
        "\t// cmplwi cr6,r29,0\n"
        "\tcr6.compare<uint32_t>(ctx.r29.u32, 0, ctx.xer);"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.47 sub_826060C0 loop guard in {path}")
        return True
    return False

def patch_recomp38_stage_loop_guards(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    target1 = (
        "loc_82567F90:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82567F98;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x82567F9C;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x82945778\n"
        "\tctx.lr = 0x82567FA0;\n"
        "\tsub_82945778(ctx, base);\n"
        "\t// cmpwi cr6,r3,0\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82567f90\n"
        "\tif (cr6.eq) goto loc_82567F90;"
    )
    repl1 = (
        "loc_82567F90:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82567F98;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x82567F9C;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x82945778\n"
        "\tctx.lr = 0x82567FA0;\n"
        "\tsub_82945778(ctx, base);\n"
        "\t// cmpwi cr6,r3,0 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount567F90 = 0;\n"
        "\t\tif (++s_loopCount567F90 > 5) {\n"
        "\t\t\ts_loopCount567F90 = 0;\n"
        "\t\t\tctx.r3.s64 = 1;\n"
        "\t\t}\n"
        "\t}\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82567f90\n"
        "\tif (cr6.eq) goto loc_82567F90;"
    )

    target2 = (
        "loc_82568024:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x8256802C;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x82568030;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x82945638\n"
        "\tctx.lr = 0x82568034;\n"
        "\tsub_82945638(ctx, base);\n"
        "\t// cmpwi cr6,r3,0\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82568024\n"
        "\tif (cr6.eq) goto loc_82568024;"
    )
    repl2 = (
        "loc_82568024:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x8256802C;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x82568030;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x82945638\n"
        "\tctx.lr = 0x82568034;\n"
        "\tsub_82945638(ctx, base);\n"
        "\t// cmpwi cr6,r3,0 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount568024 = 0;\n"
        "\t\tif (++s_loopCount568024 > 5) {\n"
        "\t\t\ts_loopCount568024 = 0;\n"
        "\t\t\tctx.r3.s64 = 1;\n"
        "\t\t}\n"
        "\t}\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82568024\n"
        "\tif (cr6.eq) goto loc_82568024;"
    )

    target3 = (
        "loc_82568048:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82568050;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x82568054;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x82951c60\n"
        "\tctx.lr = 0x82568058;\n"
        "\tsub_82951C60(ctx, base);\n"
        "\t// cmpwi cr6,r3,0\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82568048\n"
        "\tif (cr6.eq) goto loc_82568048;"
    )
    repl3 = (
        "loc_82568048:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x82568050;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x82568054;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x82951c60\n"
        "\tctx.lr = 0x82568058;\n"
        "\tsub_82951C60(ctx, base);\n"
        "\t// cmpwi cr6,r3,0 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount568048 = 0;\n"
        "\t\tif (++s_loopCount568048 > 5) {\n"
        "\t\t\ts_loopCount568048 = 0;\n"
        "\t\t\tctx.r3.s64 = 2;\n"
        "\t\t}\n"
        "\t}\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);\n"
        "\t// beq cr6,0x82568048\n"
        "\tif (cr6.eq) goto loc_82568048;"
    )

    if target1 in text:
        text = text.replace(target1, repl1)
        patched = True
    if target2 in text:
        text = text.replace(target2, repl2)
        patched = True
    if target3 in text:
        text = text.replace(target3, repl3)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.38 stage loop guards in {path}")
        return True
    return False

def patch_recomp39_stage8_loop_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_8256DC48:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x8256DC50;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x8256DC54;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x829541c0\n"
        "\tctx.lr = 0x8256DC58;\n"
        "\tsub_829541C0(ctx, base);\n"
        "\t// mr r30,r3\n"
        "\tctx.r30.u64 = ctx.r3.u64;\n"
        "\t// cmpwi cr6,r3,1\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 1, ctx.xer);\n"
        "\t// beq cr6,0x8256dc48\n"
        "\tif (cr6.eq) goto loc_8256DC48;"
    )
    repl = (
        "loc_8256DC48:\n"
        "\t// li r3,1\n"
        "\tctx.r3.s64 = 1;\n"
        "\t// bl 0x82548de0\n"
        "\tctx.lr = 0x8256DC50;\n"
        "\tsub_82548DE0(ctx, base);\n"
        "\t// bl 0x825628c8\n"
        "\tctx.lr = 0x8256DC54;\n"
        "\tsub_825628C8(ctx, base);\n"
        "\t// bl 0x829541c0\n"
        "\tctx.lr = 0x8256DC58;\n"
        "\tsub_829541C0(ctx, base);\n"
        "\t// mr r30,r3\n"
        "\tctx.r30.u64 = ctx.r3.u64;\n"
        "\t// cmpwi cr6,r3,1 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount56DC48 = 0;\n"
        "\t\tif (++s_loopCount56DC48 > 5) {\n"
        "\t\t\ts_loopCount56DC48 = 0;\n"
        "\t\t\tctx.r3.s64 = 0;\n"
        "\t\t}\n"
        "\t}\n"
        "\tcr6.compare<int32_t>(ctx.r3.s32, 1, ctx.xer);\n"
        "\t// beq cr6,0x8256dc48\n"
        "\tif (cr6.eq) goto loc_8256DC48;"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.39 stage 8 loop guard in {path}")
        return True
    return False

def patch_recomp212_sub_82F0F078_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    target1 = (
        "\t// cmpw cr6,r11,r7\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, ctx.r7.s32, ctx.xer);\n"
        "\t// add r8,r10,r9\n"
        "\tctx.r8.u64 = ctx.r10.u64 + ctx.r9.u64;\n"
        "\t// blt cr6,0x82f0f0ec\n"
        "\tif (cr6.lt) goto loc_82F0F0EC;"
    )
    repl1 = (
        "\t// cmpw cr6,r11,r7\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, ctx.r7.s32, ctx.xer);\n"
        "\t// add r8,r10,r9\n"
        "\tctx.r8.u64 = ctx.r10.u64 + ctx.r9.u64;\n"
        "\t// blt cr6,0x82f0f0ec with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountF0F0EC = 0;\n"
        "\t\tif (++s_loopCountF0F0EC > 64) {\n"
        "\t\t\ts_loopCountF0F0EC = 0;\n"
        "\t\t\tgoto loc_82F0F10C;\n"
        "\t\t}\n"
        "\t\tif (cr6.lt) goto loc_82F0F0EC;\n"
        "\t\ts_loopCountF0F0EC = 0;\n"
        "\t}"
    )

    target2 = (
        "loc_82F0F110:\n"
        "\t// lwz r11,140(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 140);\n"
        "\t// cmpwi cr6,r11,-1\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, -1, ctx.xer);\n"
        "\t// bne cr6,0x82f0f0b8\n"
        "\tif (!cr6.eq) goto loc_82F0F0B8;"
    )
    repl2 = (
        "loc_82F0F110:\n"
        "\t// lwz r11,140(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 140);\n"
        "\t// cmpwi cr6,r11,-1\n"
        "\tcr6.compare<int32_t>(ctx.r11.s32, -1, ctx.xer);\n"
        "\t// bne cr6,0x82f0f0b8 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountF0F0B8 = 0;\n"
        "\t\tif (++s_loopCountF0F0B8 > 64) {\n"
        "\t\t\ts_loopCountF0F0B8 = 0;\n"
        "\t\t\tgoto loc_82F0F11C;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_82F0F0B8;\n"
        "\t\ts_loopCountF0F0B8 = 0;\n"
        "\t}"
    )

    if target1 in text:
        text = text.replace(target1, repl1)
        patched = True
    if target2 in text:
        text = text.replace(target2, repl2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.212 sub_82F0F078 loop guard in {path}")
        return True
    return False

def patch_recomp51_sub_82626FE8_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "PPC_FUNC_IMPL(__imp__sub_82626FE8) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tPPCCRRegister cr6{};\n"
        "\t// lhz r11,4(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 4);"
    )
    repl = (
        "PPC_FUNC_IMPL(__imp__sub_82626FE8) {\n"
        "\tPPC_FUNC_PROLOGUE();\n"
        "\tif (ctx.r3.u32 < 0x10000 || ctx.r3.u32 >= PPC_MEMORY_SIZE - 8) return;\n"
        "\tif (ctx.r4.u32 < 0x10000 || ctx.r4.u32 >= PPC_MEMORY_SIZE - 4) return;\n"
        "\tuint32_t tableData_8262 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\tif (tableData_8262 < 0x10000 || tableData_8262 >= PPC_MEMORY_SIZE - 4) return;\n"
        "\tPPCCRRegister cr6{};\n"
        "\t// lhz r11,4(r3)\n"
        "\tctx.r11.u64 = PPC_LOAD_U16(ctx.r3.u32 + 4);"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.51 sub_82626FE8 guard in {path}")
        return True
    return False

def patch_recomp118_sub_82A13988_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "loc_82A139B8:\n"
        "\t// lwz r11,0(r29)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r29.u32 + 0);\n"
        "\t// addi r4,r1,80\n"
        "\tctx.r4.s64 = ctx.r1.s64 + 80;\n"
        "\t// lwzx r11,r30,r11\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r11.u32);\n"
        "\t// addi r3,r11,4\n"
        "\tctx.r3.s64 = ctx.r11.s64 + 4;\n"
        "\t// bl 0x82626fe8\n"
        "\tctx.lr = 0x82A139CC;\n"
        "\tsub_82626FE8(ctx, base);"
    )
    repl = (
        "loc_82A139B8:\n"
        "\tif (ctx.r29.u32 < 0x10000 || ctx.r29.u32 >= PPC_MEMORY_SIZE - 4) goto loc_82A139D8;\n"
        "\t// lwz r11,0(r29)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r29.u32 + 0);\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_82A139D8;\n"
        "\t// addi r4,r1,80\n"
        "\tctx.r4.s64 = ctx.r1.s64 + 80;\n"
        "\t// lwzx r11,r30,r11\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r11.u32);\n"
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 8) goto loc_82A139D8;\n"
        "\t// addi r3,r11,4\n"
        "\tctx.r3.s64 = ctx.r11.s64 + 4;\n"
        "\t// bl 0x82626fe8\n"
        "\tctx.lr = 0x82A139CC;\n"
        "\tsub_82626FE8(ctx, base);"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.118 sub_82A13988 guard in {path}")
        return True
    return False

def patch_recomp31_sub_824F8A48_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// sth r9,0(r27)\n"
        "\tPPC_STORE_U16(ctx.r27.u32 + 0, ctx.r9.u16);\n"
        "\t// bne cr6,0x824f8aac\n"
        "\tif (!cr6.eq) goto loc_824F8AAC;\n"
        "loc_824F8BA4:"
    )
    repl = (
        "\t// sth r9,0(r27)\n"
        "\tPPC_STORE_U16(ctx.r27.u32 + 0, ctx.r9.u16);\n"
        "\t// bne cr6,0x824f8aac with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCountF8AAC = 0;\n"
        "\t\tif (++s_loopCountF8AAC > 64) {\n"
        "\t\t\ts_loopCountF8AAC = 0;\n"
        "\t\t\tgoto loc_824F8BA4;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_824F8AAC;\n"
        "\t\ts_loopCountF8AAC = 0;\n"
        "\t}\n"
        "loc_824F8BA4:"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.31 sub_824F8A48 guard in {path}")
        return True
    return False

def patch_recomp20_sub_823DF6A8_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()
    patched = False

    target1 = (
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x823df6f8\n"
        "\tif (!cr6.eq) goto loc_823DF6F8;"
    )
    repl1 = (
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_823DF730;\n"
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x823df6f8 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount3DF6F8 = 0;\n"
        "\t\tif (++s_loopCount3DF6F8 > 128) {\n"
        "\t\t\ts_loopCount3DF6F8 = 0;\n"
        "\t\t\tgoto loc_823DF730;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_823DF6F8;\n"
        "\t\ts_loopCount3DF6F8 = 0;\n"
        "\t}"
    )

    target2 = (
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x823df74c\n"
        "\tif (!cr6.eq) goto loc_823DF74C;"
    )
    repl2 = (
        "\tif (ctx.r11.u32 < 0x10000 || ctx.r11.u32 >= PPC_MEMORY_SIZE - 4) goto loc_823DF730;\n"
        "\t// lwz r11,0(r11)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 0);\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x823df74c with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount3DF74C = 0;\n"
        "\t\tif (++s_loopCount3DF74C > 128) {\n"
        "\t\t\ts_loopCount3DF74C = 0;\n"
        "\t\t\tgoto loc_823DF730;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_823DF74C;\n"
        "\t\ts_loopCount3DF74C = 0;\n"
        "\t}"
    )

    if target1 in text:
        text = text.replace(target1, repl1)
        patched = True
    if target2 in text:
        text = text.replace(target2, repl2)
        patched = True

    if patched:
        path.write_text(text)
        print(f"Patched recomp.20 sub_823DF6A8 guard in {path}")
        return True
    return False

def patch_recomp106_sub_8299BD70_loop(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target = (
        "\t// clrlwi r11,r3,24\n"
        "\tctx.r11.u64 = ctx.r3.u32 & 0xFF;\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x8299c020\n"
        "\tif (!cr6.eq) goto loc_8299C020;\n"
        "loc_8299C03C:"
    )
    repl = (
        "\t// clrlwi r11,r3,24\n"
        "\tctx.r11.u64 = ctx.r3.u32 & 0xFF;\n"
        "\t// cmplwi cr6,r11,0\n"
        "\tcr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);\n"
        "\t// bne cr6,0x8299c020 with loop guard\n"
        "\t{\n"
        "\t\tstatic thread_local int s_loopCount99C020 = 0;\n"
        "\t\tif (++s_loopCount99C020 > 64) {\n"
        "\t\t\ts_loopCount99C020 = 0;\n"
        "\t\t\tgoto loc_8299C03C;\n"
        "\t\t}\n"
        "\t\tif (!cr6.eq) goto loc_8299C020;\n"
        "\t\ts_loopCount99C020 = 0;\n"
        "\t}\n"
        "loc_8299C03C:"
    )

    if target in text:
        text = text.replace(target, repl)
        path.write_text(text)
        print(f"Patched recomp.106 sub_8299BD70 loop in {path}")
        return True
    return False

def patch_recomp159_sub_82C78900_guard(path: pathlib.Path) -> bool:
    if not path.exists():
        return False
    text = path.read_text()

    target_159 = (
        "\t// lwz r3,-20708(r10)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + -20708);\n"
        "\t// mr r6,r29\n"
        "\tctx.r6.u64 = ctx.r29.u64;\n"
        "\t// li r5,0\n"
        "\tctx.r5.s64 = 0;\n"
        "\t// lwz r7,0(r3)\n"
        "\tctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t// lwz r11,28(r7)\n"
        "\tctx.r11.u64 = PPC_LOAD_U32(ctx.r7.u32 + 28);\n"
        "\t// mtctr r11\n"
        "\tctr.u64 = ctx.r11.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x82C78AA4;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t// lwzx r3,r30,r31\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r31.u32);\n"
        "\t// li r4,6\n"
        "\tctx.r4.s64 = 6;\n"
        "\t// lwz r10,0(r3)\n"
        "\tctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t// lwz r9,44(r10)\n"
        "\tctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 44);\n"
        "\t// mtctr r9\n"
        "\tctr.u64 = ctx.r9.u64;\n"
        "\t// bctrl \n"
        "\tctx.lr = 0x82C78ABC;\n"
        "\tPPC_CALL_INDIRECT_FUNC(ctr.u32);"
    )
    repl_159 = (
        "\t// lwz r3,-20708(r10)\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r10.u32 + -20708);\n"
        "\t// mr r6,r29\n"
        "\tctx.r6.u64 = ctx.r29.u64;\n"
        "\t// li r5,0\n"
        "\tctx.r5.s64 = 0;\n"
        "\tif (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 32) {\n"
        "\t\tctx.r7.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t\tif (ctx.r7.u32 >= 0x10000 && ctx.r7.u32 < PPC_MEMORY_SIZE - 32) {\n"
        "\t\t\tctx.r11.u64 = PPC_LOAD_U32(ctx.r7.u32 + 28);\n"
        "\t\t\tctr.u64 = ctx.r11.u64;\n"
        "\t\t\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < PPC_CODE_BASE + PPC_CODE_SIZE) {\n"
        "\t\t\t\tctx.lr = 0x82C78AA4;\n"
        "\t\t\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t\t\t}\n"
        "\t\t}\n"
        "\t}\n"
        "\t// lwzx r3,r30,r31\n"
        "\tctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + ctx.r31.u32);\n"
        "\t// li r4,6\n"
        "\tctx.r4.s64 = 6;\n"
        "\tif (ctx.r3.u32 >= 0x10000 && ctx.r3.u32 < PPC_MEMORY_SIZE - 48) {\n"
        "\t\tctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);\n"
        "\t\tif (ctx.r10.u32 >= 0x10000 && ctx.r10.u32 < PPC_MEMORY_SIZE - 48) {\n"
        "\t\t\tctx.r9.u64 = PPC_LOAD_U32(ctx.r10.u32 + 44);\n"
        "\t\t\tctr.u64 = ctx.r9.u64;\n"
        "\t\t\tif (ctr.u32 >= PPC_CODE_BASE && ctr.u32 < PPC_CODE_BASE + PPC_CODE_SIZE) {\n"
        "\t\t\t\tctx.lr = 0x82C78ABC;\n"
        "\t\t\t\tPPC_CALL_INDIRECT_FUNC(ctr.u32);\n"
        "\t\t\t}\n"
        "\t\t}\n"
        "\t}"
    )

    if target_159 in text:
        text = text.replace(target_159, repl_159)
        path.write_text(text)
        print(f"Patched recomp.159 sub_82C78900 guard in {path}")
        return True
    return False

if __name__ == "__main__":
    raise SystemExit(main())
