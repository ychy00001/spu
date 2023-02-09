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

#pragma once

#include "spu/core/array_ref.h"
#include "spu/mpc/kernel.h"

namespace spu::mpc::ycy1 {

using util::CExpr;
using util::Const;
using util::K;
using util::Log;
using util::N;

////////////////////////////////////////////////////////////////////
// add family
////////////////////////////////////////////////////////////////////
class AddAP : public BinaryKernel {
 public:
  static constexpr char kBindName[] = "add_ap";

  CExpr latency() const override { return Const(0); }

  CExpr comm() const override { return Const(0); }

  ArrayRef proc(KernelEvalContext* ctx, const ArrayRef& lhs,
                const ArrayRef& rhs) const override;
};

class AddAA : public BinaryKernel {
 public:
  static constexpr char kBindName[] = "add_aa";

  CExpr latency() const override { return Const(0); }

  CExpr comm() const override { return Const(0); }

  ArrayRef proc(KernelEvalContext* ctx, const ArrayRef& lhs,
                const ArrayRef& rhs) const override;
};

}  // namespace spu::mpc::ycy1
