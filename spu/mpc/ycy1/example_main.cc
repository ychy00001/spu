#include <iostream>
#include <xtensor/xadapt.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xview.hpp>

#include "vector"

#include "spu/mpc/ycy1/io.h"

#define aa printf("aaa");
#define bb printf("bbb");
#define cc aa bb
// 宏命令测试
// int main() {
//  cc
//}

void example() {
  xt::xarray<double> arr1{
      {1.0, 2.0, 3.0},
      {2.0, 5.0, 7.0},
      {2.0, 5.0, 3.0},
  };

  xt::xarray<double> arr2{5.0, 6.0, 7.0};

  xt::xarray<double> res = xt::view(arr1, 1) + arr2;

  std::cout << res << std::endl;
}

void example_reshape() {
  xt::xarray<double> arr{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  arr.reshape({2, 5});

  std::cout << arr << std::endl;
}

void example_index_access() {
  xt::xarray<double> arr1{{1.0, 2.0, 3.0}, {2.0, 5.0, 7.0}, {3.0, 6.0, 10.0}};
  std::cout << arr1(2, 2) << std::endl;
  xt::xarray<int> arr2{1, 2, 3, 4, 5, 6, 7, 8};
  std::cout << arr2(3) << std::endl;
}

void example_d_adapt() {
  std::vector<double> v = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::vector<std::size_t> shape = {2, 3};
  auto a1 = xt::adapt(v, shape);
  xt::xarray<double> a2 = {{1., 2., 3.}, {4., 5., 6.}};
  xt::xarray<double> res = a1 + a2;
  std::cout << res << std::endl;
}

void example_array_ring_type() {
  int32_t kNumel = 2;
  auto mem = std::make_shared<yacl::Buffer>(kNumel * 4);
  spu::ArrayRef raw(mem, spu::makeType<spu::RingTy>(spu::FM32), kNumel, 1, 0);
  for (int32_t i = 0; i < kNumel; i++) {
    raw.at<int32_t>(i) = i + 5;
  }

  for (int32_t i = 0; i < kNumel; i++) {
    std::cout << "raw.index:" << i << "raw.val:" << raw.at<int32_t>(i) << std::endl;
  }
}

int main() {
  // example();
  // example_reshape();
  // example_index_access();
  // example_d_adapt();
  example_array_ring_type();
  return 0;
}
