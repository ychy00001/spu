#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "vector"

int Factorial(int n);

int Factorial(int n) {
  if (n <= 1) {
    return 1;
  }
  return n * Factorial(n - 1);
}

TEST(ExampleTest, HexDebug) {
  int a = 10;
  int* ptr = &a;

  int b = 3;
  ptr = &b;

  *ptr = 10;
  EXPECT_EQ(a, *ptr);
}

TEST(ExampleTest, ErrorMsg) {
  std::vector<int> x = {1, 2, 3};
  std::vector<int> y = {1, 2, 3};
  ASSERT_EQ(x.size(), y.size()) << "Vectors x and y are of unequal length";

  for (int i = 0; i < (int)x.size(); i++) {
    EXPECT_EQ(x[i], y[i]) << "Vectors x and y differ at index : " << i;
  }
}

TEST(ExampleTest, HandlesZeroInput) { EXPECT_EQ(Factorial(0), 1); }

TEST(ExampleTest, HandlesPositiveInput) {
  EXPECT_EQ(Factorial(1), 1);
  EXPECT_EQ(Factorial(2), 2);
  EXPECT_EQ(Factorial(3), 6);
  EXPECT_EQ(Factorial(8), 40320);
}

template <typename E>
class Queue {
 public:
  Queue(int size) {
    size_ = 0;
    elPtr = new E[size];
    head = elPtr;
  };
  void Enqueue(const E& element) {
    *elPtr = element;
    elPtr++;
    size_++;
  }
  E* Dequeue() {
    if (size_ == 0) {
      return nullptr;
    }
    elPtr--;
    E* el = &(*elPtr);
    size_--;
    return el;
  };
  size_t size() const { return size_; };
  ~Queue() { delete[] head; }

 private:
  int size_;
  E* elPtr;
  E* head;
};

class QueueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    q1_.Enqueue(1);
    q2_.Enqueue(2);
    q2_.Enqueue(3);
  }

  Queue<int> q0_ = Queue<int>(0);
  Queue<int> q1_ = Queue<int>(1);
  Queue<int> q2_ = Queue<int>(2);
};

TEST_F(QueueTest, IsEmptyInitially) { EXPECT_EQ(q0_.size(), 0); }

TEST_F(QueueTest, DequeueWorks) {
  int* n = q0_.Dequeue();
  EXPECT_EQ(n, nullptr);

  n = q1_.Dequeue();
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(*n, 1);
  EXPECT_EQ(q1_.size(), 0);

  n = q2_.Dequeue();
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(*n, 3);
  EXPECT_EQ(q2_.size(), 1);

  n = q0_.Dequeue();
  EXPECT_EQ(q0_.size(), 0);
}

namespace my {
namespace project {
namespace {
class FooTest : public ::testing::Test {
 protected:
  FooTest() {}
  ~FooTest() override {}
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(FooTest, MethodBarDoesAbc) {
  const std::string input_filepath = "./package/myinputfile.dat";
  const std::string output_filepath = "./package/myoutputfile.dat";
  EXPECT_NE(input_filepath, output_filepath);
}
}  // namespace
}  // namespace project
}  // namespace my

namespace foo {
class Bar {
 protected:
  std::string aa;
  std::string bb;

 public:
  Bar() : aa("张三"), bb("李四") {}
  friend std::ostream& operator<<(std::ostream& os, const Bar& bar) {
    return os << bar.aa << bar.bb;
  }
};
}  // namespace foo

bool IsCorrectBarIntVector(std::vector<std::pair<foo::Bar, int>> bar_int) {
  if (bar_int.size() == 0) {
    return false;
  }
  return true;
}

TEST(ExampleTest, BarTest) {
  std::vector<std::pair<foo::Bar, int>> bar_ints;
  std::pair<foo::Bar, int> my_pair;
  foo::Bar a;
  my_pair.first = a;
  my_pair.second = 10;
  bar_ints.emplace_back(my_pair);
  EXPECT_TRUE(IsCorrectBarIntVector(bar_ints))
      << "bar_inits = " << testing::PrintToString(bar_ints);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class Print {
 public:
  Print(int n) : n_(n) {}
  int p() { return n_; }

 private:
  int n_;
};
// ScopedTrace
void Sub1(int n) {
  EXPECT_EQ(Print(n).p(), 9);
  EXPECT_EQ(Print(n + 1).p(), 10);
}

TEST(ExampleTest, Print) {
  {
    SCOPED_TRACE("Test SCOPED_TRACE Info");
    Sub1(1);
  }
  Sub1(9);
}

// 找不到PtType_Name在哪里定义的
enum MyEnum { AA = 1, BB = 2, CC = 3 };

TEST(ExampleTest, EnumTest) {
  char* str = MyEnum_Name(1);
  EXPECT_EQ(str == "AA")
}

#define aa print("sss")
TEST(ExampleTest, MacroTest){

}