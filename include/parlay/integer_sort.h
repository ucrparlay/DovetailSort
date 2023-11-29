
#ifndef OUR_INTEGER_SORT_H_
#define OUR_INTEGER_SORT_H_

#include "internal/counting_sort.h"
#include "internal/integer_sort.h"
#include "internal/sample_sort.h"
#include "internal/uninitialized_sequence.h"
#include "primitives.h"
#include "sequence.h"
#include "slice.h"

namespace parlay {

constexpr size_t INTEGER_SORT_BASE_CASE_SIZE = 1 << 14;
constexpr size_t SAMPLE_QUOTIENT = 500;
constexpr size_t MERGE_BASE_CASE_SIZE = 1 << 17;

template <typename InIterator, typename TmpIterator, typename GetKey>
void parlay_merge(slice<InIterator, InIterator> A,
                  slice<TmpIterator, TmpIterator> Tmp, const GetKey &g,
                  size_t n1, size_t n2) {
  using in_type = typename slice<InIterator, InIterator>::value_type;
  internal::merge_into<uninitialized_relocate_tag>(
      A.cut(0, n1), A.cut(n1, n1 + n2), Tmp.cut(0, n1 + n2),
      [&](const in_type &a, const in_type &b) { return g(a) < g(b); });
  if (n1 + n2 < MERGE_BASE_CASE_SIZE) {
    for (size_t i = 0; i < n1 + n2; i++) {
      assign_dispatch(A[i], Tmp[i], uninitialized_relocate_tag());
    }
  } else {
    parallel_for(0, n1 + n2, [&](size_t i) {
      assign_dispatch(A[i], Tmp[i], uninitialized_relocate_tag());
    });
  }
}

template <typename InIterator, typename TmpIterator, typename DistIterator,
          typename OffsetIterator, typename GetKey>
void merge_out_of_place(slice<InIterator, InIterator> A,
                        slice<TmpIterator, TmpIterator> Tmp,
                        slice<DistIterator, DistIterator> dist,
                        slice<OffsetIterator, OffsetIterator> offsets,
                        const GetKey &g, size_t n1, size_t n2) {
  assert(n1 + n2 == A.size());
  using in_type = typename slice<InIterator, InIterator>::value_type;
  using key_type = typename std::invoke_result<GetKey, in_type>::type;
  size_t num_dist = dist.size();
  sequence<size_t> heavy_offsets(num_dist + 2);
  sequence<size_t> heavy_counts(num_dist + 1);
  auto get_key = delayed_seq<key_type>(n1, [&](size_t i) { return g(A[i]); });
  heavy_counts[0] = heavy_offsets[0] = 0;
  for (size_t i = 0; i < num_dist; i++) {
    key_type k = dist[i].first;
    heavy_offsets[i + 1] =
        std::lower_bound(get_key.begin(), get_key.end(), k) - get_key.begin();
    heavy_counts[i + 1] = offsets[i + 1] - offsets[0];
  }
  heavy_offsets[num_dist + 1] = n1;
  if (n1 + n2 < MERGE_BASE_CASE_SIZE) {
    // move light keys
    for (size_t i = 0; i < num_dist + 1; i++) {
      for (size_t j = heavy_offsets[i]; j < heavy_offsets[i + 1]; j++) {
        assign_dispatch(Tmp[heavy_counts[i] + j], A[j],
                        uninitialized_relocate_tag());
      }
    }
    // move heavy keys
    for (size_t i = 0; i < num_dist; i++) {
      for (size_t j = heavy_counts[i]; j < heavy_counts[i + 1]; j++) {
        assign_dispatch(Tmp[heavy_offsets[i + 1] + j], A[n1 + j],
                        uninitialized_relocate_tag());
      }
    }
    for (size_t i = 0; i < n1 + n2; i++) {
      assign_dispatch(A[i], Tmp[i], uninitialized_relocate_tag());
    }
    return;
  }
  // move light keys
  parallel_for(0, num_dist + 1, [&](size_t i) {
    parallel_for(heavy_offsets[i], heavy_offsets[i + 1], [&](size_t j) {
      assign_dispatch(Tmp[heavy_counts[i] + j], A[j],
                      uninitialized_relocate_tag());
    });
  });
  // move heavy keys
  parallel_for(0, num_dist, [&](size_t i) {
    parallel_for(heavy_counts[i], heavy_counts[i + 1], [&](size_t j) {
      assign_dispatch(Tmp[heavy_offsets[i + 1] + j], A[n1 + j],
                      uninitialized_relocate_tag());
    });
  });
  parallel_for(0, n1 + n2, [&](size_t i) {
    assign_dispatch(A[i], Tmp[i], uninitialized_relocate_tag());
  });
}

// A spealized version of merge for integer sort
// The light keys are in A[0, n1), The heavy keys are in A[n1, n1 + n2)
// The result will be saved in A
template <typename InIterator, typename TmpIterator, typename DistIterator,
          typename OffsetIterator, typename GetKey>
void merge_inplace(slice<InIterator, InIterator> A,
                   slice<TmpIterator, TmpIterator> Tmp,
                   slice<DistIterator, DistIterator> dist,
                   slice<OffsetIterator, OffsetIterator> offsets,
                   const GetKey &g, size_t n1, size_t n2) {
  assert(n1 + n2 == A.size());
  using in_type = typename slice<InIterator, InIterator>::value_type;
  using key_type = typename std::invoke_result<GetKey, in_type>::type;
  size_t num_dist = dist.size();
  sequence<size_t> heavy_offsets(num_dist + 2);
  sequence<size_t> heavy_counts(num_dist + 1);
  auto get_key = delayed_seq<key_type>(n1, [&](size_t i) { return g(A[i]); });
  heavy_counts[0] = heavy_offsets[0] = 0;
  for (size_t i = 0; i < num_dist; i++) {
    key_type k = dist[i].first;
    heavy_offsets[i + 1] =
        std::lower_bound(get_key.begin(), get_key.end(), k) - get_key.begin();
    heavy_counts[i + 1] = offsets[i + 1] - offsets[0];
  }
  heavy_offsets[num_dist + 1] = n1;
  if (n1 + n2 < MERGE_BASE_CASE_SIZE) {
    if (n1 >= n2) {
      for (size_t i = 0; i < n2; i++) {
        assign_dispatch(Tmp[i], A[n1 + i], uninitialized_relocate_tag());
      }
      // move light keys
      for (size_t i = num_dist; i >= 1; i--) {
        for (ptrdiff_t j = heavy_offsets[i + 1] - 1;
             j >= (ptrdiff_t)heavy_offsets[i]; j--) {
          assign_dispatch(A[heavy_counts[i] + j], A[j],
                          uninitialized_relocate_tag());
        }
      }
      // move heavy keys
      for (size_t i = 0; i < num_dist; i++) {
        for (size_t j = heavy_counts[i]; j < heavy_counts[i + 1]; j++) {
          assign_dispatch(A[heavy_offsets[i + 1] + j], Tmp[j],
                          uninitialized_relocate_tag());
        }
      }
    } else {
      for (size_t i = 0; i < n1; i++) {
        assign_dispatch(Tmp[i], A[i], uninitialized_relocate_tag());
      }
      // move heavy keys
      for (size_t i = 0; i < num_dist; i++) {
        for (size_t j = heavy_counts[i]; j < heavy_counts[i + 1]; j++) {
          assign_dispatch(A[heavy_offsets[i + 1] + j], A[n1 + j],
                          uninitialized_relocate_tag());
        }
      }
      // move light keys
      for (size_t i = 0; i < num_dist + 1; i++) {
        for (size_t j = heavy_offsets[i]; j < heavy_offsets[i + 1]; j++) {
          assign_dispatch(A[heavy_counts[i] + j], Tmp[j],
                          uninitialized_relocate_tag());
        }
      }
    }
    return;
  }
  if (n1 >= n2) {
    parallel_for(0, n2, [&](size_t i) {
      assign_dispatch(Tmp[i], A[n1 + i], uninitialized_relocate_tag());
    });
    // move light keys
    for (size_t i = num_dist; i >= 1; i--) {
      if (heavy_offsets[i + 1] <= heavy_counts[i] + heavy_offsets[i]) {
        parallel_for(heavy_offsets[i], heavy_offsets[i + 1], [&](size_t j) {
          assign_dispatch(A[heavy_counts[i] + j], A[j],
                          uninitialized_relocate_tag());
        });

      } else {
        reverse_inplace(A.cut(heavy_offsets[i], heavy_offsets[i + 1]));
        reverse_inplace(
            A.cut(heavy_offsets[i] + heavy_counts[i], heavy_offsets[i + 1]));
        size_t size = heavy_counts[i];
        parallel_for(0, size, [&](size_t j) {
          assign_dispatch(A[heavy_counts[i] + heavy_offsets[i + 1] - 1 - j],
                          A[heavy_offsets[i] + j],
                          uninitialized_relocate_tag());
        });
      }
    }
    // move heavy keys
    parallel_for(0, num_dist, [&](size_t i) {
      parallel_for(heavy_counts[i], heavy_counts[i + 1], [&](size_t j) {
        assign_dispatch(A[heavy_offsets[i + 1] + j], Tmp[j],
                        uninitialized_relocate_tag());
      });
    });
  } else {
    parallel_for(0, n1, [&](size_t i) {
      assign_dispatch(Tmp[i], A[i], uninitialized_relocate_tag());
      // Tmp[i] = A[i];
    });
    // move heavy keys
    for (size_t i = 0; i < num_dist; i++) {
      if (heavy_offsets[i + 1] + heavy_counts[i + 1] <= n1 + heavy_counts[i]) {
        parallel_for(heavy_counts[i], heavy_counts[i + 1], [&](size_t j) {
          assign_dispatch(A[heavy_offsets[i + 1] + j], A[n1 + j],
                          uninitialized_relocate_tag());
        });
      } else {
        reverse_inplace(A.cut(n1 + heavy_counts[i], n1 + heavy_counts[i + 1]));
        reverse_inplace(A.cut(n1 + heavy_counts[i],
                              heavy_offsets[i + 1] + heavy_counts[i + 1]));
        size_t size = n1 - heavy_offsets[i + 1];
        parallel_for(0, size, [&](size_t j) {
          assign_dispatch(A[n1 + heavy_counts[i] - 1 - j],
                          A[heavy_offsets[i + 1] + heavy_counts[i + 1] + j],
                          uninitialized_relocate_tag());
        });
      }
    }
    // move light keys
    parallel_for(0, num_dist + 1, [&](size_t i) {
      parallel_for(heavy_offsets[i], heavy_offsets[i + 1], [&](size_t j) {
        assign_dispatch(A[heavy_counts[i] + j], Tmp[j],
                        uninitialized_relocate_tag());
      });
    });
  }
}

template <typename s_size_t, typename inplace_tag, typename assignment_tag,
          typename InIterator, typename OutIterator, typename TmpIterator,
          typename GetKey>
void integer_sort2_(slice<InIterator, InIterator> In,
                    slice<OutIterator, OutIterator> Out,
                    slice<TmpIterator, TmpIterator> Tmp, const GetKey &g,
                    size_t key_bits, double parallelism = 1.0) {
  assert(In.size() == Out.size());

  size_t n = In.size();
  using in_type = typename slice<InIterator, InIterator>::value_type;
  using key_type = typename std::invoke_result<GetKey, in_type>::type;

  if (key_bits == 0) {
    if constexpr (inplace_tag::value == false) {
      uninitialized_relocate_n(Out.begin(), In.begin(), n);
      // parallel_for(0, n, [&](size_t i) { assign_dispatch(Out[i], In[i],
      // assignment_tag()); });
    }
    return;
  }

  if (n < INTEGER_SORT_BASE_CASE_SIZE || parallelism < .0001) {
    internal::seq_radix_sort<inplace_tag, assignment_tag>(In, Out, Tmp, g,
                                                          key_bits);
    return;
  }

  // 1. sampling
  size_t heavy_threshold = log(n);
  size_t num_samples = heavy_threshold * std::cbrt(n);

  sequence<key_type> samples(num_samples);
  for (size_t i = 0; i < num_samples; i++) {
    samples[i] = g(In[hash64(i) % n]);
  }
  internal::seq_sort_inplace(
      make_slice(samples),
      [&](const key_type &a, const key_type &b) { return a < b; }, false);

  key_type bits_mask = 0;
  for (size_t i = 0; i < key_bits; i++) {
    bits_mask |= static_cast<key_type>(1) << i;
  }

  key_type max_sample = (samples.back() & bits_mask);
  // to avoid overflow
  size_t top_bits;
  if (max_sample == std::numeric_limits<key_type>::max()) {
    top_bits = sizeof(key_type) * 8;
  } else {
    top_bits = log2_up(max_sample + 1);
  }

  key_type overflow = 0;
  for (size_t i = 0; i < top_bits; i++) {
    overflow |= static_cast<key_type>(1) << i;
  }

  size_t cbrt = std::cbrt(n);
  size_t log2_light_keys = log2_up(cbrt + 1);
  // use 8 to 12 bits
  log2_light_keys = std::min<size_t>(log2_light_keys, 12);
  log2_light_keys = std::max<size_t>(log2_light_keys, 8);
  using BktId = uint32_t;
  // use count sort if remaining bits are few
  if (top_bits <= log2_light_keys) {
    size_t light_buckets = size_t{1} << top_bits;
    size_t light_mask = light_buckets - 1;
    auto get_bits = delayed_seq<uint16_t>(n, [&](size_t k) -> uint16_t {
      if ((g(In[k]) & bits_mask) > overflow) {
        return light_buckets;
      } else {
        return g(In[k]) & light_mask;
      }
    });
    size_t num_buckets = light_buckets + 1;  // with an extra overflow buckets
    sequence<size_t> bucket_offsets;
    bool one_bucket;
    std::tie(bucket_offsets, one_bucket) =
        internal::count_sort_<assignment_tag, s_size_t>(
            In, Out, make_slice(get_bits), num_buckets, parallelism,
            inplace_tag::value);
    if constexpr (inplace_tag::value == true) {
      if (!one_bucket) {
        uninitialized_relocate_n(In.begin(), Out.begin(), In.size());
      }
    }
    if (bucket_offsets[light_buckets] != bucket_offsets[light_buckets + 1]) {
      auto &Arr = inplace_tag::value ? In : Out;
      auto a = Arr.cut(bucket_offsets[light_buckets],
                       bucket_offsets[light_buckets + 1]);
      internal::seq_sort_inplace(
          a,
          [&](const in_type &i1, const in_type &i2) { return g(i1) < g(i2); },
          true);
    }
    return;
  }

  size_t shift_bits = top_bits - log2_light_keys;
  size_t light_buckets = size_t{1} << log2_light_keys;
  size_t light_mask = light_buckets - 1;
  sequence<std::pair<key_type, BktId>> heavy_seq;
  sequence<BktId> light_id(light_buckets + 1);
  size_t bucket_id = 0, light_iter = 0;
  for (size_t i = 0; i < num_samples;) {
    size_t j = 0;
    while (i + j < num_samples && samples[i] == samples[i + j]) {
      j++;
    }
    if (j >= heavy_threshold) {
      size_t bits = samples[i] >> shift_bits & light_mask;
      while (light_iter <= bits) {
        light_id[light_iter++] = bucket_id++;
      }
      heavy_seq.push_back({samples[i], bucket_id++});
    }
    i += j;
  }
  while (light_iter < light_buckets) {
    light_id[light_iter++] = bucket_id++;
  }
  light_id[light_buckets] = bucket_id;

  size_t heavy_id_size = size_t{1} << log2_up(heavy_seq.size() * 2 + 1);
  size_t heavy_id_mask = heavy_id_size - 1;
  constexpr auto SMAX = std::numeric_limits<BktId>::max();
  constexpr uint32_t BIGINT = 1e9 + 7;
  sequence<std::pair<key_type, BktId>> heavy_id(heavy_id_size,
                                                {key_type{}, SMAX});
  for (const auto &[k, v] : heavy_seq) {
    size_t idx = (k * BIGINT) >> shift_bits & heavy_id_mask;
    while (heavy_id[idx].second != SMAX) {
      idx = (idx + 1) & heavy_id_mask;
    }
    heavy_id[idx] = {k, v};
  }
  const auto lookup = [&](size_t k) {
    if ((g(In[k]) & bits_mask) > overflow) {
      // In[k] is overflowed
      return static_cast<BktId>(bucket_id);
    }
    size_t idx = (g(In[k]) * BIGINT) >> shift_bits & heavy_id_mask;
    while (heavy_id[idx].second != SMAX && heavy_id[idx].first != g(In[k])) {
      idx = (idx + 1) & heavy_id_mask;
    }
    if (heavy_id[idx].second == SMAX) {
      // In[k] is a light key
      return light_id[g(In[k]) >> shift_bits & light_mask];
    } else {
      // In[k] is a heavy key
      return heavy_id[idx].second;
    }
  };

  // 2. count the number of light/heavy keys
  size_t heavy_buckets = heavy_seq.size();
  size_t num_buckets =
      heavy_buckets + light_buckets + 1;  // with an extra overflow buckets
  assert(num_buckets == bucket_id + 1);

  const auto get_bits = delayed_seq<uint16_t>(n, lookup);

  sequence<size_t> bucket_offsets;
  bool one_bucket;
  std::tie(bucket_offsets, one_bucket) =
      internal::count_sort_<assignment_tag, s_size_t>(
          In, Out, make_slice(get_bits), num_buckets, parallelism, true,
          light_id);
  if (one_bucket) {
    integer_sort2_<s_size_t, inplace_tag, assignment_tag>(
        In, Out, Tmp, g, top_bits - log2_light_keys, parallelism);
    return;
  }

  if constexpr (inplace_tag::value == true) {
    parallel_for(0, heavy_buckets, [&](size_t i) {
      size_t start = bucket_offsets[heavy_seq[i].second];
      size_t end = bucket_offsets[heavy_seq[i].second + 1];
      uninitialized_relocate_n(In.begin() + start, Out.begin() + start,
                               end - start);
      // parallel_for(start, end, [&](size_t j) { assign_dispatch(In[j], Out[j],
      // assignment_tag()); });
    });
  }

  // 3. sort within each bucket
  size_t total_elements = 0;
  for (size_t i = 0; i < light_buckets + 1; i++) {
    total_elements +=
        bucket_offsets[light_id[i] + 1] - bucket_offsets[light_id[i]];
  }
  size_t avg_elements = total_elements / (light_buckets + 1);
  sequence<size_t> parallel_blocks;
  parallel_blocks.push_back(0);
  size_t cumulative_elements = 0;
  for (size_t i = 0; i < light_buckets + 1; i++) {
    if (cumulative_elements >= avg_elements) {
      parallel_blocks.push_back(i);
      cumulative_elements = 0;
    }
    cumulative_elements +=
        bucket_offsets[light_id[i] + 1] - bucket_offsets[light_id[i]];
  }
  parallel_blocks.push_back(light_buckets + 1);

  auto &Arr = inplace_tag::value == true ? In : Out;
  auto &Tmp2 = inplace_tag::value == true ? Out : Tmp;
  parallel_for(
      0, parallel_blocks.size() - 1,
      [&](size_t i) {
        size_t block_s = parallel_blocks[i];
        size_t block_e = parallel_blocks[i + 1];
        for (size_t j = block_s; j < block_e; j++) {
          size_t start = bucket_offsets[light_id[j]];
          size_t end = bucket_offsets[light_id[j] + 1];
          if (start != end) {
            auto a = Out.cut(start, end);
            auto b = Tmp.cut(start, end);
            if (j != light_buckets) {
              integer_sort2_<s_size_t,
                             typename std::negation<inplace_tag>::type,
                             uninitialized_relocate_tag>(
                  a, b, a, g, top_bits - log2_light_keys,
                  (parallelism * (end - start)) / (total_elements + 1));
              size_t n1 =
                  bucket_offsets[light_id[j] + 1] - bucket_offsets[light_id[j]];
              size_t n2 = bucket_offsets[light_id[j + 1]] -
                          bucket_offsets[light_id[j] + 1];
              if (n1 != 0 && n2 != 0) {
                size_t s_seq = light_id[j] - j;
                size_t t_seq = light_id[j + 1] - (j + 1);
                merge_inplace(
                    Arr.cut(start, start + n1 + n2),
                    Tmp2.cut(start, start + n1 + n2),
                    heavy_seq.cut(s_seq, t_seq),
                    bucket_offsets.cut(light_id[j] + 1, light_id[j + 1]), g, n1,
                    n2);
              }
            } else {
              if constexpr (std::negation<inplace_tag>::value == true) {
                internal::seq_sort_inplace(
                    a,
                    [&](const in_type &i1, const in_type &i2) {
                      return g(i1) < g(i2);
                    },
                    true);
              } else {
                internal::seq_sort_<uninitialized_relocate_tag>(
                    a, b,
                    [&](const in_type &i1, const in_type &i2) {
                      return g(i1) < g(i2);
                    },
                    true);
              }
            }
          }
        }
      },
      1 / parallelism);
}

template <typename Iterator, typename GetKey>
auto integer_sort2(slice<Iterator, Iterator> In, const GetKey &g) {
  using in_type = typename slice<Iterator, Iterator>::value_type;
  auto Out = sequence<in_type>::uninitialized(In.size());
  auto Tmp = sequence<in_type>::uninitialized(In.size());
  size_t max32 = static_cast<size_t>((std::numeric_limits<uint32_t>::max)());
  using key_type = typename std::invoke_result<
      GetKey, typename slice<Iterator, Iterator>::value_type>::type;
  if (In.size() < max32) {
    integer_sort2_<uint32_t, std::false_type, uninitialized_copy_tag>(
        In, make_slice(Out), make_slice(Tmp), g, sizeof(key_type) * 8);
  } else {
    integer_sort2_<size_t, std::false_type, uninitialized_copy_tag>(
        In, make_slice(Out), make_slice(Tmp), g, sizeof(key_type) * 8);
  }
  return Out;
}

template <typename Iterator, typename GetKey>
void integer_sort_inplace2(slice<Iterator, Iterator> In, const GetKey &g) {
  using in_type = typename slice<Iterator, Iterator>::value_type;
  auto Tmp = sequence<in_type>::uninitialized(In.size());
  size_t max32 = static_cast<size_t>((std::numeric_limits<uint32_t>::max)());
  using key_type = typename std::invoke_result<
      GetKey, typename slice<Iterator, Iterator>::value_type>::type;
  if (In.size() < max32) {
    integer_sort2_<uint32_t, std::true_type, uninitialized_relocate_tag>(
        In, make_slice(Tmp), In, g, sizeof(key_type) * 8);
  } else {
    integer_sort2_<size_t, std::true_type, uninitialized_relocate_tag>(
        In, make_slice(Tmp), In, g, sizeof(key_type) * 8);
  }
}

}  // namespace parlay

#endif  // OUR_INTEGER_SORT_H_
