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

#include "spu/device/executor.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Value.h"
#include "yacl/base/exception.h"

#include "spu/kernel/context.h"
#include "spu/kernel/value.h"

namespace spu::device {

const spu::Value &SymbolScope::lookupValue(mlir::Value key) const {
  std::unique_lock lk(mu_);
  auto itr = symbols_.find(key);

  if (itr != symbols_.end()) {
    return itr->second;
  }

  if (parent_ != nullptr) {
    return parent_->lookupValue(key);
  }

  // Somehow cannot find this value on stack, print a reasonable error
  SPDLOG_ERROR("Should not be here, symbol not found");
  YACL_THROW("TODO: add more details");
  // YACL_THROW("Try to get a non-exist value, defined at {} ",
  //            mlirObjectToString(*v.getDefiningOp()));
}

bool SymbolScope::hasValueUnsafe(mlir::Value key) const {
  auto itr = symbols_.find(key);

  if (itr != symbols_.end()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->hasValue(key);
  }

  return false;
}

bool SymbolScope::hasValues(absl::Span<mlir::Value const> keys) const {
  std::unique_lock lk(mu_);
  for (const auto &key : keys) {
    if (!hasValueUnsafe(key)) {
      return false;
    }
  }
  return true;
}

bool SymbolScope::hasValue(mlir::Value key) const {
  std::unique_lock lk(mu_);
  return hasValueUnsafe(key);
}

void SymbolScope::addValue(mlir::Value key, const spu::Value &val) {
  std::unique_lock lk(mu_);
  symbols_[key] = val;
}

void SymbolScope::addValue(mlir::Value key, spu::Value &&val) {
  std::unique_lock lk(mu_);
  symbols_[key] = std::move(val);
}

std::vector<spu::Value> runRegion(OpExecutor *executor,                //
                                  HalContext *hctx,                    //
                                  SymbolScope *parent_scope,           //
                                  mlir::Region &region,                //
                                  absl::Span<spu::Value const> params, //
                                  const ExecutionOptions &opts) {
  YACL_ENFORCE(region.getNumArguments() == params.size(),
               "region requires {} arguments while got number of params {}",
               region.getRegionNumber(), params.size());

  // create a new scope for this region.
  SymbolScope sscope(parent_scope);

  // inject the parameters to region's symbol table.
  for (const auto &blkarg : region.getArguments()) {
    sscope.addValue(blkarg, params[blkarg.getArgNumber()]);
  }

  YACL_ENFORCE(region.hasOneBlock());
  if (opts.do_parallel) {
    return runBlockParallel(executor, hctx, &sscope, region.front(), params,
                            opts);
  } else {
    return runBlock(executor, hctx, &sscope, region.front(), params, opts);
  }
}

std::vector<spu::Value> runBlock(OpExecutor *executor, HalContext *hctx,
                                 SymbolScope *symbols, mlir::Block &block,
                                 absl::Span<spu::Value const> params,
                                 const ExecutionOptions &opts) {
  for (auto &op : block.without_terminator()) {
    executor->runKernel(hctx, symbols, op, opts);
  }

  if (auto *termOp = block.getTerminator()) {
    // TODO: enforce ReturnLike
    std::vector<spu::Value> results;
    results.reserve(termOp->getNumOperands());
    for (const auto operand : termOp->getOperands()) {
      results.emplace_back(symbols->lookupValue(operand));
    }
    return results;
  }

  // No terminator
  YACL_THROW("Should not be here");
}

struct SymbolTableEvent {
  std::condition_variable cv;
  std::mutex mutex;

  std::atomic<int64_t> count = 0;
};

class OpExecTask final {
  std::unique_ptr<HalContext> hctx_ = nullptr;
  // here we assume executor is thread-safe (stateless)
  OpExecutor *executor_ = nullptr;
  SymbolScope *sscope_ = nullptr;
  mlir::Operation *op_ = nullptr;
  SymbolTableEvent *event_ = nullptr;

public:
  OpExecTask() = default;
  explicit OpExecTask(std::unique_ptr<HalContext> hctx, OpExecutor *executor,
                      SymbolScope *sscope, mlir::Operation *op,
                      SymbolTableEvent *event)
      : hctx_(std::move(hctx)), executor_(executor), sscope_(sscope), op_(op),
        event_(event) {}

  bool ready() {
    std::vector<mlir::Value> operands;
    for (const auto &operand : op_->getOperands()) {
      operands.push_back(operand);
    }
    return sscope_->hasValues(operands);
  }

  void run() {
    // wait for this operation ready.
    {
      using namespace std::chrono_literals;
      std::unique_lock lk(event_->mutex);
      event_->cv.wait(lk, [this]() { return this->ready(); });
    }

    executor_->runKernel(hctx_.get(), sscope_, *op_);
    {
      std::unique_lock lk(event_->mutex);
      event_->cv.notify_all();
    }
  }
};

std::vector<spu::Value> runBlockParallel(OpExecutor *executor, HalContext *hctx,
                                         SymbolScope *symbols,
                                         mlir::Block &block,
                                         absl::Span<spu::Value const> params,
                                         const ExecutionOptions &opts) {
  // The strategy is try to execute operations without dependency as much as
  // possible. For each (maybe) parallel operation we allocate a new hal
  // context.
  SymbolTableEvent st_event;
  std::vector<OpExecTask> tasks;
  std::vector<std::future<void>> futures;
  for (auto &op : block.without_terminator()) {
    tasks.emplace_back(hctx->fork(), executor, symbols, &op, &st_event);
  }

  futures.reserve(tasks.size());
  for (auto &task : tasks) {
    futures.push_back(std::async(std::launch::async, &OpExecTask::run, &task));
  }

  for (auto &future : futures) {
    future.get();
  }

  if (auto *termOp = block.getTerminator()) {
    // TODO: enforce ReturnLike
    std::vector<spu::Value> results;
    results.reserve(termOp->getNumOperands());
    for (const auto operand : termOp->getOperands()) {
      results.emplace_back(symbols->lookupValue(operand));
    }
    return results;
  }

  // No terminator
  YACL_THROW("Should not be here");
}

} // namespace spu::device
