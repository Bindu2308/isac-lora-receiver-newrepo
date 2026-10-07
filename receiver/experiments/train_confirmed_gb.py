"""
experiments/train_confirmed_gb.py -- validates q_{n,g} using H1's
CONFIRMED ACK outcomes (success/failed), closing the paper's stated gap
that the original AUC=0.619 analysis used only observational proxies.

Usage: python3 experiments/train_confirmed_gb.py data/experiments/<run_id>
(pass multiple run-id dirs to pool them, e.g. the h2 and h3 backups)

Uses GroupKFold split by `block`, matching the paper's existing
node-grouped / telegram-grouped leakage discipline for the original GB
model -- never a random row split.

This model is NOT wired into the live scheduler. It is an offline
validation/analysis artifact only, exactly like the paper's original
auxiliary GB classifier.
"""
import sys, os
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.model_selection import GroupKFold, cross_val_predict
from sklearn.metrics import (roc_auc_score, average_precision_score,
                              brier_score_loss)
from sklearn.calibration import calibration_curve

frames = []
for run_dir in sys.argv[1:]:
    o = pd.read_csv(os.path.join(run_dir, "outcomes.csv"))
    o = o[o.outcome.isin(["success", "failed"])].copy()
    o["y"] = (o.outcome == "success").astype(int)
    o["run"] = os.path.basename(run_dir.rstrip("/"))
    frames.append(o)
df = pd.concat(frames, ignore_index=True)
print(f"Loaded {len(df)} confirmed outcomes from {len(frames)} run(s)")

# gws can be "1", "2" (single-gateway sends only -- drop multi-gw broadcast rows)
df["gws_norm"] = df.gws.astype(str).str.replace(r"\.0$", "", regex=True)
df = df[df.gws_norm.isin(["1", "2"])].copy()
df["gws"] = df.gws_norm
df["gw"] = df.gws.astype(int)
df["belief_chosen"] = np.where(df.gw == 1, df.b1, df.b2)
df["belief_other"] = np.where(df.gw == 1, df.b2, df.b1)
df["belief_gap"] = df.belief_chosen - df.belief_other
df["interferer_on"] = (df.interferer_at_send == "ON").astype(int)
df["policy_state_aware"] = (df.policy == "state-aware").astype(int)

feat_cols = ["belief_chosen", "belief_other", "belief_gap", "gw",
             "interferer_on", "policy_state_aware"]
X = df[feat_cols].fillna(0.5).values
y = df["y"].values
groups = df["block"].astype(str) + "_" + df["run"]

n_groups = groups.nunique()
if n_groups < 3:
    print(f"Only {n_groups} groups -- too few for GroupKFold, using 2-fold")
    n_splits = 2
else:
    n_splits = min(4, n_groups)

gkf = GroupKFold(n_splits=n_splits)
clf = GradientBoostingClassifier(n_estimators=100, max_depth=3,
                                  learning_rate=0.1, random_state=42)

proba = cross_val_predict(clf, X, y, cv=gkf, groups=groups,
                           method="predict_proba")[:, 1]

print(f"\n== Confirmed-delivery GB, grouped {n_splits}-fold CV (groups=block x run)")
print(f"AUC          = {roc_auc_score(y, proba):.3f}")
print(f"PR-AUC       = {average_precision_score(y, proba):.3f}")
print(f"Brier score  = {brier_score_loss(y, proba):.4f}  (lower is better; 0.25 = uninformative)")

frac_pos, mean_pred = calibration_curve(y, proba, n_bins=10, strategy="quantile")
print("\n== Calibration (predicted vs. observed, 10 quantile bins)")
for mp, fp in zip(mean_pred, frac_pos):
    print(f"  predicted {mp:.3f}  ->  observed {fp:.3f}")

clf.fit(X, y)
print("\n== Feature importance (fit on full data)")
for name, imp in sorted(zip(feat_cols, clf.feature_importances_),
                         key=lambda t: -t[1]):
    print(f"  {name:<20s} {imp:.3f}")

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(4, 4))
    ax.plot([0, 1], [0, 1], "k--", label="perfectly calibrated")
    ax.plot(mean_pred, frac_pos, "o-", label="confirmed-delivery GB")
    ax.set_xlabel("Mean predicted probability")
    ax.set_ylabel("Observed frequency")
    ax.legend()
    fig.tight_layout()
    out = os.path.join(sys.argv[1], "calibration_curve.png")
    fig.savefig(out, dpi=150)
    print(f"\nSaved calibration plot to {out}")
except ImportError:
    print("\n(matplotlib not installed -- skipping plot; numbers above are still valid)")
