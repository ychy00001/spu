// Copyright 2021 Ant Group Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "spu/mpc/ycy1/io.h"

#include <functional>

#include "gtest/gtest.h"

#include "spu/mpc/util/ring_ops.h"

namespace spu::mpc::ycy1 {

using CreateIoFn =
    std::function<std::unique_ptr<IoInterface>(FieldType field, size_t npc)>;

// This test fixture defines the standard test cases for the io interface.
//
// Protocol implementers should instantiate this test when a new protocol is
// added. see
// [here](https://google.github.io/googletest/advanced.html#creating-value-parameterized-abstract-tests)
// for more details.
class IoTest : public ::testing::TestWithParam<
                   std::tuple<CreateIoFn, size_t, FieldType>> {};

const int32_t kNumel = 2;

ArrayRef rand(FieldType field, size_t size) {
  ArrayRef res(makeType<RingTy>(field), size);
  return res;
}

TEST_P(IoTest, MakePublicAndReconstruct) {
  const auto create_io = std::get<0>(GetParam());
  const size_t npc = std::get<1>(GetParam());
  const FieldType field = std::get<2>(GetParam());

  auto io = create_io(field, npc);

  // 构建原始数据
  auto mem = std::make_shared<yacl::Buffer>(kNumel * 4);
  ArrayRef raw(mem, makeType<RingTy>(field), kNumel, 1, 0);
  for (int32_t i = 0; i < kNumel; i++) {
    // 数据是 5 和 6
    raw.at<int32_t>(i) = i + 5;
  }

  // 模拟share分享后数据
  auto mem_share = std::make_shared<yacl::Buffer>(kNumel * 4);
  ArrayRef raw_share(mem_share, makeType<RingTy>(field), kNumel, 1, 0);
  for (int32_t i = 0; i < kNumel; i++) {
    // 数据是 5 和 6
    raw_share.at<int32_t>(i) = i + 15;
  }

  auto shares = io->toShares(raw, VIS_SECRET);
  for (size_t i = 0; i < shares.size(); i++) {
    for (int32_t j = 0; j < kNumel; j++) {
      // 分享的时候+10 数据初始化+5
      EXPECT_EQ(shares[i].at<int32_t>(j), j + 15);
    }
    // 比较分享后数据 aby3下是无法比较的，因为数据长度都做了变化，当前模拟的协议可以进行比较
    EXPECT_TRUE(ring_all_equal(raw_share, shares[i]));
  }
  auto result = io->fromShares(shares);

  for (int32_t i = 0; i < kNumel; i++) {
    EXPECT_EQ((raw.at<int32_t>(i) + 10)*3, result.at<int32_t>(i));
  }
}

// TEST_P(DISABLE_IoTest, MakeSecretAndReconstruct) {
//   const auto create_io = std::get<0>(GetParam());
//   const size_t npc = std::get<1>(GetParam());
//   const FieldType field = std::get<2>(GetParam());
//
//   auto io = create_io(field, npc);
//
//   auto raw = ring_rand(field, kNumel);
//   auto shares = io->toShares(raw, VIS_SECRET);
//   auto result = io->fromShares(shares);
//
//   EXPECT_TRUE(ring_all_equal(raw, result));
// }
//
// TEST_P(DISABLE_IoTest, MakeSecretAndReconstructWithInvalidOwnerRank) {
//   const auto create_io = std::get<0>(GetParam());
//   const size_t npc = std::get<1>(GetParam());
//   const FieldType field = std::get<2>(GetParam());
//
//   auto io = create_io(field, npc);
//
//   auto raw = ring_rand(field, kNumel);
//   auto shares = io->toShares(raw, VIS_SECRET, -1);
//   auto result = io->fromShares(shares);
//
//   EXPECT_TRUE(ring_all_equal(raw, result));
// }
//
// TEST_P(DISABLE_IoTest, MakeSecretAndReconstructWithValidOwnerRank) {
//   const auto create_io = std::get<0>(GetParam());
//   const size_t npc = std::get<1>(GetParam());
//   const FieldType field = std::get<2>(GetParam());
//
//   auto io = create_io(field, npc);
//
//   auto raw = ring_rand(field, kNumel);
//   auto shares = io->toShares(raw, VIS_SECRET, 0);
//   auto result = io->fromShares(shares);
//
//   EXPECT_TRUE(ring_all_equal(raw, result));
// }

INSTANTIATE_TEST_SUITE_P(
    Ycy1IoTest, IoTest,
    testing::Combine(testing::Values(makeYcy1Io),  //
                     testing::Values(3),           //
                     testing::Values(FieldType::FM32)),
    [](const testing::TestParamInfo<IoTest::ParamType>& p) {
      return fmt::format("{}x{}", std::get<1>(p.param), std::get<2>(p.param));
    });

}  // namespace spu::mpc::ycy1
