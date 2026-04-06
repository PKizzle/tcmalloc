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

#include <algorithm>
#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "tcmalloc/internal/config.h"

#if defined(__aarch64__) && defined(__linux__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

GOOGLE_MALLOC_SECTION_BEGIN
namespace tcmalloc {
namespace tcmalloc_internal {

namespace {

#if defined(__aarch64__) && defined(__linux__)

// Parse a hexadecimal number from the buffer, advancing the pointer past
// the parsed digits.  Returns 0 if no hex digits are found.
uintptr_t ParseHex(const char*& p, const char* end) {
  uintptr_t result = 0;
  bool found = false;
  while (p < end) {
    char c = *p;
    if (c >= '0' && c <= '9') {
      result = (result << 4) | static_cast<uintptr_t>(c - '0');
      found = true;
    } else if (c >= 'a' && c <= 'f') {
      result = (result << 4) | static_cast<uintptr_t>(c - 'a' + 10);
      found = true;
    } else {
      break;
    }
    ++p;
  }
  return found ? result : 0;
}

// Detect virtual address bits by parsing /proc/self/maps.
//
// Each line has the format: <hex_start>-<hex_end> <perms> <offset> ...
// We find the highest end address and compute the minimum number of bits
// needed to represent addresses in the process, then round up to the nearest
// valid aarch64 VA size (39 or 48).
//
// Uses raw open/read/close syscalls to avoid heap allocation, which is critical
// since this runs during malloc initialization.
int DetectFromProcMaps() {
  // Use open() directly -- on Linux this is a thin syscall wrapper that does
  // not allocate memory.
  int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return 0;

  uintptr_t highest_addr = 0;
  char buf[4096];
  int leftover = 0;

  for (;;) {
    ssize_t n = read(fd, buf + leftover,
                     static_cast<size_t>(sizeof(buf) - 1 - leftover));
    if (n <= 0) break;
    n += leftover;
    leftover = 0;
    buf[n] = '\0';

    const char* p = buf;
    const char* end = buf + n;

    while (p < end) {
      const char* line_start = p;

      // Parse the end address from "<start>-<end> ..."
      // Skip start address
      ParseHex(p, end);

      if (p < end && *p == '-') {
        ++p;
        uintptr_t end_addr = ParseHex(p, end);
        if (end_addr > highest_addr) {
          highest_addr = end_addr;
        }
      }

      // Skip to end of line
      while (p < end && *p != '\n') ++p;
      if (p < end) {
        ++p;  // skip '\n'
      } else {
        // Partial line at end of buffer -- save for next read
        leftover = static_cast<int>(end - line_start);
        if (leftover > 0 &&
            leftover < static_cast<int>(sizeof(buf) - 1)) {
          __builtin_memmove(buf, line_start, static_cast<size_t>(leftover));
        } else {
          leftover = 0;
        }
        break;
      }
    }
  }

  close(fd);

  if (highest_addr == 0) return 0;

  // Compute bits needed: position of the highest set bit + 1
  int bits = 64 - __builtin_clzll(highest_addr);

  // Round up to the next valid aarch64 VA size.
  // With 4KB pages: 39 bits (3-level PT), 48 bits (4-level PT).
  if (bits <= 39) return 39;
  if (bits <= 48) return 48;
  // ARMv8.2-LVA: 52 bits (unlikely in practice but handle gracefully)
  return 52;
}

// Fallback: probe whether 48-bit virtual addresses are available by attempting
// an mmap at a high address.  If the kernel returns an address in the upper
// half of the 48-bit range, we have 48-bit VA; otherwise assume 39-bit.
int DetectWithMmapProbe() {
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) return 39;

  void* probe_addr = reinterpret_cast<void*>(
      (uintptr_t{1} << 47) - static_cast<uintptr_t>(page_size));

  void* result = mmap(probe_addr, static_cast<size_t>(page_size), PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (result == MAP_FAILED) {
    return 39;
  }

  bool is_high = reinterpret_cast<uintptr_t>(result) >= (uintptr_t{1} << 38);
  munmap(result, static_cast<size_t>(page_size));
  return is_high ? 48 : 39;
}

#endif  // __aarch64__ && __linux__

int DetectVirtualAddressBits() {
#if defined(__aarch64__) && defined(__linux__)
  int bits = DetectFromProcMaps();
  if (bits > 0) {
    return std::min(bits, kMaxAddressBits);
  }
  // Fallback: mmap probe
  return std::min(DetectWithMmapProbe(), kMaxAddressBits);
#else
  return kMaxAddressBits;
#endif
}

}  // namespace

int EffectiveAddressBits() {
  ABSL_CONST_INIT static int bits;
  ABSL_CONST_INIT static absl::once_flag flag;

  absl::base_internal::LowLevelCallOnce(&flag, [&]() {
    bits = DetectVirtualAddressBits();
  });
  return bits;
}

}  // namespace tcmalloc_internal
}  // namespace tcmalloc
GOOGLE_MALLOC_SECTION_END
