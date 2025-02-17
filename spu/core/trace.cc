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

#include "spu/core/trace.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "yacl/base/exception.h"

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace spu {
namespace internal {

// global uuid, for multiple profiling context.
int64_t genActionUuid() {
  static std::atomic<int64_t> s_counter = 0;
  return ++s_counter;
}

}  // namespace internal

namespace {

#ifdef __linux__

[[maybe_unused]] std::string ReadProcSelfStatusByKey(const std::string& key) {
  std::string ret;
  std::ifstream self_status("/proc/self/status");
  std::string line;
  while (std::getline(self_status, line)) {
    std::vector<absl::string_view> fields =
        absl::StrSplit(line, absl::ByChar(':'));
    if (fields.size() == 2 && key == absl::StripAsciiWhitespace(fields[0])) {
      ret = absl::StripAsciiWhitespace(fields[1]);
    }
  }
  return ret;
}

[[maybe_unused]] float ReadVMxFromProcSelfStatus(const std::string& key) {
  const std::string str_usage = ReadProcSelfStatusByKey(key);
  std::vector<absl::string_view> fields =
      absl::StrSplit(str_usage, absl::ByChar(' '));
  if (fields.size() == 2) {
    size_t ret = 0;
    if (!absl::SimpleAtoi(fields[0], &ret)) {
      return -1;
    }
    return static_cast<float>(ret) / 1024 / 1024;
  }
  return -1;
}

[[maybe_unused]] float GetPeakMemUsage() {
  return ReadVMxFromProcSelfStatus("VmHWM");
}

[[maybe_unused]] float GetCurrentMemUsage() {
  return ReadVMxFromProcSelfStatus("VmRSS");
}

#elif defined(__APPLE__)

[[maybe_unused]] float GetCurrentMemUsage() {
  struct mach_task_basic_info t_info;
  mach_msg_type_number_t t_info_count = MACH_TASK_BASIC_INFO_COUNT;

  auto ret = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                       reinterpret_cast<task_info_t>(&t_info), &t_info_count);

  if (KERN_SUCCESS != ret || MACH_TASK_BASIC_INFO_COUNT != t_info_count) {
    return -1;
  }
  return static_cast<float>(t_info.resident_size) / 1024 / 1024;
}

[[maybe_unused]] float GetPeakMemUsage() {
  struct mach_task_basic_info t_info;
  mach_msg_type_number_t t_info_count = MACH_TASK_BASIC_INFO_COUNT;

  auto ret = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                       reinterpret_cast<task_info_t>(&t_info), &t_info_count);

  if (KERN_SUCCESS != ret || MACH_TASK_BASIC_INFO_COUNT != t_info_count) {
    return -1;
  }
  return static_cast<float>(t_info.resident_size_max) / 1024 / 1024;
}

#endif

[[maybe_unused]] std::string getIndentString(size_t indent) {
  constexpr size_t kMaxIndent = 30;

  static std::once_flag flag;
  static std::array<std::string, kMaxIndent> cache;
  std::call_once(flag, [&]() {
    for (size_t idx = 0; idx < kMaxIndent; idx++) {
      cache[idx] = std::string(idx * 2, ' ');
    }
  });

  return cache[std::min(indent, kMaxIndent - 1)];
}

// global variables.
static std::mutex g_tracer_map_mutex;
static std::unordered_map<std::string, std::shared_ptr<Tracer>> g_tracers;
static int64_t g_trace_flag;
static std::shared_ptr<spdlog::logger> g_trace_logger;

std::shared_ptr<spdlog::logger> getDefaultLogger() {
  static std::once_flag flag;

  std::call_once(flag, []() {
    auto console_sink =
        std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    g_trace_logger = std::make_shared<spdlog::logger>("TR", console_sink);
    // default
    // g_trace_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    g_trace_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] %v");
  });

  return g_trace_logger;
}

}  // namespace

void Tracer::logActionBegin(int64_t id, int64_t flag, const std::string& name,
                            const std::string& detail) {
  if ((flag & mask_ & TR_MODALL) == 0 || (mask_ & TR_LOGB) == 0) {
    // module is disabled or logging is disabled, ignore.
    return;
  }

  if (mask_ & TR_LOGM) {
    logger_->info("[B] [M{}] {}({})", GetPeakMemUsage(), name, detail);
  } else {
    logger_->info("[B] {}({})", name, detail);
  }
}

void Tracer::logActionEnd(int64_t id, int64_t flag, const std::string& name,
                          const std::string& detail) {
  if ((flag & mask_ & TR_MODALL) == 0 || (mask_ & TR_LOGE) == 0) {
    // module is disabled or logging is disabled, ignore.
    return;
  }

  if (mask_ & TR_LOGM) {
    logger_->info("[E] [M{}] {}({})", GetPeakMemUsage(), name, detail);
  } else {
    logger_->info("[E] {}({})", name, detail);
  }
}

void initTrace(int64_t tr_flag, std::shared_ptr<spdlog::logger> tr_logger) {
  g_trace_flag = tr_flag;

  if (tr_logger != nullptr) {
    g_trace_logger = tr_logger;
  }
}

std::shared_ptr<Tracer> getTracer(const std::string& name) {
  std::unique_lock lock(g_tracer_map_mutex);
  auto itr = g_tracers.find(name);
  if (itr != g_tracers.end()) {
    return itr->second;
  }

  auto tracer =
      std::make_shared<Tracer>(name, g_trace_flag, getDefaultLogger());
  g_tracers.emplace(name, tracer);
  return tracer;
}

void registerTracer(std::shared_ptr<Tracer> tracer) {
  std::unique_lock lock(g_tracer_map_mutex);
  g_tracers[tracer->name()] = tracer;
}

void MemProfilingGuard::enable(int i, std::string_view m, std::string_view n) {
  indent_ = i * 2;
  module_ = m;
  name_ = n;
  start_peak_ = GetPeakMemUsage();
  enable_ = true;
  SPDLOG_DEBUG("{}{}.{}: before peak {:.2f}GB, current {:.2f}GB",
               std::string(indent_, ' '), module_, name_, start_peak_,
               GetCurrentMemUsage());
}

MemProfilingGuard::~MemProfilingGuard() {
  if (!enable_) {
    return;
  }
  auto p = GetPeakMemUsage();
  auto increase = p - start_peak_;
  if (increase >= 0.01) {
    SPDLOG_DEBUG("{}{}.{}: peak {:.2f}GB, increase {:.2f}GB, current {:.2f}GB",
                 std::string(indent_, ' '), module_, name_, p, increase,
                 GetCurrentMemUsage());
  } else {
    SPDLOG_DEBUG("{}{}.{}: peak {:.2f}GB, current {:.2f}GB",
                 std::string(indent_, ' '), module_, name_, p,
                 GetCurrentMemUsage());
  }
}

}  // namespace spu
