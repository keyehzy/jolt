#ifndef SIMPLE_TDD_H
#define SIMPLE_TDD_H

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace simple_tdd {

class TestResult {
public:
  bool passed;
  std::string testName;
  std::string message;

  TestResult(bool p, const std::string &name, const std::string &msg = "")
      : passed(p), testName(name), message(msg) {}
};

class TestRunner {
private:
  std::vector<std::pair<std::string, std::function<void()>>> tests;
  std::vector<TestResult> results;
  int pass_count = 0;
  int fail_count = 0;

public:
  static TestRunner &the() {
    static TestRunner instance;
    return instance;
  }

  void add(const std::string &testName, std::function<void()> testFunc) {
    tests.push_back(std::make_pair(testName, testFunc));
  }

  void run() {
    for (const auto &test : tests) {
      try {
        test.second();
        ++pass_count;
        results.push_back(TestResult(true, test.first));
      } catch (const std::exception &e) {
        ++fail_count;
        results.push_back(TestResult(false, test.first, e.what()));
      }
    }
  }

  void print_results() {
    std::cout << "==== Test Results ====" << std::endl;

    for (const auto &result : results) {
      if (result.passed) {
        std::cout << "✓ PASS: " << result.testName << std::endl;
      } else {
        std::cout << "✗ FAIL: " << result.testName << std::endl;
        std::cout << "  Error: " << result.message << std::endl;
      }
    }

    std::cout << "======================" << std::endl;
    std::cout << "Summary: " << pass_count << " passed, " << fail_count
              << " failed" << std::endl;
    std::cout << "Total tests: " << (pass_count + fail_count) << std::endl;
  }

  void clean() {
    results.clear();
    pass_count = 0;
    fail_count = 0;
  }
};

class AssertionFailure : public std::exception {
private:
  std::string message;

public:
  AssertionFailure(const std::string &msg) : message(msg) {}

  const char *what() const noexcept override { return message.c_str(); }
};

void assert(bool condition, const std::string &message = "") {
  if (!condition) {
    std::stringstream ss;
    ss << "Assertion failed: expected true";
    if (!message.empty()) {
      ss << " - " << message;
    }
    throw AssertionFailure(ss.str());
  }
}
} // namespace simple_tdd

#define TEST(name)                                                             \
  void name();                                                                 \
  static bool name##_registered =                                              \
      (simple_tdd::TestRunner::the().add(#name, name), true);                  \
  void name()

#define ASSERT(condition) simple_tdd::assert(condition, #condition)

#define ASSERT_EQ(expected, actual)                                            \
  simple_tdd::assert((expected) == (actual),                                   \
                     "Expected: " #expected ", Actual: " #actual)

#define ASSERT_NE(expected, actual)                                            \
  simple_tdd::assert((expected) != (actual),                                   \
                     "Expected: " #expected ", Actual: " #actual)

#define ASSERT_TRUE(condition)                                                 \
  simple_tdd::assert(condition, #condition)                                    \
      simple_tdd::assert((condition) == true, #condition)

#define ASSERT_FALSE(condition)                                                \
  simple_tdd::assert(condition, #condition)                                    \
      simple_tdd::assert((condition) == false, #condition)

#define RUN_TESTS()                                                            \
  simple_tdd::TestRunner::the().run();                                         \
  simple_tdd::TestRunner::the().print_results();                               \
  simple_tdd::TestRunner::the().clean();

#define TEST_MAIN()                                                            \
  int main(int argc, char *argv[]) {                                           \
    (void)argc;                                                                \
    (void)argv;                                                                \
    RUN_TESTS();                                                               \
    return 0;                                                                  \
  }

#endif // SIMPLE_TDD_H