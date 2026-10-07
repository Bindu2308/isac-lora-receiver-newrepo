"""
experiments/overhead_analysis.py -- engineering cost table for H2/H3 data.
Groups by (policy, ack_quota-implied-by-decisions) when block/quota info
is available, so H3's scarcity curve is visible.
"""
import sys, os
import pandas as pd

ACK_AIRTIME_S = 0.04634
ACK_BYTES = 7

d = sys.argv[1]
dec = pd.read_csv(os.path.join(d, "decisions.csv"))

group_cols = ["policy", "block"]
print(f"== ACK transmission overhead by {group_cols}")
rows = []
for key, g in dec.groupby(group_cols):
    sent = g[g.sent_gws.notna() & (g.sent_gws != "")]
    n_candidates = len(g)
    n_gw_tx = sent.sent_gws.astype(str).str.count(r"\d+").sum()
    rej = g[g.reject_reason.notna() & (g.reject_reason != "")]
    rej_quota = g[g.reject_reason == "quota"]
    row = dict(zip(group_cols, key if isinstance(key, tuple) else (key,)))
    row.update(dict(candidates=n_candidates, acks_sent=len(sent),
                     gw_transmissions=int(n_gw_tx),
                     airtime_s=round(n_gw_tx * ACK_AIRTIME_S, 2),
                     bytes_on_air=int(n_gw_tx * ACK_BYTES),
                     rejected=len(rej), rejected_quota=len(rej_quota),
                     reject_rate=round(len(rej) / n_candidates, 3) if n_candidates else 0))
    rows.append(row)
tbl = pd.DataFrame(rows).set_index(group_cols).sort_index()
print(tbl.to_string())

print("\n== Gateway utilization (share of ACK transmissions per gateway)")
sent = dec[dec.sent_gws.notna() & (dec.sent_gws != "")]
util = sent.groupby("policy").apply(
    lambda g: g.sent_gws.astype(str).str.split("+").explode().value_counts(normalize=True)
).unstack(fill_value=0).round(3)
print(util.to_string())

tp = os.path.join(d, "telemetry.csv")
if os.path.exists(tp):
    tm = pd.read_csv(tp)
    print("\n== Node RX-window on-time (proxy for node-side energy cost)")
    print(f"  mean = {tm.rx_on_ms_prev.mean():.0f} ms, "
          f"median = {tm.rx_on_ms_prev.median():.0f} ms, "
          f"p95 = {tm.rx_on_ms_prev.quantile(0.95):.0f} ms")
    mj = tm.rx_on_ms_prev.mean() * 4.6e-3 * 3.3
    print(f"  approx energy/telegram = {mj:.2f} mJ (4.6 mA RX @ 3.3 V)")
