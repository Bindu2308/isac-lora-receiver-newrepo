import csv, sys, os, collections, statistics as st
d = sys.argv[1] if len(sys.argv) > 1 else "receiver/data/experiments/snr_campaign"
if not os.path.exists(f"{d}/decisions.csv"):
    d = "receiver/data/experiments/smoke"
print("using", d)
dec = list(csv.DictReader(open(f"{d}/decisions.csv")))
out = list(csv.DictReader(open(f"{d}/outcomes.csv")))

A = collections.defaultdict(collections.Counter)
for r in dec:
    k = (r["policy"], r["block"])
    A[k]["cand"] += 1
    if r["sent_gws"]:
        A[k]["sent"] += 1
        if r["sent_gws"] == "1":
            A[k]["gw1"] += 1

O = collections.defaultdict(collections.Counter)
G = collections.defaultdict(collections.Counter)
S = collections.defaultdict(collections.Counter)
for r in out:
    o = r["outcome"]
    if o not in ("success", "failed", "unavailable"):
        continue
    k = (r["policy"], r["block"])
    O[k][o] += 1
    if o != "unavailable":
        if r["gws"] in ("1", "2"):
            G[(r["policy"], r["gws"])][o] += 1
        S[(r["policy"], r["interferer_at_send"])][o] += 1

def rate(c):
    n = c["success"] + c["failed"]
    return c["success"] / n if n else float("nan")

pols = sorted({k[0] for k in A})
blocks = sorted({k[1] for k in A}, key=int)
print(f"{'policy':12s} {'blocks':>6s} {'cand':>6s} {'sent':>6s} {'GW1%':>6s} {'confirmed':>9s} {'mean':>7s} {'SD':>6s}")
per = {}
for p in pols:
    rs, cand, sent, gw1, conf = [], 0, 0, 0, 0
    for b in blocks:
        k = (p, b)
        if k not in A:
            continue
        cand += A[k]["cand"]; sent += A[k]["sent"]; gw1 += A[k]["gw1"]
        conf += O[k]["success"] + O[k]["failed"]
        r = rate(O[k])
        if r == r:
            rs.append((b, r))
    per[p] = dict(rs)
    vals = [v for _, v in rs]
    m = st.mean(vals) if vals else float("nan")
    sd = st.stdev(vals) if len(vals) > 1 else float("nan")
    print(f"{p:12s} {len(rs):6d} {cand:6d} {sent:6d} {100*gw1/max(sent,1):6.1f} {conf:9d} {m:7.3f} {sd:6.3f}")

print("\nPer-gateway confirmed delivery (single-gateway ACKs):")
for (p, g), c in sorted(G.items()):
    print(f"  {p:12s} GW{g}: {rate(c):.3f} (n={c['success']+c['failed']})")

print("\nConfirmed delivery by interferer state at send:")
for (p, s), c in sorted(S.items()):
    print(f"  {p:12s} interferer={s:5s}: {rate(c):.3f} (n={c['success']+c['failed']})")

print("\nPaired block differences (a - b):")
for a, b in [("snr-greedy", "round-robin"), ("state-aware", "round-robin"), ("state-aware", "snr-greedy")]:
    if a in per and b in per:
        common = sorted(set(per[a]) & set(per[b]), key=int)
        diffs = [per[a][x] - per[b][x] for x in common]
        if len(diffs) > 1:
            print(f"  {a} - {b}: mean {100*st.mean(diffs):+.2f} pp, SD {100*st.stdev(diffs):.2f}, n={len(diffs)} blocks")
