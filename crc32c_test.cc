// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>

#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "crc32c.h"

extern "C" {
uint32_t crc32c_hw(const char *, size_t);
uint32_t crc32c_sw(const char *, size_t);
}

namespace {
void MaybeReportMismatch(const char *label, uint32_t got, uint32_t want,
                         size_t len, int *failures) {
  if (got == want) return;
  fprintf(stderr, "%s mismatch: 0x%08x vs 0x%08x len %zu\n", label, got, want,
          len);
  (*failures)++;
}
}  // namespace

int main(int argc, char **argv) {
  int failures = 0;

  // Known vector: CRC32C("123456789") = 0xe3069283
  static const char *k_vec = "123456789";
  uint32_t vec_sw = crc32c_sw(k_vec, strlen(k_vec));
  MaybeReportMismatch("known-vector-sw", vec_sw, 0xe3069283, strlen(k_vec),
                      &failures);
  uint32_t vec_dispatch = crc32c(k_vec, strlen(k_vec));
  MaybeReportMismatch("dispatch", vec_dispatch, vec_sw, strlen(k_vec),
                      &failures);
  if (crc32c_hw_available()) {
    uint32_t vec_hw = crc32c_hw(k_vec, strlen(k_vec));
    MaybeReportMismatch("known-vector-hw", vec_hw, vec_sw, strlen(k_vec),
                        &failures);
  }

  // Alignment and edge-length coverage.
  std::vector<size_t> offsets = {0, 1, 2, 3, 4, 7, 8, 15, 31, 63};
  std::vector<size_t> lengths = {0, 1, 2, 3, 4, 7, 8, 15, 16,
                                 31, 32, 63, 64, 255, 256, 511};
  std::vector<unsigned char> pattern(1024);
  for (size_t i = 0; i < pattern.size(); ++i) pattern[i] = (unsigned char)(i * 5 + 1);

  for (size_t off : offsets) {
    for (size_t len : lengths) {
      if (off + len > pattern.size()) continue;
      const char *p = reinterpret_cast<const char *>(pattern.data() + off);
      uint32_t sw = crc32c_sw(p, len);
      uint32_t disp = crc32c(p, len);
      MaybeReportMismatch("dispatch", disp, sw, len, &failures);
      if (crc32c_hw_available()) {
        uint32_t hw = crc32c_hw(p, len);
        MaybeReportMismatch("hw", hw, sw, len, &failures);
      }
    }
  }

  // Random buffers.
  std::knuth_b rndeng((std::random_device()()));
  std::uniform_int_distribution<int> size_dist(1, 1048576);
  std::uniform_int_distribution<int> d_dist(0, 255);
  std::string buf;
  for (int i = 0; i < 100; i++) {
    size_t len = size_dist(rndeng);
    buf.resize(len);
    for (size_t j = 0; j < len; j++) buf[j] = static_cast<char>(d_dist(rndeng));
    uint32_t sw = crc32c_sw(buf.data(), len);
    uint32_t disp = crc32c(buf.data(), len);
    MaybeReportMismatch("dispatch", disp, sw, len, &failures);
    if (crc32c_hw_available()) {
      uint32_t hw = crc32c_hw(buf.data(), len);
      MaybeReportMismatch("hw", hw, sw, len, &failures);
    }
  }

  // Selfcheck exercises the dispatcher and forces SW if a mismatch is found.
  if (crc32c_selfcheck() != 0) {
    fprintf(stderr, "crc32c_selfcheck forced software path\n");
    failures++;
  }

  return failures == 0 ? 0 : 1;
}
