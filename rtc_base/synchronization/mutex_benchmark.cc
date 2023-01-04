/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "benchmark/benchmark.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/unused.h"

namespace webrtc {

class PerfTestData {
 public:
  PerfTestData() : cache_line_barrier_1_(), cache_line_barrier_2_() {
    cache_line_barrier_1_[0]++;  // Avoid 'is not used'.
    cache_line_barrier_2_[0]++;  // Avoid 'is not used'.
  }

  int AddToCounter(int add) {
    MutexLock mu(&mu_);
    my_counter_ += add;
    return 0;
  }

 private:
  uint8_t cache_line_barrier_1_[64];
  Mutex mu_;
  uint8_t cache_line_barrier_2_[64];
  int64_t my_counter_ = 0;
};

void BM_LockWithMutex(benchmark::State& state) {
  static PerfTestData test_data;
  for (auto s : state) {
    RTC_UNUSED(s);
    benchmark::DoNotOptimize(test_data.AddToCounter(2));
  }
}

BENCHMARK(BM_LockWithMutex)->Threads(1);
BENCHMARK(BM_LockWithMutex)->Threads(2);
BENCHMARK(BM_LockWithMutex)->Threads(4);
BENCHMARK(BM_LockWithMutex)->ThreadPerCpu();

}  // namespace webrtc

/*

Results:

NB when reproducing: Remember to turn of power management features such as CPU
scaling before running!

pthreads (Linux):
----------------------------------------------------------------------
Run on (12 X 4500 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x6)
  L1 Instruction 32 KiB (x6)
  L2 Unified 1024 KiB (x6)
  L3 Unified 8448 KiB (x1)
Load Average: 0.26, 0.28, 0.44
----------------------------------------------------------------------
Benchmark                            Time             CPU   Iterations
----------------------------------------------------------------------
BM_LockWithMutex/threads:1        13.4 ns         13.4 ns     52192906
BM_LockWithMutex/threads:2        44.2 ns         88.4 ns      8189944
BM_LockWithMutex/threads:4        52.0 ns          198 ns      3743244
BM_LockWithMutex/threads:12       84.9 ns          944 ns       733524

std::mutex performs like the pthread implementation (Linux).

Abseil (Linux):
----------------------------------------------------------------------
Run on (12 X 4500 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x6)
  L1 Instruction 32 KiB (x6)
  L2 Unified 1024 KiB (x6)
  L3 Unified 8448 KiB (x1)
Load Average: 0.27, 0.24, 0.37
----------------------------------------------------------------------
Benchmark                            Time             CPU   Iterations
----------------------------------------------------------------------
BM_LockWithMutex/threads:1        15.0 ns         15.0 ns     46550231
BM_LockWithMutex/threads:2        91.1 ns          182 ns      4059212
BM_LockWithMutex/threads:4        40.8 ns          131 ns      5496560
BM_LockWithMutex/threads:12       37.0 ns          130 ns      5377668



1/4 2023 macOS Monterey 12.6 Mac Mini (2018, i7-8700B) results.
---------------------------------------------------------------------------
6 physical core, and Hyper-Threading enabled. Turbo Boost is disabled.

os_unfair_lock:
Run on (12 X 3200 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x6)
  L1 Instruction 32 KiB (x6)
  L2 Unified 256 KiB (x6)
  L3 Unified 12288 KiB (x1)
Load Average: 1.45, 1.92, 1.94
-----------------------------------------------------------------------------
Benchmark                                   Time             CPU   Iterations
-----------------------------------------------------------------------------
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1               15.7 ns         15.7 ns     44639726
BM_LockWithMutex/threads:1_mean          15.7 ns         15.7 ns           10
BM_LockWithMutex/threads:1_median        15.7 ns         15.7 ns           10
BM_LockWithMutex/threads:1_stddev       0.003 ns        0.002 ns           10
BM_LockWithMutex/threads:1_cv            0.02 %          0.01 %            10
BM_LockWithMutex/threads:2               85.4 ns          171 ns      4013072
BM_LockWithMutex/threads:2               90.4 ns          181 ns      4013072
BM_LockWithMutex/threads:2               89.0 ns          178 ns      4013072
BM_LockWithMutex/threads:2               88.2 ns          176 ns      4013072
BM_LockWithMutex/threads:2               83.4 ns          167 ns      4013072
BM_LockWithMutex/threads:2               88.4 ns          177 ns      4013072
BM_LockWithMutex/threads:2               90.4 ns          181 ns      4013072
BM_LockWithMutex/threads:2               90.3 ns          180 ns      4013072
BM_LockWithMutex/threads:2               87.6 ns          175 ns      4013072
BM_LockWithMutex/threads:2               85.2 ns          170 ns      4013072
BM_LockWithMutex/threads:2_mean          87.8 ns          176 ns           10
BM_LockWithMutex/threads:2_median        88.3 ns          176 ns           10
BM_LockWithMutex/threads:2_stddev        2.43 ns         4.87 ns           10
BM_LockWithMutex/threads:2_cv            2.77 %          2.77 %            10
BM_LockWithMutex/threads:4               53.8 ns          213 ns      3347640
BM_LockWithMutex/threads:4               53.7 ns          213 ns      3347640
BM_LockWithMutex/threads:4               53.6 ns          213 ns      3347640
BM_LockWithMutex/threads:4               53.6 ns          213 ns      3347640
BM_LockWithMutex/threads:4               53.7 ns          214 ns      3347640
BM_LockWithMutex/threads:4               53.1 ns          211 ns      3347640
BM_LockWithMutex/threads:4               53.8 ns          214 ns      3347640
BM_LockWithMutex/threads:4               53.8 ns          214 ns      3347640
BM_LockWithMutex/threads:4               53.9 ns          214 ns      3347640
BM_LockWithMutex/threads:4               53.5 ns          213 ns      3347640
BM_LockWithMutex/threads:4_mean          53.7 ns          213 ns           10
BM_LockWithMutex/threads:4_median        53.7 ns          213 ns           10
BM_LockWithMutex/threads:4_stddev       0.226 ns        0.961 ns           10
BM_LockWithMutex/threads:4_cv            0.42 %          0.45 %            10
BM_LockWithMutex/threads:12              45.4 ns          533 ns      1346304
BM_LockWithMutex/threads:12              45.7 ns          543 ns      1346304
BM_LockWithMutex/threads:12              45.1 ns          537 ns      1346304
BM_LockWithMutex/threads:12              45.0 ns          535 ns      1346304
BM_LockWithMutex/threads:12              44.7 ns          533 ns      1346304
BM_LockWithMutex/threads:12              44.9 ns          536 ns      1346304
BM_LockWithMutex/threads:12              45.1 ns          533 ns      1346304
BM_LockWithMutex/threads:12              45.1 ns          537 ns      1346304
BM_LockWithMutex/threads:12              45.3 ns          539 ns      1346304
BM_LockWithMutex/threads:12              44.9 ns          536 ns      1346304
BM_LockWithMutex/threads:12_mean         45.1 ns          536 ns           10
BM_LockWithMutex/threads:12_median       45.1 ns          536 ns           10
BM_LockWithMutex/threads:12_stddev      0.268 ns         3.26 ns           10
BM_LockWithMutex/threads:12_cv           0.59 %          0.61 %            10


pthread:
Run on (12 X 3200 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x6)
  L1 Instruction 32 KiB (x6)
  L2 Unified 256 KiB (x6)
  L3 Unified 12288 KiB (x1)
Load Average: 1.59, 1.92, 1.94
-----------------------------------------------------------------------------
Benchmark                                   Time             CPU   Iterations
-----------------------------------------------------------------------------
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1               22.2 ns         22.2 ns     31452193
BM_LockWithMutex/threads:1               22.3 ns         22.3 ns     31452193
BM_LockWithMutex/threads:1_mean          22.3 ns         22.3 ns           10
BM_LockWithMutex/threads:1_median        22.3 ns         22.3 ns           10
BM_LockWithMutex/threads:1_stddev       0.005 ns        0.003 ns           10
BM_LockWithMutex/threads:1_cv            0.02 %          0.01 %            10
BM_LockWithMutex/threads:2               56.9 ns         93.4 ns      7708360
BM_LockWithMutex/threads:2               56.9 ns         93.0 ns      7708360
BM_LockWithMutex/threads:2               55.6 ns         91.2 ns      7708360
BM_LockWithMutex/threads:2               57.2 ns         93.8 ns      7708360
BM_LockWithMutex/threads:2               57.5 ns         94.4 ns      7708360
BM_LockWithMutex/threads:2               57.1 ns         93.5 ns      7708360
BM_LockWithMutex/threads:2               57.3 ns         93.7 ns      7708360
BM_LockWithMutex/threads:2               57.0 ns         93.1 ns      7708360
BM_LockWithMutex/threads:2               57.0 ns         93.4 ns      7708360
BM_LockWithMutex/threads:2               57.3 ns         94.1 ns      7708360
BM_LockWithMutex/threads:2_mean          57.0 ns         93.3 ns           10
BM_LockWithMutex/threads:2_median        57.0 ns         93.4 ns           10
BM_LockWithMutex/threads:2_stddev       0.511 ns        0.876 ns           10
BM_LockWithMutex/threads:2_cv            0.90 %          0.94 %            10
BM_LockWithMutex/threads:4               56.6 ns          191 ns      3739268
BM_LockWithMutex/threads:4               56.7 ns          191 ns      3739268
BM_LockWithMutex/threads:4               56.2 ns          189 ns      3739268
BM_LockWithMutex/threads:4               56.2 ns          190 ns      3739268
BM_LockWithMutex/threads:4               56.8 ns          191 ns      3739268
BM_LockWithMutex/threads:4               56.6 ns          191 ns      3739268
BM_LockWithMutex/threads:4               56.6 ns          191 ns      3739268
BM_LockWithMutex/threads:4               56.5 ns          190 ns      3739268
BM_LockWithMutex/threads:4               56.2 ns          189 ns      3739268
BM_LockWithMutex/threads:4               56.0 ns          189 ns      3739268
BM_LockWithMutex/threads:4_mean          56.4 ns          190 ns           10
BM_LockWithMutex/threads:4_median        56.5 ns          191 ns           10
BM_LockWithMutex/threads:4_stddev       0.255 ns        0.866 ns           10
BM_LockWithMutex/threads:4_cv            0.45 %          0.46 %            10
BM_LockWithMutex/threads:12              62.6 ns          670 ns      1046304
BM_LockWithMutex/threads:12              61.3 ns          662 ns      1046304
BM_LockWithMutex/threads:12              62.5 ns          676 ns      1046304
BM_LockWithMutex/threads:12              61.7 ns          669 ns      1046304
BM_LockWithMutex/threads:12              60.7 ns          662 ns      1046304
BM_LockWithMutex/threads:12              60.7 ns          662 ns      1046304
BM_LockWithMutex/threads:12              61.5 ns          667 ns      1046304
BM_LockWithMutex/threads:12              62.3 ns          676 ns      1046304
BM_LockWithMutex/threads:12              61.0 ns          665 ns      1046304
BM_LockWithMutex/threads:12              61.9 ns          668 ns      1046304
BM_LockWithMutex/threads:12_mean         61.6 ns          668 ns           10
BM_LockWithMutex/threads:12_median       61.6 ns          667 ns           10
BM_LockWithMutex/threads:12_stddev      0.713 ns         5.36 ns           10
BM_LockWithMutex/threads:12_cv           1.16 %          0.80 %            10


abseil:
Run on (12 X 3200 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x6)
  L1 Instruction 32 KiB (x6)
  L2 Unified 256 KiB (x6)
  L3 Unified 12288 KiB (x1)
Load Average: 1.47, 1.95, 1.96
-----------------------------------------------------------------------------
Benchmark                                   Time             CPU   Iterations
-----------------------------------------------------------------------------
BM_LockWithMutex/threads:1               20.6 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.6 ns         20.6 ns     34049177
BM_LockWithMutex/threads:1               20.6 ns         20.6 ns     34049177
BM_LockWithMutex/threads:1               20.5 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.5 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.6 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.5 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.6 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.6 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1               20.5 ns         20.5 ns     34049177
BM_LockWithMutex/threads:1_mean          20.6 ns         20.5 ns           10
BM_LockWithMutex/threads:1_median        20.6 ns         20.5 ns           10
BM_LockWithMutex/threads:1_stddev       0.006 ns        0.005 ns           10
BM_LockWithMutex/threads:1_cv            0.03 %          0.03 %            10
BM_LockWithMutex/threads:2               90.4 ns          178 ns      4309402
BM_LockWithMutex/threads:2               85.9 ns          168 ns      4309402
BM_LockWithMutex/threads:2               91.6 ns          180 ns      4309402
BM_LockWithMutex/threads:2               92.0 ns          181 ns      4309402
BM_LockWithMutex/threads:2               88.2 ns          173 ns      4309402
BM_LockWithMutex/threads:2               85.4 ns          167 ns      4309402
BM_LockWithMutex/threads:2               84.6 ns          166 ns      4309402
BM_LockWithMutex/threads:2               92.2 ns          182 ns      4309402
BM_LockWithMutex/threads:2               90.6 ns          178 ns      4309402
BM_LockWithMutex/threads:2               88.3 ns          174 ns      4309402
BM_LockWithMutex/threads:2_mean          88.9 ns          175 ns           10
BM_LockWithMutex/threads:2_median        89.4 ns          176 ns           10
BM_LockWithMutex/threads:2_stddev        2.88 ns         5.88 ns           10
BM_LockWithMutex/threads:2_cv            3.24 %          3.37 %            10
BM_LockWithMutex/threads:4               99.7 ns          342 ns      2071664
BM_LockWithMutex/threads:4               99.7 ns          341 ns      2071664
BM_LockWithMutex/threads:4               97.2 ns          334 ns      2071664
BM_LockWithMutex/threads:4               94.3 ns          320 ns      2071664
BM_LockWithMutex/threads:4               99.8 ns          342 ns      2071664
BM_LockWithMutex/threads:4               97.2 ns          330 ns      2071664
BM_LockWithMutex/threads:4               98.1 ns          336 ns      2071664
BM_LockWithMutex/threads:4               98.0 ns          335 ns      2071664
BM_LockWithMutex/threads:4               96.9 ns          330 ns      2071664
BM_LockWithMutex/threads:4               98.2 ns          336 ns      2071664
BM_LockWithMutex/threads:4_mean          97.9 ns          335 ns           10
BM_LockWithMutex/threads:4_median        98.0 ns          335 ns           10
BM_LockWithMutex/threads:4_stddev        1.66 ns         6.60 ns           10
BM_LockWithMutex/threads:4_cv            1.70 %          1.97 %            10
BM_LockWithMutex/threads:12              56.5 ns          162 ns      4101624
BM_LockWithMutex/threads:12              56.6 ns          166 ns      4101624
BM_LockWithMutex/threads:12              56.9 ns          166 ns      4101624
BM_LockWithMutex/threads:12              57.0 ns          169 ns      4101624
BM_LockWithMutex/threads:12              56.4 ns          165 ns      4101624
BM_LockWithMutex/threads:12              56.4 ns          162 ns      4101624
BM_LockWithMutex/threads:12              56.7 ns          163 ns      4101624
BM_LockWithMutex/threads:12              56.3 ns          164 ns      4101624
BM_LockWithMutex/threads:12              56.3 ns          165 ns      4101624
BM_LockWithMutex/threads:12              57.4 ns          165 ns      4101624
BM_LockWithMutex/threads:12_mean         56.7 ns          165 ns           10
BM_LockWithMutex/threads:12_median       56.6 ns          165 ns           10
BM_LockWithMutex/threads:12_stddev      0.360 ns         2.13 ns           10
BM_LockWithMutex/threads:12_cv           0.64 %          1.29 %            10


1/4 2023 macOS Ventura 13.1 M1 Pro (14'', 2021) results.
---------------------------------------------------------------------------
This is a big.LITTLE 6/2 machine, and there's no control over CPU affinity
or power management, take these results with a grain of salt.
Conditions on the machine were otherwise idle.
Note that the results show slower execution/higher variance for 2 threads.
This does not necessarily mean the implementation is worse there, it could
be the case that the test conditions cause placement on the E-cores and thus
giving better power efficiency.
We should measure placement with the dtrace sched provider in the future.

os_unfair_lock:
Run on (8 X 24.1213 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB (x8)
  L1 Instruction 128 KiB (x8)
  L2 Unified 4096 KiB (x4)
Load Average: 1.28, 1.57, 2.73
----------------------------------------------------------------------------
Benchmark                                  Time             CPU   Iterations
----------------------------------------------------------------------------
BM_LockWithMutex/threads:1              7.87 ns         7.87 ns     75444855
BM_LockWithMutex/threads:1              7.85 ns         7.85 ns     75444855
BM_LockWithMutex/threads:1              7.84 ns         7.84 ns     75444855
BM_LockWithMutex/threads:1              7.86 ns         7.86 ns     75444855
BM_LockWithMutex/threads:1              7.86 ns         7.86 ns     75444855
BM_LockWithMutex/threads:1              7.85 ns         7.85 ns     75444855
BM_LockWithMutex/threads:1              7.85 ns         7.85 ns     75444855
BM_LockWithMutex/threads:1              7.86 ns         7.86 ns     75444855
BM_LockWithMutex/threads:1              7.86 ns         7.86 ns     75444855
BM_LockWithMutex/threads:1              7.85 ns         7.85 ns     75444855
BM_LockWithMutex/threads:1_mean         7.85 ns         7.85 ns           10
BM_LockWithMutex/threads:1_median       7.85 ns         7.85 ns           10
BM_LockWithMutex/threads:1_stddev      0.008 ns        0.007 ns           10
BM_LockWithMutex/threads:1_cv           0.10 %          0.09 %            10
BM_LockWithMutex/threads:2               114 ns          228 ns      3505872
BM_LockWithMutex/threads:2               119 ns          239 ns      3505872
BM_LockWithMutex/threads:2               116 ns          232 ns      3505872
BM_LockWithMutex/threads:2               111 ns          221 ns      3505872
BM_LockWithMutex/threads:2               105 ns          210 ns      3505872
BM_LockWithMutex/threads:2               110 ns          219 ns      3505872
BM_LockWithMutex/threads:2               103 ns          206 ns      3505872
BM_LockWithMutex/threads:2              95.9 ns          192 ns      3505872
BM_LockWithMutex/threads:2              98.6 ns          197 ns      3505872
BM_LockWithMutex/threads:2               133 ns          265 ns      3505872
BM_LockWithMutex/threads:2_mean          111 ns          221 ns           10
BM_LockWithMutex/threads:2_median        110 ns          220 ns           10
BM_LockWithMutex/threads:2_stddev       10.8 ns         21.7 ns           10
BM_LockWithMutex/threads:2_cv           9.81 %          9.84 %            10
BM_LockWithMutex/threads:4              32.7 ns          128 ns      4000000
BM_LockWithMutex/threads:4              33.4 ns          131 ns      4000000
BM_LockWithMutex/threads:4              29.3 ns          117 ns      4000000
BM_LockWithMutex/threads:4              33.4 ns          131 ns      4000000
BM_LockWithMutex/threads:4              32.4 ns          127 ns      4000000
BM_LockWithMutex/threads:4              35.6 ns          140 ns      4000000
BM_LockWithMutex/threads:4              33.4 ns          131 ns      4000000
BM_LockWithMutex/threads:4              35.1 ns          138 ns      4000000
BM_LockWithMutex/threads:4              28.9 ns          115 ns      4000000
BM_LockWithMutex/threads:4              27.8 ns          111 ns      4000000
BM_LockWithMutex/threads:4_mean         32.2 ns          127 ns           10
BM_LockWithMutex/threads:4_median       33.0 ns          129 ns           10
BM_LockWithMutex/threads:4_stddev       2.65 ns         9.73 ns           10
BM_LockWithMutex/threads:4_cv           8.25 %          7.66 %            10
BM_LockWithMutex/threads:8              26.9 ns          206 ns      3483736
BM_LockWithMutex/threads:8              26.6 ns          206 ns      3483736
BM_LockWithMutex/threads:8              27.3 ns          209 ns      3483736
BM_LockWithMutex/threads:8              26.6 ns          205 ns      3483736
BM_LockWithMutex/threads:8              26.4 ns          203 ns      3483736
BM_LockWithMutex/threads:8              26.3 ns          204 ns      3483736
BM_LockWithMutex/threads:8              27.2 ns          208 ns      3483736
BM_LockWithMutex/threads:8              26.9 ns          208 ns      3483736
BM_LockWithMutex/threads:8              26.6 ns          206 ns      3483736
BM_LockWithMutex/threads:8              29.6 ns          216 ns      3483736
BM_LockWithMutex/threads:8_mean         27.0 ns          207 ns           10
BM_LockWithMutex/threads:8_median       26.7 ns          206 ns           10
BM_LockWithMutex/threads:8_stddev      0.962 ns         3.67 ns           10
BM_LockWithMutex/threads:8_cv           3.56 %          1.77 %            10


pthread:
Run on (8 X 24.1216 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB (x8)
  L1 Instruction 128 KiB (x8)
  L2 Unified 4096 KiB (x4)
Load Average: 0.98, 1.39, 2.48
----------------------------------------------------------------------------
Benchmark                                  Time             CPU   Iterations
----------------------------------------------------------------------------
BM_LockWithMutex/threads:1              8.50 ns         8.50 ns     71190302
BM_LockWithMutex/threads:1              8.50 ns         8.50 ns     71190302
BM_LockWithMutex/threads:1              8.51 ns         8.51 ns     71190302
BM_LockWithMutex/threads:1              8.50 ns         8.50 ns     71190302
BM_LockWithMutex/threads:1              8.49 ns         8.49 ns     71190302
BM_LockWithMutex/threads:1              8.53 ns         8.53 ns     71190302
BM_LockWithMutex/threads:1              8.50 ns         8.50 ns     71190302
BM_LockWithMutex/threads:1              8.49 ns         8.49 ns     71190302
BM_LockWithMutex/threads:1              8.49 ns         8.49 ns     71190302
BM_LockWithMutex/threads:1              8.63 ns         8.58 ns     71190302
BM_LockWithMutex/threads:1_mean         8.52 ns         8.51 ns           10
BM_LockWithMutex/threads:1_median       8.50 ns         8.50 ns           10
BM_LockWithMutex/threads:1_stddev      0.041 ns        0.028 ns           10
BM_LockWithMutex/threads:1_cv           0.49 %          0.32 %            10
BM_LockWithMutex/threads:2              21.9 ns         39.5 ns     17133556
BM_LockWithMutex/threads:2              29.5 ns         55.3 ns     17133556
BM_LockWithMutex/threads:2              29.8 ns         56.2 ns     17133556
BM_LockWithMutex/threads:2              20.6 ns         37.5 ns     17133556
BM_LockWithMutex/threads:2              27.4 ns         51.4 ns     17133556
BM_LockWithMutex/threads:2              23.8 ns         44.2 ns     17133556
BM_LockWithMutex/threads:2              44.1 ns         85.1 ns     17133556
BM_LockWithMutex/threads:2              23.9 ns         44.2 ns     17133556
BM_LockWithMutex/threads:2              94.2 ns          186 ns     17133556
BM_LockWithMutex/threads:2              85.0 ns          167 ns     17133556
BM_LockWithMutex/threads:2_mean         40.0 ns         76.7 ns           10
BM_LockWithMutex/threads:2_median       28.5 ns         53.4 ns           10
BM_LockWithMutex/threads:2_stddev       27.1 ns         54.5 ns           10
BM_LockWithMutex/threads:2_cv          67.60 %         71.12 %            10
BM_LockWithMutex/threads:4              26.6 ns         73.3 ns      9404148
BM_LockWithMutex/threads:4              26.2 ns         72.2 ns      9404148
BM_LockWithMutex/threads:4              27.2 ns         75.3 ns      9404148
BM_LockWithMutex/threads:4              26.0 ns         71.4 ns      9404148
BM_LockWithMutex/threads:4              25.3 ns         69.3 ns      9404148
BM_LockWithMutex/threads:4              26.3 ns         72.5 ns      9404148
BM_LockWithMutex/threads:4              25.3 ns         69.8 ns      9404148
BM_LockWithMutex/threads:4              24.9 ns         68.8 ns      9404148
BM_LockWithMutex/threads:4              25.9 ns         71.1 ns      9404148
BM_LockWithMutex/threads:4              25.6 ns         70.4 ns      9404148
BM_LockWithMutex/threads:4_mean         25.9 ns         71.4 ns           10
BM_LockWithMutex/threads:4_median       25.9 ns         71.3 ns           10
BM_LockWithMutex/threads:4_stddev      0.673 ns         1.99 ns           10
BM_LockWithMutex/threads:4_cv           2.60 %          2.78 %            10
BM_LockWithMutex/threads:8              21.0 ns          136 ns      4937184
BM_LockWithMutex/threads:8              21.4 ns          139 ns      4937184
BM_LockWithMutex/threads:8              20.7 ns          138 ns      4937184
BM_LockWithMutex/threads:8              21.4 ns          138 ns      4937184
BM_LockWithMutex/threads:8              21.8 ns          140 ns      4937184
BM_LockWithMutex/threads:8              22.5 ns          142 ns      4937184
BM_LockWithMutex/threads:8              22.7 ns          145 ns      4937184
BM_LockWithMutex/threads:8              22.0 ns          141 ns      4937184
BM_LockWithMutex/threads:8              21.2 ns          136 ns      4937184
BM_LockWithMutex/threads:8              22.1 ns          139 ns      4937184
BM_LockWithMutex/threads:8_mean         21.7 ns          139 ns           10
BM_LockWithMutex/threads:8_median       21.6 ns          139 ns           10
BM_LockWithMutex/threads:8_stddev      0.650 ns         2.72 ns           10
BM_LockWithMutex/threads:8_cv           3.00 %          1.95 %            10

absl:
Run on (8 X 24.1233 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB (x8)
  L1 Instruction 128 KiB (x8)
  L2 Unified 4096 KiB (x4)
Load Average: 1.24, 1.37, 2.12
----------------------------------------------------------------------------
Benchmark                                  Time             CPU   Iterations
----------------------------------------------------------------------------
BM_LockWithMutex/threads:1              8.19 ns         8.19 ns     72725762
BM_LockWithMutex/threads:1              8.21 ns         8.20 ns     72725762
BM_LockWithMutex/threads:1              8.18 ns         8.18 ns     72725762
BM_LockWithMutex/threads:1              8.18 ns         8.18 ns     72725762
BM_LockWithMutex/threads:1              8.20 ns         8.20 ns     72725762
BM_LockWithMutex/threads:1              8.19 ns         8.19 ns     72725762
BM_LockWithMutex/threads:1              8.22 ns         8.21 ns     72725762
BM_LockWithMutex/threads:1              8.19 ns         8.19 ns     72725762
BM_LockWithMutex/threads:1              8.20 ns         8.20 ns     72725762
BM_LockWithMutex/threads:1              8.18 ns         8.18 ns     72725762
BM_LockWithMutex/threads:1_mean         8.19 ns         8.19 ns           10
BM_LockWithMutex/threads:1_median       8.19 ns         8.19 ns           10
BM_LockWithMutex/threads:1_stddev      0.012 ns        0.011 ns           10
BM_LockWithMutex/threads:1_cv           0.15 %          0.13 %            10
BM_LockWithMutex/threads:2              76.1 ns          152 ns     12599332
BM_LockWithMutex/threads:2              78.9 ns          157 ns     12599332
BM_LockWithMutex/threads:2              84.0 ns          168 ns     12599332
BM_LockWithMutex/threads:2              81.9 ns          164 ns     12599332
BM_LockWithMutex/threads:2              83.5 ns          167 ns     12599332
BM_LockWithMutex/threads:2              77.9 ns          155 ns     12599332
BM_LockWithMutex/threads:2              86.3 ns          173 ns     12599332
BM_LockWithMutex/threads:2              66.1 ns          131 ns     12599332
BM_LockWithMutex/threads:2              69.9 ns          139 ns     12599332
BM_LockWithMutex/threads:2              76.2 ns          152 ns     12599332
BM_LockWithMutex/threads:2_mean         78.1 ns          156 ns           10
BM_LockWithMutex/threads:2_median       78.4 ns          156 ns           10
BM_LockWithMutex/threads:2_stddev       6.37 ns         13.2 ns           10
BM_LockWithMutex/threads:2_cv           8.15 %          8.47 %            10
BM_LockWithMutex/threads:4              24.9 ns         57.3 ns     12202456
BM_LockWithMutex/threads:4              25.0 ns         62.0 ns     12202456
BM_LockWithMutex/threads:4              25.1 ns         59.1 ns     12202456
BM_LockWithMutex/threads:4              23.0 ns         56.0 ns     12202456
BM_LockWithMutex/threads:4              23.6 ns         64.6 ns     12202456
BM_LockWithMutex/threads:4              26.5 ns         55.6 ns     12202456
BM_LockWithMutex/threads:4              22.7 ns         51.6 ns     12202456
BM_LockWithMutex/threads:4              21.6 ns         56.5 ns     12202456
BM_LockWithMutex/threads:4              25.0 ns         50.7 ns     12202456
BM_LockWithMutex/threads:4              27.2 ns         57.8 ns     12202456
BM_LockWithMutex/threads:4_mean         24.4 ns         57.1 ns           10
BM_LockWithMutex/threads:4_median       24.9 ns         56.9 ns           10
BM_LockWithMutex/threads:4_stddev       1.74 ns         4.21 ns           10
BM_LockWithMutex/threads:4_cv           7.10 %          7.38 %            10
BM_LockWithMutex/threads:8              22.9 ns         56.1 ns     18038736
BM_LockWithMutex/threads:8              22.3 ns         56.6 ns     18038736
BM_LockWithMutex/threads:8              21.7 ns         57.2 ns     18038736
BM_LockWithMutex/threads:8              19.9 ns         46.6 ns     18038736
BM_LockWithMutex/threads:8              18.2 ns         47.9 ns     18038736
BM_LockWithMutex/threads:8              17.2 ns         37.7 ns     18038736
BM_LockWithMutex/threads:8              19.8 ns         56.3 ns     18038736
BM_LockWithMutex/threads:8              18.5 ns         49.1 ns     18038736
BM_LockWithMutex/threads:8              16.2 ns         38.3 ns     18038736
BM_LockWithMutex/threads:8              16.5 ns         38.4 ns     18038736
BM_LockWithMutex/threads:8_mean         19.3 ns         48.4 ns           10
BM_LockWithMutex/threads:8_median       19.2 ns         48.5 ns           10
BM_LockWithMutex/threads:8_stddev       2.42 ns         8.08 ns           10
BM_LockWithMutex/threads:8_cv          12.50 %         16.68 %            10
*/
