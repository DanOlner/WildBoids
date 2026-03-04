#!/usr/bin/env python3
"""Validate morphology evolution across champion JSON files.

Checks that:
1. Morphology genomes are present in champions
2. Eye angles and arc fractions vary across generations
3. Arc widths sum to the configured budget (within tolerance)
4. No degenerate values (zero-width eyes, NaN, etc.)

Usage:
    python scripts/validate_morphology_evolution.py data/champions/champion_gen*.json
    python scripts/validate_morphology_evolution.py --config data/sim_config.json data/champions/*.json
"""

import json
import sys
import argparse
import math
import re
from pathlib import Path


def extract_gen_number(path: Path) -> int:
    """Extract generation number from filename like champion_gen42.json."""
    m = re.search(r'gen(\d+)', path.stem)
    return int(m.group(1)) if m else -1


def load_config(config_path: str) -> dict:
    with open(config_path) as f:
        return json.load(f)


def validate_champion(path: Path, config_groups: list[dict] | None) -> dict:
    """Validate a single champion file. Returns a results dict."""
    with open(path) as f:
        spec = json.load(f)

    result = {
        'path': str(path),
        'gen': extract_gen_number(path),
        'has_morphology': False,
        'groups': [],
        'errors': [],
        'warnings': [],
    }

    morpho = spec.get('morphologyGenome')
    if morpho is None:
        result['warnings'].append('No morphologyGenome found')
        return result

    result['has_morphology'] = True
    groups = morpho.get('groups', [])

    for gi, group in enumerate(groups):
        angles = group.get('angles', [])
        arc_fracs = group.get('arcFracs', [])

        group_result = {
            'index': gi,
            'eye_count': len(angles),
            'angles': angles,
            'arc_fracs': arc_fracs,
            'arc_frac_sum': sum(arc_fracs) if arc_fracs else 0,
        }

        # Check for degenerate values
        for i, a in enumerate(angles):
            if math.isnan(a) or math.isinf(a):
                result['errors'].append(f'Group {gi} eye {i}: angle is {a}')
        for i, f in enumerate(arc_fracs):
            if math.isnan(f) or math.isinf(f):
                result['errors'].append(f'Group {gi} eye {i}: arc_frac is {f}')
            if f <= 0:
                result['errors'].append(f'Group {gi} eye {i}: arc_frac is {f} (should be > 0)')

        # Check eye count matches config
        if config_groups and gi < len(config_groups):
            expected = config_groups[gi].get('eyeCount', 0)
            if len(angles) != expected:
                result['errors'].append(
                    f'Group {gi}: has {len(angles)} eyes, config expects {expected}')

        # Check arc widths sum to budget
        if config_groups and gi < len(config_groups) and arc_fracs:
            budget_deg = config_groups[gi].get('totalArcDeg', 360)
            budget_rad = budget_deg * math.pi / 180
            total_frac = sum(arc_fracs)
            if total_frac > 0:
                arc_widths = [(f / total_frac) * budget_rad for f in arc_fracs]
                arc_sum = sum(arc_widths)
                if abs(arc_sum - budget_rad) > 0.01:
                    result['errors'].append(
                        f'Group {gi}: arc widths sum to {math.degrees(arc_sum):.2f}°, '
                        f'expected {budget_deg}°')
                group_result['arc_widths_deg'] = [math.degrees(w) for w in arc_widths]
                group_result['arc_budget_deg'] = budget_deg

        result['groups'].append(group_result)

    return result


def check_variation(results: list[dict]) -> list[str]:
    """Check that morphology varies across generations."""
    messages = []
    morpho_results = [r for r in results if r['has_morphology']]

    if len(morpho_results) < 2:
        messages.append('Not enough champions with morphology to check variation')
        return messages

    first = morpho_results[0]
    last = morpho_results[-1]

    for gi in range(len(first['groups'])):
        if gi >= len(last['groups']):
            break

        first_angles = first['groups'][gi]['angles']
        last_angles = last['groups'][gi]['angles']
        first_fracs = first['groups'][gi]['arc_fracs']
        last_fracs = last['groups'][gi]['arc_fracs']

        angle_diffs = [abs(a - b) for a, b in zip(first_angles, last_angles)]
        frac_diffs = [abs(a - b) for a, b in zip(first_fracs, last_fracs)]

        max_angle_diff = max(angle_diffs) if angle_diffs else 0
        max_frac_diff = max(frac_diffs) if frac_diffs else 0
        mean_angle_diff = sum(angle_diffs) / len(angle_diffs) if angle_diffs else 0
        mean_frac_diff = sum(frac_diffs) / len(frac_diffs) if frac_diffs else 0

        messages.append(
            f'Group {gi} (gen {first["gen"]} → gen {last["gen"]}): '
            f'angle diff mean={math.degrees(mean_angle_diff):.2f}° max={math.degrees(max_angle_diff):.2f}°, '
            f'arc_frac diff mean={mean_frac_diff:.4f} max={max_frac_diff:.4f}')

        if max_angle_diff < 0.001 and max_frac_diff < 0.001:
            messages.append(f'  WARNING: Group {gi} shows NO variation — morphology may not be evolving!')

    return messages


def print_summary(results: list[dict], config_groups: list[dict] | None):
    """Print a summary table of morphology across generations."""
    morpho_results = [r for r in results if r['has_morphology']]

    if not morpho_results:
        print('No champions with morphology genome found.')
        return

    print(f'\n{"Gen":>5}  ', end='')
    for gi, g in enumerate(morpho_results[0]['groups']):
        budget = ''
        if config_groups and gi < len(config_groups):
            budget = f' ({config_groups[gi].get("totalArcDeg", "?")}° budget)'
        print(f'  Group {gi}{budget:>20}', end='')
    print()

    print(f'{"":>5}  ', end='')
    for g in morpho_results[0]['groups']:
        print(f'  {"angle_std":>10} {"frac_std":>10} {"frac_sum":>10}', end='')
    print()
    print('-' * (7 + 34 * len(morpho_results[0]['groups'])))

    for r in morpho_results:
        print(f'{r["gen"]:>5}  ', end='')
        for g in r['groups']:
            angles = g['angles']
            fracs = g['arc_fracs']
            if angles:
                mean_a = sum(angles) / len(angles)
                std_a = math.sqrt(sum((a - mean_a)**2 for a in angles) / len(angles))
            else:
                std_a = 0
            if fracs:
                mean_f = sum(fracs) / len(fracs)
                std_f = math.sqrt(sum((f - mean_f)**2 for f in fracs) / len(fracs))
            else:
                std_f = 0
            print(f'  {math.degrees(std_a):>10.2f} {std_f:>10.4f} {g["arc_frac_sum"]:>10.4f}', end='')
        print()


def main():
    parser = argparse.ArgumentParser(description='Validate morphology evolution in champion files')
    parser.add_argument('files', nargs='+', help='Champion JSON files')
    parser.add_argument('--config', default=None, help='sim_config.json path (for budget validation)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show per-eye details')
    args = parser.parse_args()

    config_groups = None
    if args.config:
        cfg = load_config(args.config)
        me = cfg.get('morphologyEvolution', {})
        config_groups = me.get('groups', [])
        print(f'Config: {len(config_groups)} groups')
        for gi, g in enumerate(config_groups):
            print(f'  Group {gi}: {g["eyeCount"]} eyes, {g["totalArcDeg"]}° budget, {g["maxRange"]} range')

    paths = sorted([Path(f) for f in args.files], key=extract_gen_number)
    results = [validate_champion(p, config_groups) for p in paths]

    # Report errors
    has_errors = False
    for r in results:
        for e in r['errors']:
            print(f'ERROR [{Path(r["path"]).name}]: {e}')
            has_errors = True
        for w in r['warnings']:
            if args.verbose:
                print(f'WARN  [{Path(r["path"]).name}]: {w}')

    # Variation check
    print('\n--- Variation across generations ---')
    for msg in check_variation(results):
        print(msg)

    # Summary table
    print_summary(results, config_groups)

    if args.verbose:
        for r in results:
            if not r['has_morphology']:
                continue
            print(f'\n=== Gen {r["gen"]} ({Path(r["path"]).name}) ===')
            for g in r['groups']:
                print(f'  Group {g["index"]} ({g["eye_count"]} eyes):')
                for i, (a, f) in enumerate(zip(g['angles'], g['arc_fracs'])):
                    arc_w = ''
                    if 'arc_widths_deg' in g:
                        arc_w = f'  arc={g["arc_widths_deg"][i]:.2f}°'
                    print(f'    Eye {i:2d}: angle={math.degrees(a):7.2f}°  frac={f:.4f}{arc_w}')

    if has_errors:
        print('\nVALIDATION FAILED — see errors above')
        sys.exit(1)
    else:
        print('\nAll checks passed.')


if __name__ == '__main__':
    main()
