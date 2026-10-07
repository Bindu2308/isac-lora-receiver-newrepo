#!/bin/bash
# usage: ./run_snr_campaign.sh [BLOCKS] [MINUTES_PER_RUN]
BLOCKS=${1:-4}
MIN=${2:-5}
START=${3:-1}
cd "$(dirname "$0")/receiver" || exit 1
mkdir -p data/experiments
for b in $(seq "$START" $((START+BLOCKS-1))); do
  for p in $(shuf -e ewma belief-only state-aware); do
    echo "=== $(date +%T) block $b policy $p ($MIN min) ==="
    timeout -s INT $((MIN*60)) python3 main.py --policy "$p" \
        --run-id snr_campaign --block "$b" --scheduler-seed "$b" \
        > "data/experiments/run_b${b}_${p}.log" 2>&1
    sleep 10
  done
done
echo DONE
