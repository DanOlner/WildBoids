#!/usr/bin/env python3
"""Compare evolved boid champion neural networks and produce an HTML report.

Usage:
    python scripts/compare_champions.py data/champions/prey_gen48.json data/champions/pred_gen76.json
    python scripts/compare_champions.py -o report.html champion1.json champion2.json
"""

import json
import argparse
import base64
import io
import math
from dataclasses import dataclass, field
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.colors as mcolors
import numpy as np
import networkx as nx


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class SensorLabel:
    node_id: int
    eye_type: str       # "short", "long", "speed", "angular_velocity", "noise"
    eye_index: int      # which eye (or -1 for proprioceptive)
    channel: str        # "food", "same", "opposite", or "" for proprioceptive
    angle_deg: float    # center angle of the eye (0 for proprioceptive)
    short_label: str    # e.g. "S5.food(-58°)" or "speed"

@dataclass
class ThrusterLabel:
    node_id: int
    label: str
    max_thrust: float

@dataclass
class ChampionData:
    path: str
    label: str
    species: str
    compound_eyes: dict
    thrusters: list
    genome: dict
    morphology: dict | None
    input_map: dict = field(default_factory=dict)   # node_id -> SensorLabel
    output_map: dict = field(default_factory=dict)   # node_id -> ThrusterLabel
    hidden_ids: list = field(default_factory=list)
    enabled_connections: list = field(default_factory=list)
    recurrent_connections: list = field(default_factory=list)


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_champion(path: str) -> ChampionData:
    with open(path) as f:
        spec = json.load(f)

    p = Path(path)
    species = spec.get('type', 'unknown')
    label = f"{species.capitalize()} ({p.stem})"

    champ = ChampionData(
        path=str(path),
        label=label,
        species=species,
        compound_eyes=spec.get('compoundEyes', {}),
        thrusters=spec.get('thrusters', []),
        genome=spec.get('genome', {}),
        morphology=spec.get('morphologyGenome'),
    )

    _build_input_map(champ)
    _build_output_map(champ)
    _extract_network_info(champ)
    return champ


def _build_input_map(champ: ChampionData):
    ce = champ.compound_eyes
    channels = ce.get('channels', ['food', 'same', 'opposite'])
    eyes = ce.get('eyes', [])
    long_eyes = ce.get('longRangeEyes', [])

    idx = 0
    for eye in eyes:
        angle = eye['centerAngleDeg']
        for ch in channels:
            champ.input_map[idx] = SensorLabel(
                node_id=idx, eye_type='short', eye_index=eye['id'],
                channel=ch, angle_deg=angle,
                short_label=f"S{eye['id']}.{ch}({angle:+.0f}°)")
            idx += 1

    for eye in long_eyes:
        angle = eye['centerAngleDeg']
        for ch in channels:
            champ.input_map[idx] = SensorLabel(
                node_id=idx, eye_type='long', eye_index=eye['id'],
                channel=ch, angle_deg=angle,
                short_label=f"L{eye['id']}.{ch}({angle:+.0f}°)")
            idx += 1

    if ce.get('speedSensor'):
        champ.input_map[idx] = SensorLabel(idx, 'speed', -1, '', 0, 'speed')
        idx += 1
    if ce.get('angularVelocitySensor'):
        champ.input_map[idx] = SensorLabel(idx, 'angular_velocity', -1, '', 0, 'ang_vel')
        idx += 1
    if ce.get('noiseSensor'):
        champ.input_map[idx] = SensorLabel(idx, 'noise', -1, '', 0, 'noise')
        idx += 1
    if ce.get('shoalingSensor'):
        champ.input_map[idx] = SensorLabel(idx, 'shoaling', -1, '', 0, 'shoaling')
        idx += 1
    if ce.get('hungerSensor'):
        champ.input_map[idx] = SensorLabel(idx, 'hunger', -1, '', 0, 'hunger')
        idx += 1


def _build_output_map(champ: ChampionData):
    n_inputs = len(champ.input_map)
    for i, t in enumerate(champ.thrusters):
        node_id = n_inputs + i
        champ.output_map[node_id] = ThrusterLabel(
            node_id=node_id, label=t['label'], max_thrust=t['maxThrust'])


def _extract_network_info(champ: ChampionData):
    nodes = champ.genome.get('nodes', [])
    conns = champ.genome.get('connections', [])

    input_ids = set(champ.input_map.keys())
    output_ids = set(champ.output_map.keys())

    champ.hidden_ids = sorted(
        n['id'] for n in nodes if n['id'] not in input_ids and n['id'] not in output_ids)
    champ.enabled_connections = [c for c in conns if c.get('enabled', True)]

    # Detect recurrent connections (target feeds back to source via any path)
    # Simple heuristic: connection is recurrent if target <= source and target is not an input
    # More accurate: check if target is an ancestor of source in the DAG
    all_hidden_and_output = set(champ.hidden_ids) | output_ids
    forward_edges = {}  # node -> set of targets
    for c in champ.enabled_connections:
        forward_edges.setdefault(c['source'], set()).add(c['target'])

    champ.recurrent_connections = []
    for c in champ.enabled_connections:
        # Self-recurrent
        if c['source'] == c['target']:
            champ.recurrent_connections.append(c)
            continue
        # Output feeding back to hidden/output
        if c['source'] in output_ids and c['target'] in all_hidden_and_output:
            champ.recurrent_connections.append(c)
            continue
        # Hidden node with lower ID feeding back (heuristic for NEAT where hidden IDs increase)
        if (c['source'] in set(champ.hidden_ids) and c['target'] in set(champ.hidden_ids)
                and c['target'] <= c['source']):
            champ.recurrent_connections.append(c)


# ---------------------------------------------------------------------------
# Analysis functions
# ---------------------------------------------------------------------------

def direct_weight_matrix(champ: ChampionData) -> np.ndarray:
    """Build input×output direct-connection weight matrix."""
    input_ids = sorted(champ.input_map.keys())
    output_ids = sorted(champ.output_map.keys())
    mat = np.zeros((len(input_ids), len(output_ids)))

    in_idx = {nid: i for i, nid in enumerate(input_ids)}
    out_idx = {nid: i for i, nid in enumerate(output_ids)}

    for c in champ.enabled_connections:
        if c['source'] in in_idx and c['target'] in out_idx:
            mat[in_idx[c['source']], out_idx[c['target']]] = c['weight']
    return mat


def effective_weight_matrix(champ: ChampionData, max_depth: int = 4) -> np.ndarray:
    """Compute effective input→output weights by tracing through hidden nodes.

    At each hidden node with sigmoid activation, multiply by gain of 0.25.
    Sums all paths up to max_depth hops.
    """
    input_ids = sorted(champ.input_map.keys())
    output_ids = sorted(champ.output_map.keys())
    mat = np.zeros((len(input_ids), len(output_ids)))

    in_idx = {nid: i for i, nid in enumerate(input_ids)}
    out_idx = {nid: i for i, nid in enumerate(output_ids)}

    # Build adjacency: source -> [(target, weight)]
    adj = {}
    for c in champ.enabled_connections:
        adj.setdefault(c['source'], []).append((c['target'], c['weight']))

    # Node activation gains
    node_gain = {}
    for n in champ.genome.get('nodes', []):
        if n.get('activation') == 'sigmoid':
            node_gain[n['id']] = 0.25
        else:
            node_gain[n['id']] = 1.0

    # DFS from each input
    recurrent_set = {(c['source'], c['target']) for c in champ.recurrent_connections}

    for in_id in input_ids:
        # Stack: (current_node, accumulated_weight, depth, visited)
        stack = [(in_id, 1.0, 0, {in_id})]
        while stack:
            node, acc_w, depth, visited = stack.pop()
            if depth > max_depth:
                continue
            if node in out_idx:
                mat[in_idx[in_id], out_idx[node]] += acc_w
                continue
            for target, weight in adj.get(node, []):
                if target in visited:
                    continue
                if (node, target) in recurrent_set:
                    continue  # skip recurrent in path tracing
                gain = node_gain.get(target, 1.0)
                new_w = acc_w * weight * gain
                stack.append((target, new_w, depth + 1, visited | {target}))
    return mat


def find_top_paths(champ: ChampionData, output_id: int,
                   max_depth: int = 4, top_k: int = 5) -> list[dict]:
    """Find the K strongest input-to-output paths for a given output."""
    output_ids = set(champ.output_map.keys())

    # Build reverse adjacency: target -> [(source, weight)]
    rev_adj = {}
    recurrent_set = {(c['source'], c['target']) for c in champ.recurrent_connections}
    for c in champ.enabled_connections:
        if (c['source'], c['target']) not in recurrent_set:
            rev_adj.setdefault(c['target'], []).append((c['source'], c['weight']))

    node_gain = {}
    for n in champ.genome.get('nodes', []):
        if n.get('activation') == 'sigmoid':
            node_gain[n['id']] = 0.25
        else:
            node_gain[n['id']] = 1.0

    paths = []
    # BFS backward from output
    stack = [([output_id], 1.0)]
    while stack:
        path, acc_w = stack.pop()
        current = path[-1]
        if len(path) - 1 > max_depth:
            continue
        if current in champ.input_map:
            paths.append({'path': list(reversed(path)), 'weight': acc_w})
            continue
        for source, weight in rev_adj.get(current, []):
            if source in set(path):
                continue
            gain = node_gain.get(current, 1.0)
            stack.append((path + [source], acc_w * weight * gain))

    paths.sort(key=lambda p: abs(p['weight']), reverse=True)
    return paths[:top_k]


def find_processing_chains(champ: ChampionData) -> list[list[int]]:
    """Find chains of hidden nodes connected to each other."""
    hidden_set = set(champ.hidden_ids)
    recurrent_set = {(c['source'], c['target']) for c in champ.recurrent_connections}

    # Build hidden-to-hidden adjacency (non-recurrent only)
    h_adj = {}
    for c in champ.enabled_connections:
        if (c['source'] in hidden_set and c['target'] in hidden_set
                and (c['source'], c['target']) not in recurrent_set):
            h_adj.setdefault(c['source'], []).append(c['target'])

    # Find all chains by DFS from nodes with no hidden predecessors
    hidden_with_pred = set()
    for targets in h_adj.values():
        for t in targets:
            hidden_with_pred.add(t)

    roots = [h for h in champ.hidden_ids if h not in hidden_with_pred and h in h_adj]

    chains = []
    for root in roots:
        stack = [[root]]
        while stack:
            path = stack.pop()
            extended = False
            for target in h_adj.get(path[-1], []):
                if target not in set(path):
                    stack.append(path + [target])
                    extended = True
            if not extended and len(path) > 1:
                chains.append(path)

    # Deduplicate (keep longest chain for each root)
    chains.sort(key=len, reverse=True)
    seen = set()
    unique = []
    for chain in chains:
        key = tuple(chain)
        if not any(set(chain).issubset(set(c)) for c in unique):
            unique.append(chain)
    return unique


def hidden_node_inputs(champ: ChampionData) -> dict[int, list[tuple[int, float]]]:
    """For each hidden node, list its direct input connections with weights."""
    hidden_set = set(champ.hidden_ids)
    result = {h: [] for h in champ.hidden_ids}
    for c in champ.enabled_connections:
        if c['target'] in hidden_set:
            result[c['target']].append((c['source'], c['weight']))
    return result


# ---------------------------------------------------------------------------
# Visualization helpers
# ---------------------------------------------------------------------------

CHANNEL_COLORS = {'food': '#2ca02c', 'same': '#1f77b4', 'opposite': '#ff7f0e'}
PROPRIOCEPTIVE_COLOR = '#7f7f7f'


def fig_to_base64(fig, dpi=150):
    buf = io.BytesIO()
    fig.savefig(buf, format='png', dpi=dpi, bbox_inches='tight', facecolor='white')
    buf.seek(0)
    encoded = base64.b64encode(buf.read()).decode('utf-8')
    plt.close(fig)
    return f'data:image/png;base64,{encoded}'


def sensor_color(label: SensorLabel) -> str:
    if label.channel in CHANNEL_COLORS:
        return CHANNEL_COLORS[label.channel]
    return PROPRIOCEPTIVE_COLOR


# ---------------------------------------------------------------------------
# Plot: Direct weight heatmap
# ---------------------------------------------------------------------------

def plot_weight_heatmap(champ: ChampionData, matrix: np.ndarray,
                        title: str = "Direct Weights") -> plt.Figure:
    input_ids = sorted(champ.input_map.keys())
    output_ids = sorted(champ.output_map.keys())

    n_in, n_out = matrix.shape
    fig_height = max(8, n_in * 0.18)
    fig, ax = plt.subplots(figsize=(3 + n_out * 1.2, fig_height))

    vmax = max(abs(matrix.min()), abs(matrix.max()), 1.0)
    im = ax.imshow(matrix, aspect='auto', cmap='RdBu_r', vmin=-vmax, vmax=vmax,
                   interpolation='nearest')
    fig.colorbar(im, ax=ax, shrink=0.5, label='Weight')

    # Y-axis labels (inputs) — show every input with color
    y_labels = []
    y_colors = []
    for nid in input_ids:
        s = champ.input_map[nid]
        y_labels.append(s.short_label)
        y_colors.append(sensor_color(s))

    ax.set_yticks(range(n_in))
    ax.set_yticklabels(y_labels, fontsize=5)
    for i, color in enumerate(y_colors):
        ax.get_yticklabels()[i].set_color(color)

    # Add group separators
    n_short = len(champ.compound_eyes.get('eyes', [])) * len(champ.compound_eyes.get('channels', []))
    n_long = len(champ.compound_eyes.get('longRangeEyes', [])) * len(champ.compound_eyes.get('channels', []))
    for sep in [n_short, n_short + n_long]:
        if 0 < sep < n_in:
            ax.axhline(sep - 0.5, color='black', linewidth=1)

    # X-axis labels (outputs)
    x_labels = [champ.output_map[nid].label for nid in output_ids]
    ax.set_xticks(range(n_out))
    ax.set_xticklabels(x_labels, rotation=45, ha='right', fontsize=9)

    # Annotate strong weights
    for i in range(n_in):
        for j in range(n_out):
            v = matrix[i, j]
            if abs(v) > 1.0:
                ax.text(j, i, f'{v:.1f}', ha='center', va='center',
                        fontsize=5, color='white' if abs(v) > vmax * 0.7 else 'black')

    ax.set_title(f'{title}\n{champ.label}', fontsize=11, fontweight='bold')
    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# Plot: Network overview graph
# ---------------------------------------------------------------------------

def plot_network_overview(champ: ChampionData, top_n: int = 30) -> plt.Figure:
    """Full hidden-layer graph plus top-N direct input→output connections.

    Always shows ALL hidden nodes and ALL connections involving hidden nodes,
    so the graph accurately represents the brain's complexity. On top of that,
    adds the top-N strongest direct input→output connections.
    """
    G = nx.DiGraph()
    input_ids = set(champ.input_map.keys())
    output_ids = set(champ.output_map.keys())
    hidden_set = set(champ.hidden_ids)

    # 1) Always include all hidden nodes
    for hid in champ.hidden_ids:
        G.add_node(hid, layer=1, label=f'H{hid}')

    # 2) Always include all outputs
    for nid in output_ids:
        G.add_node(nid, layer=2, label=champ.output_map[nid].label)

    # 3) Include ALL connections that touch a hidden node (hidden↔hidden,
    #    input→hidden, hidden→output, output→hidden for recurrent)
    hidden_conns = []
    direct_conns = []  # input→output only
    for c in champ.enabled_connections:
        src, tgt = c['source'], c['target']
        if src in hidden_set or tgt in hidden_set:
            hidden_conns.append(c)
        elif src in input_ids and tgt in output_ids:
            direct_conns.append(c)

    # Add input nodes that connect to hidden nodes (only those involved)
    for c in hidden_conns:
        src, tgt = c['source'], c['target']
        if src in input_ids and src not in G:
            G.add_node(src, layer=0, label=champ.input_map[src].short_label)
        G.add_edge(src, tgt, weight=c['weight'])

    # 4) Add top-N strongest direct input→output connections
    direct_conns.sort(key=lambda c: abs(c['weight']), reverse=True)
    for c in direct_conns[:top_n]:
        src = c['source']
        if src not in G:
            G.add_node(src, layer=0, label=champ.input_map[src].short_label)
        G.add_edge(src, c['target'], weight=c['weight'])

    if len(G.nodes) == 0:
        fig, ax = plt.subplots(figsize=(8, 6))
        ax.text(0.5, 0.5, 'No connections', ha='center', va='center', transform=ax.transAxes)
        return fig

    # Layout
    pos = nx.multipartite_layout(G, subset_key='layer', align='horizontal')
    # Rotate so inputs on left, outputs on right
    pos = {n: (y, -x) for n, (x, y) in pos.items()}

    fig, ax = plt.subplots(figsize=(14, max(8, len(G.nodes) * 0.3)))

    # Draw edges
    recurrent_set = {(c['source'], c['target']) for c in champ.recurrent_connections}
    for u, v, data in G.edges(data=True):
        w = data['weight']
        color = '#2166ac' if w > 0 else '#b2182b'
        style = '--' if (u, v) in recurrent_set else '-'
        alpha = min(1.0, abs(w) / 5.0 + 0.2)
        width = min(4.0, abs(w) * 0.5 + 0.3)
        ax.annotate('', xy=pos[v], xytext=pos[u],
                     arrowprops=dict(arrowstyle='->', color=color, alpha=alpha,
                                     lw=width, linestyle=style,
                                     connectionstyle='arc3,rad=0.1'))

    # Draw nodes
    for nid in G.nodes:
        x, y = pos[nid]
        data = G.nodes[nid]
        if data['layer'] == 0:
            s = champ.input_map.get(nid)
            c = sensor_color(s) if s else PROPRIOCEPTIVE_COLOR
            ax.scatter(x, y, s=80, c=c, zorder=5, edgecolors='black', linewidths=0.5)
            ax.text(x - 0.02, y, data['label'], ha='right', va='center', fontsize=5)
        elif data['layer'] == 2:
            ax.scatter(x, y, s=200, c='gold', marker='s', zorder=5,
                       edgecolors='black', linewidths=1)
            ax.text(x + 0.02, y, data['label'], ha='left', va='center',
                    fontsize=8, fontweight='bold')
        else:
            ax.scatter(x, y, s=120, c='lightblue', zorder=5,
                       edgecolors='black', linewidths=0.5)
            ax.text(x, y, data['label'], ha='center', va='center', fontsize=6)

    # Legend
    legend_elements = [
        mpatches.Patch(color='#2ca02c', label='food'),
        mpatches.Patch(color='#1f77b4', label='same'),
        mpatches.Patch(color='#ff7f0e', label='opposite'),
        mpatches.Patch(color='#7f7f7f', label='proprioceptive'),
        plt.Line2D([0], [0], color='#2166ac', label='positive weight'),
        plt.Line2D([0], [0], color='#b2182b', label='negative weight'),
        plt.Line2D([0], [0], color='gray', linestyle='--', label='recurrent'),
    ]
    ax.legend(handles=legend_elements, loc='lower right', fontsize=7, ncol=2)

    ax.set_title(f'Network Overview (all {len(champ.hidden_ids)} hidden nodes + top {top_n} direct)\n{champ.label}',
                 fontsize=12, fontweight='bold')
    ax.axis('off')
    fig.tight_layout()
    return fig


def plot_network_force(champ: ChampionData, top_n: int = 30) -> plt.Figure:
    """Force-directed graph where strongly connected nodes cluster together.

    Uses the same node/edge selection as plot_network_overview but with
    spring_layout. Edge attraction is proportional to |weight|, so nodes
    that share strong connections are pulled together into clusters.
    Output nodes are pinned to the right edge to anchor the layout.
    """
    G = nx.DiGraph()
    input_ids = set(champ.input_map.keys())
    output_ids = set(champ.output_map.keys())
    hidden_set = set(champ.hidden_ids)

    for hid in champ.hidden_ids:
        G.add_node(hid, layer=1, label=f'H{hid}')
    for nid in output_ids:
        G.add_node(nid, layer=2, label=champ.output_map[nid].label)

    hidden_conns = []
    direct_conns = []
    for c in champ.enabled_connections:
        src, tgt = c['source'], c['target']
        if src in hidden_set or tgt in hidden_set:
            hidden_conns.append(c)
        elif src in input_ids and tgt in output_ids:
            direct_conns.append(c)

    for c in hidden_conns:
        src, tgt = c['source'], c['target']
        if src in input_ids and src not in G:
            G.add_node(src, layer=0, label=champ.input_map[src].short_label)
        G.add_edge(src, tgt, weight=c['weight'])

    direct_conns.sort(key=lambda c: abs(c['weight']), reverse=True)
    for c in direct_conns[:top_n]:
        src = c['source']
        if src not in G:
            G.add_node(src, layer=0, label=champ.input_map[src].short_label)
        G.add_edge(src, c['target'], weight=c['weight'])

    if len(G.nodes) == 0:
        fig, ax = plt.subplots(figsize=(8, 6))
        ax.text(0.5, 0.5, 'No connections', ha='center', va='center', transform=ax.transAxes)
        return fig

    # Pin output nodes along the right edge, evenly spaced
    sorted_outputs = sorted(output_ids)
    fixed_pos = {}
    for i, nid in enumerate(sorted_outputs):
        if nid in G:
            y = (i - len(sorted_outputs) / 2) * 0.8
            fixed_pos[nid] = (3.0, y)

    # Spring layout with |weight| as attraction strength
    # Use the absolute weight so strong connections (positive or negative) pull together
    for u, v in G.edges():
        G[u][v]['spring_weight'] = abs(G[u][v]['weight']) + 0.1

    pos = nx.spring_layout(
        G, k=1.5 / max(1, len(G.nodes) ** 0.4),  # spacing
        iterations=200,
        weight='spring_weight',
        pos=fixed_pos,     # seed with fixed output positions
        fixed=list(fixed_pos.keys()),  # keep outputs pinned
        seed=42,
    )

    fig, ax = plt.subplots(figsize=(16, max(10, len(G.nodes) * 0.2)))

    # Draw edges
    recurrent_set = {(c['source'], c['target']) for c in champ.recurrent_connections}
    for u, v, data in G.edges(data=True):
        w = data['weight']
        color = '#2166ac' if w > 0 else '#b2182b'
        style = '--' if (u, v) in recurrent_set else '-'
        alpha = min(1.0, abs(w) / 5.0 + 0.15)
        width = min(3.5, abs(w) * 0.4 + 0.2)
        ax.annotate('', xy=pos[v], xytext=pos[u],
                     arrowprops=dict(arrowstyle='->', color=color, alpha=alpha,
                                     lw=width, linestyle=style,
                                     connectionstyle='arc3,rad=0.05'))

    # Draw nodes
    for nid in G.nodes:
        x, y = pos[nid]
        data = G.nodes[nid]
        if data['layer'] == 0:
            s = champ.input_map.get(nid)
            c = sensor_color(s) if s else PROPRIOCEPTIVE_COLOR
            ax.scatter(x, y, s=60, c=c, zorder=5, edgecolors='black', linewidths=0.5)
            ax.text(x, y + 0.06, data['label'], ha='center', va='bottom', fontsize=4.5,
                    rotation=30)
        elif data['layer'] == 2:
            ax.scatter(x, y, s=250, c='gold', marker='s', zorder=5,
                       edgecolors='black', linewidths=1)
            ax.text(x + 0.12, y, data['label'], ha='left', va='center',
                    fontsize=9, fontweight='bold')
        else:
            # Size hidden nodes by their degree (more connections = bigger)
            deg = G.degree(nid)
            size = 80 + deg * 8
            ax.scatter(x, y, s=size, c='lightblue', zorder=5,
                       edgecolors='black', linewidths=0.8)
            ax.text(x, y, data['label'], ha='center', va='center', fontsize=7,
                    fontweight='bold')

    legend_elements = [
        mpatches.Patch(color='#2ca02c', label='food'),
        mpatches.Patch(color='#1f77b4', label='same'),
        mpatches.Patch(color='#ff7f0e', label='opposite'),
        mpatches.Patch(color='#7f7f7f', label='proprioceptive'),
        plt.Line2D([0], [0], color='#2166ac', label='positive weight'),
        plt.Line2D([0], [0], color='#b2182b', label='negative weight'),
        plt.Line2D([0], [0], color='gray', linestyle='--', label='recurrent'),
    ]
    ax.legend(handles=legend_elements, loc='lower left', fontsize=7, ncol=2)

    ax.set_title(
        f'Force-Directed Network ({len(champ.hidden_ids)} hidden, '
        f'clustering by connection strength)\n{champ.label}',
        fontsize=12, fontweight='bold')
    ax.axis('off')
    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# Plot: Per-output subgraph
# ---------------------------------------------------------------------------

def plot_per_output_graph(champ: ChampionData, output_id: int,
                          top_input_n: int = 10) -> plt.Figure:
    """Focused subgraph for a single output node."""
    output_ids = set(champ.output_map.keys())
    input_ids = set(champ.input_map.keys())
    hidden_set = set(champ.hidden_ids)
    recurrent_set = {(c['source'], c['target']) for c in champ.recurrent_connections}

    # Build reverse adjacency (non-recurrent)
    rev_adj = {}
    weight_map = {}
    for c in champ.enabled_connections:
        rev_adj.setdefault(c['target'], []).append(c['source'])
        weight_map[(c['source'], c['target'])] = c['weight']

    # BFS backward from output to find all reachable hidden nodes
    reachable_hidden = set()
    queue = [output_id]
    visited = {output_id}
    while queue:
        node = queue.pop(0)
        for source in rev_adj.get(node, []):
            if source in hidden_set and source not in visited:
                if (source, node) not in recurrent_set:
                    reachable_hidden.add(source)
                    visited.add(source)
                    queue.append(source)

    # Find direct input connections to this output and reachable hidden nodes
    targets_of_interest = reachable_hidden | {output_id}
    input_weights = []  # (input_id, target, weight)
    for c in champ.enabled_connections:
        if c['source'] in input_ids and c['target'] in targets_of_interest:
            input_weights.append((c['source'], c['target'], c['weight']))

    input_weights.sort(key=lambda x: abs(x[2]), reverse=True)
    top_inputs = input_weights[:top_input_n]
    included_inputs = {iw[0] for iw in top_inputs}

    # Build graph
    G = nx.DiGraph()
    all_nodes = included_inputs | reachable_hidden | {output_id}

    for nid in all_nodes:
        if nid in input_ids:
            G.add_node(nid, layer=0, label=champ.input_map[nid].short_label)
        elif nid in output_ids:
            G.add_node(nid, layer=2, label=champ.output_map[nid].label)
        else:
            G.add_node(nid, layer=1, label=f'H{nid}')

    # Add edges between included nodes
    for c in champ.enabled_connections:
        if c['source'] in all_nodes and c['target'] in all_nodes:
            G.add_edge(c['source'], c['target'], weight=c['weight'])

    if len(G.nodes) < 2:
        fig, ax = plt.subplots(figsize=(6, 4))
        lbl = champ.output_map[output_id].label
        ax.text(0.5, 0.5, f'No significant connections to {lbl}',
                ha='center', va='center', transform=ax.transAxes)
        ax.set_title(lbl)
        return fig

    pos = nx.multipartite_layout(G, subset_key='layer', align='horizontal')
    pos = {n: (y, -x) for n, (x, y) in pos.items()}

    fig, ax = plt.subplots(figsize=(10, max(4, len(all_nodes) * 0.25)))

    for u, v, data in G.edges(data=True):
        w = data['weight']
        color = '#2166ac' if w > 0 else '#b2182b'
        style = '--' if (u, v) in recurrent_set else '-'
        alpha = min(1.0, abs(w) / 5.0 + 0.3)
        width = min(3.0, abs(w) * 0.4 + 0.3)
        ax.annotate('', xy=pos[v], xytext=pos[u],
                     arrowprops=dict(arrowstyle='->', color=color, alpha=alpha,
                                     lw=width, linestyle=style,
                                     connectionstyle='arc3,rad=0.08'))
        # Label edge weight
        mid_x = (pos[u][0] + pos[v][0]) / 2
        mid_y = (pos[u][1] + pos[v][1]) / 2
        if abs(w) > 1.0:
            ax.text(mid_x, mid_y, f'{w:.1f}', fontsize=5, ha='center', va='center',
                    bbox=dict(boxstyle='round,pad=0.1', facecolor='white', alpha=0.7))

    for nid in G.nodes:
        x, y = pos[nid]
        data = G.nodes[nid]
        if data['layer'] == 0:
            s = champ.input_map.get(nid)
            c = sensor_color(s) if s else PROPRIOCEPTIVE_COLOR
            ax.scatter(x, y, s=60, c=c, zorder=5, edgecolors='black', linewidths=0.5)
            ax.text(x - 0.01, y, data['label'], ha='right', va='center', fontsize=5)
        elif data['layer'] == 2:
            ax.scatter(x, y, s=200, c='gold', marker='s', zorder=5,
                       edgecolors='black', linewidths=1)
            ax.text(x + 0.01, y, data['label'], ha='left', va='center',
                    fontsize=9, fontweight='bold')
        else:
            ax.scatter(x, y, s=100, c='lightblue', zorder=5,
                       edgecolors='black', linewidths=0.5)
            ax.text(x, y, data['label'], ha='center', va='center', fontsize=6)

    lbl = champ.output_map[output_id].label
    ax.set_title(f'{lbl} — {champ.label}', fontsize=10, fontweight='bold')
    ax.axis('off')
    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# Plot: Hidden node receptive fields (polar)
# ---------------------------------------------------------------------------

def plot_receptive_fields(champ: ChampionData) -> plt.Figure | None:
    """Polar plots showing what each hidden node 'sees'."""
    if not champ.hidden_ids:
        return None

    h_inputs = hidden_node_inputs(champ)
    # Only plot hidden nodes with spatial inputs
    spatial_hidden = []
    for hid in champ.hidden_ids:
        spatial = [(src, w) for src, w in h_inputs[hid]
                   if src in champ.input_map and champ.input_map[src].eye_type in ('short', 'long')]
        if spatial:
            spatial_hidden.append((hid, spatial))

    if not spatial_hidden:
        return None

    n = len(spatial_hidden)
    cols = min(4, n)
    rows = math.ceil(n / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(3.5 * cols, 3.5 * rows),
                              subplot_kw={'projection': 'polar'})
    if rows == 1 and cols == 1:
        axes = np.array([[axes]])
    elif rows == 1:
        axes = axes.reshape(1, -1)
    elif cols == 1:
        axes = axes.reshape(-1, 1)

    for idx, (hid, spatial) in enumerate(spatial_hidden):
        r, c = divmod(idx, cols)
        ax = axes[r, c]
        ax.set_theta_offset(math.pi / 2)
        ax.set_theta_direction(-1)

        for src, w in spatial:
            s = champ.input_map[src]
            angle_rad = math.radians(s.angle_deg)
            color = sensor_color(s)
            # Bar height = |weight|, direction shows sign
            bar_color = color if w > 0 else '#333333'
            alpha = 0.7 if w > 0 else 0.4
            ax.bar(angle_rad, abs(w), width=0.15, bottom=0,
                   color=bar_color, alpha=alpha, edgecolor='none')

        # Find self-recurrent
        self_rec = any(c['source'] == hid and c['target'] == hid
                       for c in champ.recurrent_connections)
        rec_marker = ' ↻' if self_rec else ''

        # Find what outputs this hidden node feeds
        output_targets = []
        for conn in champ.enabled_connections:
            if conn['source'] == hid and conn['target'] in champ.output_map:
                output_targets.append(
                    f"{champ.output_map[conn['target']].label}({conn['weight']:+.1f})")

        ax.set_title(f'H{hid}{rec_marker}\n→ {", ".join(output_targets[:3])}',
                     fontsize=7, pad=10)
        ax.set_yticks([])
        ax.set_ylim(0, max(abs(w) for _, w in spatial) * 1.3)

    # Hide empty subplots
    for idx in range(len(spatial_hidden), rows * cols):
        r, c = divmod(idx, cols)
        axes[r, c].set_visible(False)

    fig.suptitle(f'Hidden Node Receptive Fields — {champ.label}',
                 fontsize=12, fontweight='bold')
    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# Plot: Morphology polar (sensor layout)
# ---------------------------------------------------------------------------

def plot_morphology(champ: ChampionData) -> plt.Figure:
    """Polar plot showing eye positions and arc widths."""
    ce = champ.compound_eyes
    eyes = ce.get('eyes', [])
    long_eyes = ce.get('longRangeEyes', [])

    fig, axes = plt.subplots(1, 2, figsize=(10, 5), subplot_kw={'projection': 'polar'})

    for ax_idx, (eye_list, title) in enumerate([(eyes, 'Short-range'), (long_eyes, 'Long-range')]):
        ax = axes[ax_idx]
        ax.set_theta_offset(math.pi / 2)
        ax.set_theta_direction(-1)

        cmap = plt.cm.tab20
        for i, eye in enumerate(eye_list):
            angle = math.radians(eye['centerAngleDeg'])
            half_arc = math.radians(eye['arcWidthDeg'] / 2)
            theta = np.linspace(angle - half_arc, angle + half_arc, 30)
            r_vals = np.ones_like(theta)
            color = cmap(i % 20)
            ax.fill_between(theta, 0, r_vals, alpha=0.5, color=color)
            ax.plot([angle, angle], [0, 1.05], color=color, linewidth=1.5, alpha=0.8)
            ax.text(angle, 1.15, f'{eye["id"]}', ha='center', va='center',
                    fontsize=6, color=color)

        ax.annotate('FWD', xy=(0, 1.25), ha='center', va='center',
                     fontsize=7, fontweight='bold', color='red', annotation_clip=False)
        ax.set_ylim(0, 1.3)
        ax.set_yticks([])
        ax.set_title(f'{title} ({len(eye_list)} eyes)', fontsize=10, fontweight='bold', pad=20)

    fig.suptitle(f'Sensor Layout — {champ.label}', fontsize=12, fontweight='bold', y=1.02)
    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# Plot: Summary stats comparison
# ---------------------------------------------------------------------------

def plot_summary_stats(champions: list[ChampionData]) -> plt.Figure:
    """Bar charts comparing network statistics."""
    labels = [c.label for c in champions]
    n = len(champions)

    metrics = {
        'Hidden Nodes': [len(c.hidden_ids) for c in champions],
        'Enabled Connections': [len(c.enabled_connections) for c in champions],
        'Recurrent Connections': [len(c.recurrent_connections) for c in champions],
        'Mean |Weight|': [
            np.mean([abs(c['weight']) for c in ch.enabled_connections])
            if ch.enabled_connections else 0
            for ch in champions
        ],
        'Max |Weight|': [
            max(abs(c['weight']) for c in ch.enabled_connections)
            if ch.enabled_connections else 0
            for ch in champions
        ],
    }

    n_metrics = len(metrics)
    fig, axes = plt.subplots(1, n_metrics, figsize=(3 * n_metrics, 4))
    if n_metrics == 1:
        axes = [axes]

    colors = plt.cm.Set2(np.linspace(0, 1, max(n, 3)))

    for ax, (metric, values) in zip(axes, metrics.items()):
        bars = ax.bar(range(n), values, color=colors[:n])
        ax.set_title(metric, fontsize=9, fontweight='bold')
        ax.set_xticks(range(n))
        ax.set_xticklabels([c.species.capitalize() for c in champions],
                           fontsize=8, rotation=30, ha='right')
        for bar, v in zip(bars, values):
            ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                    f'{v:.1f}' if isinstance(v, float) else str(v),
                    ha='center', va='bottom', fontsize=7)

    fig.suptitle('Network Statistics', fontsize=12, fontweight='bold')
    fig.tight_layout()
    return fig


def plot_weight_distributions(champions: list[ChampionData]) -> plt.Figure:
    """Overlaid weight distribution histograms."""
    fig, ax = plt.subplots(figsize=(8, 4))
    colors = plt.cm.Set2(np.linspace(0, 1, max(len(champions), 3)))

    for i, champ in enumerate(champions):
        weights = [c['weight'] for c in champ.enabled_connections]
        ax.hist(weights, bins=50, alpha=0.5, color=colors[i], label=champ.label,
                density=True, edgecolor='none')

    ax.axvline(0, color='black', linewidth=0.5, linestyle='--')
    ax.set_xlabel('Connection Weight')
    ax.set_ylabel('Density')
    ax.set_title('Weight Distributions', fontsize=12, fontweight='bold')
    ax.legend(fontsize=8)
    fig.tight_layout()
    return fig


# ---------------------------------------------------------------------------
# HTML generation
# ---------------------------------------------------------------------------

def generate_html(champions: list[ChampionData], figures: dict[str, str],
                  path_tables: dict) -> str:
    """Build self-contained HTML report."""
    from datetime import date

    # Summary cards
    cards_html = ''
    for c in champions:
        weights = [conn['weight'] for conn in c.enabled_connections]
        cards_html += f'''
        <div class="card">
            <h3>{c.label}</h3>
            <table class="stats">
                <tr><td>Species</td><td><strong>{c.species}</strong></td></tr>
                <tr><td>Input nodes</td><td>{len(c.input_map)}</td></tr>
                <tr><td>Output nodes</td><td>{len(c.output_map)}</td></tr>
                <tr><td>Hidden nodes</td><td><strong>{len(c.hidden_ids)}</strong></td></tr>
                <tr><td>Enabled connections</td><td>{len(c.enabled_connections)}</td></tr>
                <tr><td>Recurrent connections</td><td>{len(c.recurrent_connections)}</td></tr>
                <tr><td>Mean |weight|</td><td>{np.mean([abs(w) for w in weights]):.3f}</td></tr>
                <tr><td>Max |weight|</td><td>{max(abs(w) for w in weights):.2f}</td></tr>
            </table>
        </div>'''

    # Processing chains
    chains_html = ''
    for c in champions:
        chains = find_processing_chains(c)
        self_rec = [h for h in c.hidden_ids
                    if any(conn['source'] == h and conn['target'] == h
                           for conn in c.recurrent_connections)]
        chains_html += f'<div class="card"><h3>{c.label}</h3>'
        if chains:
            chains_html += '<h4>Processing Chains</h4><ul>'
            for chain in chains:
                chain_str = ' → '.join(f'H{n}' for n in chain)
                chains_html += f'<li><code>{chain_str}</code></li>'
            chains_html += '</ul>'
        if self_rec:
            chains_html += f'<h4>Self-Recurrent (Memory) Nodes</h4><p>{", ".join(f"H{h}" for h in self_rec)}</p>'
        if not chains and not self_rec:
            chains_html += '<p>No multi-node chains or self-recurrent nodes found.</p>'
        chains_html += '</div>'

    # Top paths tables
    paths_html = ''
    for c in champions:
        paths_html += f'<h3>{c.label}</h3>'
        for out_id in sorted(c.output_map.keys()):
            out_label = c.output_map[out_id].label
            key = (c.path, out_id)
            paths = path_tables.get(key, [])
            if not paths:
                continue
            paths_html += f'<h4>{out_label}</h4><table class="paths"><tr><th>Path</th><th>Effective Weight</th></tr>'
            for p in paths:
                route = []
                for nid in p['path']:
                    if nid in c.input_map:
                        route.append(f'<span class="input-node">{c.input_map[nid].short_label}</span>')
                    elif nid in c.output_map:
                        route.append(f'<span class="output-node">{c.output_map[nid].label}</span>')
                    else:
                        route.append(f'<span class="hidden-node">H{nid}</span>')
                route_str = ' → '.join(route)
                w = p['weight']
                w_class = 'weight-positive' if w > 0 else 'weight-negative'
                paths_html += f'<tr><td>{route_str}</td><td class="{w_class}">{w:+.3f}</td></tr>'
            paths_html += '</table>'

    # Assemble figures into sections
    def fig_row(keys, section_figures=figures):
        html = '<div class="figure-row">'
        for k in keys:
            if k in section_figures:
                html += f'<div class="figure-cell"><img src="{section_figures[k]}"></div>'
        html += '</div>'
        return html

    def fig_stack(keys, section_figures=figures):
        """Place figures stacked vertically (full width each)."""
        html = ''
        for k in keys:
            if k in section_figures:
                html += f'<div class="figure-full"><img src="{section_figures[k]}"></div>'
        return html

    # Build per-output section
    output_ids = sorted(champions[0].output_map.keys())
    per_output_html = ''
    for out_id in output_ids:
        out_label = champions[0].output_map[out_id].label
        per_output_html += f'<h3>{out_label}</h3>'
        keys = [f'per_output_{c.path}_{out_id}' for c in champions]
        per_output_html += fig_row(keys)

    files_list = ', '.join(f'<code>{Path(c.path).name}</code>' for c in champions)

    html = f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Champion Neural Network Comparison</title>
<style>
    body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
           max-width: 1600px; margin: 0 auto; padding: 20px; background: #fafafa; }}
    h1 {{ border-bottom: 3px solid #333; padding-bottom: 10px; }}
    h2 {{ border-bottom: 2px solid #666; padding-bottom: 5px; margin-top: 40px; }}
    .meta {{ color: #666; margin-bottom: 20px; }}
    .card-row {{ display: flex; gap: 20px; flex-wrap: wrap; }}
    .card {{ flex: 1; min-width: 280px; background: white; border: 1px solid #ddd;
             border-radius: 8px; padding: 15px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }}
    .stats td {{ padding: 4px 12px 4px 0; }}
    .stats td:first-child {{ color: #666; }}
    .figure-row {{ display: flex; gap: 10px; flex-wrap: wrap; justify-content: center; }}
    .figure-cell {{ flex: 1; min-width: 300px; text-align: center; }}
    .figure-cell img {{ max-width: 100%; border: 1px solid #eee; border-radius: 4px; }}
    .figure-full {{ text-align: center; margin-bottom: 15px; }}
    .figure-full img {{ max-width: 100%; border: 1px solid #eee; border-radius: 4px; }}
    table.paths {{ border-collapse: collapse; width: 100%; margin-bottom: 15px; }}
    table.paths td, table.paths th {{ border: 1px solid #ddd; padding: 6px 10px; text-align: left; }}
    table.paths th {{ background: #f5f5f5; }}
    .weight-positive {{ color: #2166ac; font-weight: bold; }}
    .weight-negative {{ color: #b2182b; font-weight: bold; }}
    .input-node {{ color: #555; font-size: 0.9em; }}
    .hidden-node {{ color: #0077cc; font-weight: bold; }}
    .output-node {{ color: #cc7700; font-weight: bold; }}
    code {{ background: #f0f0f0; padding: 2px 6px; border-radius: 3px; }}
</style>
</head>
<body>
<h1>Champion Neural Network Comparison</h1>
<p class="meta">Generated: {date.today()} | Files: {files_list}</p>

<h2>1. Architecture Summary</h2>
<div class="card-row">{cards_html}</div>

{fig_row(['summary_stats'])}
{fig_row(['weight_distributions'])}

<h2>2. Sensor Layout</h2>
{fig_row([f'morphology_{c.path}' for c in champions])}

<h2>3. Network Overview (Layered)</h2>
{fig_stack([f'overview_{c.path}' for c in champions])}

<h2>3b. Network Overview (Force-Directed)</h2>
<p style="color:#666">Nodes cluster by connection strength &mdash; strongly connected groups pull together. Outputs pinned on right. Hidden node size reflects degree (number of connections).</p>
{fig_stack([f'force_{c.path}' for c in champions])}

<h2>4. Direct Connection Weights (Input × Output)</h2>
{fig_row([f'direct_heatmap_{c.path}' for c in champions])}

<h2>5. Effective Weights (Through Hidden Nodes)</h2>
{fig_row([f'effective_heatmap_{c.path}' for c in champions])}

<h2>6. Per-Thruster Network Detail</h2>
{per_output_html}

<h2>7. Hidden Node Analysis</h2>
<div class="card-row">{chains_html}</div>
{fig_row([f'receptive_{c.path}' for c in champions])}

<h2>8. Strongest Signal Paths</h2>
{paths_html}

</body>
</html>'''

    return html


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Compare evolved boid champion neural networks.')
    parser.add_argument('champions', nargs='+', help='Champion JSON files (1-4)')
    parser.add_argument('--output', '-o', default='champion_comparison.html',
                        help='Output HTML file (default: champion_comparison.html)')
    parser.add_argument('--top-n', type=int, default=30,
                        help='Strongest connections in overview graph (default: 30)')
    args = parser.parse_args()

    if len(args.champions) > 4:
        print('Warning: only first 4 champions will be compared')
        args.champions = args.champions[:4]

    print(f'Loading {len(args.champions)} champion(s)...')
    champions = [load_champion(p) for p in args.champions]
    for c in champions:
        print(f'  {c.label}: {len(c.input_map)} inputs, {len(c.output_map)} outputs, '
              f'{len(c.hidden_ids)} hidden, {len(c.enabled_connections)} connections')

    figures = {}

    # Summary stats
    print('Generating summary stats...')
    figures['summary_stats'] = fig_to_base64(plot_summary_stats(champions))
    figures['weight_distributions'] = fig_to_base64(plot_weight_distributions(champions))

    for c in champions:
        print(f'Analysing {c.label}...')

        # Morphology
        figures[f'morphology_{c.path}'] = fig_to_base64(plot_morphology(c))

        # Network overview (layered + force-directed)
        figures[f'overview_{c.path}'] = fig_to_base64(plot_network_overview(c, args.top_n))
        figures[f'force_{c.path}'] = fig_to_base64(plot_network_force(c, args.top_n))

        # Heatmaps
        dm = direct_weight_matrix(c)
        figures[f'direct_heatmap_{c.path}'] = fig_to_base64(plot_weight_heatmap(c, dm, 'Direct Weights'))
        em = effective_weight_matrix(c)
        figures[f'effective_heatmap_{c.path}'] = fig_to_base64(
            plot_weight_heatmap(c, em, 'Effective Weights'))

        # Per-output graphs
        for out_id in sorted(c.output_map.keys()):
            fig = plot_per_output_graph(c, out_id)
            figures[f'per_output_{c.path}_{out_id}'] = fig_to_base64(fig)

        # Receptive fields
        rf_fig = plot_receptive_fields(c)
        if rf_fig:
            figures[f'receptive_{c.path}'] = fig_to_base64(rf_fig)

    # Top paths
    print('Computing signal paths...')
    path_tables = {}
    for c in champions:
        for out_id in sorted(c.output_map.keys()):
            paths = find_top_paths(c, out_id)
            path_tables[(c.path, out_id)] = paths

    # Generate HTML
    print('Generating HTML report...')
    html = generate_html(champions, figures, path_tables)

    with open(args.output, 'w') as f:
        f.write(html)
    print(f'Report saved to: {args.output}')


if __name__ == '__main__':
    main()
