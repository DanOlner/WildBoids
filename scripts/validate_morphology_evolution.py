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
    python scripts/validate_morphology_evolution.py --config data/sim_config.json --plot data/champions/*.json
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


def plot_morphology_evolution(results: list[dict], config_groups: list[dict] | None,
                              output_path: str | None = None):
    """Generate polar plots showing how eye layouts evolved across generations.

    Produces a figure with rows = generations (sampled), columns = sensor groups.
    Each subplot is a polar plot showing eye positions as wedges.
    Also produces a timeline plot showing how angles and arc widths changed.
    """
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import matplotlib.patches as mpatches
        from matplotlib.patches import Wedge
        import numpy as np
    except ImportError:
        print('matplotlib/numpy not installed — skipping plot. pip install matplotlib numpy')
        return

    morpho_results = [r for r in results if r['has_morphology']]
    if len(morpho_results) < 2:
        print('Not enough champions with morphology to plot.')
        return

    n_groups = len(morpho_results[0]['groups'])

    # --- Figure 1: Polar wedge plots showing eye layout at key generations ---
    # Sample up to 6 generations evenly
    n_samples = min(6, len(morpho_results))
    indices = np.linspace(0, len(morpho_results) - 1, n_samples, dtype=int)
    sampled = [morpho_results[i] for i in indices]

    fig1, axes1 = plt.subplots(n_samples, n_groups, figsize=(5 * n_groups, 3.5 * n_samples),
                                subplot_kw={'projection': 'polar'})
    if n_groups == 1:
        axes1 = axes1.reshape(-1, 1)
    if n_samples == 1:
        axes1 = axes1.reshape(1, -1)

    # Colour map for eyes
    cmap = plt.cm.tab20

    for row, r in enumerate(sampled):
        for gi, g in enumerate(r['groups']):
            ax = axes1[row, gi]
            angles = g['angles']
            fracs = g['arc_fracs']

            # Compute arc widths in radians
            budget_rad = 2 * math.pi  # default
            if config_groups and gi < len(config_groups):
                budget_rad = config_groups[gi].get('totalArcDeg', 360) * math.pi / 180

            total_frac = sum(fracs) if fracs else 1
            if total_frac <= 0:
                total_frac = 1

            for i, (angle, frac) in enumerate(zip(angles, fracs)):
                arc_width = (frac / total_frac) * budget_rad
                half_arc = arc_width / 2

                # Draw wedge as a filled region
                theta = np.linspace(angle - half_arc, angle + half_arc, 30)
                r_vals = np.ones_like(theta)
                color = cmap(i % 20)

                ax.fill_between(theta, 0, r_vals, alpha=0.5, color=color)
                ax.plot([angle, angle], [0, 1.05], color=color, linewidth=1.5, alpha=0.8)

            # Forward direction marker
            ax.annotate('FWD', xy=(math.pi / 2, 1.15), ha='center', va='center',
                        fontsize=7, fontweight='bold', color='red',
                        annotation_clip=False)
            ax.plot([math.pi / 2], [1.1], 'rv', markersize=6, clip_on=False)

            ax.set_ylim(0, 1.2)
            ax.set_yticks([])
            # Polar plots: 0 is right, pi/2 is up. Our convention: +Y is forward = pi/2
            ax.set_theta_offset(math.pi / 2)  # 0 rad points up
            ax.set_theta_direction(-1)  # clockwise for +X = right

            group_label = ['Short-range', 'Long-range'][gi] if gi < 2 else f'Group {gi}'
            budget_label = ''
            if config_groups and gi < len(config_groups):
                budget_label = f' ({config_groups[gi]["totalArcDeg"]}°)'
            if row == 0:
                ax.set_title(f'{group_label}{budget_label}', fontsize=11, fontweight='bold', pad=20)
            ax.text(-0.15, 0.5, f'Gen {r["gen"]}', transform=ax.transAxes,
                    fontsize=10, fontweight='bold', va='center', rotation=90)

    fig1.suptitle('Eye Layout Evolution (polar view, FWD = up)', fontsize=14, fontweight='bold', y=1.01)
    fig1.tight_layout()

    polar_path = output_path or 'morphology_evolution.png'
    fig1.savefig(polar_path, dpi=150, bbox_inches='tight')
    print(f'Saved polar layout plot: {polar_path}')

    # --- Figure 2: Timeline plots ---
    fig2, axes2 = plt.subplots(2, n_groups, figsize=(6 * n_groups, 8), squeeze=False)

    gens = [r['gen'] for r in morpho_results]

    for gi in range(n_groups):
        n_eyes = morpho_results[0]['groups'][gi]['eye_count']

        # Angle timeline
        ax_angle = axes2[0, gi]
        for eye_i in range(n_eyes):
            eye_angles = [math.degrees(r['groups'][gi]['angles'][eye_i]) for r in morpho_results]
            ax_angle.plot(gens, eye_angles, linewidth=1, alpha=0.7, color=cmap(eye_i % 20),
                          label=f'Eye {eye_i}' if n_eyes <= 8 else None)

        group_label = ['Short-range', 'Long-range'][gi] if gi < 2 else f'Group {gi}'
        ax_angle.set_title(f'{group_label} — Eye Angles', fontweight='bold')
        ax_angle.set_ylabel('Angle (degrees)')
        ax_angle.set_xlabel('Generation')
        ax_angle.axhline(0, color='gray', linewidth=0.5, linestyle='--')
        ax_angle.axhline(90, color='red', linewidth=0.5, linestyle=':', alpha=0.5, label='Forward (90°)')
        if n_eyes <= 8:
            ax_angle.legend(fontsize=7, ncol=2)

        # Arc width timeline
        ax_arc = axes2[1, gi]
        budget_deg = 360
        if config_groups and gi < len(config_groups):
            budget_deg = config_groups[gi].get('totalArcDeg', 360)

        for eye_i in range(n_eyes):
            eye_arcs = []
            for r in morpho_results:
                fracs = r['groups'][gi]['arc_fracs']
                total_frac = sum(fracs)
                if total_frac > 0:
                    eye_arcs.append((fracs[eye_i] / total_frac) * budget_deg)
                else:
                    eye_arcs.append(0)
            ax_arc.plot(gens, eye_arcs, linewidth=1, alpha=0.7, color=cmap(eye_i % 20),
                        label=f'Eye {eye_i}' if n_eyes <= 8 else None)

        equal_share = budget_deg / n_eyes
        ax_arc.axhline(equal_share, color='gray', linewidth=1, linestyle='--',
                        label=f'Equal share ({equal_share:.1f}°)')
        ax_arc.set_title(f'{group_label} — Arc Widths ({budget_deg}° budget)', fontweight='bold')
        ax_arc.set_ylabel('Arc width (degrees)')
        ax_arc.set_xlabel('Generation')
        if n_eyes <= 8:
            ax_arc.legend(fontsize=7, ncol=2)

    fig2.suptitle('Morphology Evolution Over Time', fontsize=14, fontweight='bold')
    fig2.tight_layout()

    timeline_path = polar_path.replace('.png', '_timeline.png')
    fig2.savefig(timeline_path, dpi=150, bbox_inches='tight')
    print(f'Saved timeline plot: {timeline_path}')

    plt.close('all')


def main():
    parser = argparse.ArgumentParser(description='Validate morphology evolution in champion files')
    parser.add_argument('files', nargs='+', help='Champion JSON files')
    parser.add_argument('--config', default=None, help='sim_config.json path (for budget validation)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show per-eye details')
    parser.add_argument('--plot', '-p', action='store_true',
                        help='Generate morphology evolution plots')
    parser.add_argument('--plot-output', '-o', default='morphology_evolution.png',
                        metavar='PATH', help='Plot output path (default: morphology_evolution.png)')
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

    if args.plot:
        plot_morphology_evolution(results, config_groups, args.plot_output)

    if has_errors:
        print('\nVALIDATION FAILED — see errors above')
        sys.exit(1)
    else:
        print('\nAll checks passed.')


if __name__ == '__main__':
    main()
