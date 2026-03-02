#!/usr/bin/env python3
"""Package champion files with the current sim config into a dated folder.

Usage:
  python3 package_champions.py                  # auto-pick fittest from coevolution_log.csv
  python3 package_champions.py --prey 92 --pred 78   # specify generation numbers
  python3 package_champions.py --latest              # pick highest generation number
"""

import argparse
import csv
import shutil
import re
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).parent.parent
DATA_DIR = ROOT / "data"
CHAMPIONS_DIR = DATA_DIR / "champions"
PREDATORS_DIR = CHAMPIONS_DIR / "predators"
PACKAGES_DIR = DATA_DIR / "champion_packages"
CONFIG_FILE = DATA_DIR / "sim_config.json"
LOG_FILE = ROOT / "coevolution_log.csv"


def find_by_gen(directory: Path, pattern: str, gen: int) -> Path | None:
    """Find the champion file for a specific generation."""
    for f in directory.glob(pattern):
        m = re.search(r"gen(\d+)", f.name)
        if m and int(m.group(1)) == gen:
            return f
    return None


def find_highest_gen(directory: Path, pattern: str) -> Path | None:
    """Find the champion file with the highest generation number."""
    best_gen = -1
    best_file = None
    for f in directory.glob(pattern):
        m = re.search(r"gen(\d+)", f.name)
        if m:
            gen = int(m.group(1))
            if gen > best_gen:
                best_gen = gen
                best_file = f
    return best_file


def fittest_from_log() -> tuple[int | None, int | None]:
    """Read coevolution_log.csv and return (prey_gen, pred_gen) with highest best fitness."""
    if not LOG_FILE.exists():
        return None, None

    best_prey_gen, best_prey_fit = None, -1.0
    best_pred_gen, best_pred_fit = None, -1.0

    with open(LOG_FILE) as f:
        reader = csv.DictReader(f)
        for row in reader:
            gen = int(row["gen"])
            prey_fit = float(row["prey_best"])
            pred_fit = float(row["pred_best"])
            if prey_fit > best_prey_fit:
                best_prey_fit = prey_fit
                best_prey_gen = gen
            if pred_fit > best_pred_fit:
                best_pred_fit = pred_fit
                best_pred_gen = gen

    return best_prey_gen, best_pred_gen


def find_prey(gen: int) -> Path | None:
    """Find prey champion file, trying champion_prey_gen* then champion_gen*."""
    f = find_by_gen(CHAMPIONS_DIR, "champion_prey_gen*.json", gen)
    if f is None:
        f = find_by_gen(CHAMPIONS_DIR, "champion_gen*.json", gen)
    return f


def main():
    parser = argparse.ArgumentParser(description="Package champion files with sim config")
    parser.add_argument("--prey", type=int, help="Prey generation number")
    parser.add_argument("--pred", type=int, help="Predator generation number")
    parser.add_argument("--latest", action="store_true", help="Use highest generation (not fittest)")
    args = parser.parse_args()

    if args.latest:
        # Highest gen mode
        prey = find_highest_gen(CHAMPIONS_DIR, "champion_prey_gen*.json")
        if prey is None:
            prey = find_highest_gen(CHAMPIONS_DIR, "champion_gen*.json")
        predator = find_highest_gen(PREDATORS_DIR, "champion_predator_gen*.json")
        print("Mode: highest generation")
    elif args.prey is not None or args.pred is not None:
        # Explicit gen mode
        prey = find_prey(args.prey) if args.prey is not None else None
        predator = find_by_gen(PREDATORS_DIR, "champion_predator_gen*.json", args.pred) if args.pred is not None else None
        print(f"Mode: explicit (prey gen {args.prey}, pred gen {args.pred})")
    else:
        # Auto mode: read from coevolution_log.csv
        prey_gen, pred_gen = fittest_from_log()
        if prey_gen is None:
            print("No coevolution_log.csv found — falling back to highest generation")
            prey = find_highest_gen(CHAMPIONS_DIR, "champion_prey_gen*.json")
            if prey is None:
                prey = find_highest_gen(CHAMPIONS_DIR, "champion_gen*.json")
            predator = find_highest_gen(PREDATORS_DIR, "champion_predator_gen*.json")
        else:
            print(f"Mode: fittest from log (prey gen {prey_gen}, pred gen {pred_gen})")
            prey = find_prey(prey_gen)
            predator = find_by_gen(PREDATORS_DIR, "champion_predator_gen*.json", pred_gen)

    if prey is None:
        print("No prey champion found!")
        sys.exit(1)

    # Build folder name: YYYY-MM-DD or YYYY-MM-DD_2 etc.
    today = date.today().isoformat()
    pkg_dir = PACKAGES_DIR / today
    n = 1
    while pkg_dir.exists():
        n += 1
        pkg_dir = PACKAGES_DIR / f"{today}_{n}"

    pkg_dir.mkdir(parents=True)

    # Copy files
    shutil.copy2(prey, pkg_dir / prey.name)
    print(f"  Prey:     {prey.name}")

    if predator:
        shutil.copy2(predator, pkg_dir / predator.name)
        print(f"  Predator: {predator.name}")
    else:
        print("  Predator: (none)")

    shutil.copy2(CONFIG_FILE, pkg_dir / "sim_config.json")
    print(f"  Config:   sim_config.json")

    print(f"\nPackaged to: {pkg_dir.relative_to(ROOT)}")
    print(f"Folder name for commit message: {pkg_dir.name}")


if __name__ == "__main__":
    main()
