"""
experiments/hmm_hw_validation.py -- closes the "HMM validated only in
simulation" limitation using decisions.csv's already-logged (b1, b2,
interferer) columns. No receiver code change needed -- this data was
already being logged by ack_ledger.py.

Usage: python3 experiments/hmm_hw_validation.py data/experiments/<run_id>
"""
import sys, os
import pandas as pd, numpy as np
from sklearn.metrics import roc_auc_score, confusion_matrix

d = pd.read_csv(os.path.join(sys.argv[1], "decisions.csv"))
d = d[d.interferer.isin(["ON", "OFF"])].copy()
y = (d.interferer == "ON").astype(int)
maj = 1 - y.mean()
print(f"n={len(d)}  P(ON)={y.mean():.3f}  majority-class (always OFF) baseline={maj:.3f}")

for col in ["b1", "b2"]:
    auc = roc_auc_score(y, 1 - d[col])
    pred_on = d[col] < 0.5
    acc = (pred_on == y).mean()
    cm = confusion_matrix(y, pred_on)
    print(f"\n{col}: mean(ON)={d[y==1][col].mean():.3f}  mean(OFF)={d[y==0][col].mean():.3f}")
    print(f"  AUC(belief -> interferer ON) = {auc:.3f}")
    print(f"  Accuracy @ 0.5 threshold = {acc:.3f} (majority baseline = {maj:.3f})")
    print(f"  Confusion matrix [[TN,FP],[FN,TP]]:\n{cm}")
