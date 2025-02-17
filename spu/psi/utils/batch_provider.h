// Copyright 2022 Ant Group Co., Ltd.
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

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "spu/psi/io/io.h"
#include "spu/psi/utils/csv_header_analyzer.h"

namespace spu::psi {

/// Interface which produce batch of strings.
class IBatchProvider {
 public:
  virtual ~IBatchProvider() = default;

  // Read at most `batch_size` items and return them. An empty returned vector
  // is treated as the end of stream.
  virtual std::vector<std::string> ReadNextBatch(size_t batch_size) = 0;
};

class MemoryBatchProvider : public IBatchProvider {
 public:
  explicit MemoryBatchProvider(const std::vector<std::string>& items)
      : items_(items) {}

  std::vector<std::string> ReadNextBatch(size_t batch_size) override;

  const std::vector<std::string>& items() const;

 private:
  const std::vector<std::string>& items_;
  size_t cursor_index_ = 0;
};

class CsvBatchProvider : public IBatchProvider {
 public:
  explicit CsvBatchProvider(const std::string& path,
                            const std::vector<std::string>& target_fields);

  std::vector<std::string> ReadNextBatch(size_t batch_size) override;

 private:
  const std::string path_;
  std::unique_ptr<io::InputStream> in_;
  CsvHeaderAnalyzer analyzer_;
};

class CachedCsvBatchProvider : public IBatchProvider {
 public:
  explicit CachedCsvBatchProvider(const std::string& path,
                                  const std::vector<std::string>& target_fields,
                                  size_t bucket_size = 100000000,
                                  bool shuffle = false);

  std::vector<std::string> ReadNextBatch(size_t batch_size) override;

 private:
  void ReadAndShuffle(size_t read_index, bool thread_model = false);

  std::shared_ptr<CsvBatchProvider> provider_;

  size_t bucket_size_;
  bool shuffle_;

  std::array<std::vector<std::string>, 2> bucket_items_;
  size_t cursor_index_ = 0;
  size_t bucket_index_ = 0;

  std::array<std::future<void>, 2> f_read;
};

}  // namespace spu::psi
