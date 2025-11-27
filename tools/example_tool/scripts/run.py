#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse
import pathlib
import sys


def parse_args():
    parser = argparse.ArgumentParser(description="Example tool for Script Toolbox demo")
    parser.add_argument("--source", required=True, help="Input file path")
    parser.add_argument("--rows", type=int, default=100, help="Number of rows to read")
    return parser.parse_args()


def main():
    args = parse_args()
    source_path = pathlib.Path(args.source)

    print(f"[example_tool] Running with source={source_path} rows={args.rows}")

    if not source_path.exists():
        print(f"[example_tool] WARNING: file does not exist: {source_path}", file=sys.stderr)
        return 1

    try:
        with source_path.open("r", encoding="utf-8", errors="ignore") as f:
            for i, line in enumerate(f):
                if i >= args.rows:
                    break
                print(f"{i+1:04d}: {line.rstrip()}")
    except Exception as exc:  # noqa: BLE001
        print(f"[example_tool] ERROR: failed to read file: {exc}", file=sys.stderr)
        return 1

    print("[example_tool] Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
