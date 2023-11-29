#include "parallel.h"
#include "primitives.h"
#include "sequence.h"
#include "utilities.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <utility>
using namespace std;

extern size_t n;
constexpr int base = 1e9 + 7;

uint32_t _hash(const string &s) {
  uint32_t ret = 0;
  for (auto c : s) {
    ret = ret * base + (islower(c) ? c - 'a' : 26);
  }
  return ret;
}

__uint128_t _hash(__uint128_t v) {
  __uint128_t w = parlay::hash64(2 * v);
  w = w << 64 | parlay::hash64(2 * v + 1);
  return w;
}

uint64_t _hash(uint64_t v) {
  return parlay::hash64(v);
}

uint32_t _hash(uint32_t v) {
  return parlay::hash32(v);
}

template<class T>
parlay::sequence<T> uniform_generator(size_t num_keys) {
  printf("uniform distribution with num_keys: %zu\n", num_keys);
  parlay::sequence<T> seq(n);
  parlay::parallel_for(0, n, [&](size_t i) {
    size_t v = i % num_keys;
    // seq[i] = _hash(static_cast<T>(v));
    seq[i] = v;
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<T> exponential_generator(double lambda) {
  printf("exponential distribution with lambda: %.10f\n", lambda);
  size_t cutoff = n;
  parlay::sequence<size_t> nums(cutoff + 1, 0);

  parlay::parallel_for(0, cutoff, [&](size_t i) { nums[i] = max(1.0, n * (lambda * exp(-lambda * (i + 0.5)))); });
  size_t tot = scan_inplace(make_slice(nums));
  assert(tot >= n);

  parlay::sequence<T> seq(n);
  parlay::parallel_for(0, cutoff, [&](size_t i) {
    parlay::parallel_for(nums[i], min(n, nums[i + 1]), [&](size_t j) {
      // seq[j] = _hash(static_cast<T>(i));
      seq[j] = i;
    });
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<T> zipfian_generator(double s) {
  printf("zipfian distribution with s: %f\n", s);
  size_t cutoff = n;
  auto harmonic = parlay::delayed_seq<double>(cutoff, [&](size_t i) { return 1.0 / pow(i + 1, s); });
  double sum = parlay::reduce(make_slice(harmonic));
  double v = n / sum;
  parlay::sequence<size_t> nums(cutoff + 1, 0);
  parlay::parallel_for(0, cutoff, [&](size_t i) { nums[i] = max(1.0, v / pow(i + 1, s)); });
  size_t tot = scan_inplace(make_slice(nums));
  assert(tot >= n);
  parlay::sequence<T> seq(n);
  parlay::parallel_for(0, cutoff, [&](size_t i) {
    parlay::parallel_for(nums[i], min(n, nums[i + 1]), [&](size_t j) {
      // seq[j] = _hash(static_cast<T>(i));
      seq[j] = i;
    });
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<T> bits_exp_generator(size_t rate) {
  printf("bits exp distribution with rate: %zu\n", rate);
  parlay::sequence<T> seq(n);
  size_t num_bits = 8 * sizeof(T);
  for (size_t b = 0; b < num_bits; b++) {
    parlay::parallel_for(0, n, [&](size_t i) {
      if (_hash((uint64_t)(i * num_bits + b)) % rate != 0) {
        seq[i] |= static_cast<T>(1) << b;
      }
    });
  }
  return random_shuffle(seq);
}

constexpr size_t STRING_LENGTH = 10;
std::string random_string(size_t seed) {
  auto randchar = [&]() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[seed % max_index];
  };
  std::string str(STRING_LENGTH, 0);
  std::generate_n(str.begin(), STRING_LENGTH, randchar);
  return str;
}

template<class T>
parlay::sequence<pair<T, string>> uniform_strings_generator(size_t num_keys) {
  printf("uniform distribution with num_keys: %zu\n", num_keys);
  parlay::sequence<pair<T, string>> seq(n);
  parlay::parallel_for(0, n, [&](size_t i) {
    size_t v = i % num_keys;
    // seq[i] = {_hash(static_cast<T>(v)), random_string(_hash(static_cast<T>(i + n)))};
    seq[i] = {static_cast<T>(v), random_string(_hash(static_cast<T>(i + n)))};
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, string>> exponential_strings_generator(double lambda) {
  printf("exponential distribution with lambda: %.10f\n", lambda);
  size_t cutoff = n;
  parlay::sequence<size_t> nums(cutoff + 1, 0);

  parlay::parallel_for(0, cutoff, [&](size_t i) { nums[i] = max(1.0, n * (lambda * exp(-lambda * (i + 0.5)))); });
  size_t tot = scan_inplace(make_slice(nums));
  assert(tot >= n);

  parlay::sequence<pair<T, string>> seq(n);
  parlay::parallel_for(0, cutoff, [&](size_t i) {
    parlay::parallel_for(nums[i], min(n, nums[i + 1]), [&](size_t j) {
      // seq[j] = {_hash(static_cast<T>(i)), random_string(_hash(static_cast<T>(j + n)))};
      seq[j] = {static_cast<T>(i), random_string(_hash(static_cast<T>(j + n)))};
    });
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, string>> zipfian_strings_generator(double s) {
  printf("zipfian distribution with s: %f\n", s);
  size_t cutoff = n;
  auto harmonic = parlay::delayed_seq<double>(cutoff, [&](size_t i) { return 1.0 / pow(i + 1, s); });
  double sum = parlay::reduce(make_slice(harmonic));
  double v = n / sum;
  parlay::sequence<size_t> nums(cutoff + 1, 0);
  parlay::parallel_for(0, cutoff, [&](size_t i) { nums[i] = max(1.0, v / pow(i + 1, s)); });
  size_t tot = scan_inplace(make_slice(nums));
  assert(tot >= n);
  parlay::sequence<pair<T, string>> seq(n);
  parlay::parallel_for(0, cutoff, [&](size_t i) {
    parlay::parallel_for(nums[i], min(n, nums[i + 1]), [&](size_t j) {
      // seq[j] = {_hash(static_cast<T>(i)), random_string(_hash(static_cast<T>(j + n)))};
      seq[j] = {static_cast<T>(i), random_string(_hash(static_cast<T>(j + n)))};
    });
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, string>> bits_exp_strings_generator(size_t rate) {
  printf("bits exp distribution with rate: %zu\n", rate);
  parlay::sequence<pair<T, string>> seq(n);
  parlay::parallel_for(0, n, [&](size_t i) { seq[i].second = random_string(_hash(static_cast<T>(i))); });
  size_t num_bits = 8 * sizeof(T);
  for (size_t b = 0; b < num_bits; b++) {
    parlay::parallel_for(0, n, [&](size_t i) {
      if (_hash((uint64_t)(i * num_bits + b)) % rate != 0) {
        seq[i].first |= static_cast<T>(1) << b;
      }
    });
  }
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, T>> uniform_pairs_generator(size_t num_keys) {
  printf("uniform distribution with num_keys: %zu\n", num_keys);
  parlay::sequence<pair<T, T>> seq(n);
  parlay::parallel_for(0, n, [&](size_t i) {
    size_t v = i % num_keys;
    seq[i] = {_hash(static_cast<T>(v)), _hash(static_cast<T>(i + n))};
    // seq[i] = {static_cast<T>(v), _hash(static_cast<T>(i + n))};
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, T>> exponential_pairs_generator(double lambda) {
  printf("exponential distribution with lambda: %.10f\n", lambda);
  size_t cutoff = n;
  parlay::sequence<size_t> nums(cutoff + 1, 0);

  parlay::parallel_for(0, cutoff, [&](size_t i) { nums[i] = max(1.0, n * (lambda * exp(-lambda * (i + 0.5)))); });
  size_t tot = scan_inplace(make_slice(nums));
  assert(tot >= n);

  parlay::sequence<pair<T, T>> seq(n);
  parlay::parallel_for(0, cutoff, [&](size_t i) {
    parlay::parallel_for(nums[i], min(n, nums[i + 1]), [&](size_t j) {
      seq[j] = {_hash(static_cast<T>(i)), _hash(static_cast<T>(j + n))};
      // seq[j] = {static_cast<T>(i), _hash(static_cast<T>(j + n))};
    });
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, T>> zipfian_pairs_generator(double s) {
  printf("zipfian distribution with s: %f\n", s);
  size_t cutoff = n;
  auto harmonic = parlay::delayed_seq<double>(cutoff, [&](size_t i) { return 1.0 / pow(i + 1, s); });
  double sum = parlay::reduce(make_slice(harmonic));
  double v = n / sum;
  parlay::sequence<size_t> nums(cutoff + 1, 0);
  parlay::parallel_for(0, cutoff, [&](size_t i) { nums[i] = max(1.0, v / pow(i + 1, s)); });
  size_t tot = scan_inplace(make_slice(nums));
  assert(tot >= n);
  parlay::sequence<pair<T, T>> seq(n);
  parlay::parallel_for(0, cutoff, [&](size_t i) {
    parlay::parallel_for(nums[i], min(n, nums[i + 1]), [&](size_t j) {
      seq[j] = {_hash(static_cast<T>(i)), _hash(static_cast<T>(j + n))};
      // seq[j] = {static_cast<T>(i), _hash(static_cast<T>(j + n))};
    });
  });
  return random_shuffle(seq);
}

template<class T>
parlay::sequence<pair<T, T>> bits_exp_pairs_generator(size_t rate) {
  printf("bits exp distribution with rate: %zu\n", rate);
  parlay::sequence<pair<T, T>> seq(n);
  parlay::parallel_for(0, n, [&](size_t i) { seq[i].second = _hash(static_cast<T>(i)); });
  size_t num_bits = 8 * sizeof(T);
  for (size_t b = 0; b < num_bits; b++) {
    parlay::parallel_for(0, n, [&](size_t i) {
      if (_hash((uint64_t)(i * num_bits + b)) % rate != 0) {
        seq[i].first |= static_cast<T>(1) << b;
      }
    });
  }
  return random_shuffle(seq);
}

template<class T>
void get_distribution(parlay::sequence<pair<T, T>> seq) {
  parlay::sort_inplace(make_slice(seq));
  auto flags =
      parlay::delayed_seq<bool>(seq.size(), [&](size_t i) { return i == 0 || seq[i].first != seq[i - 1].first; });
  auto index = parlay::pack_index<size_t>(flags);
  size_t num_keys = index.size();
  parlay::sequence<size_t> nums(num_keys);
  parlay::parallel_for(0, num_keys, [&](size_t i) {
    if (i == num_keys - 1) {
      nums[i] = seq.size() - index[i];
    } else {
      nums[i] = index[i + 1] - index[i];
    }
  });
  size_t max_occur = parlay::reduce(make_slice(nums), parlay::maxm<size_t>());
  printf("# num of distinct keys: %zu\n", num_keys);
  printf("# num of max occurrences: %zu\n", max_occur);
}

auto read_graph(const string filename) {
  ifstream ifs(filename);
  if (!ifs.is_open()) {
    cerr << "Error: Cannot open file " << filename << '\n';
    abort();
  }
  size_t n, m, sizes;
  ifs.read(reinterpret_cast<char *>(&n), sizeof(size_t));
  ifs.read(reinterpret_cast<char *>(&m), sizeof(size_t));
  ifs.read(reinterpret_cast<char *>(&sizes), sizeof(size_t));
  assert(sizes == (n + 1) * 8 + m * 4 + 3 * 8);

  auto offset = parlay::sequence<uint64_t>(n + 1);
  ifs.read(reinterpret_cast<char *>(offset.begin()), sizeof(uint64_t) * (n + 1));
  auto csr = parlay::sequence<uint32_t>(m);
  ifs.read(reinterpret_cast<char *>(csr.begin()), sizeof(uint32_t) * m);
  auto edges = parlay::sequence<pair<uint32_t, uint32_t>>::uninitialized(m);
  parlay::parallel_for(0, n, [&](uint32_t i) {
    parlay::parallel_for(offset[i], offset[i + 1], [&](size_t j) { edges[j] = make_pair(csr[j], i); });
  });
  if (ifs.peek() != EOF) {
    cerr << "Error: Bad data\n";
    abort();
  }
  ifs.close();
  return edges;
}

template<int dim>
auto read_points(ifstream &ifs, size_t n, size_t k) {
  assert(k * sizeof(double) == sizeof(array<double, dim>));
  parlay::sequence<array<double, dim>> points(n);
  array<std::atomic<double>, dim> box_upp, box_low;
  for (int i = 0; i < dim; i++) {
    box_upp[i] = std::numeric_limits<double>::lowest();
    box_low[i] = std::numeric_limits<double>::max();
  }
  ifs.read(reinterpret_cast<char *>(points.data()), n * k * sizeof(double));
  array<double, dim> len;
  parlay::parallel_for(0, n, [&](size_t i) {
    for (size_t j = 0; j < dim; j++) {
      parlay::write_max(&box_upp[j], points[i][j], [](const double &a, const double &b) { return a < b; });
      parlay::write_min(&box_low[j], points[i][j], [](const double &a, const double &b) { return a < b; });
    }
  });
  int bits = sizeof(uint32_t) * 8 / dim;
  for (int i = 0; i < dim; i++) {
    constexpr double eps = 1e-6;
    len[i] = (box_upp[i] - box_low[i]) / (1 << bits) + eps;
  }
  parlay::sequence<pair<uint32_t, uint32_t>> seq(n);
  parlay::parallel_for(0, n, [&](size_t i) {
    seq[i].second = i;
    for (int j = 0; j < dim; j++) {
      double v = (points[i][j] - box_low[j]) / len[j];
      int id = floor(v);
      assert(id >= 0 && id < (1 << bits));
      for (int k = 0; k < bits; k++) {
        if (id >> k & 1) {
          seq[i].first |= 1 << (k * dim + j);
        }
      }
    }
  });
  return seq;
}

auto read_points(const string filename) {
  ifstream ifs(filename);
  string header;
  size_t n, k;
  ifs.read(reinterpret_cast<char *>(&n), sizeof(size_t));
  ifs.read(reinterpret_cast<char *>(&k), sizeof(size_t));
  if (k == 2) {
    return read_points<2>(ifs, n, k);
  } else if (k == 3) {
    return read_points<3>(ifs, n, k);
  } else {
    printf("Try with lower dimensions\n");
    abort();
  }
  ifs.close();
}

auto read_ngram(const string filename) {
  ifstream ifs(filename);
  if (!ifs.is_open()) {
    cerr << "Error: Cannot open file " << filename << '\n';
    abort();
  }
  size_t n;
  ifs >> n;
  parlay::sequence<pair<string, string>> pre_sub(n);
  for (size_t i = 0; i < n; i++) {
    size_t v;
    ifs >> v >> pre_sub[i].first >> pre_sub[i].second;
  }
  ifs.close();
  return random_shuffle(pre_sub);
}

template<class T>
void write_to_file(const parlay::sequence<pair<T, T>> &seq) {
  ofstream ofs("sequence.bin");
  ofs.write(reinterpret_cast<const char *>(seq.begin()), sizeof(pair<T, T>) * n);
  ofs.close();
}

template<class T>
parlay::sequence<pair<T, T>> read_from_file() {
  ifstream ifs("sequence.bin");
  parlay::sequence<pair<T, T>> seq(n);
  ifs.read(reinterpret_cast<char *>(seq.begin()), sizeof(pair<T, T>) * n);
  ifs.close();
  return seq;
}

template<class T>
void get_counts(parlay::sequence<pair<T, T>> seq) {
  parlay::sort_inplace(make_slice(seq));
  auto flags =
      parlay::delayed_seq<bool>(seq.size() + 1, [&](size_t i) { return i == 0 || seq[i].first != seq[i - 1].first; });
  auto index = parlay::pack_index<size_t>(flags);
  auto counts = parlay::tabulate(index.size() - 1, [&](size_t i) { return index[i + 1] - index[i]; });
  parlay::sort_inplace(make_slice(counts));
  size_t cbrtlogn = 0;
  size_t thousand = 0;
  size_t five_thousand = 0;
  size_t ten_thousand = 0;
  size_t n = seq.size();
  for (size_t i = 0; i < counts.size(); i++) {
    if (counts[i] >= std::cbrt(n) * log(n)) {
      cbrtlogn += counts[i];
    }
    if (counts[i] >= 1000) {
      thousand += counts[i];
    }
    if (counts[i] >= 5000) {
      five_thousand += counts[i];
    }
    if (counts[i] >= 10000) {
      ten_thousand += counts[i];
    }
  }
  std::ofstream ofs("counts.tsv", std::ios::app);
  ofs << counts.size() << '\t' << cbrtlogn << '\t' << thousand << '\t' << five_thousand << '\t' << ten_thousand << '\n';
  ofs.close();
}
