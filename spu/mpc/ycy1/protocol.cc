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

#include "spu/mpc/ycy1/arithmetic.h"
#include "spu/mpc/ycy1/type.h"
#include "spu/mpc/common/abprotocol.h"
#include "spu/mpc/common/prg_state.h"
#include "spu/mpc/common/pub2k.h"
#include "spu/mpc/object.h"
#include "spu/mpc/util/communicator.h"

namespace spu::mpc {

std::unique_ptr<Object> makeYcy1Protocol(
    const RuntimeConfig& conf,
    const std::shared_ptr<yacl::link::Context>& lctx) {
  ycy1::registerTypes();

  auto obj = std::make_unique<Object>("YCY1");

  obj->addState<Z2kState>(conf.field());

  // add communicator
  obj->addState<Communicator>(lctx);

  // register random states & kernels.
  obj->addState<PrgState>(lctx);

  // register public kernels.
  regPub2kKernels(obj.get());

  // register api kernels
  regABKernels(obj.get());

  // register arithmetic & binary kernels
  obj->regKernel<ycy1::AddAP>();
  obj->regKernel<ycy1::AddAA>();
  return obj;
}

}  // namespace spu::mpc
