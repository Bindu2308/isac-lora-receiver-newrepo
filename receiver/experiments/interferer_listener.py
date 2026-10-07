"""
experiments/interferer_listener.py -- H4 ground truth on the SERVER clock.
Interferer sends UDP "IF,<seq>,<ON|OFF>,<millis>" on every transition and
once per second; each datagram is stamped with the Pi's time.time().
"""
import csv
import os
import socket
import threading
import time


class InterfererListener:
    def __init__(self, exp_dir, port=5005, stale_s=3.0):
        os.makedirs(exp_dir, exist_ok=True)
        self.path = os.path.join(exp_dir, "interferer.csv")
        self.port, self.stale_s = port, stale_s
        self._state, self._last = "UNKNOWN", 0.0

    def state(self):
        return self._state if time.time() - self._last <= self.stale_s else "UNKNOWN"

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("0.0.0.0", self.port))
        new = not os.path.exists(self.path)
        with open(self.path, "a", newline="") as fh:
            w = csv.writer(fh)
            if new:
                w.writerow(["t_pi", "seq", "state", "dev_millis", "transition"])
            while True:
                data, _ = s.recvfrom(128)
                now = time.time()
                try:
                    tag, seq, st, ms = data.decode().strip().split(",")
                except ValueError:
                    continue
                if tag != "IF":
                    continue
                transition = int(st != self._state)
                self._state, self._last = st, now
                w.writerow([now, seq, st, ms, transition])
                fh.flush()
