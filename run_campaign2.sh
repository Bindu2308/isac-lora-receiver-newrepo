#!/bin/bash
# usage: ./run_campaign2.sh RUNID "pol1 pol2 ..." BLOCKS MINUTES START_BLOCK [extra main.py args]
RUNID=$1; POLS=$2; BLOCKS=$3; MIN=$4; START=$5; shift 5; EXTRA="$@"
cd "$(dirname "$0")/receiver" || exit 1
if pgrep -f "main.py" > /dev/null; then echo "main.py already running, aborting"; exit 1; fi
mkdir -p data/experiments
for b in $(seq "$START" $((START+BLOCKS-1))); do
  for p in $(shuf -e $POLS); do
    echo "=== $(date +%T) $RUNID block $b policy $p ($MIN min) ==="
    timeout -s INT $((MIN*60)) python3 main.py --policy "$p" \
        --run-id "$RUNID" --block "$b" --scheduler-seed "$b" $EXTRA \
        > "data/experiments/${RUNID}_b${b}_${p}.log" 2>&1
    sleep 10
  done
done
echo DONE
