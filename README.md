
# DovetailSort: A Parallel Integer Sort Algorithm
This repository contains code for our paper "Parallel Integer Sort: Theory and Practice".
Our testing code is in ``include/parlay/integer_sort.cpp`` and our algorithm is implemented in ``include/parlay/integer_sort.h``. The rest of the code are from the publicly available library [ParlayLib](https://github.com/cmuparlay/parlaylib).
 

Requirements
--------
+ g++ or clang with C++17 features support (tested with g++ 12.1.1 and clang 14.0.6) on Linux machines.

Getting Code
--------
The code can be downloaded using git:
```bash
git clone https://github.com/ucrparlay/DovetailSort.git
```

Compilation
--------
A makefile is provided in ``include/parlay/``.
```bash
cd include/parlay/
make
```

Running Code
--------
```bash
numactl -i all ./integer_sort <n>
```
where n is the input size (by default $10^9$). It runs on all synthetic datasets (uniform, exponential, Zipfian, and bit-exponential distributions) with both 32-bit and 64-bit by default.

Contact
--------
If you have any questions, please submit an issue to this repository (recommended) or send an email to the author at xdong038@ucr.edu.  

Reference
--------
Xiaojun Dong, Laxman Dhulipala, Yan Gu, Yihan Sun. Parallel Integer Sort: Theory and Practice. To appear at *ACM SIGPLAN Symposium on Principles and Practice of Parallel Programming (PPoPP)*, 2024.  

If you use our code, please cite our paper:
```
@inproceedings{dong2024parallel,
  author    = {Dong, Xiaojun and Dhulipala, Laxman and Gu, Yan and Sun, Yihan},
  title     = {Parallel Integer Sort: Theory and Practice},
  booktitle = {ACM SIGPLAN Symposium on Principles and Practice of Parallel Programming (PPoPP)},
  year      = {2024},
}
```