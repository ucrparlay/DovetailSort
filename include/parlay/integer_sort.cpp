#include "integer_sort.h"

#include <fstream>
#include <utility>

#include "generator.h"
#include "internal/get_time.h"
#include "internal/integer_sort.h"
#include "random.h"
using namespace std;
using namespace parlay;

size_t n = 1e9;
size_t kNumTests = 1;
constexpr int NUM_ROUNDS = 5;

std::string test_name(int id) {
  switch (id) {
    case 0: return "integer_sort";
    case 1: return "std::sort";
    default: assert(0);
  }
  return "";
}

template<typename T, typename GetKey>
void check_correctness(const sequence<T> &in, const GetKey &g) {
  if (in.size() > 1e10) {
    printf("Skipped\n");
    return;
  }
  auto out = in;
  auto out2 = in;
  integer_sort_inplace2(make_slice(out), g);
  internal::integer_sort_inplace(make_slice(out2), g);
  parallel_for(0, out.size(), [&](size_t i) { assert(out[i] == out2[i]); });

  out = integer_sort(make_slice(in), g);
  out2 = internal::integer_sort(make_slice(in), g);
  parallel_for(0, out.size(), [&](size_t i) { assert(out[i] == out2[i]); });

  printf("Passed\n");
}

template<typename T, typename GetKey>
double test(const sequence<T> &in, const GetKey &g, int id) {
  std::cout << "test_name: " << test_name(id) << std::endl;
  double total_time = 0;
  for (int i = 0; i <= NUM_ROUNDS; i++) {
    auto seq = in;
    internal::timer t;
    switch (id) {
      case 0: integer_sort_inplace2(make_slice(seq), g); break;
      case 1: std::sort(seq.begin(), seq.end(), [&](const T &a, const T &b) { return g(a) < g(b); }); break;
      default: assert(0);
    }
    t.stop();
    if (i == 0) {
      printf("Warmup: %f\n", t.total_time());
    } else {
      printf("Round %d: %f\n", i, t.total_time());
      total_time += t.total_time();
    }
  }
  double avg = total_time / NUM_ROUNDS;
  printf("Average: %f\n", avg);
  return avg;
}

template<typename T, typename GetKey>
void run_all(const sequence<T> &seq, const GetKey &g, int id = -1) {
  vector<double> times;
  if (id == -1) {
    for (size_t i = 0; i < kNumTests; i++) {
      times.push_back(test(seq, g, i));
      check_correctness(seq, g);
      printf("\n");
    }
  } else {
    times.push_back(test(seq, g, id));
    check_correctness(seq, g);
    printf("\n");
  }
  ofstream ofs("DTSort.tsv", ios::app);
  for (auto t : times) {
    ofs << t << '\t';
  }
  ofs << '\n';
  ofs.close();
}

template<class T>
void run_all_dist(int id = -1) {
  // uniform distribution
  vector<size_t> num_keys{1000000000, 10000000, 100000, 1000, 10};
  for (auto v : num_keys) {
    auto seq = uniform_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }

  // exponential distribution
  vector<double> lambda{0.00001, 0.00002, 0.00005, 0.00007, 0.0001};
  for (auto v : lambda) {
    auto seq = exponential_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }

  // zipfian distribution
  vector<double> s{0.6, 0.8, 1, 1.2, 1.5};
  for (auto v : s) {
    auto seq = zipfian_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }

  // bits exp distribution
  vector<size_t> rate{10, 30, 50, 100, 300};
  for (auto v : rate) {
    auto seq = bits_exp_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }
}

void run_all_graphs(int id = -1) {
  string path = "/data0/graphs/links/";
  vector<string> graphs = {"soc-LiveJournal1.bin", "twitter.bin", "Cosmo50_5.bin", "sd_arc.bin", "clueweb.bin"};
  for (auto g : graphs) {
    printf("%s\n", g.c_str());
    auto seq = read_graph(path + g);
    run_all(
        seq, [](const pair<uint32_t, uint32_t> &a) { return a.first; }, id);
  }
}

void run_all_points(int id = -1) {
  string path = "/data0/geometry/";
  vector<string> points = {"GeoLifeNoScale.geobin",   "Cosmo50.geobin",          "OpenStreetMap.geobin",
                           "SS-Varden_1e9_2d.geobin", "SS-Varden_1e9_3d.geobin", "SS-Varden_2e9_2d.geobin",
                           "SS-Varden_2e9_3d.geobin"};
  for (auto p : points) {
    printf("%s\n", p.c_str());
    auto seq = read_points(path + p);
    run_all(
        seq, [](const pair<uint32_t, uint32_t> &a) { return a.first; }, id);
  }
}

template<class T>
void run_all_sizes(int id = -1) {
  vector<size_t> sizes{1000000,   2000000,   5000000,   10000000,   20000000,  50000000,
                       100000000, 200000000, 500000000, 1000000000, 2000000000};
  for (auto input_size : sizes) {
    n = input_size;
    // uniform distribution
    vector<size_t> num_keys{10000000, 1000};
    for (auto v : num_keys) {
      auto seq = uniform_pairs_generator<T>(v);
      run_all(
          seq, [](const pair<T, T> &a) { return a.first; }, id);
    }

    // exponential distribution
    vector<double> lambda{0.00002, 0.00007};
    for (auto v : lambda) {
      auto seq = exponential_pairs_generator<T>(v);
      run_all(
          seq, [](const pair<T, T> &a) { return a.first; }, id);
    }

    // zipfian distribution
    vector<double> s{0.8, 1.2};
    for (auto v : s) {
      auto seq = zipfian_pairs_generator<T>(v);
      run_all(
          seq, [](const pair<T, T> &a) { return a.first; }, id);
    }

    // bits exp distribution
    vector<size_t> rate{30, 100};
    for (auto v : rate) {
      auto seq = bits_exp_pairs_generator<T>(v);
      run_all(
          seq, [](const pair<T, T> &a) { return a.first; }, id);
    }
  }
}

template<class T>
void run_rep_dist(int id = -1) {
  // uniform distribution
  vector<size_t> num_keys{10000000, 1000};
  for (auto v : num_keys) {
    auto seq = uniform_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }

  // exponential distribution
  vector<double> lambda{0.00002, 0.00007};
  for (auto v : lambda) {
    auto seq = exponential_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }

  // zipfian distribution
  vector<double> s{0.8, 1.2};
  for (auto v : s) {
    auto seq = zipfian_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }

  // bits exp distribution
  vector<size_t> rate{30, 100};
  for (auto v : rate) {
    auto seq = bits_exp_pairs_generator<T>(v);
    run_all(
        seq, [](const pair<T, T> &a) { return a.first; }, id);
  }
}

int main(int argc, char *argv[]) {
  if (argc >= 2) {
    n = atoll(argv[1]);
  }
  std::cout << "argc = " << argc << std::endl;
  printf("n: %zu\n", n);

  int id = 0;
  run_all_dist<uint32_t>(id);
  run_all_dist<uint64_t>(id);
  run_all_graphs(id);
  run_all_points(id);

  return 0;
}
