"""python3 experiments/analyze.py data/experiments/<run_id>"""
import sys, os
import numpy as np, pandas as pd
d = sys.argv[1]
o = pd.read_csv(os.path.join(d, "outcomes.csv"))
sent = o[o.outcome.isin(["success", "failed", "unavailable"])].copy()
res = sent[sent.outcome != "unavailable"].copy(); res["ok"] = (res.outcome == "success").astype(int)
print("== H1 outcome counts\n", sent.outcome.value_counts().to_string())
print("\n== P(ACK delivered) by gateway\n", res.groupby("gws").ok.agg(["mean", "count"]).round(3).to_string())
print("\n== by node\n", res.groupby("node").ok.agg(["mean", "count"]).round(3).to_string())
single = res[res.gws.astype(str).isin(["1", "2"])].copy()
single["belief"] = np.where(single.gws.astype(str) == "1", single.b1, single.b2)
try:
    from sklearn.metrics import roc_auc_score
    print("\n== AUC(HMM belief of chosen gw -> delivered):",
          round(roc_auc_score(single.ok, single.belief), 3), f"n={len(single)}")
except Exception as e:
    print("AUC n/a:", e)
if res.block.nunique() > 1 and res.policy.nunique() > 1:
    t = res.groupby(["block", "policy"]).ok.mean().unstack()
    print("\n== H2 per-block delivery\n", t.round(3).to_string())
    rng = np.random.default_rng(0)
    for p in [c for c in t.columns if c != "state-aware"]:
        diff = (t["state-aware"] - t[p]).dropna().values
        bs = [rng.choice(diff, len(diff)).mean() for _ in range(5000)]
        print(f"SAF-LoRa - {p}: {diff.mean():+.3f}  95% CI [{np.percentile(bs,2.5):+.3f}, {np.percentile(bs,97.5):+.3f}]  blocks={len(diff)}")
if "interferer_at_send" in res and res.interferer_at_send.nunique() > 1:
    print("\n== H4 delivery by interferer state x gateway\n",
          res.groupby(["interferer_at_send", "gws"]).ok.agg(["mean", "count"]).round(3).to_string())
    dc = pd.read_csv(os.path.join(d, "decisions.csv"))
    print("\n== H4 gateway choice by interferer state\n",
          pd.crosstab(dc.interferer, dc.sent_gws.fillna("none"), normalize="index").round(3).to_string())
dc = pd.read_csv(os.path.join(d, "decisions.csv"))
print("\n== H3 candidates by reject reason\n", dc.reject_reason.fillna("sent").value_counts().to_string())
tp = os.path.join(d, "telemetry.csv")
if os.path.exists(tp):
    tm = pd.read_csv(tp)
    print(f"\n== node RX-window on-time: mean {tm.rx_on_ms_prev.mean():.0f} ms "
          f"(energy/telegram ~ {tm.rx_on_ms_prev.mean()*4.6e-3*3.3:.2f} mJ at 4.6 mA, 3.3 V)")
