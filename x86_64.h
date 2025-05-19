#pragma once

#include <array>
#include <cassert>
#include <vector>

#include "int_type.h"

#define todo()        __builtin_trap()
#define unreachable() __builtin_trap()

namespace x86_64 {
#define expand_imm32(x) \
  static_cast<u8>(x >> 0), static_cast<u8>(x >> 8), static_cast<u8>(x >> 16), static_cast<u8>(x >> 24)

const u8 REXW      = 0x48;  // REX prefix for 64-bit operand size
const u8 MOV_REG   = 0x89;  // Move register to register
const u8 MOV_MEM_X = 0x8B;  // Move memory to register/memory
const u8 MOV_IMM_X = 0xC7;  // Move immediate to register/memory

const u8 ADD_MEM_X = 0x03;  // Add register to memory

const u8 PUSH_RBP = 0x55;  // Push RBP onto the stack
const u8 POP_RBP  = 0x5D;  // Pop RBP from the stack

enum class SIB_Scale : u8 {
  _1 = 0b00,
  _2 = 0b01,
  _4 = 0b10,
  _8 = 0b11,
};

enum class MOD : u8 {
  Displacement_0     = 0b00,
  Displacement_8     = 0b01,
  Displacement_32    = 0b10,
  Register_Adressing = 0b11,
};

enum class REX : u8 {
  W = 0b01001000,  // 0 - Operand size determined by CS.D; 1 - 64 Bit Operand Size
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

  R8  = 0b0000,
  R9  = 0b0001,
  R10 = 0b0010,
  R11 = 0b0011,
  R12 = 0b0100,
  R13 = 0b0101,
  R14 = 0b0110,
  R15 = 0b0111,
};

enum class Operand_Type : u8 {
  None,
  Register,
  Memory,
  Immediate_8,
  Immediate_32,
};

struct Operand_Memory {
  Reg base;
  u8 scale;
  u8 displacement;
};

struct Operand {
  Operand_Type type;
  union {
    Reg reg;
    u8 imm8;
    u32 imm32;
    Operand_Memory memory;
  };
};

const Operand RAX = {.type = Operand_Type::Register, .reg = Reg::RAX};
const Operand RCX = {.type = Operand_Type::Register, .reg = Reg::RCX};
const Operand RDX = {.type = Operand_Type::Register, .reg = Reg::RDX};
const Operand RBX = {.type = Operand_Type::Register, .reg = Reg::RBX};
const Operand RSP = {.type = Operand_Type::Register, .reg = Reg::RSP};
const Operand RBP = {.type = Operand_Type::Register, .reg = Reg::RBP};
const Operand RSI = {.type = Operand_Type::Register, .reg = Reg::RSI};
const Operand RDI = {.type = Operand_Type::Register, .reg = Reg::RDI};
const Operand R8  = {.type = Operand_Type::Register, .reg = Reg::R8};
const Operand R9  = {.type = Operand_Type::Register, .reg = Reg::R9};
const Operand R10 = {.type = Operand_Type::Register, .reg = Reg::R10};
const Operand R11 = {.type = Operand_Type::Register, .reg = Reg::R11};
const Operand R12 = {.type = Operand_Type::Register, .reg = Reg::R12};
const Operand R13 = {.type = Operand_Type::Register, .reg = Reg::R13};
const Operand R14 = {.type = Operand_Type::Register, .reg = Reg::R14};
const Operand R15 = {.type = Operand_Type::Register, .reg = Reg::R15};

enum class Instruction_Extension_Type : u8 {
  Register,
  Op_Code,
};

enum class Operand_Encoding_Type : u8 {
  None,
  Register,
  Register_Memory,
  Immediate_8,
  Immediate_32,
};

struct Instruction_Encoding {
  u16 opcode;
  Instruction_Extension_Type extension_type;
  u8 op_code_extension;
  std::array<Operand_Encoding_Type, 2> operand_encoding_type;
};

typedef std::vector<Instruction_Encoding> Mnemonic;

const Mnemonic MOV = {
    {.opcode                = 0x89,
     .extension_type        = Instruction_Extension_Type::Register,
     .operand_encoding_type = {Operand_Encoding_Type::Register_Memory, Operand_Encoding_Type::Register}},
    {.opcode                = 0xC7,
     .extension_type        = Instruction_Extension_Type::Op_Code,
     .op_code_extension     = 0,
     .operand_encoding_type = {Operand_Encoding_Type::Register_Memory, Operand_Encoding_Type::Immediate_32}}};

const Mnemonic ADD = {
    {.opcode                = 0x83,
     .extension_type        = Instruction_Extension_Type::Op_Code,
     .op_code_extension     = 0,
     .operand_encoding_type = {Operand_Encoding_Type::Register_Memory, Operand_Encoding_Type::Immediate_8}},
    {.opcode                = 0x03,
     .extension_type        = Instruction_Extension_Type::Register,
     .operand_encoding_type = {Operand_Encoding_Type::Register, Operand_Encoding_Type::Register_Memory}}};

const Mnemonic SUB = {
    {.opcode                = 0x83,
     .extension_type        = Instruction_Extension_Type::Op_Code,
     .op_code_extension     = 5,
     .operand_encoding_type = {Operand_Encoding_Type::Register_Memory, Operand_Encoding_Type::Immediate_8}},
};

const Mnemonic RET = {
    {.opcode                = 0xC3,
     .extension_type        = Instruction_Extension_Type::Register,
     .operand_encoding_type = {Operand_Encoding_Type::None, Operand_Encoding_Type::None}},
};

struct Instruction {
  Mnemonic mnemonic;
  std::array<Operand, 2> operands;
};

std::vector<u8> encode(Instruction instruction);

Operand imm8(u8 imm);
Operand imm32(u32 imm);
Operand mem(Reg base, u8 scale, u8 displacement);
Operand stack(u8 offset);

std::vector<u8> sub_rsp_imm_8(u8 imm);
std::vector<u8> add_rsp_imm_8(u8 imm);
std::vector<u8> mov_stack_offset_imm32(u8 offset, u32 imm);
std::vector<u8> add_rax_from_stack_offset(u8 offset);
std::vector<u8> mov_rax_from_stack_offset(u8 offset);
}  // namespace x86_64