#!/usr/bin/env python3
"""Validate correctness, deterministic results and standalone report creation."""
import csv
import math
from pathlib import Path
import subprocess
import sys
import tempfile


def load_csv(path):
    with path.open(newline='',encoding='utf-8') as file:
        return list(csv.DictReader(file))


def main():
    engine, report_script=sys.argv[1:3]
    with tempfile.TemporaryDirectory(prefix='himera-eval-') as td:
        base=Path(td)
        folders=[base/'first',base/'second']
        for folder in folders:
            subprocess.run([engine,'--out',str(folder)],check=True,capture_output=True,text=True,timeout=30)
            subprocess.run([sys.executable,report_script,'--input',str(folder)],check=True,capture_output=True,text=True,timeout=30)
            page=(folder/'report.html').read_text(encoding='utf-8')
            assert 'HIMERA / AI Evidence Lab' in page
            assert 'D* Lite' in page and 'GOAP' in page and 'Fixed threshold' in page
        x,y=[load_csv(f/'metrics.csv') for f in folders]
        assert len(x)>500 and len(y)==len(x)
        assert {r['agent'] for r in x}=={'Security','Hollow','Trooper','Director'}
        # Ignore machine-dependent wall-clock timings when assessing reproducibility.
        static=lambda records:[{k:v for k,v in row.items() if k!='value'} | ({'value':row['value']} if row['metric']!='planning_us' else {'value':'volatile'}) for row in records]
        assert static(x)==static(y), 'Non-time metrics must be deterministic'
        assert load_csv(folders[0]/'decisions.csv')==load_csv(folders[1]/'decisions.csv')
        cases=lambda agent,approach,metric:[float(r['value']) for r in x if r['agent']==agent and r['approach']==approach and r['metric']==metric]
        assert sum(cases('Security','GOAP','goal_reached'))==7
        assert sum(cases('Hollow','Belief + reachability','reachable'))==6
        assert sum(cases('Trooper','D* Lite','optimal_path'))==45
        assert sum(cases('Director','Utility AI','constraint_violations'))==0
        assert all(math.isfinite(float(r['value'])) for r in x)
        print(f'v0.4 verification passed: {len(x)} metric rows, 4 agent groups, deterministic reports, optimal routes, valid constraints.')


if __name__=='__main__':
    main()
