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

#include "spu/mpc/ycy1/arithmetic.h"

#include "spu/mpc/ycy1/type.h"
#include "spu/mpc/ycy1/value.h"
#include "spu/core/array_ref.h"

#include "spu/core/trace.h"
#include "spu/mpc/common/abprotocol.h"
#include "spu/mpc/util/communicator.h"
#include "spu/mpc/common/pub2k.h"
#include "spu/mpc/util/ring_ops.h"


namespace spu::mpc::ycy1 {

////////////////////////////////////////////////////////////////////
// add family
////////////////////////////////////////////////////////////////////
ArrayRef AddAP::proc(KernelEvalContext* ctx, const ArrayRef& lhs,
                     const ArrayRef& rhs) const {
  YACL_ENFORCE(false, "暂不支持名密文混合相加");
  SPU_TRACE_MPC_LEAF(ctx, lhs, rhs);

  auto* comm = ctx->caller()->getState<Communicator>();
  const auto* lhs_ty = lhs.eltype().as<AShrTy>();
  const auto* rhs_ty = rhs.eltype().as<Pub2kTy>();

  YACL_ENFORCE(lhs_ty->field() == rhs_ty->field());
  const auto field = lhs_ty->field();

  auto rank = comm->getRank();

  return DISPATCH_ALL_FIELDS(field, "_", [&]() {
    using U = ring2k_t;

    ArrayRef out(makeType<AShrTy>(field), lhs.numel());

    auto _lhs = ArrayView<std::array<U, 2>>(lhs);
    auto _rhs = ArrayView<U>(rhs);
    auto _out = ArrayView<std::array<U, 2>>(out);

    pforeach(0, lhs.numel(), [&](int64_t idx) {
      _out[idx][0] = _lhs[idx][0];
      _out[idx][1] = _lhs[idx][1];
      if (rank == 0) _out[idx][1] += _rhs[idx];
      if (rank == 1) _out[idx][0] += _rhs[idx];
    });
    return out;
  });
}

/**
 * 两个密文相加
 */
ArrayRef AddAA::proc(KernelEvalContext* ctx, const ArrayRef& lhs,
                     const ArrayRef& rhs) const {
  SPU_TRACE_MPC_LEAF(ctx, lhs, rhs);

  const auto* lhs_ty = lhs.eltype().as<AYcyTy>();
  const auto* rhs_ty = rhs.eltype().as<AYcyTy>();

  YACL_ENFORCE(lhs_ty->field() == rhs_ty->field());
  const auto field = lhs_ty->field();

  return DISPATCH_ALL_FIELDS(field, "_", [&]() {
    using U = ring2k_t;

    ArrayRef out(makeType<AShrTy>(field), lhs.numel());

    auto _lhs = ArrayView<U>(lhs);
    auto _rhs = ArrayView<U>(rhs);
    auto _out = ArrayView<U>(out);

    pforeach(0, lhs.numel(),
             [&](int64_t idx) { _out[idx] = _lhs[idx] + _rhs[idx]; });
    return out;
  });
}

}  // namespace spu::mpc::ycy1
