"""
experiments/parse_reconstruction_logs.py -- recovers per-block, per-policy
telegram reconstruction rate from the .log files an H2 run already wrote
(b{block}_{policy}.log), with NO new hardware run required.

Usage: python3 experiments/parse_reconstruction_logs.py data/experiments/<h2_run_id>
"""
import sys, os, re, glob
import pandas as pd
from scipy import stats
import numpy as np

d = sys.argv[1]
rows = []
for path in glob.glob(os.path.join(d, "b*_*.log")):
    fname = os.path.basename(path)
    m = re.match(r"b(\d+)_(.+)\.log", fname)
    if not m:
        continue
    block, policy = int(m.group(1)), m.group(2)
    recon = timeout = 0
    with open(path, errors="ignore") as f:
        for line in f:
            if "RECONSTRUCTED" in line:
                recon += 1
            elif "TIMED OUT" in line:
                timeout += 1
    total = recon + timeout
    rows.append(dict(block=block, policy=policy, reconstructed=recon,
                      timed_out=timeout, total=total,
                      rate=recon / total if total else float("nan")))

df = pd.DataFrame(rows).sort_values(["block", "policy"])
print("== Per-block reconstruction rate (recovered from .log files)\n",
      df.to_string(index=False))

t = df.pivot(index="block", columns="policy", values="rate")
print("\n== Pivot\n", t.round(4).to_string())
print(f"\n== Per-policy mean +/- SD across {t.shape[0]} blocks")
print((t.mean().round(4).astype(str) + " +/- " + t.std().round(4).astype(str)).to_string())

if "state-aware" in t.columns:
    rng = np.random.default_rng(0)
    for p in [c for c in t.columns if c != "state-aware"]:
        pair = t[["state-aware", p]].dropna()
        diff = (pair["state-aware"] - pair[p]).values
        if len(diff) < 3:
            continue
        bs = [rng.choice(diff, len(diff), replace=True).mean() for _ in range(10000)]
        try:
            wp = stats.wilcoxon(diff).pvalue
        except ValueError:
            wp = float("nan")
        print(f"\nstate-aware vs {p}: mean diff={diff.mean():+.4f}, "
              f"95% CI=[{np.percentile(bs,2.5):+.4f}, {np.percentile(bs,97.5):+.4f}], "
              f"Wilcoxon p={wp:.4f}")
