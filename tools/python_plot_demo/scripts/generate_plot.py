#!/usr/bin/env python3
"""Render a PNG plot showcasing dual sensor trends."""
from __future__ import annotations

import argparse
import os
import pathlib

import matplotlib.pyplot as plt
import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Python plot demo tool")
    parser.add_argument("--title", default="Sensor Trend Example", help="Figure title")
    parser.add_argument(
        "--points",
        type=int,
        default=80,
        help="Number of samples per series (min 16, max 720)",
    )
    return parser.parse_args()


def resolve_output_dir() -> pathlib.Path:
    env_dir = os.getenv("TOOL_OUTPUT_DIR")
    base = pathlib.Path(env_dir) if env_dir else pathlib.Path.cwd() / "outputs"
    base.mkdir(parents=True, exist_ok=True)
    return base


def build_series(points: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    x_axis = np.linspace(0.0, 60.0, points)
    signal_a = 50 + 10 * np.sin(x_axis / 6) + 4 * np.cos(x_axis / 13)
    signal_b = 46 + 11 * np.cos(x_axis / 7) + 2.5 * np.sin(x_axis / 5)
    return x_axis, signal_a, signal_b


def main() -> int:
    args = parse_args()
    points = max(16, min(args.points, 720))
    x_axis, signal_a, signal_b = build_series(points)

    fig, ax = plt.subplots(figsize=(8, 4.5), dpi=120)
    ax.plot(x_axis, signal_a, label="Sensor A", color="#2B6CB0", linewidth=2.2)
    ax.plot(x_axis, signal_b, label="Sensor B", color="#C53030", linewidth=2.0)
    ax.fill_between(x_axis, signal_a, signal_b, color="#63B3ED", alpha=0.15)

    ax.set_title(args.title)
    ax.set_xlabel("Minutes")
    ax.set_ylabel("Utilization (%)")
    ax.set_ylim(30, 75)
    ax.grid(alpha=0.25, linestyle="--")
    ax.legend(loc="upper right")

    output_dir = resolve_output_dir()
    figure_path = output_dir / "python_plot_demo.png"
    fig.tight_layout()
    fig.savefig(figure_path)
    plt.close(fig)

    print(f"[python_plot_demo] Plot saved to {figure_path}")
    print(f"[python_plot_demo] Rendered with {points} points per series")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
