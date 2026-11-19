#!/usr/bin/env python3
"""Make an offline HTML viva dashboard from C++ evaluation CSV files.

No external packages, CDNs or browser server required. This never fabricates metrics.
"""
import argparse
import csv
import html
import statistics
from collections import defaultdict
from pathlib import Path


def read_csv(path):
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def grouped_mean(rows, agent, metric, approach):
    vals = [float(r['value']) for r in rows if r['agent'] == agent and r['metric'] == metric and r['approach'] == approach]
    if not vals:
        raise ValueError(f'No matching metric: {agent}, {metric}, {approach}')
    return statistics.mean(vals)


def grouped_sum(rows, agent, metric, approach):
    vals = [float(r['value']) for r in rows if r['agent'] == agent and r['metric'] == metric and r['approach'] == approach]
    if not vals:
        raise ValueError(f'No matching metric: {agent}, {metric}, {approach}')
    return sum(vals)


def number(n, places=2):
    return f'{n:,.{places}f}'


def chart(title, values, unit='', better='lower'):
    maximum = max([x[1] for x in values] + [1e-9])
    rows = []
    for name, value, color in values:
        width = min(100, max(0, value / maximum * 100))
        rows.append(f'''<div class="bar-label"><span>{html.escape(name)}</span><strong>{number(value)} {html.escape(unit)}</strong></div>
<div class="bar-track"><div class="bar" style="width:{width:.3f}%;background:{color}"></div></div>''')
    return f'''<article class="card"><h3>{html.escape(title)}</h3><p class="hint">Smaller is better: {better == 'lower' and 'yes' or 'no'}.</p>{''.join(rows)}</article>'''


def render(metrics, decisions):
    mean = lambda a, m, v: grouped_mean(metrics, a, m, v)
    total = lambda a, m, v: grouped_sum(metrics, a, m, v)
    primary = '#38bdf8'
    baseline = '#fbbf24'
    security_adv = mean('Security', 'plan_cost', 'GOAP')
    security_base = mean('Security', 'plan_cost', 'Reactive rules')
    hollow_reach_adv = total('Hollow', 'reachable', 'Belief + reachability')
    hollow_reach_base = total('Hollow', 'reachable', 'Raw sound target')
    dstar_exp = total('Trooper', 'expanded_states', 'D* Lite')
    astar_exp = total('Trooper', 'expanded_states', 'Fresh A*')
    dstar_time = total('Trooper', 'planning_us', 'D* Lite')
    astar_time = total('Trooper', 'planning_us', 'Fresh A*')
    director_adv = total('Director', 'constraint_violations', 'Utility AI')
    director_base = total('Director', 'constraint_violations', 'Fixed threshold')
    if any([total('Security', 'goal_reached', 'GOAP') < 7,
            hollow_reach_adv < 6,
            total('Trooper', 'optimal_path', 'D* Lite') < 45,
            director_adv]):
        raise ValueError('Critical evaluation invariant failed; refusing to present as valid')
    metric_cards = [
        ('Security', f'{number(security_adv)} vs {number(security_base)}', 'Mean action cost · GOAP vs reactive rules'),
        ('Hollow', f'{int(hollow_reach_adv)}/6 vs {int(hollow_reach_base)}/6', 'Reachable destinations · belief vs raw sound'),
        ('Trooper', f'{int(dstar_exp):,} vs {int(astar_exp):,}', 'Total expanded states · D* Lite vs A*'),
        ('Director', f'{int(director_adv)} vs {int(director_base)}', 'Constraint violations · utility vs fixed threshold'),
    ]
    cards = ''.join(f'<div class="stat"><span>{html.escape(a)}</span><strong>{html.escape(v)}</strong><small>{html.escape(desc)}</small></div>' for a,v,desc in metric_cards)
    charts = [
        chart('Security · average symbolic action cost', [('GOAP',security_adv,primary),('Reactive rules',security_base,baseline)], 'cost units'),
        chart('Hollow · reachable targets', [('Belief + reachability',hollow_reach_adv,primary),('Raw sound target',hollow_reach_base,baseline)], 'of 6',better='higher'),
        chart('Trooper · total search expansions', [('D* Lite',dstar_exp,primary),('Fresh A*',astar_exp,baseline)], 'states'),
        chart('Trooper · total measured planning time', [('D* Lite',dstar_time,primary),('Fresh A*',astar_time,baseline)], 'µs'),
        chart('Director · safety constraint violations', [('Utility AI',director_adv,primary),('Fixed threshold',director_base,baseline)], 'violations'),
    ]
    order={'Security':0,'Hollow':1,'Trooper':2,'Director':3}
    decisions.sort(key=lambda row:(order.get(row['agent'],100),row['scenario'],row['approach']))
    table=''.join('''<tr data-agent="{agent}"><td>{agent}</td><td>{scenario}</td><td><span class="approach">{approach}</span></td><td>{decision}</td><td>{explanation}</td></tr>'''.format(**{k:html.escape(str(r[k])) for k in ('agent','scenario','approach','decision','explanation')}) for r in decisions)
    return '''<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>HIMERA v0.4 | AI Evaluation Lab</title>
<style>
:root {color-scheme:dark;font-family:Inter,system-ui,-apple-system,Segoe UI,sans-serif;--panel:#151e30;--text:#e5ecff;--muted:#9caecc}
*{box-sizing:border-box}body{margin:0;background:#0b1120;color:var(--text)}header{padding:36px max(24px,calc((100vw - 1160px)/2));background:radial-gradient(circle at 90% 0%,#12395c,#101a30 60%,#101a30);border-bottom:1px solid #34425c}
h1{font-size:clamp(1.7rem,4vw,2.8rem);margin:8px 0 10px}p{line-height:1.55}header p{color:#bfcee7;max-width:800px}.eyebrow{letter-spacing:.14em;color:#58d7fa;font-size:.8rem;font-weight:800}
main{max-width:1210px;margin:auto;padding:24px}h2{font-size:1.26rem;margin:35px 0 15px}.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}.stat,.card,.panel{background:var(--panel);border:1px solid #2e3c52;border-radius:14px;padding:20px}
.stat span{color:#82c9e8;font-size:.85rem}.stat strong{display:block;font-size:1.7rem;margin:10px 0}.stat small,.hint{color:var(--muted);font-size:.82rem}.charts{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:14px}.card h3{font-size:1rem;margin:0}.card .hint{margin:5px 0 18px}.bar-label{display:flex;justify-content:space-between;gap:12px;font-size:.87rem;margin:15px 0 7px}.bar-label strong{white-space:nowrap}.bar-track{height:12px;background:#293549;border-radius:20px;overflow:hidden}.bar{height:100%;border-radius:20px;min-width:0}
.panel{overflow:auto}.panel p,.info{color:#aebfdb}table{border-collapse:collapse;width:100%;font-size:.86rem;min-width:780px}th{text-align:left;color:#b9c9e8;border-bottom:1px solid #405270;padding:12px 10px}td{padding:11px 10px;border-bottom:1px solid #29384f;vertical-align:top}td:nth-child(4){min-width:180px}.approach{color:#68d6ff}select{padding:9px 12px;background:#0b1527;border:1px solid #58718b;color:white;border-radius:8px}a{color:#75d7fc}.footer{margin:30px 0;color:#8a9db8;font-size:.85rem}
</style></head><body>
<header><div class="eyebrow">SE3062 · INTELLIGENT SYSTEMS · EVALUATION</div><h1>HIMERA / AI Evidence Lab</h1><p>Reproducible, synthetic comparisons generated by the project’s actual C++ agents. Baselines are deliberately simple reference policies. These numbers are not Unreal Engine FPS benchmarks and do not prove universal algorithm superiority.</p></header>
<main><h2>Evaluation summary</h2><div class="stats">''' + cards + '''</div><h2>Measured comparisons</h2><div class="charts">''' + ''.join(charts) + '''</div>
<h2>Decision traces <select id="filter" aria-label="Filter decisions by agent"><option value="all">All agents</option><option>Security</option><option>Hollow</option><option>Trooper</option><option>Director</option></select></h2>
<div class="panel"><p>All choices below come from the evaluation runner. Scenario names, action plans, and descriptions can be used to reproduce and explain individual decisions in a viva.</p><table><thead><tr><th>Agent</th><th>Scenario</th><th>Approach</th><th>Selected action / result</th><th>Explanation</th></tr></thead><tbody>''' + table + '''</tbody></table></div>
<h2>Interpretation and limitations</h2><div class="panel info"><p><strong>Security:</strong> symbolic GOAP minimizes a specified action cost; the baseline is a greedy rule policy and does not proactively seek cover. These costs are simulated units, not real damage or milliseconds.</p><p><strong>Hollow:</strong> a Bayesian-style grid belief chooses a reachable target; the raw-sound baseline directly targets the reported sound tile. The test does not measure real-world tracking accuracy.</p><p><strong>Trooper:</strong> both planners use identical weighted grid costs and Manhattan heuristic. Every path was checked for correct cost. Expanded nodes and runtime are different metrics: incremental D* Lite can visit fewer nodes while still taking longer in small cases.</p><p><strong>Director:</strong> fairness means obeying documented health, ammo, safe-zone, cooldown and enemy-cap constraints. The fixed threshold intentionally ignores these. Zero constraint violations does not by itself imply a more enjoyable encounter.</p><p><strong>Repeatability:</strong> scenarios and non-time metrics are deterministic; measured microseconds vary with machine, compiler, operating system, load and run order. Collect repeated timing trials before making performance claims. No randomized population or Unreal NPC combat evaluation is included.</p></div>
<p class="footer">Source: evaluation/metrics.csv and evaluation/decisions.csv · Generated entirely offline using Python standard library. Review both CSVs alongside the source code before citing results.</p></main>
<script>document.getElementById('filter').addEventListener('change',function(){let selected=this.value;document.querySelectorAll('tr[data-agent]').forEach(row=>row.hidden=selected!=='all'&&row.dataset.agent!==selected)});</script></body></html>'''


def main():
    parser=argparse.ArgumentParser(description='Create offline HIMERA viva evidence dashboard')
    parser.add_argument('--input',default='evaluation-results',help='Folder containing metrics.csv and decisions.csv')
    parser.add_argument('--output',default=None,help='HTML destination (default: INPUT/report.html)')
    args=parser.parse_args()
    folder=Path(args.input)
    output=Path(args.output) if args.output else folder/'report.html'
    metrics=read_csv(folder/'metrics.csv')
    decisions=read_csv(folder/'decisions.csv')
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(render(metrics,decisions),encoding='utf-8')
    print('Wrote',output.resolve())


if __name__=='__main__':
    main()
