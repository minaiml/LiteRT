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

#include "litert/core/version.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>
#include "litert/c/internal/litert_abi_header.h"
#include "litert/c/litert_common.h"

namespace litert::internal {
namespace {

TEST(VersionTest, IsSameVersion) {
  EXPECT_TRUE(IsSameVersion({1, 2, 3}, {1, 2, 3}));
  EXPECT_FALSE(IsSameVersion({1, 2, 3}, {1, 2, 4}));
  EXPECT_FALSE(IsSameVersion({1, 2, 3}, {1, 3, 3}));
  EXPECT_FALSE(IsSameVersion({1, 2, 3}, {2, 2, 3}));
}

TEST(VersionTest, IsCompatibleVersionMajorMismatch) {
  // Major mismatch is always incompatible.
  EXPECT_FALSE(IsCompatibleVersion({1, 0, 0}, {2, 0, 0}));
  EXPECT_FALSE(IsCompatibleVersion({2, 0, 0}, {1, 0, 0}));
  EXPECT_FALSE(IsCompatibleVersion({0, 1, 0}, {1, 1, 0}));
}

TEST(VersionTest, IsCompatibleVersionPreOneZero) {
  // For major == 0, minor must match exactly.
  EXPECT_TRUE(IsCompatibleVersion({0, 1, 0}, {0, 1, 0}));
  EXPECT_TRUE(
      IsCompatibleVersion({0, 1, 0}, {0, 1, 1}));  // Patch mismatch allowed
  EXPECT_TRUE(
      IsCompatibleVersion({0, 1, 1}, {0, 1, 0}));  // Patch mismatch allowed
  EXPECT_FALSE(IsCompatibleVersion({0, 1, 0}, {0, 2, 0}));
  EXPECT_FALSE(IsCompatibleVersion({0, 2, 0}, {0, 1, 0}));
}

TEST(VersionTest, IsCompatibleVersionPostOneZero) {
  // For major >= 1 under Option B, both backward and forward minor versions
  // within the same major version are compatible.
  // Format: IsCompatibleVersion(vendor, runtime)

  // Same version is compatible.
  EXPECT_TRUE(IsCompatibleVersion({1, 1, 0}, {1, 1, 0}));

  // Runtime is newer -> Compatible (backward compatibility).
  EXPECT_TRUE(IsCompatibleVersion({1, 1, 0}, {1, 2, 0}));
  EXPECT_TRUE(IsCompatibleVersion({1, 1, 5}, {1, 2, 0}));

  // Runtime is older -> Compatible (Option B forward compatibility).
  EXPECT_TRUE(IsCompatibleVersion({1, 2, 0}, {1, 1, 0}));

  // Patch version is ignored.
  EXPECT_TRUE(IsCompatibleVersion({1, 1, 1}, {1, 1, 2}));
  EXPECT_TRUE(IsCompatibleVersion({1, 1, 2}, {1, 1, 1}));
}

TEST(VersionTest, IsCompatibleVersionAsRuntime) {
  // Test compatibility against current runtime version defined in headers.
  LiteRtApiVersion current_runtime = {LITERT_API_VERSION_MAJOR,
                                      LITERT_API_VERSION_MINOR,
                                      LITERT_API_VERSION_PATCH};

  EXPECT_TRUE(IsCompatibleVersionAsRuntime(current_runtime));

  // Create a compatible version by changing only the patch version.
  LiteRtApiVersion compatible_version = current_runtime;
  compatible_version.patch += 1;
  EXPECT_TRUE(IsCompatibleVersionAsRuntime(compatible_version));

  // Create an incompatible version by changing the major version.
  LiteRtApiVersion incompatible_version_major = current_runtime;
  incompatible_version_major.major += 1;
  EXPECT_FALSE(IsCompatibleVersionAsRuntime(incompatible_version_major));
}

struct DummyAbiHeaderInterface {
  LiteRtAbiHeader abi_header;
  int (*func1)();
  int (*func2)();
  int (*func3)();
};

TEST(VersionTest, AbiHasApiAndIsCompatible) {
  // Scenario 1: Older interface that only supports func1 and func2
  // (major=1, minor=0).
  size_t old_size = offsetof(DummyAbiHeaderInterface, func2) +
                    sizeof(DummyAbiHeaderInterface::func2);

  std::vector<uint8_t> buffer(sizeof(DummyAbiHeaderInterface), 0);
  auto* old_interface =
      reinterpret_cast<DummyAbiHeaderInterface*>(buffer.data());
  old_interface->abi_header.struct_size = old_size;
  old_interface->abi_header.major_version = 1;
  old_interface->abi_header.minor_version = 0;
  old_interface->func1 = []() { return 1; };
  old_interface->func2 = []() { return 2; };
  // Even if func3 is non-null in memory, struct_size excludes it
  old_interface->func3 = []() { return 3; };

  EXPECT_TRUE(LITERT_ABI_IS_COMPATIBLE(old_interface, 1, 0));
  EXPECT_FALSE(LITERT_ABI_IS_COMPATIBLE(old_interface, 1, 1));
  EXPECT_FALSE(LITERT_ABI_IS_COMPATIBLE(old_interface, 2, 0));

  EXPECT_TRUE(LITERT_ABI_HAS_API(old_interface, 1, func1));
  EXPECT_TRUE(LITERT_ABI_HAS_API(old_interface, 1, func2));
  EXPECT_FALSE(LITERT_ABI_HAS_API(old_interface, 1, func3));
  EXPECT_FALSE(LITERT_ABI_HAS_API(old_interface, 2, func1));

  // Scenario 2: Newer interface that supports all functions (major=1, minor=1).
  DummyAbiHeaderInterface new_interface;
  new_interface.abi_header.struct_size = sizeof(DummyAbiHeaderInterface);
  new_interface.abi_header.major_version = 1;
  new_interface.abi_header.minor_version = 1;
  new_interface.func1 = []() { return 1; };
  new_interface.func2 = []() { return 2; };
  new_interface.func3 = []() { return 3; };

  EXPECT_TRUE(LITERT_ABI_IS_COMPATIBLE(&new_interface, 1, 0));
  EXPECT_TRUE(LITERT_ABI_IS_COMPATIBLE(&new_interface, 1, 1));
  EXPECT_TRUE(LITERT_ABI_HAS_API(&new_interface, 1, func1));
  EXPECT_TRUE(LITERT_ABI_HAS_API(&new_interface, 1, func2));
  EXPECT_TRUE(LITERT_ABI_HAS_API(&new_interface, 1, func3));

  // Scenario 3: Partial member size
  DummyAbiHeaderInterface partial_interface;
  partial_interface.abi_header.struct_size =
      offsetof(DummyAbiHeaderInterface, func2) + 1;
  partial_interface.abi_header.major_version = 1;
  partial_interface.abi_header.minor_version = 0;
  partial_interface.func1 = []() { return 1; };
  partial_interface.func2 = []() { return 2; };
  EXPECT_TRUE(LITERT_ABI_HAS_API(&partial_interface, 1, func1));
  EXPECT_FALSE(LITERT_ABI_HAS_API(&partial_interface, 1, func2));

  // Scenario 4: Null pointer
  DummyAbiHeaderInterface* null_interface = nullptr;
  EXPECT_FALSE(LITERT_ABI_IS_COMPATIBLE(null_interface, 1, 0));
  EXPECT_FALSE(LITERT_ABI_HAS_API(null_interface, 1, func1));
}

TEST(VersionTest, NegotiateInterfaceOptionBValidMajor) {
  DummyAbiHeaderInterface v1_table = {
      .abi_header =
          {
              .struct_size = sizeof(DummyAbiHeaderInterface),
              .major_version = 1,
              .minor_version = 2,
              .reserved = 0,
          },
      .func1 = []() { return 1; },
      .func2 = []() { return 2; },
      .func3 = []() { return 3; },
  };

  auto mock_query = [&](int id, LiteRtApiVersion ver,
                        LiteRtInterface* out) -> LiteRtStatus {
    if (ver.major == 1) {
      *out = &v1_table;
      return kLiteRtStatusOk;
    }
    return kLiteRtStatusErrorUnsupported;
  };

  LiteRtInterface resolved = nullptr;
  LiteRtApiVersion negotiated = {0, 0, 0};

  // Match major 1
  EXPECT_EQ(
      NegotiateInterface(mock_query, 0, {1, 0, 0},
                         /*expected_abi_major=*/1, &resolved, &negotiated),
      kLiteRtStatusOk);
  EXPECT_EQ(resolved, &v1_table);
  EXPECT_EQ(negotiated.major, 1);
  EXPECT_EQ(negotiated.minor, 2);
  EXPECT_TRUE(IsCompatibleAbiV1Version(negotiated));

  // Querying with major 2 returns Unsupported from V1 plugin
  LiteRtInterface resolved_v2 = nullptr;
  EXPECT_EQ(NegotiateInterface(mock_query, 0, {2, 0, 0},
                               /*expected_abi_major=*/2, &resolved_v2),
            kLiteRtStatusErrorUnsupported);
  EXPECT_EQ(resolved_v2, nullptr);

  // Also test LITERT_ABI_HAS_API on resolved table
  const auto* resolved_table =
      static_cast<const DummyAbiHeaderInterface*>(resolved);
  EXPECT_TRUE(LITERT_ABI_HAS_API(resolved_table, 1, func1));
  EXPECT_TRUE(LITERT_ABI_HAS_API(resolved_table, 1, func2));
  EXPECT_TRUE(LITERT_ABI_HAS_API(resolved_table, 1, func3));
}

TEST(VersionTest, NegotiateInterfaceOptionBMajorMismatch) {
  DummyAbiHeaderInterface v2_table = {
      .abi_header =
          {
              .struct_size = sizeof(DummyAbiHeaderInterface),
              .major_version = 2,
              .minor_version = 0,
              .reserved = 0,
          },
  };

  auto mock_query = [&](int id, LiteRtApiVersion ver,
                        LiteRtInterface* out) -> LiteRtStatus {
    *out = &v2_table;
    return kLiteRtStatusOk;
  };

  LiteRtInterface resolved = nullptr;
  LiteRtApiVersion negotiated = {0, 0, 0};

  // Runtime expects major 1, but table returned has major 2 -> WrongVersion
  EXPECT_EQ(
      NegotiateInterface(mock_query, 0, {1, 0, 0},
                         /*expected_abi_major=*/1, &resolved, &negotiated),
      kLiteRtStatusErrorWrongVersion);
  EXPECT_EQ(resolved, nullptr);
}

TEST(VersionTest, NegotiateInterfaceRejectsNullOutputOnSuccess) {
  // Buggy query function returning Ok but leaving *out as nullptr
  auto buggy_query = [&](int id, LiteRtApiVersion ver,
                         LiteRtInterface* out) -> LiteRtStatus {
    *out = nullptr;
    return kLiteRtStatusOk;
  };

  LiteRtInterface resolved = nullptr;
  LiteRtApiVersion negotiated = {0, 0, 0};

  EXPECT_EQ(
      NegotiateInterface(buggy_query, 0, {1, 0, 0},
                         /*expected_abi_major=*/1, &resolved, &negotiated),
      kLiteRtStatusErrorRuntimeFailure);
}

}  // namespace
}  // namespace litert::internal
