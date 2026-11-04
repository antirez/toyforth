"""Combine static word counts with the interpreter's native runtime reports."""

from collections import Counter
import csv
from html import escape
import math
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
PROFILING = Path(__file__).resolve().parent
EXAMPLES = [ROOT / "examples" / name for name in ("pi.toy", "pi_golf.toy")]
CONTROLS = {"do", "loop", "i", "j", "k", "if", "else", "then"}
EXPECTED = "314159265358979323846264338327"
BEGIN = "<!-- pi-usage:start -->"
END = "<!-- pi-usage:end -->"
TOKEN = re.compile(r"(?<![^\s;])[+-]\d+|\d+|[A-Za-z_][A-Za-z_0-9]*|[:;+*/%=-]")


def source_counts(path):
    tokens = TOKEN.findall(path.read_text())
    definitions = {tokens[i + 1] for i, word in enumerate(tokens) if word == ":"}
    counts = Counter(
        word for i, word in enumerate(tokens)
        if word not in {":", ";"}
        and (i == 0 or tokens[i - 1] != ":")
        and not re.fullmatch(r"[+-]?\d+", word)
    )
    return counts, definitions


def runtime_counts(path):
    output = (PROFILING / f"{path.stem}.stdout.txt").read_text()
    if output.splitlines()[1:] != [EXPECTED, "Stack content at end: []"]:
        raise RuntimeError(f"{path.name} did not print exactly 30 digits and an empty stack.")
    counts = Counter()
    lines = (PROFILING / f"{path.stem}.profile.txt").read_text().splitlines()
    for line in lines[1:]:
        word, calls, total_ms, avg_us = line.split()
        counts[word] = int(calls)
    if counts["pi_digits"] != 1 or counts["emit"] != 31:
        raise RuntimeError("Incomplete profiling data.")
    return counts


def write_histogram(path, rows):
    width, pitch, top = 1440, 28, 150
    bottom = top + pitch * len(rows)
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{bottom+90}" '
        f'viewBox="0 0 {width} {bottom+90}" role="img" aria-labelledby="title desc">',
        '<title id="title">Pi function-word usage at 30 digits</title>',
        '<desc id="desc">All used Toy words, comparing original and golfed programs. '
        'Source counts are linear; runtime counts use log10(1 + count).</desc>',
        '<rect width="100%" height="100%" fill="white"/>',
        '<style>text{font-family:Arial,sans-serif;fill:#172033;font-size:13px}'
        '.word{font-family:monospace;font-size:15px}.heading{font-size:18px;font-weight:bold}</style>',
    ]

    def text(x, y, value, css="", anchor="start"):
        parts.append(f'<text x="{x:.1f}" y="{y:.1f}" class="{css}" '
                     f'text-anchor="{anchor}">{escape(str(value))}</text>')

    text(20, 30, "Toy word usage · 30 digits", "heading")
    for x, color, label in [(20, "#2563eb", "pi.toy"), (170, "#d97706", "pi_golf.toy")]:
        parts.append(f'<rect x="{x}" y="49" width="16" height="9" fill="{color}"/>')
        text(x + 23, 58, label)
    text(360, 58, "Exact counts are printed beside each bar; zero means unused.")
    maximum_code = max(max(row[2:4]) for row in rows)
    code_limit = max(5, math.ceil(maximum_code / 5) * 5)
    maximum_runtime = max(max(row[4:6]) for row in rows)
    log_limit = math.ceil(math.log10(maximum_runtime + 1))
    panels = [
        (190, 430, 2, lambda count: count / code_limit,
         list(range(0, code_limit + 1, 5)), "Source occurrences · linear"),
        (880, 430, 4, lambda count: math.log10(count + 1) / log_limit,
         [0, 1] + [10 ** i for i in range(1, log_limit + 1)],
         "Runtime dispatches · log10(1 + count)"),
    ]
    for left, span, offset, scale, ticks, heading in panels:
        text(left, 95, heading, "heading")
        for tick in ticks:
            x = left + span * scale(tick)
            parts.append(f'<path d="M{x:.1f},130 V{bottom}" stroke="#e5e7eb"/>')
            text(x, 121, f"{tick:,}", anchor="middle")
        for i, row in enumerate(rows):
            y = top + pitch * i
            if offset == 2:
                text(20, y + 4, row[0], "word")
            for series, color in [(0, "#2563eb"), (1, "#d97706")]:
                count = row[offset + series]
                length = span * scale(count)
                bar_y = y - 9 + series * 10
                parts.append(f'<rect x="{left}" y="{bar_y}" width="{length:.1f}" '
                             f'height="7" fill="{color}"><title>'
                             f'{escape(row[0])}: {count:,}</title></rect>')
                text(left + length + 6, bar_y + 7, f"{count:,}")
    text(20, bottom + 35, "Includes user words, built-ins, and control flow. Numeric literals and definition headers are excluded.")
    text(20, bottom + 58, "Runtime counts include the final newline emit. Skipped words are excluded; closing then words are counted.")
    parts.append("</svg>\n")
    path.write_text("\n".join(parts))


def main():
    source_data = [source_counts(path) for path in EXAMPLES]
    runtime = [runtime_counts(path) for path in EXAMPLES]
    names = sorted(set().union(*(set(data[0]) for data in source_data), *map(set, runtime)))
    definitions = set().union(*(data[1] for data in source_data))
    rows = []
    for name in names:
        kind = "defined" if name in definitions else "control" if name in CONTROLS else "builtin"
        rows.append((name, kind, source_data[0][0][name], source_data[1][0][name],
                     runtime[0][name], runtime[1][name]))
    with (PROFILING / "pi_usage.csv").open("w", newline="") as output:
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(["word", "kind", "source_pi", "source_golf", "runtime_pi", "runtime_golf"])
        writer.writerows(rows)
    write_histogram(PROFILING / "pi_usage.svg", rows)
    totals = [sum(row[column] for row in rows) for column in range(2, 6)]
    report = [
        "![Pi function usage at 30 digits](pi_usage.svg)", "",
        "| Word | Kind | Source: pi | Source: golf | Runtime: pi | Runtime: golf |",
        "| --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for word, kind, *counts in rows:
        report.append(f"| `{word}` | {kind} | " + " | ".join(f"{count:,}" for count in counts) + " |")
    report.append("| **Total** | | " + " | ".join(f"{count:,}" for count in totals) + " |")
    summary = [
        "| At 30 digits | pi.toy | pi_golf.toy |",
        "| --- | ---: | ---: |",
        f"| Distinct words | {len(source_data[0][0])} | {len(source_data[1][0])} |",
        f"| Source word calls | {totals[0]:,} | {totals[1]:,} |",
        f"| Runtime word calls | {totals[2]:,} | {totals[3]:,} |",
    ]
    for readme, content in [(ROOT / "README.md", summary), (PROFILING / "README.md", report)]:
        text = readme.read_text()
        start, end = text.index(BEGIN) + len(BEGIN), text.index(END)
        readme.write_text(text[:start] + "\n\n" + "\n".join(content) + "\n\n" + text[end:])
    print(f"Profiled {len(rows)} words at 30 digits; source totals {totals[:2]}, runtime totals {totals[2:]}.")


if __name__ == "__main__":
    main()
