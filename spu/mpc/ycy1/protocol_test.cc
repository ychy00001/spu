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

#include "spu/mpc/ycy1/protocol.h"

#include <functional>

#include "gtest/gtest.h"
#include "yacl/link/link.h"

#include "spu/core/shape_util.h"
#include "spu/mpc/api.h"
#include "spu/mpc/object.h"
#include "spu/mpc/ycy1/value.h"
#include "spu/mpc/util/communicator.h"
#include "spu/mpc/common/abprotocol.h"
#include "spu/mpc/common/pub2k.h"
#include "spu/mpc/util/ring_ops.h"
#include "spu/mpc/util/simulate.h"

namespace spu::mpc::test {
namespace {

constexpr int64_t kNumel = 1000;

/**
 * abprotocol测试用于测试验证编写协议是否正确
 */
//===============abprotocol_test.h相关内容================
using CreateObjectFn = std::function<std::unique_ptr<Object>(
    const RuntimeConfig& conf,
    const std::shared_ptr<yacl::link::Context>& lctx)>;

class ArithmeticTest : public ::testing::TestWithParam<
                           std::tuple<CreateObjectFn, RuntimeConfig, size_t>> {
};

bool verifyCost(Kernel* kernel, std::string_view name, FieldType field,
                size_t numel, size_t npc, const Communicator::Stats& cost) {
  if (kernel->kind() == Kernel::Kind::kDynamic) {
    return true;
  }

  auto comm = kernel->comm();
  auto latency = kernel->latency();

  bool succeed = true;
  constexpr size_t kBitsPerBytes = 8;
  const auto expectedComm = comm->eval(field, npc) * numel;
  const auto realComm = cost.comm * kBitsPerBytes;

  float diff;
  if (expectedComm == 0) {
    diff = realComm;
  } else {
    diff = (realComm - expectedComm) / expectedComm;
  }
  if (realComm < expectedComm || diff > kernel->getCommTolerance()) {
    fmt::print("Failed: {} comm mismatch, expected={}, got={}\n", name,
               expectedComm, realComm);
    succeed = false;
  }

  if (latency->eval(field, npc) != cost.latency) {
    fmt::print("Failed: {} latency mismatch, expected={}, got={}\n", name,
               latency->eval(field, npc), cost.latency);
    succeed = false;
  }

  return succeed;
}
//===============api_test.h相关内容================
using CreateComputeFn = std::function<std::unique_ptr<Object>(
    const RuntimeConfig& conf,
    const std::shared_ptr<yacl::link::Context>& lctx)>;

class ApiTest : public ::testing::TestWithParam<
                    std::tuple<CreateComputeFn, RuntimeConfig, size_t>> {};

RuntimeConfig makeConfig(FieldType field) {
  RuntimeConfig conf;
  conf.set_field(field);
  return conf;
}

}  // namespace

//=====================abprotocol_test.cc================

ArrayRef makeAYcyShare(int32_t val) {
  int64_t num = 1;
  FieldType field = FieldType::FM32;
  auto mem_share = std::make_shared<yacl::Buffer>(4 * num);
  ArrayRef raw_share(mem_share, makeType<RingTy>(field), num, 1, 0);
  for (int32_t i = 0; i < num; i++) {
    // 数据是 5 和 6
    raw_share.at<int32_t>(i) = val + 10;
  }
  return ycy1::makeAYcyTyShare(raw_share, field, 1);
}

ArrayRef makeAYcyResult(ArrayRef a, ArrayRef b) {
  FieldType field = FieldType::FM32;
  ArrayRef out(makeType<Pub2kTy>(field), a.numel());
  DISPATCH_ALL_FIELDS(field, "_", [&]() {
    auto _out = ArrayView<ring2k_t>(out);
    auto a_view = ArrayView<ring2k_t>(a);
    auto b_view = ArrayView<ring2k_t>(b);
    for (auto idx = 0; idx < a_view.numel(); idx++) {
      _out[idx] = a_view[idx] + b_view[idx];
    }
  });
  return out;
}

TEST_P(ArithmeticTest, addAA) {
  const auto factory = std::get<0>(GetParam());
  const RuntimeConfig& conf = std::get<1>(GetParam());
  const size_t npc = std::get<2>(GetParam());

  util::simulate(npc, [&](std::shared_ptr<yacl::link::Context> lctx) {
    auto obj = factory(conf, lctx);

    /* GIVEN */
    auto p0 = 5;
    auto p1 = 6;
    auto a0 = makeAYcyShare(p0);
    auto a1 = makeAYcyShare(p1);
    auto result = makeAYcyResult(a0, a1);
    /* WHEN */
    auto prev = obj->getState<Communicator>()->getStats();
    auto tmp = add_aa(obj.get(), a0, a1);
    auto cost = obj->getState<Communicator>()->getStats() - prev;

    /* THEN */
    EXPECT_TRUE(ring_all_equal(tmp, result));
    EXPECT_TRUE(verifyCost(obj->getKernel("add_aa"), "add_aa", conf.field(),
                           kNumel, npc, cost));
  });
}

/**
 * api_test用于验证对接hal 硬件抽象层-Hardware Abstraction Layer
 * (mlir提供)下发执行流程的接口
 */
//=====================api_test.cc================
TEST_P(ApiTest, add_ss) {
  const auto factory = std::get<0>(GetParam());
  const RuntimeConfig& conf = std::get<1>(GetParam());
  const size_t npc = std::get<2>(GetParam());

  util::simulate(npc, [&](std::shared_ptr<yacl::link::Context> lctx) {
    auto obj = factory(conf, lctx);

    /* GIVEN */
    auto p0 = 5;
    auto p1 = 6;
    auto a0 = makeAYcyShare(p0);
    auto a1 = makeAYcyShare(p1);
    auto result = makeAYcyResult(a0, a1);

    /* WHEN */
    auto tmp = add_ss(obj.get(), a0, a1);

    /* THEN */
    EXPECT_TRUE(ring_all_equal(tmp, result));
  });
}

//=======================测试内容初始化====================
INSTANTIATE_TEST_SUITE_P(
    Ycy1, ApiTest,
    testing::Combine(testing::Values(makeYcy1Protocol),             //
                     testing::Values(makeConfig(FieldType::FM32)),  //
                     testing::Values(3)),                           //
    [](const testing::TestParamInfo<ApiTest::ParamType>& p) {
      return fmt::format("{}x{}", std::get<1>(p.param).field(),
                         std::get<2>(p.param));
    });

INSTANTIATE_TEST_SUITE_P(
    Ycy1, ArithmeticTest,
    testing::Combine(testing::Values(makeYcy1Protocol),             //
                     testing::Values(makeConfig(FieldType::FM32)),  //
                     testing::Values(3)),                           //
    [](const testing::TestParamInfo<ArithmeticTest::ParamType>& p) {
      return fmt::format("{}x{}", std::get<1>(p.param).field(),
                         std::get<2>(p.param));
    });

}  // namespace spu::mpc::test
