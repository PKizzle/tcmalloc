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

#ifndef TCMALLOC_INTERNAL_ADDRESS_BITS_H_
#define TCMALLOC_INTERNAL_ADDRESS_BITS_H_

#include <cstddef>

#include "absl/base/attributes.h"
#include "tcmalloc/internal/config.h"

GOOGLE_MALLOC_SECTION_BEGIN
namespace tcmalloc {
namespace tcmalloc_internal {

// Returns the effective number of virtual address bits for the current system.
//
// On aarch64 Linux, the virtual address space size depends on the kernel's
// page table configuration (CONFIG_PGTABLE_LEVELS).  With 3-level page tables
// (e.g. Raspberry Pi OS), only 39 bits are available; with 4-level page tables,
// 48 bits are available.  This function detects the actual size at runtime by
// inspecting /proc/self/maps, with a fallback to mmap probing.
//
// On other architectures, returns kMaxAddressBits (the compile-time constant).
//
// The returned value is always <= kMaxAddressBits.
ABSL_ATTRIBUTE_PURE_FUNCTION int EffectiveAddressBits();

}  // namespace tcmalloc_internal
}  // namespace tcmalloc
GOOGLE_MALLOC_SECTION_END

#endif  // TCMALLOC_INTERNAL_ADDRESS_BITS_H_
