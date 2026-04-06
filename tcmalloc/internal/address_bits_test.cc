// Copyright 2024 The TCMalloc Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tcmalloc/internal/address_bits.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tcmalloc/internal/config.h"

namespace tcmalloc {
namespace tcmalloc_internal {
namespace {

TEST(AddressBits, EffectiveAddressBitsInRange) {
  const int bits = EffectiveAddressBits();

  // Must not exceed the compile-time maximum.
  EXPECT_LE(bits, kAddressBits);

  // Must be at least as large as the minimum reasonable VA size for 64-bit
  // systems.  On aarch64, the minimum is 39 (3-level page tables with 4KB
  // pages).
  if (sizeof(void*) == 8) {
    EXPECT_GE(bits, 39);
  }
}

TEST(AddressBits, ConsistentAcrossCalls) {
  // The value must be the same every time (cached).
  EXPECT_EQ(EffectiveAddressBits(), EffectiveAddressBits());
}

#if defined(__x86_64__)
TEST(AddressBits, X86ReturnsCompileTimeConstant) {
  // On x86_64, the runtime detection is a no-op and should return kAddressBits.
  EXPECT_EQ(EffectiveAddressBits(), kAddressBits);
}
#endif

#if defined(__aarch64__) && defined(__linux__)
TEST(AddressBits, Aarch64ReturnsValidSize) {
  const int bits = EffectiveAddressBits();
  // On aarch64 Linux, valid VA sizes are 39 (3-level PT) or 48 (4-level PT).
  EXPECT_TRUE(bits == 39 || bits == 48)
      << "Unexpected effective address bits: " << bits;
}
#endif

}  // namespace
}  // namespace tcmalloc_internal
}  // namespace tcmalloc
