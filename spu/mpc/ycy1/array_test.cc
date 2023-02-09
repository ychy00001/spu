//
// Created by Rain on 2023/2/3.
//
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "spu/core/array_ref.h"

// ArrayRef Test
TEST(ArrayTest, ValueTest) {
  constexpr size_t kBufSize = 40;
  auto mem = std::make_shared<yacl::Buffer>(kBufSize);

  // GIVEN 40字节存放数据，数据类型为int类型，一共10个元素，步幅为1，偏移量为0
  spu::ArrayRef a(mem, spu::I32, 10, 1, 0);
  EXPECT_EQ(a.numel(), kBufSize / spu::I32.size());
  EXPECT_EQ(a.stride(), 1);
  EXPECT_EQ(a.elsize(), spu::I32.size());
  EXPECT_EQ(a.offset(), 0);
  for (int32_t i = 0; i < 4; i++) {
    // 只能存放10个数据
    a.at<int32_t>(i) = i;
  }
  EXPECT_EQ(a.numel(), 10);
}