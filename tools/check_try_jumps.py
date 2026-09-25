#!/usr/bin/env python3
"""Refuse a jump out of an FL_TRY body.

FL_TRY opens a `do {` and pushes an exception environment; the clause macros and
FL_END_TRY pop it again. A `return` or `goto` that leaves the region between FL_TRY
and its FL_END_TRY skips that pop, so the thread's environment stack keeps a pointer
to a dead stack frame and the next throw longjmps into it. A `break` or `continue`
written directly in a clause body binds to FL_TRY's own `while (0)` rather than to
any enclosing loop, which exits the region the same way.

Nothing in the language diagnoses either one: both compile, and the damage shows up
later as a jump into a destroyed frame. This pass is the diagnosis.

Not reported, because neither leaves the region:
  - a `break` or `continue` inside a loop or switch nested in the body, which that
    construct binds;
  - a `goto` whose label is defined inside the same region.

Usage: check_try_jumps.py [root]   (default: the repository root)
Exit status: 0 when clean, 1 when a jump is found, 2 on a usage error.
"""

import os
import re
import subprocess
import sys

SOURCE_SUFFIXES = (".c", ".h", ".cpp", ".hpp")

# Vendored, generated, and build-output trees. dist/ and build/dist/ hold per-service
# copies of the same headers, so a finding there would duplicate its original.
EXCLUDED_PREFIXES = ("third_party/", "dist/", "build/dist/", "target/", "morgue/")

TOKEN = re.compile(r"[A-Za-z_][A-Za-z_0-9]*|[{}:]")

# Reported wherever they appear in the region: both can leave it from any depth.
ESCAPING = ("return", "goto")

# Reported only at the depth of a clause body, where FL_TRY's own while (0) binds them.
LOOP_JUMPS = ("break", "continue")

# An identifier followed by ':' that introduces something other than a goto label.
NOT_A_LABEL = ("case", "default", "public", "private", "protected")


def strip_noncode(text):
    """Blank out comments, string and character literals, and preprocessor lines.

    Every removed character becomes a space and every newline is kept, so line
    numbers and brace nesting in the result still match the original file. The
    preprocessor lines go because the FL_TRY family is *defined* with `#define`, and
    a definition is not a call site.
    """
    out = []
    i = 0
    n = len(text)
    at_line_start = True

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if ch == "\n":
            out.append(ch)
            at_line_start = True
            i += 1
            continue

        if ch == "#" and at_line_start:
            # Consume the whole logical line, backslash continuations included.
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            while out and out[-1] == " " and i < n and _continued(out, text, i):
                out.append("\n")
                i += 1
                while i < n and text[i] != "\n":
                    out.append(" ")
                    i += 1
            continue

        if not ch.isspace():
            at_line_start = False

        if ch == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue

        if ch == "/" and nxt == "*":
            out.append("  ")
            i += 2
            while i < n and text[i : i + 2] != "*/":
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
            continue

        if ch == '"' or ch == "'":
            quote = ch
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    out.append("  ")
                    i += 2
                    continue
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append(" ")
            i += 1
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def _continued(out, text, i):
    """True when the preprocessor line just consumed ended in a backslash."""
    end = i
    while end > 0 and text[end - 1] in " \t":
        end -= 1
    return end > 0 and text[end - 1] == "\\"


def line_index(code):
    """Return a function mapping a character offset to a 1-based line number."""
    newlines = [m.start() for m in re.finditer(r"\n", code)]

    def line_of(offset):
        lo, hi = 0, len(newlines)
        while lo < hi:
            mid = (lo + hi) // 2
            if newlines[mid] < offset:
                lo = mid + 1
            else:
                hi = mid
        return lo + 1

    return line_of


def scan(text):
    """Return sorted (line, keyword) for every jump that leaves an FL_TRY region."""
    code = strip_noncode(text)
    line_of = line_index(code)
    tokens = list(TOKEN.finditer(code))

    findings = []
    depth = 0
    frames = []  # brace depth in force where each still-open FL_TRY was seen
    labels = []  # label names defined inside each open region
    gotos = []  # (line, target) recorded per open region

    for index, match in enumerate(tokens):
        tok = match.group(0)

        if tok == "{":
            depth += 1
            continue

        if tok == "}":
            depth -= 1
            continue

        if tok == "FL_TRY":
            frames.append(depth)
            labels.append(set())
            gotos.append([])
            continue

        if tok == "FL_END_TRY":
            if not frames:
                continue

            frames.pop()
            defined = labels.pop()
            for line, target in gotos.pop():
                if target not in defined:
                    findings.append((line, "goto"))
            continue

        if not frames:
            continue

        following = tokens[index + 1].group(0) if index + 1 < len(tokens) else ""

        # A label definition is IDENT ':' -- but not a case label or an access
        # specifier, and not the ':' of a ternary, which never follows a bare
        # identifier that is also a statement start.
        if following == ":" and tok not in NOT_A_LABEL and tok not in ESCAPING:
            labels[-1].add(tok)
            continue

        if tok == "goto":
            # Resolved at FL_END_TRY, so a label defined later in the region counts.
            gotos[-1].append((line_of(match.start()), following))
            continue

        if tok == "return":
            findings.append((line_of(match.start()), tok))
            continue

        if tok in LOOP_JUMPS and depth == frames[-1] + 1:
            findings.append((line_of(match.start()), tok))

    return sorted(set(findings))


def repo_files(root):
    try:
        listing = subprocess.run(
            ["git", "-C", root, "ls-files", "*.c", "*.h", "*.cpp", "*.hpp"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.split()
    except (OSError, subprocess.CalledProcessError):
        listing = []
        for dirpath, _, names in os.walk(root):
            for name in names:
                if name.endswith(SOURCE_SUFFIXES):
                    rel = os.path.relpath(os.path.join(dirpath, name), root)
                    listing.append(rel.replace(os.sep, "/"))

    return [f for f in listing if not f.startswith(EXCLUDED_PREFIXES)]


def main(argv):
    if len(argv) > 2:
        print(__doc__, file=sys.stderr)
        return 2

    default_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    root = argv[1] if len(argv) > 1 else default_root
    if not os.path.isdir(root):
        print("check_try_jumps: not a directory: " + root, file=sys.stderr)
        return 2

    total = 0
    for rel in repo_files(root):
        path = os.path.join(root, rel)
        try:
            with open(path, encoding="utf-8", errors="replace") as handle:
                text = handle.read()
        except OSError as err:
            print("check_try_jumps: " + rel + ": " + str(err), file=sys.stderr)
            return 2

        if "FL_TRY" not in text:
            continue

        for line, keyword in scan(text):
            # MSVC's diagnostic layout, so an editor jumps to the site from the log.
            print(rel + "(" + str(line) + ") : error : '" + keyword
                  + "' leaves an FL_TRY body")
            total += 1

    if total:
        print("check_try_jumps: " + str(total) + " jump(s) out of an FL_TRY body",
              file=sys.stderr)
        return 1

    print("check_try_jumps: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
