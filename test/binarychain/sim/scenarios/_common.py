"""Shared scaffolding for scenario scripts (path setup, pretty-print)."""

from __future__ import annotations

import os
import sys


def setup_path() -> None:
    """Make `sim` importable when running scenarios directly via python."""
    here = os.path.dirname(os.path.abspath(__file__))
    # .../sim/scenarios/ -> .../
    parent = os.path.normpath(os.path.join(here, "..", ".."))
    if parent not in sys.path:
        sys.path.insert(0, parent)


def section(title: str) -> None:
    bar = "=" * 78
    print()
    print(bar)
    print(f"  {title}")
    print(bar)


def step(text: str) -> None:
    print(f"  - {text}")


def kv(label: str, value) -> None:
    print(f"      {label:<30} {value}")
