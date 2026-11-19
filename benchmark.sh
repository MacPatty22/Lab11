#!/bin/bash
# benchmark.sh
# Times the Lab 11 movie program generating 50 frames for every
# combination of process count and thread count, and prints a
# markdown table to paste into README.md.
#
# Course: CPE 2600, Lab 12 - Multithreading
# Author: Mac Patterson
#
# EDIT THESE three lines to match your Lab 11 movie program:
MOVIE="./mandelmovie"   # name of your movie executable
PROC_FLAG="-p"          # flag that sets the number of processes
THREAD_FLAG="-t"        # flag you added that sets threads per image

PROCS=(1 2 5 10 20)
THREADS=(1 2 5 10 20)

TIMEFORMAT=%R   # make bash's `time` print only wall-clock seconds

# Header row: threads across the top
printf "| Procs \\\\ Threads |"
for t in "${THREADS[@]}"; do printf " %s |" "$t"; done
printf "\n|---|"
for t in "${THREADS[@]}"; do printf "%s" "---|"; done
printf "\n"

# One row per process count
for p in "${PROCS[@]}"; do
    printf "| %s |" "$p"
    for t in "${THREADS[@]}"; do
        secs=$( { time "$MOVIE" "$PROC_FLAG" "$p" "$THREAD_FLAG" "$t" > /dev/null; } 2>&1 )
        printf " %s |" "$secs"
    done
    printf "\n"
done

# Clean up the frames from the last run
rm -f mandel*.jpg
