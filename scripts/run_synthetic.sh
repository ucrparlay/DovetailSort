#!/bin/bash

cd include/parlay/
truncate -s 0 DTSort.tsv

make -B
numactl -i all ./integer_sort

cd ../..
mv include/parlay/DTSort.tsv results/synthetic.tsv
