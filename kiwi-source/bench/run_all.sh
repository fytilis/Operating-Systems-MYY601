#!/bin/bash

# Parameters
OPS_LIST=(100000 300000 600000 1000000)
RATIOS=("50-50" "20-80" "80-20")
THREADS_LIST=(25 20 10 5 3)
RANDOM=("1")

# Output folder
mkdir -p logs

for OPS in "${OPS_LIST[@]}"; do
  for RATIO in "${RATIOS[@]}"; do
    for THREADS in "${THREADS_LIST[@]}"; do
      echo "Running: ./kiwi-bench mix $OPS $RATIO $THREADS $RANDOM"
      rm -rf testdb

      # Log file name
      LOG="logs/mix_${OPS}_${RATIO//-}_t${THREADS}.txt"

      # Run and save output
      ./kiwi-bench mix $OPS $RATIO $THREADS $RANDOM | tee "$LOG"
      echo "Saved to $LOG"
      echo "----------------------------------------------"
    done
  done
done
