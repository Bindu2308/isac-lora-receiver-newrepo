"""
experiments/h2_stats.py -- proper block-paired statistics for H2.

Usage: python3 experiments/h2_stats.py data/experiments/<h2_run_id>

Reports, for state-aware vs each other policy, across the 8 randomized
blocks:
  - mean paired difference
  - bootstrap 95% CI (as before)
  - Wilcoxon signed-rank test (non-parametric, appropriate for n=8 paired
    blocks -- doesn't assume normality, unlike a paired t-test)
  - Cohen's d for paired samples (effect size: mean(diff)/std(diff))
"""
import sys, os
import numpy as np
import pandas as pd
from scipy import stats

d = sys.argv[1]
o = pd.read_csv(os.path.join(d, "outcomes.csv"))
res = o[o.outcome.isin(["success", "failed"])].copy()
res["ok"] = (res.outcome == "success").astype(int)

t = res.groupby(["block", "policy"]).ok.mean().unstack()
print("== Per-block delivery rate\n", t.round(4).to_string())
print(f"\n== Per-policy mean +/- SD across {t.shape[0]} blocks")
print((t.mean().round(4).astype(str) + " +/- " + t.std().round(4).astype(str)).to_string())

print("\n== SAF-LoRa (state-aware) vs each policy, paired by block")
rng = np.random.default_rng(0)
for p in [c for c in t.columns if c != "state-aware"]:
    pair = t[["state-aware", p]].dropna()
    diff = (pair["state-aware"] - pair[p]).values
    n = len(diff)
    if n < 3:
        print(f"{p}: insufficient paired blocks (n={n})")
        continue
    mean_d, sd_d = diff.mean(), diff.std(ddof=1)
    cohens_d = mean_d / sd_d if sd_d > 0 else float("nan")
    bs = [rng.choice(diff, n, replace=True).mean() for _ in range(10000)]
    ci_lo, ci_hi = np.percentile(bs, [2.5, 97.5])
    try:
        wstat, wp = stats.wilcoxon(diff)
    except ValueError:
        wstat, wp = float("nan"), float("nan")
    sig = "significant (p<0.05)" if wp == wp and wp < 0.05 else "NOT significant"
    print(f"\n{p} (n={n} blocks):")
    print(f"  mean diff        = {mean_d:+.4f}")
    print(f"  95% CI (boot)    = [{ci_lo:+.4f}, {ci_hi:+.4f}]")
    print(f"  Cohen's d        = {cohens_d:+.3f}")
    print(f"  Wilcoxon p-value = {wp:.4f}  -> {sig}")
