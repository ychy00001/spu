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

#include "spu/mpc/io_interface.h"

namespace spu::mpc::ycy1 {

class Ycy1Io final : public BaseIo {
 public:
  using BaseIo::BaseIo;

  std::vector<ArrayRef> toShares(const ArrayRef& raw, Visibility vis,
                                 int owner_rank) const override;

  ArrayRef fromShares(const std::vector<ArrayRef>& shares) const override;

  std::vector<ArrayRef> makeBitSecret(const ArrayRef& raw) const override;
  bool hasBitSecretSupport() const override { return true; }
};

std::unique_ptr<Ycy1Io> makeYcy1Io(FieldType type, size_t npc);
}  // namespace spu::mpc::aby3
