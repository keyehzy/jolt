#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

#include "simple_tdd.h"

typedef int64_t i64;
typedef uint8_t u8;

struct x86_64_Instr {
  static const u8 REXW = 0x48;    // REX prefix for 64-bit operand size
  static const u8 MOV_IMM = 0xB8; // Move immediate to register
  static const u8 MOV_REG = 0x89; // Move register to register
  static const u8 MOV_MEM = 0x8B; // Move memory to register
  static const u8 RET = 0xC3;     // Return from function
};

class Jit {
private:
  void *data_;
  size_t size_;

public:
  Jit(size_t size) : size_(size) {
    // Allocate memory with read, write, and execute permissions
    data_ = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (data_ == MAP_FAILED) {
      throw std::runtime_error("Memory allocation failed");
    }
  }

  ~Jit() {
    if (data_ != MAP_FAILED) {
      munmap(data_, size_);
    }
  }

  void *data() { return data_; }

  size_t size() { return size_; }

  void finalize() {
    if (mprotect(data_, size_, PROT_READ | PROT_EXEC) != 0) {
      throw std::runtime_error("Failed to change memory protection");
    }
  }
};

typedef i64 (*JitConstant)();
int make_constant(int value) {
  // Example:
  // i64 x() {
  //   return value;
  // }
  Jit jit(4096);

  u8 code[] = {
      x86_64_Instr::MOV_IMM,  //
      static_cast<u8>(value), //
      0x00,                   //
      0x00,                   //
      0x00,                   // mov eax, value
      x86_64_Instr::RET,      // ret
  };

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code, sizeof(code));

  // Make the memory executable
  jit.finalize();

  // Cast the memory to a function pointer
  JitConstant func = reinterpret_cast<JitConstant>(jit.data());

  // Call the function
  return func();
}

typedef i64 (*JitIdentity)(i64);
i64 make_identity(i64 value) {
  // Example:
  // i64 x(i64 y) {
  //   return y;
  // }
  Jit jit(4096);

  u8 code[] = {
      0x55,                  // push   rbp
      x86_64_Instr::REXW,    //
      x86_64_Instr::MOV_REG, //
      0xE5,                  // mov    rbp,rsp
      x86_64_Instr::REXW,    //
      x86_64_Instr::MOV_REG, //
      0x7D,                  //
      0xF8,                  // mov    QWORD PTR [rbp-0x8],rdi
      x86_64_Instr::REXW,    //
      x86_64_Instr::MOV_MEM, //
      0x45,                  //
      0xF8,                  // mov    rax,QWORD PTR [rbp-0x8]
      0x5D,                  // pop    rbp
      x86_64_Instr::RET,     // ret
  };

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code, sizeof(code));

  // Make the memory executable
  jit.finalize();

  // Cast the memory to a function pointer
  JitIdentity func = reinterpret_cast<JitIdentity>(jit.data());

  // Call the function
  return func(value);
}

TEST(should_correctly_return_constant) {
  ASSERT_EQ(make_constant(0), 0);
  ASSERT_EQ(make_constant(42), 42);
  ASSERT_EQ(make_constant(100), 100);
}

TEST(should_correctly_return_identity) {
  ASSERT_EQ(make_identity(0), 0);
  ASSERT_EQ(make_identity(42), 42);
  ASSERT_EQ(make_identity(100), 100);
}

TEST_MAIN();