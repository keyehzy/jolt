#include <fcntl.h>

#include <array>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

#include "int_type.h"
#include "jit.h"
#include "simple_tdd.h"

#define todo()        __builtin_trap()
#define unreachable() __builtin_trap()

template <typename T, typename U>
constexpr T narrow_cast(U &&u) noexcept {
  return static_cast<T>(std::forward<U>(u));
}

namespace x86_64 {
#define expand_imm32(x) \
  static_cast<u8>(x >> 0), static_cast<u8>(x >> 8), static_cast<u8>(x >> 16), static_cast<u8>(x >> 24)

const u8 REXW      = 0x48;  // REX prefix for 64-bit operand size
const u8 MOV_REG   = 0x89;  // Move register to register
const u8 MOV_MEM_X = 0x8B;  // Move memory to register/memory
const u8 MOV_IMM_X = 0xC7;  // Move immediate to register/memory

const u8 ADD_IMM_X = 0x83;  // Add sign-extended immediate to register
const u8 ADD_MEM_X = 0x03;  // Add register to memory

const u8 SUB_IMM_X = 0x83;  // Subtract immediate from register/memory

const u8 PUSH_RBP = 0x55;  // Push RBP onto the stack
const u8 POP_RBP  = 0x5D;  // Pop RBP from the stack

// const u8 RET = 0xC3;  // Return from function

enum class MOD : u8 {
  Zero_Byte_Displacement = 0b00,
  One_Byte_Displacement  = 0b01,
  Four_Byte_Displacement = 0b10,
  Register_Adressing     = 0b11,
};

enum class REX : u8 {
  W = 0b01010000,  // 0 - Operand size determined by CS.D; 1 - 64 Bit Operand Size
  R = 0b01000100,  // Extension of the ModR/M reg field
  X = 0b01000010,  // Extension of the SIB index field
  B = 0b01000001,  // Extension of the ModR/M r/m field, SIB base field, or Opcode reg field
};

enum class Reg : u8 {
  RAX = 0b000,
  RCX = 0b001,
  RDX = 0b010,
  RBX = 0b011,
  RSP = 0b100,
  RBP = 0b101,
  RSI = 0b110,
  RDI = 0b111,
};

enum class Operand_Type : u8 {
  None,
  Register,
  Memory,
  Immediate,
};

struct Operand {
  Operand_Type type;
  union {
    Reg reg;
    u32 imm;
  };
};

enum class Instruction_Extension_Type : u8 {
  Register,
  Op_Code,
};

enum class Operand_Encoding_Type : u8 {
  None,
  Register,
  Register_Memory,
  Immediate,
};

struct Instruction_Encoding {
  u16 opcode;
  Instruction_Extension_Type instr_extension_type;
  std::array<Operand_Encoding_Type, 2> operand_encoding_type;
};

const std::vector<Instruction_Encoding> MOV = {
    {
        {0x89,
         Instruction_Extension_Type::Register,
         {Operand_Encoding_Type::Register_Memory, Operand_Encoding_Type::Register}},
    },
};

const std::vector<Instruction_Encoding> RET = {
    {
        {0xC3, Instruction_Extension_Type::Register, {Operand_Encoding_Type::None, Operand_Encoding_Type::None}},
    },
};

struct Instruction {
  std::vector<Instruction_Encoding> mnemonic;
  std::array<Operand, 2> operands;
};

// Functions
std::vector<u8> encode(Instruction instruction) {
  std::vector<u8> buf;
  for (const auto &encoding : instruction.mnemonic) {
    for (size_t i = 0; i < instruction.operands.size(); ++i) {
      Operand_Encoding_Type operand_encoding_type = encoding.operand_encoding_type[i];
      Operand_Type operand_type                   = instruction.operands[i].type;
      if (operand_type == Operand_Type::None && operand_encoding_type == Operand_Encoding_Type::None) {
        continue;
      }
      if (operand_type == Operand_Type::Register &&
          ((operand_encoding_type == Operand_Encoding_Type::Register_Memory) ||
           (operand_encoding_type == Operand_Encoding_Type::Register))) {
        continue;
      }
      unreachable();
    }

    bool requires_modrm = false;
    for (size_t i = 0; i < instruction.operands.size(); ++i) {
      Operand_Type operand_type = instruction.operands[i].type;
      if (operand_type == Operand_Type::Register) {
        requires_modrm = true;
        buf.push_back(REXW);
        break;
      }
    }
    buf.push_back(encoding.opcode);

    if (requires_modrm) {
      u8 modrm = (static_cast<u8>(MOD::Register_Adressing) << 6) | (static_cast<u8>(instruction.operands[0].reg) << 3) |
                 static_cast<u8>(instruction.operands[1].reg);
      buf.push_back(modrm);
    }
  }
  return buf;
}

// std::vector<u8> mov(Reg reg1, Reg reg2) {
//   return {REXW, MOV_REG,
//           (static_cast<u8>(MOD::Register_Adressing) << 6) | (static_cast<u8>(reg1) << 3) | static_cast<u8>(reg2)};
// }

std::vector<u8> sub_rsp_imm_8(u8 imm) { return {REXW, SUB_IMM_X, 0xEC, imm}; }

std::vector<u8> add_rsp_imm_8(u8 imm) { return {REXW, ADD_IMM_X, 0xC4, imm}; }

std::vector<u8> mov_stack_offset_imm32(u8 offset, u32 imm) {
  return {REXW, MOV_IMM_X, 0x04, offset, expand_imm32(imm)};
}

std::vector<u8> add_rax_from_stack_offset(u8 offset) { return {REXW, ADD_MEM_X, 0x04, offset}; }

std::vector<u8> mov_rax_from_stack_offset(u8 offset) { return {REXW, MOV_MEM_X, 0x04, offset}; }
}  // namespace x86_64

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
    x86_64::Operand rbp     = {x86_64::Operand_Type::Register, {x86_64::Reg::RBP}};
    x86_64::Operand rsp     = {x86_64::Operand_Type::Register, {x86_64::Reg::RSP}};
    x86_64::Instruction mov = {x86_64::MOV, {rbp, rsp}};
    append(x86_64::encode(mov));
  }

  void reserve_stack(size_t count) { append(x86_64::sub_rsp_imm_8(static_cast<u8>(count))); }

  void restore_stack(size_t count) { append(x86_64::add_rsp_imm_8(static_cast<u8>(count))); }

  void function_epilogue() { append(x86_64::POP_RBP); }

  void function_return() {
    x86_64::Instruction ret = {x86_64::RET, {}};
    append(x86_64::encode(ret));
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

  code.append({
      x86_64::REXW,         //
      x86_64::MOV_IMM_X,    //
      0xC0,                 //
      expand_imm32(value),  // mov   rax, imm32
  });

  x86_64::Instruction ret = {x86_64::RET, {}};
  code.append(x86_64::encode(ret));

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

  code.append(x86_64::mov_stack_offset_imm32(0x24, 1));

  x86_64::Operand rbp     = {x86_64::Operand_Type::Register, {x86_64::Reg::RAX}};
  x86_64::Operand rsp     = {x86_64::Operand_Type::Register, {x86_64::Reg::RDI}};
  x86_64::Instruction mov = {x86_64::MOV, {rbp, rsp}};
  code.append(x86_64::encode(mov));

  code.append(x86_64::add_rax_from_stack_offset(0x24));

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