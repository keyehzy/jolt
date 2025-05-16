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
  static const u8 REXW = 0x48;      // REX prefix for 64-bit operand size
  static const u8 MOV_IMM = 0xB8;   // Move immediate to register
  static const u8 MOV_REG = 0x89;   // Move register to register
  static const u8 MOV_MEM_X = 0x8B; // Move memory to register/memory
  static const u8 MOV_IMM_X = 0xC7; // Move immediate to register/memory

  static const u8 ADD_IMM_X = 0x83; // Add sign-extended immediate to register
  static const u8 ADD_REG = 0x01;   // Add register to register
  static const u8 ADD_MEM_X = 0x03; // Add register to memory

  static const u8 SUB_IMM_X = 0x83; // Subtract immediate from register/memory

  static const u8 PUSH_RBP = 0x55; // Push RBP onto the stack
  static const u8 POP_RBP = 0x5D;  // Pop RBP from the stack

  static const u8 RET = 0xC3; // Return from function
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
      x86_64_Instr::REXW,           //
      x86_64_Instr::MOV_IMM_X,      //
      0xC0,                         //
      static_cast<u8>(value >> 0),  //
      static_cast<u8>(value >> 8),  //
      static_cast<u8>(value >> 16), //
      static_cast<u8>(value >> 24), //
      x86_64_Instr::RET,            // ret
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
      // Prologue
      x86_64_Instr::PUSH_RBP, //
      x86_64_Instr::REXW,     //
      x86_64_Instr::MOV_REG,  //
      0xE5,                   // mov rbp,rsp

      // Reserve stack space
      x86_64_Instr::REXW,      //
      x86_64_Instr::SUB_IMM_X, //
      0xEC,                    //
      0x10,                    // sub rsp, 16

      // Store the parameter (RDI) at [rsp]
      x86_64_Instr::REXW,    //
      x86_64_Instr::MOV_REG, //
      0x3C,                  //
      0x24,                  // mov    QWORD PTR [rsp],rdi

      // Move parameter (RDI) to RAX
      x86_64_Instr::REXW,      //
      x86_64_Instr::MOV_MEM_X, //
      0x04,                    //
      0x24,                    // mov    rax,QWORD PTR [rsp]

      // Restore the stack pointer
      x86_64_Instr::REXW,      //
      x86_64_Instr::ADD_IMM_X, //
      0xC4,                    //
      0x10,                    // Value to add (16)

      // Epilogue
      x86_64_Instr::POP_RBP,

      // Return
      x86_64_Instr::RET,
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

typedef i64 (*JitIncrement)(i64);
i64 make_increment(i64 value) {
  // Example:
  // i64 x(i64 y) {
  //   i64 increment = 1;
  //   return y + increment;
  // }
  Jit jit(4096);

  u8 code[] = {
      // Prologue
      x86_64_Instr::PUSH_RBP, //
      x86_64_Instr::REXW,     //
      x86_64_Instr::MOV_REG,  //
      0xE5,                   //

      // Reserve stack space
      x86_64_Instr::REXW,      //
      x86_64_Instr::SUB_IMM_X, //
      0xEC,                    // ModR/M byte for RSP as destination
      0x10,                    // sub rsp, 16

      // Store the increment value (1) at [rsp]
      x86_64_Instr::REXW,      //
      x86_64_Instr::MOV_IMM_X, //
      0x04,                    //
      0x24,                    //
      0x1,                     // Value to store (1)
      0x0,                     //
      0x0,                     //
      0x0,                     // mov QWORD PTR [rsp], 1

      // Move parameter (RDI) to RAX
      x86_64_Instr::REXW,    //
      x86_64_Instr::MOV_REG, //
      0xF8,                  //

      // Add the increment value to RAX
      x86_64_Instr::REXW,      //
      x86_64_Instr::ADD_MEM_X, //
      0x04,                    //
      0x24,                    //

      // Restore the stack pointer
      x86_64_Instr::REXW,      //
      x86_64_Instr::ADD_IMM_X, //
      0xC4,                    //
      0x10,                    // Value to add (16)

      // Epilogue
      x86_64_Instr::POP_RBP,

      // Return
      x86_64_Instr::RET,
  };

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code, sizeof(code));

  // Make the memory executable
  jit.finalize();

  // Cast the memory to a function pointer
  JitIncrement func = reinterpret_cast<JitIncrement>(jit.data());

  // Call the function
  return func(value);
}

TEST(should_correctly_return_constant) {
  ASSERT_EQ(make_constant(0), 0);
  ASSERT_EQ(make_constant(42), 42);
  ASSERT_EQ(make_constant(100), 100);
  ASSERT_EQ(make_constant(-1), -1);
}

TEST(should_correctly_return_identity) {
  ASSERT_EQ(make_identity(0), 0);
  ASSERT_EQ(make_identity(42), 42);
  ASSERT_EQ(make_identity(100), 100);
  ASSERT_EQ(make_identity(-1), -1);
}

TEST(should_increment_value) {
  ASSERT_EQ(make_increment(0), 1);
  ASSERT_EQ(make_increment(42), 43);
  ASSERT_EQ(make_increment(100), 101);
  ASSERT_EQ(make_increment(-1), 0);
}

TEST_MAIN();