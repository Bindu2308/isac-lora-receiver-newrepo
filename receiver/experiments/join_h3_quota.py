"""
experiments/join_h3_quota.py -- tags each decisions.csv row with the
quota that was active at that timestamp, using quota_runs.csv's
(rep, quota, start_timestamp) records. Recovers the H3 scarcity table
from data you already collected -- no rerun needed.
"""
import sys, os
import pandas as pd

d = sys.argv[1]
dec = pd.read_csv(os.path.join(d, "decisions.csv"))
qr = pd.read_csv(os.path.join(d, "quota_runs.csv"),
                  names=["rep", "quota", "start_t"])
qr = qr.sort_values("start_t").reset_index(drop=True)

def tag(t):
    prior = qr[qr.start_t <= t]
    if prior.empty:
        return None, None
    row = prior.iloc[-1]
    return row.rep, row.quota

dec[["rep", "quota"]] = dec["t"].apply(lambda t: pd.Series(tag(t)))
dec = dec.dropna(subset=["quota"])
dec["quota"] = dec["quota"].astype(int)

print("== ACK candidates / rejections by quota")
g = dec.groupby("quota")
tbl = pd.DataFrame({
    "candidates": g.size(),
    "sent": g.apply(lambda x: (x.sent_gws.notna() & (x.sent_gws != "")).sum()),
    "rejected_quota": g.apply(lambda x: (x.reject_reason == "quota").sum()),
    "rejected_airtime": g.apply(lambda x: (x.reject_reason == "airtime").sum()),
})
tbl["reject_rate"] = (tbl.rejected_quota / tbl.candidates).round(3)
print(tbl.sort_index(ascending=False).to_string())
