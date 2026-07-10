// Copyright 2026 Google LLC.
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

#ifndef THIRD_PARTY_ODML_LITERT_LITERT_C_INTERNAL_LITERT_ABI_HEADER_H_
#define THIRD_PARTY_ODML_LITERT_LITERT_C_INTERNAL_LITERT_ABI_HEADER_H_

#include <stddef.h>
#include <stdint.h>

#include "litert/c/litert_common.h"

#ifdef __cplusplus
#include <type_traits>
extern "C" {
#endif

/**
 * @brief Header containing ABI version and size metadata.
 * Must be the first member of any ABI-stable struct (offset 0).
 */
typedef struct LiteRtAbiHeader {
  // The physical size of the parent structure (including this header).
  uint16_t struct_size;

  // Bumped when introducing breaking changes or a completely new struct layout.
  uint16_t major_version;

  // Bumped when adding new methods (appended) or deprecating existing ones.
  uint16_t minor_version;

  // Reserved for future use (e.g., flags or compatibility metadata).
  uint16_t reserved;
} LiteRtAbiHeader;

LITERT_ABI_STATIC_ASSERT(sizeof(LiteRtAbiHeader) == 8,
                         "LiteRtAbiHeader size mismatch");

/**
 * @brief Checks ABI version compatibility. Evaluates header exactly once.
 */
static inline int LiteRtIsAbiCompatible(const LiteRtAbiHeader* header,
                                        uint16_t req_major,
                                        uint16_t req_minor) {
  return header &&
         header->major_version == req_major &&
         header->minor_version >= req_minor;
}

/**
 * @brief Checks if a struct member at member_end_offset is within bounds.
 */
static inline int LiteRtIsAbiMemberPresent(const LiteRtAbiHeader* header,
                                           uint16_t req_major,
                                           size_t member_end_offset) {
  return header &&
         header->major_version == req_major &&
         header->struct_size >= member_end_offset;
}

#define LITERT_ABI_IS_COMPATIBLE(instance_ptr, req_major, req_minor) \
  LiteRtIsAbiCompatible(                                             \
      (const LiteRtAbiHeader*)(instance_ptr), (req_major), (req_minor))

#if defined(__cplusplus)
#define LITERT_ABI_OFFSET(ptr, member) \
  offsetof(std::remove_pointer_t<decltype(ptr)>, member)
#elif defined(__GNUC__) || defined(__clang__)
#define LITERT_ABI_OFFSET(ptr, member) \
  offsetof(__typeof__(*(ptr)), member)
#else
#define LITERT_ABI_OFFSET(ptr, member) \
  offsetof(typeof(*(ptr)), member)
#endif

#define LITERT_ABI_HAS_MEMBER(instance_ptr, req_major, member)        \
  LiteRtIsAbiMemberPresent(                                           \
      (const LiteRtAbiHeader*)(instance_ptr), (req_major),            \
      LITERT_ABI_OFFSET(instance_ptr, member) + sizeof((instance_ptr)->member))

#if defined(__cplusplus)
#define LITERT_ABI_HAS_API(instance_ptr, req_major, api_member)             \
  ([&](auto* _litert_p) {                                                   \
    return LITERT_ABI_HAS_MEMBER(_litert_p, req_major, api_member) &&       \
           ((_litert_p)->api_member != NULL);                               \
  })(instance_ptr)
#elif defined(__GNUC__) || defined(__clang__)
#define LITERT_ABI_HAS_API(instance_ptr, req_major, api_member) \
  __extension__({                                               \
    __typeof__(instance_ptr) _litert_p = (instance_ptr);       \
    LITERT_ABI_HAS_MEMBER(_litert_p, req_major, api_member) &&  \
        ((_litert_p)->api_member != NULL);                      \
  })
#else
#define LITERT_ABI_HAS_API(instance_ptr, req_major, api_member) \
  (LITERT_ABI_HAS_MEMBER(instance_ptr, req_major, api_member) && \
   ((instance_ptr)->api_member != NULL))
#endif

#ifdef __cplusplus
}
#endif

#endif  // THIRD_PARTY_ODML_LITERT_LITERT_C_INTERNAL_LITERT_ABI_HEADER_H_
