#!/bin/bash

declare -a threads=(#"PARLAY_NUM_THREADS=1"
                    "PARLAY_NUM_THREADS=2"
                    "PARLAY_NUM_THREADS=4"
                    "PARLAY_NUM_THREADS=8"
                    "PARLAY_NUM_THREADS=16"
                    "PARLAY_NUM_THREADS=24"
                    "PARLAY_NUM_THREADS=48"
                    "PARLAY_NUM_THREADS=96"
                    "PARLAY_NUM_THREADS=192"
)

declare -a commands=(#"taskset -c 0-3:4"                  # 1 thread
                     "taskset -c 0-7:4 numactl -i all"    # 2 threads
                     "taskset -c 0-15:4 numactl -i all"   # 4 threads
                     "taskset -c 0-31:4 numactl -i all"   # 8 threads
                     "taskset -c 0-63:4 numactl -i all"   # 16 threads
                     "taskset -c 0-95:4 numactl -i all"   # 24 threads
                     "taskset -c 0-95:2 numactl -i all"   # 48 threads
                     "taskset -c 0-95 numactl -i all"     # 96 threads
                     "numactl -i all"                     # 192 threads
)

declare -a num_threads=(2 4 8 16 24 48 96 192)

cd include/parlay/
truncate -s 0 DTSort.tsv

make SERIAL=1 -B
export PARLAY_NUM_THREADS=1
printf "1\t" >> DTSort.tsv
taskset -c 0 ./integer_sort parallel

make -B
for i in ${!commands[@]}; do
  printf "${num_threads[$i]}\t" >> DTSort.tsv
  export ${threads[$i]}
  ${commands[$i]} ./integer_sort parallel
done

cd ../..
mv include/parlay/DTSort.tsv results/parallel_scalability.tsv
