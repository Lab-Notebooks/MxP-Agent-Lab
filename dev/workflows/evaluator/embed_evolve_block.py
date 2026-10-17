#!/usr/bin/env python3
"""embed_evolve_block.py -- put // EVOLVE-BLOCK-START/END around C/C++ functions.

    python3 dev/workflows/evaluator/embed_evolve_block.py FILE --functions NAME [NAME ...]
    python3 dev/workflows/evaluator/embed_evolve_block.py FILE --list

The named function definitions (found by name + brace matching, comments and
strings ignored) must be contiguous in FILE, apart from blank lines and
comments between them; one block is wrapped around the whole run.
Idempotent: if every named function already sits inside a block, the file is
left alone.  --list prints the existing blocks and the functions in each.
"""

import argparse
import re
import sys

START = "// EVOLVE-BLOCK-START"
END = "// EVOLVE-BLOCK-END"


def _mask(src):
    """src with comments and string/char literals replaced by spaces (same offsets)."""
    out, i, n = list(src), 0, len(src)
    while i < n:
        two = src[i:i + 2]
        if two == "//":
            j = src.find("\n", i)
            j = n if j < 0 else j
        elif two == "/*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
        elif src[i] in "\"'":
            q, j = src[i], i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == "\\" else 1
            j += 1
        else:
            i += 1
            continue
        for k in range(i, min(j, n)):
            if out[k] != "\n":
                out[k] = " "
        i = j
    return "".join(out)


def find_function(src, masked, name):
    """(start_offset_of_line, end_offset_after_closing_brace) of the definition."""
    for m in re.finditer(r"\b%s\s*\(" % re.escape(name), masked):
        # skip to the matching ')' then require '{' (a definition, not a call/decl)
        depth, k = 0, m.end() - 1
        while k < len(masked):
            depth += {"(": 1, ")": -1}.get(masked[k], 0)
            if depth == 0:
                break
            k += 1
        rest = re.match(r"\s*(const\s*)?(noexcept\s*)?\{", masked[k + 1:])
        if not rest:
            continue
        line_start = src.rfind("\n", 0, m.start()) + 1
        # the return type may sit on previous lines (template<...>, attributes)
        while line_start > 0:
            prev = src.rfind("\n", 0, line_start - 1) + 1
            prev_line = masked[prev:line_start - 1].strip()
            if prev_line and not prev_line.endswith((";", "}", "{")) and not prev_line.startswith("#"):
                line_start = prev
            else:
                break
        b = k + 1 + rest.end() - 1
        depth = 0
        for e in range(b, len(masked)):
            depth += {"{": 1, "}": -1}.get(masked[e], 0)
            if depth == 0:
                end = src.find("\n", e)
                return line_start, (len(src) if end < 0 else end + 1)
    return None


def blocks(src):
    return [(m.start(), src.index(END, m.end())) for m in re.finditer(re.escape(START), src)]


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--functions", nargs="+")
    g.add_argument("--list", action="store_true")
    a = ap.parse_args(argv[1:])

    src = open(a.file, encoding="utf-8").read()
    masked = _mask(src)
    existing = blocks(src)

    if a.list:
        for n, (s, e) in enumerate(existing, 1):
            names = re.findall(r"^[\w:<>,\s\*&]*?\b(\w+)\s*\([^;{]*\)\s*\{", masked[s:e], re.MULTILINE)
            names = [x for x in names if x not in ("if", "for", "while", "switch", "catch", "return")]
            print("block %d: lines %d-%d: %s" % (n, src.count("\n", 0, s) + 1, src.count("\n", 0, e) + 1,
                                                ", ".join(names) or "(no functions)"))
        if not existing:
            print("no EVOLVE-BLOCK in %s" % a.file)
        return 0

    spans = []
    for name in a.functions:
        span = find_function(src, masked, name)
        if span is None:
            sys.stderr.write("embed: no definition of '%s' in %s\n" % (name, a.file))
            return 1
        spans.append((span, name))
    inside = [any(s <= sp[0] and sp[1] <= e for s, e in existing) for sp, _ in spans]
    if all(inside):
        print("embed: %s already inside EVOLVE-BLOCK(s); unchanged" % ", ".join(a.functions))
        return 0
    if any(inside):
        sys.stderr.write("embed: some functions are already in a block, some not; fix by hand\n")
        return 1

    spans.sort()
    lo, hi = spans[0][0][0], spans[-1][0][1]
    for (s0, e0), (s1, _) in zip([sp for sp, _ in spans], [sp for sp, _ in spans][1:]):
        if masked[e0:s1].strip():
            sys.stderr.write("embed: functions are not contiguous (code between them); "
                             "reorder or embed separately\n")
            return 1
    if any(s < hi and lo < e for s, e in existing):
        sys.stderr.write("embed: range overlaps an existing block\n")
        return 1

    new = src[:lo] + START + "\n" + src[lo:hi] + END + "\n" + src[hi:]
    open(a.file, "w", encoding="utf-8").write(new)
    print("embed: wrapped %s in %s (lines %d-%d)" % (", ".join(n for _, n in spans), a.file,
                                                     new.count("\n", 0, lo) + 1,
                                                     new.count("\n", 0, hi) + 3))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
