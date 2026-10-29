#!/usr/bin/env python3
"""Enforce a minimum executable-line coverage for each CMake service module."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys


def normalize(path: str) -> str:
    return os.path.normcase(os.path.realpath(path))


def read_manifest(path: Path) -> dict[str, list[str]]:
    modules: dict[str, list[str]] = {}
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            module, source = line.split("|", 1)
        except ValueError as error:
            raise ValueError(f"Invalid manifest entry on line {line_number}: {line}") from error
        if not module or not source:
            raise ValueError(f"Invalid manifest entry on line {line_number}: {line}")
        modules.setdefault(module, []).append(normalize(source))
    if not modules:
        raise ValueError("The CMake service module manifest is empty")
    return modules


def read_lcov(path: Path) -> dict[str, dict[int, int]]:
    records: dict[str, dict[int, int]] = {}
    source: str | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if raw_line.startswith("SF:"):
            source = normalize(raw_line[3:])
            records.setdefault(source, {})
        elif source is not None and raw_line.startswith("DA:"):
            fields = raw_line[3:].split(",")
            if len(fields) < 2:
                raise ValueError(f"Malformed LCOV line record: {raw_line}")
            line_number, hit_count = int(fields[0]), int(fields[1])
            current = records[source].get(line_number, 0)
            records[source][line_number] = max(current, hit_count)
        elif raw_line == "end_of_record":
            source = None
    return records


def evaluate(
    modules: dict[str, list[str]], records: dict[str, dict[int, int]], threshold: float
) -> tuple[list[str], list[str]]:
    summaries: list[str] = []
    failures: list[str] = []
    for module, sources in modules.items():
        executable = covered = 0
        missing: list[str] = []
        for source in sources:
            lines = records.get(source)
            if not lines:
                missing.append(source)
                continue
            executable += len(lines)
            covered += sum(1 for hits in lines.values() if hits > 0)
        percent = 100.0 * covered / executable if executable else 0.0
        summaries.append(
            f"{module}: {covered}/{executable} executable lines covered ({percent:.2f}%)"
        )
        if missing:
            failures.append(
                f"{module}: missing coverage data for {len(missing)} source file(s): "
                + ", ".join(missing)
            )
        if executable == 0:
            failures.append(f"{module}: no executable-line coverage data was measured")
        elif percent < threshold:
            failures.append(
                f"{module}: {percent:.2f}% is below the required {threshold:.2f}%"
            )
    return summaries, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--lcov", type=Path, required=True)
    parser.add_argument("--threshold", type=float, default=70.0)
    args = parser.parse_args()
    if not 0.0 <= args.threshold <= 100.0:
        parser.error("--threshold must be between 0 and 100")

    try:
        modules = read_manifest(args.manifest)
        records = read_lcov(args.lcov)
        summaries, failures = evaluate(modules, records, args.threshold)
    except (OSError, ValueError) as error:
        print(f"Coverage check error: {error}", file=sys.stderr)
        return 2

    print("Per-service executable-line coverage:")
    for summary in summaries:
        print(f"  {summary}")
    if failures:
        print("Coverage threshold check failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print(f"All service modules meet the {args.threshold:.2f}% line-coverage threshold.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
