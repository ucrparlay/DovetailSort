
# DovetailSort: A Parallel Integer Sort Algorithm
This repository contains code for our paper "Parallel Integer Sort: Theory and Practice".
DovetailSort is a parallel most-significant digit (MSD) integer sort algorithm, which first partitions all keys
into buckets based on the integer encoding (i.e., 8–12 highest bits), and recurses within each bucket. 
Our algorithm can avoid sorting heavily duplicated keys in each recursive call by first detecting and splitting them out.
After the light keys are fully sorted, we *dovetail* (merge) the heavy and light keys in increasing order.


Our testing code is in ``include/parlay/integer_sort.cpp`` and our algorithm is implemented in ``include/parlay/integer_sort.h``. 
We provide a generator for the following distributions in ``include/parlay/generator.h``.
The rest of the code is from the publicly available library [ParlayLib](https://github.com/cmuparlay/parlaylib).
- Uniform distribution (https://en.wikipedia.org/wiki/Discrete_uniform_distribution)
- Exponential distribution (https://en.wikipedia.org/wiki/Exponential_distribution)
- Zipfian distribution (https://en.wikipedia.org/wiki/Zipf%27s_law)
- Bit-Exponential distribution (An adversarial distribution for integer sort introduced in our paper)
 

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

Applications
--------
We provide two representative applications on graph and geometric processing.
- Graph transpose: Given a directed graph $G=(V,E)$, the graph transpose problem is to generate $G^T=(V, E^T)$ where $E^T=$ { $(v, u): (u, v) \in E$ }
- Morton sort. For a $d$-dimensional point, a z-value is calculated by interleaving the binary representations of each coordinate.

The datasets we use to test the applications can be found in our [Google Drive](https://drive.google.com/drive/folders/1FpAhXxzJPXqc60XiHMEK3DVLMcuNcCOX?usp=sharing).

Contact
--------
If you have any questions, please submit an issue to this repository (recommended) or send an email to the author at xdong038@ucr.edu.  

Reference
--------
Xiaojun Dong, Laxman Dhulipala, Yan Gu, Yihan Sun. [Parallel Integer Sort: Theory and Practice](https://dl.acm.org/doi/10.1145/3627535.3638483). *ACM SIGPLAN Symposium on Principles and Practice of Parallel Programming (PPoPP)*, pp. 301-315, 2024.  
Xiaojun Dong, Laxman Dhulipala, Yan Gu, Yihan Sun. [Parallel Integer Sort: Theory and Practice](https://arxiv.org/abs/2401.00710). *arXiv preprint: 2401.00710*, 2024.  


If you use our code, please cite our paper:
```
@inproceedings{dong2024parallel,
  author    = {Dong, Xiaojun and Dhulipala, Laxman and Gu, Yan and Sun, Yihan},
  title     = {Parallel Integer Sort: Theory and Practice},
  booktitle = {ACM SIGPLAN Symposium on Principles and Practice of Parallel Programming (PPoPP)},
  year      = {2024},
}
```
