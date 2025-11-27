#!/usr/bin/env python3
"""Generate a synthetic KPI table for demo purposes."""
from __future__ import annotations

import argparse
import os
import pathlib
import random
from typing import List

import pandas as pd


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Python table demo tool")
    parser.add_argument("--rows", type=int, default=6, help="Number of records to generate")
    parser.add_argument(
        "--title",
        default="Quarterly KPI Overview",
        help="Header text printed along with the table",
    )
    return parser.parse_args()


def resolve_output_dir() -> pathlib.Path:
    env_dir = os.getenv("TOOL_OUTPUT_DIR")
    base = pathlib.Path(env_dir) if env_dir else pathlib.Path.cwd() / "outputs"
    base.mkdir(parents=True, exist_ok=True)
    return base


def build_dataset(row_count: int) -> pd.DataFrame:
    rng = random.Random(20241128)
    regions: List[str] = ["North", "East", "South", "West"]
    segments: List[str] = ["Retail", "Wholesale", "Online"]
    quarters: List[str] = ["Q1", "Q2", "Q3", "Q4"]
    data = []
    for index in range(row_count):
        base = 75000 + rng.randint(-12000, 18000)
        growth = round(rng.uniform(-5.0, 12.0), 1)
        data.append(
            {
                "Region": regions[index % len(regions)],
                "Segment": segments[(index * 2) % len(segments)],
                "Quarter": quarters[index % len(quarters)],
                "Revenue": base,
                "Growth": growth,
            }
        )
    return pd.DataFrame(data)


def main() -> int:
    args = parse_args()
    row_count = max(3, min(args.rows, 64))
    df = build_dataset(row_count)
    df["Rank"] = df["Revenue"].rank(ascending=False, method="first").astype(int)
    df = df.sort_values("Rank")[
        ["Rank", "Region", "Segment", "Quarter", "Revenue", "Growth"]
    ].reset_index(drop=True)

    output_dir = resolve_output_dir()
    table_path = output_dir / "python_table_demo.csv"
    df.to_csv(table_path, index=False)

    display_df = df.copy()
    display_df["Revenue"] = display_df["Revenue"].map(lambda val: f"${val:,.0f}")
    display_df["Growth"] = display_df["Growth"].map(lambda val: f"{val:+.1f}%")

    print(f"[python_table_demo] {args.title} ({len(display_df)} rows)")
    print(display_df.to_string(index=False))
    print(f"[python_table_demo] Table saved to {table_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
