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

#include "crc32c.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#define X86_ONLY 1
#else
#define X86_ONLY 0
#endif

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/* Dispatch state. */
static pthread_once_t g_crc32c_once = PTHREAD_ONCE_INIT;
static volatile int g_force_sw = 0;
static crc32c_impl g_impl = CRC32C_IMPL_SW;

static int cpu_has_hw_crc(void) {
#if X86_ONLY
  __builtin_cpu_init();
  return __builtin_cpu_supports("sse4.2");
#else
  return 0;
#endif
}

uint32_t crc32c_sw(const char *src, size_t len) {
  const unsigned char *s = (unsigned char *)src;
  uint32_t h = ~0;
  while (len--) {
    h ^= *s++;
    for (int k = 0; k < 8; k++) h = h & 1 ? (h >> 1) ^ POLY : h >> 1;
  }
  return ~h;
}

#if X86_ONLY
__attribute__((target("sse4.2")))
static uint32_t crc32c_hw_body(const char *src, size_t len) {
  const unsigned char *s = (const unsigned char *)src;
  uint64_t hh = ~0u;
#ifdef __x86_64__
  while (len > 7) {
    uint64_t v;
    memcpy(&v, s, sizeof(v));
    hh = _mm_crc32_u64(hh, v);
    s += 8;
    len -= 8;
  }
  uint32_t h = (uint32_t)hh;
#else
  uint32_t h = (uint32_t)hh;
#endif
  if (len > 3) {
    uint32_t v;
    memcpy(&v, s, sizeof(v));
    h = _mm_crc32_u32(h, v);
    s += 4;
    len -= 4;
  }
  if (len > 1) {
    uint16_t v;
    memcpy(&v, s, sizeof(v));
    h = _mm_crc32_u16(h, v);
    s += 2;
    len -= 2;
  }
  if (len > 0) {
    uint8_t v;
    memcpy(&v, s, sizeof(v));
    h = _mm_crc32_u8(h, v);
  }
  return ~h;
}

uint32_t crc32c_hw(const char *src, size_t len) { return crc32c_hw_body(src, len); }
#else
uint32_t crc32c_hw(const char *src, size_t len) { return crc32c_sw(src, len); }
#endif

static void crc32c_init(void) {
  if (g_force_sw) {
    g_impl = CRC32C_IMPL_SW;
    return;
  }

  const char *env = getenv("CRC32C_FORCE");
  if (env != NULL) {
    if (strcmp(env, "sw") == 0) {
      g_force_sw = 1;
    } else if (strcmp(env, "hw") == 0) {
      g_force_sw = 0;
      if (!cpu_has_hw_crc()) {
        g_force_sw = 1;
      }
    }
  }

  if (!g_force_sw && cpu_has_hw_crc()) {
    g_impl = CRC32C_IMPL_HW;
  } else {
    g_impl = CRC32C_IMPL_SW;
  }
}

const char *crc32c_impl_name(void) {
  pthread_once(&g_crc32c_once, crc32c_init);
  return (g_impl == CRC32C_IMPL_HW) ? "hw" : "sw";
}

int crc32c_hw_available(void) { return cpu_has_hw_crc(); }

void crc32c_force_software(void) {
  g_force_sw = 1;
  pthread_once(&g_crc32c_once, crc32c_init);
  g_impl = CRC32C_IMPL_SW;
}

int crc32c_selfcheck(void) {
#if X86_ONLY
  pthread_once(&g_crc32c_once, crc32c_init);
  if (g_impl != CRC32C_IMPL_HW) {
    return 0;  // Nothing to check or hw unavailable.
  }

  static const char *k_vec = "123456789";
  int failures = 0;

  /* Known vector. */
  uint32_t hw = crc32c_hw_body(k_vec, strlen(k_vec));
  uint32_t sw = crc32c_sw(k_vec, strlen(k_vec));
  if (hw != sw) failures++;

  /* Alignment/length coverage. */
  uint8_t pattern[512];
  for (size_t i = 0; i < sizeof(pattern); i++) pattern[i] = (uint8_t)(i * 3 + 1);

  const size_t offsets[] = {0, 1, 2, 3, 4, 7, 8, 15, 31, 63};
  const size_t lengths[] = {0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 255};
  for (size_t oi = 0; oi < sizeof(offsets) / sizeof(offsets[0]); oi++) {
    for (size_t li = 0; li < sizeof(lengths) / sizeof(lengths[0]); li++) {
      size_t off = offsets[oi];
      size_t len = lengths[li];
      if (off + len > sizeof(pattern)) continue;
      const char *p = (const char *)(pattern + off);
      uint32_t hw_v = crc32c_hw_body(p, len);
      uint32_t sw_v = crc32c_sw(p, len);
      if (hw_v != sw_v) {
        failures++;
      }
    }
  }

  if (failures) {
    crc32c_force_software();
    return -1;
  }
#endif
  return 0;
}

uint32_t crc32c(const char *src, size_t len) {
  pthread_once(&g_crc32c_once, crc32c_init);
  if (g_impl == CRC32C_IMPL_HW) {
#if X86_ONLY
    return crc32c_hw_body(src, len);
#else
    return crc32c_sw(src, len);
#endif
  }
  return crc32c_sw(src, len);
}
