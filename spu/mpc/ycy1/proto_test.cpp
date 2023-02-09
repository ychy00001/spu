//
// Created by Rain on 2023/2/3.
//
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "string"

#include "spu/mpc/ycy1/proto_example.pb.h"

// proto_buf自带枚举的方法，索引转字符串
TEST(ProtoTest, EnumName) {
  std::string name = example::SexEnum_Name(1);
  EXPECT_EQ(name, "MAN");
}

// proto_buf自带枚举方法，字符串转索引
TEST(ProtoTest, EnumParse) {
  example::SexEnum enm = example::MAN;
  example::SexEnum* testEnum = &enm;
  bool parseFlag = example::SexEnum_Parse(std::string("WOMAN"), testEnum);
  EXPECT_TRUE(parseFlag);
  EXPECT_EQ(static_cast<int32_t>(*testEnum), 2);
}