"""
experiments/ack_ledger.py -- H1/H2/H3/H4 logging.

Feedback byte (upper 5 bits of the fragment's total_fragments byte;
protocol.py splits it off):
    bits 0-1  status  1 = node RECEIVED an ACK for its previous telegram
                      2 = node heard NO ACK for its previous telegram
                      0 = no report (legacy firmware / first telegram)
    bits 2-4  seq     (previous telegram_id) & 7  -- binds the report

Files in data/experiments/<run_id>/:
  decisions.csv  every ACK candidate: sent gateways or rejection reason
  outcomes.csv   one row per sent ACK: success / failed / unavailable,
                 plus rows for NOT-sent candidates the node reported on
  telemetry.csv  node-side RX-window time + ACK RSSI/SNR (from payload)
"""
import csv
import os
import struct
import threading
import time

ST_RECEIVED, ST_MISSED = 1, 2


class AckLedger:
    def __init__(self, exp_dir, policy, block, scheduler, interferer_state_fn,
                 expiry_s=90.0):
        os.makedirs(exp_dir, exist_ok=True)
        self.policy, self.block = policy, block
        self.scheduler = scheduler
        self.istate = interferer_state_fn
        self.expiry_s = expiry_s
        self._lock = threading.Lock()
        self._pending = {}
        self._not_sent = {}
        self._reported = {}
        self._f = {}
        for name, hdr in {
            "decisions": ["t", "node", "tel", "ack_flag", "policy", "block", "sent_gws",
                          "reject_reason", "b1", "b2", "b3", "score1", "score2", "score3",
                          "used_gw1", "used_gw2", "used_gw3", "interferer"],
            "outcomes": ["t_send", "t_report", "node", "tel", "gws", "policy", "block",
                         "b1", "b2", "b3", "interferer_at_send", "outcome"],
            "telemetry": ["t", "node", "tel", "rx_on_ms_prev", "ack_rssi_prev",
                          "ack_snr_prev", "status_prev", "fixed_period"],
        }.items():
            path = os.path.join(exp_dir, f"{name}.csv")
            new = not os.path.exists(path)
            fh = open(path, "a", newline="")
            w = csv.writer(fh)
            if new:
                w.writerow(hdr)
            self._f[name] = (fh, w)

    def _write(self, name, row):
        fh, w = self._f[name]
        w.writerow(row)
        fh.flush()

    def record_decision(self, node, tel, ack_flag, sent_gws, b1, b2, reason, b3=None):
        now = time.time()
        sc = self.scheduler.last_scores
        used = self.scheduler.used_capacity()
        ist = self.istate()
        self._write("decisions", [now, node, tel, int(ack_flag), self.policy, self.block,
                                  "+".join(map(str, sent_gws)), reason or "",
                                  round(b1, 4), round(b2, 4),
                                  round(b3, 4) if b3 is not None else "",
                                  round(sc.get(1, float("nan")), 4),
                                  round(sc.get(2, float("nan")), 4),
                                  round(sc.get(3, float("nan")), 4) if 3 in sc else "",
                                  used.get(1, 0), used.get(2, 0), used.get(3, ""), ist])
        row = dict(t=now, node=node, tel=tel, gws=list(sent_gws), b1=b1, b2=b2, b3=b3, ist=ist)
        with self._lock:
            (self._pending if sent_gws else self._not_sent)[(node, tel)] = row

    def on_feedback(self, node, cur_tel, fb):
        status, seq = fb & 0x3, (fb >> 2) & 0x7
        if status not in (ST_RECEIVED, ST_MISSED):
            return
        prev = cur_tel - 1
        if prev < 0 or (prev & 7) != seq:
            return
        key = (node, prev)
        now = time.time()
        with self._lock:
            if key in self._reported:
                return
            self._reported[key] = now
            row = self._pending.pop(key, None)
            ns = self._not_sent.pop(key, None)
        if row is not None:
            delivered = status == ST_RECEIVED
            outcome = "success" if delivered else "failed"
            for g in row["gws"]:
                self.scheduler.belief.record_receipt_report(node, g, delivered)
            self._write_outcome(row, now, outcome)
        elif ns is not None:
            self._write_outcome(ns, now, "not_sent_" + ("heard" if status == ST_RECEIVED
                                                        else "missed"))

    def _write_outcome(self, r, t_report, outcome):
        self._write("outcomes", [r["t"], t_report, r["node"], r["tel"],
                                 "+".join(map(str, r["gws"])), self.policy, self.block,
                                 round(r["b1"], 4), round(r["b2"], 4),
                                 round(r["b3"], 4) if r.get("b3") is not None else "",
                                 r["ist"], outcome])

    def on_telemetry(self, node, tel, message: bytes):
        """Node payload: [0]='T' [1:3] rx_on_ms u16 BE [3] rssi i8 [4] snr*4 i8
        [5] status [6] fixed_period flag -- all about the PREVIOUS telegram."""
        if len(message) < 7 or message[0] != 0x54:
            return
        rx_ms = struct.unpack(">H", message[1:3])[0]
        rssi = struct.unpack("b", message[3:4])[0]
        snr = struct.unpack("b", message[4:5])[0] / 4.0
        self._write("telemetry", [time.time(), node, tel, rx_ms, rssi, snr,
                                  message[5], message[6]])

    def expiry_loop(self, stop_event):
        """Sent ACKs with no report after expiry_s -> 'unavailable'."""
        while not stop_event.is_set():
            time.sleep(5.0)
            now = time.time()
            with self._lock:
                old = [k for k, r in self._pending.items() if now - r["t"] > self.expiry_s]
                rows = [self._pending.pop(k) for k in old]
                for k in [k for k, r in self._not_sent.items() if now - r["t"] > self.expiry_s]:
                    self._not_sent.pop(k)
                for k in [k for k, t in self._reported.items() if now - t > 600]:
                    self._reported.pop(k)
            for r in rows:
                self._write_outcome(r, "", "unavailable")
