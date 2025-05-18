#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

#include "simple_tdd.h"

typedef int32_t i32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t i64;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;

class Jit {
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

 private:
  void *data_;
  size_t size_;
};

#define expand_imm32(x)                                                       \
  static_cast<u8>(x >> 0), static_cast<u8>(x >> 8), static_cast<u8>(x >> 16), \
      static_cast<u8>(x >> 24)

struct x86_64_Instr {
  static const u8 REXW = 0x48;       // REX prefix for 64-bit operand size
  static const u8 MOV_REG = 0x89;    // Move register to register
  static const u8 MOV_MEM_X = 0x8B;  // Move memory to register/memory
  static const u8 MOV_IMM_X = 0xC7;  // Move immediate to register/memory

  static const u8 ADD_IMM_X = 0x83;  // Add sign-extended immediate to register
  static const u8 ADD_MEM_X = 0x03;  // Add register to memory

  static const u8 SUB_IMM_X = 0x83;  // Subtract immediate from register/memory

  static const u8 PUSH_RBP = 0x55;  // Push RBP onto the stack
  static const u8 POP_RBP = 0x5D;   // Pop RBP from the stack

  static const u8 RET = 0xC3;  // Return from function

  // Functions
  static std::vector<u8> sub_rsp_imm_8(u8 imm) {
    return {REXW, SUB_IMM_X, 0xEC, imm};
  }

  static std::vector<u8> add_rsp_imm_8(u8 imm) {
    return {REXW, ADD_IMM_X, 0xC4, imm};
  }

  static std::vector<u8> mov_to_stack_offset_imm32(u8 offset, u32 imm) {
    return {REXW, MOV_IMM_X, 0x04, offset, expand_imm32(imm)};
  }
};

class AsmStream {
 public:
  AsmStream() = default;

  void append(const std::vector<u8> &code) {
    code_.insert(code_.end(), code.begin(), code.end());
  }

  void append(std::initializer_list<u8> lst) { code_.insert(code_.end(), lst); }

  u8 *data() { return code_.data(); }

  size_t size() { return code_.size(); }

  void function_prologue() {
    append({
        x86_64_Instr::PUSH_RBP,  //
        x86_64_Instr::REXW,      //
        x86_64_Instr::MOV_REG,   //
        0xE5,                    // mov rbp,rsp
    });
  }

  void reserve_stack(size_t count) {
    append(x86_64_Instr::sub_rsp_imm_8(static_cast<u8>(count)));
  }

  void restore_stack(size_t count) {
    append(x86_64_Instr::add_rsp_imm_8(static_cast<u8>(count)));
  }

  void function_epilogue() {
    append({
        x86_64_Instr::POP_RBP,
    });
  }

  void function_return() {
    append({
        x86_64_Instr::RET,
    });
  }

 private:
  std::vector<u8> code_;
};

typedef i64 (*JitConstant)();
int make_constant(int value) {
  // Example:
  // i64 x() {
  //   return value;
  // }
  Jit jit(4096);

  u8 code[] = {
      x86_64_Instr::REXW,       //
      x86_64_Instr::MOV_IMM_X,  //
      0xC0,                     //
      expand_imm32(value),      //
      x86_64_Instr::RET,        // ret
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

  AsmStream code;

  code.function_prologue();
  code.reserve_stack(16);

  code.append({
      // Store the parameter (RDI) at [rsp]
      x86_64_Instr::REXW,     //
      x86_64_Instr::MOV_REG,  //
      0x3C,                   //
      0x24,                   // mov    QWORD PTR [rsp],rdi
  });

  code.append({
      // Move parameter (RDI) to RAX
      x86_64_Instr::REXW,       //
      x86_64_Instr::MOV_MEM_X,  //
      0x04,                     //
      0x24,                     // mov    rax,QWORD PTR [rsp]
  });

  code.restore_stack(16);
  code.function_epilogue();
  code.function_return();

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code.data(), code.size());

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

  AsmStream code;
  code.function_prologue();
  code.reserve_stack(16);

  code.append(x86_64_Instr::mov_to_stack_offset_imm32(0x24, 1));

  code.append({
      // Move parameter (RDI) to RAX
      x86_64_Instr::REXW,     //
      x86_64_Instr::MOV_REG,  //
      0xF8,                   // mov rax,rdi
  });

  code.append({
      // Add the increment value to RAX
      x86_64_Instr::REXW,       //
      x86_64_Instr::ADD_MEM_X,  //
      0x04,                     //
      0x24,                     // add rax,QWORD PTR [rsp]
  });

  code.restore_stack(16);
  code.function_epilogue();
  code.function_return();

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code.data(), code.size());

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