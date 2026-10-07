"""
experiments/per_gateway_qg.py -- per-gateway, per-policy confirmed
ACK-delivery rate q_g with Wilson 95% CIs, from outcomes.csv you
already have. No receiver code change needed.

Usage: python3 experiments/per_gateway_qg.py data/experiments/<run_id>
"""
import sys, os
import pandas as pd
from statsmodels.stats.proportion import proportion_confint

o = pd.read_csv(os.path.join(sys.argv[1], "outcomes.csv"))
o = o[o.outcome.isin(["success", "failed"])].copy()
o["ok"] = (o.outcome == "success").astype(int)
o["gw"] = o.gws.astype(str).str.replace(r"\.0$", "", regex=True)
o = o[o.gw.isin(["1", "2"])]

for policy, g in o.groupby("policy"):
    print(f"\n== {policy}")
    for gw, gg in g.groupby("gw"):
        k, n = gg.ok.sum(), len(gg)
        lo, hi = proportion_confint(k, n, method="wilson")
        print(f"  GW{gw}: q_g={k/n:.3f} [{lo:.3f},{hi:.3f}]  n={n}")
