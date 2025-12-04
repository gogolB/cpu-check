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


#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CRC32C_IMPL_AUTO = 0,
  CRC32C_IMPL_HW,
  CRC32C_IMPL_SW,
} crc32c_impl;

// Returns the selected implementation name: "hw" or "sw".
const char *crc32c_impl_name(void);

// Returns 1 if the CPU supports the CRC32 instruction, 0 otherwise.
int crc32c_hw_available(void);

// Forces the dispatcher to use the pure software implementation.
void crc32c_force_software(void);

// Runs a quick self-check comparing hw vs sw (if hw available). Returns 0 on
// success; on any mismatch, forces software and returns non-zero.
int crc32c_selfcheck(void);

uint32_t crc32c(const char *s, size_t len);

#ifdef __cplusplus
}
#endif
