#include <cstring>

#include "int_type.h"
#include "jit.h"
#include "simple_tdd.h"
#include "x86_64.h"

class Asm_Buffer {
 public:
  Asm_Buffer() = default;

  void append(const std::vector<u8> &code) { code_.insert(code_.end(), code.begin(), code.end()); }
  void append(std::initializer_list<u8> lst) { code_.insert(code_.end(), lst); }
  void append(u8 code) { code_.push_back(code); }
  u8 *data() { return code_.data(); }
  size_t size() { return code_.size(); }

  void function_prologue() {
    append(x86_64::PUSH_RBP);
    append(x86_64::encode({x86_64::MOV, {x86_64::RBP, x86_64::RSP}}));
  }

  void reserve_stack(size_t count) { append(x86_64::sub_rsp_imm_8(static_cast<u8>(count))); }
  void restore_stack(size_t count) { append(x86_64::add_rsp_imm_8(static_cast<u8>(count))); }
  void function_epilogue() { append(x86_64::POP_RBP); }

  void function_return() {
    x86_64::Instruction ret = {x86_64::RET, {}};
    append(x86_64::encode(ret));
  }

  void dump() {
    for (size_t i = 0; i < code_.size(); ++i) {
      printf("%02X ", code_[i]);
    }
    printf("\n");
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

  Asm_Buffer code;

  code.append({x86_64::encode({x86_64::MOV, {x86_64::RAX, x86_64::imm32(value)}})});
  code.append(x86_64::encode({x86_64::RET, {}}));

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code.data(), code.size());

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

  Asm_Buffer code;

  code.function_prologue();
  code.reserve_stack(16);

  code.append({
      // Store the parameter (RDI) at [rsp]
      x86_64::REXW,     //
      x86_64::MOV_REG,  //
      0x3C,             //
      0x24,             // mov    QWORD PTR [rsp],rdi
  });

  code.append(x86_64::mov_rax_from_stack_offset(0x24));

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

  Asm_Buffer code;
  code.function_prologue();
  code.reserve_stack(16);
  //   code.append(x86_64::mov_stack_offset_imm32(0x8, 1));

  code.append(x86_64::encode({x86_64::MOV, {x86_64::RAX, x86_64::RDI}}));  // Load y into RAX
  code.append(x86_64::mov_stack_offset_imm32(0x8, 1));                     // MOV [RSP + 0x8], 1 (lower 32 bits)
  code.append(x86_64::mov_stack_offset_imm32(0xC, 0));                     // MOV [RSP + 0xC], 0 (upper 32 bits)
  code.append(x86_64::add_rax_from_stack_offset(0x8));

  code.restore_stack(16);
  code.function_epilogue();
  code.function_return();
  code.dump();

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