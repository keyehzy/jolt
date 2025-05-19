#include "x86_64.h"

namespace x86_64 {
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
      if (operand_type == Operand_Type::Immediate_8 && operand_encoding_type == Operand_Encoding_Type::Immediate_8) {
        continue;
      }
      unreachable();
    }

    bool requires_modrm = false;
    u8 reg_or_op_code   = 0;
    u8 rex_byte         = 0;
    u8 r_m              = 0;
    for (size_t i = 0; i < instruction.operands.size(); ++i) {
      Operand_Type operand_type                   = instruction.operands[i].type;
      Operand_Encoding_Type operand_encoding_type = encoding.operand_encoding_type[i];
      if (operand_type == Operand_Type::Register) {
        requires_modrm = true;
        rex_byte |= static_cast<u8>(REX::W);
        if (operand_encoding_type == Operand_Encoding_Type::Register) {
          assert(encoding.extension_type != Instruction_Extension_Type::Op_Code);
          reg_or_op_code = static_cast<u8>(instruction.operands[i].reg);
        } else if (operand_encoding_type == Operand_Encoding_Type::Register_Memory) {
          r_m = static_cast<u8>(instruction.operands[i].reg);
        }
      }
    }

    if (encoding.extension_type == Instruction_Extension_Type::Op_Code) {
      reg_or_op_code = encoding.op_code_extension;
    }

    if (rex_byte) {
      buf.push_back(rex_byte);
    }

    buf.push_back(encoding.opcode);

    if (requires_modrm) {
      u8 modrm = (static_cast<u8>(MOD::Register_Adressing) << 6) | (reg_or_op_code << 3) | r_m;
      buf.push_back(modrm);
    }

    for (size_t i = 0; i < instruction.operands.size(); ++i) {
      Operand_Type operand_type = instruction.operands[i].type;
      if (operand_type == Operand_Type::Immediate_8) {
        buf.push_back(instruction.operands[i].imm8);
      }
    }
    return buf;
  }
  unreachable();
}

Operand imm8(u8 imm) { return {.type = Operand_Type::Immediate_8, .imm8 = imm}; }

std::vector<u8> sub_rsp_imm_8(u8 value) { return encode({SUB, {RSP, imm8(value)}}); }

std::vector<u8> add_rsp_imm_8(u8 value) { return encode({ADD, {RSP, imm8(value)}}); }

std::vector<u8> mov_stack_offset_imm32(u8 offset, u32 imm) {
  return {REXW, MOV_IMM_X, 0x04, offset, expand_imm32(imm)};
}

std::vector<u8> add_rax_from_stack_offset(u8 offset) { return {REXW, ADD_MEM_X, 0x04, offset}; }

std::vector<u8> mov_rax_from_stack_offset(u8 offset) { return {REXW, MOV_MEM_X, 0x04, offset}; }

}  // namespace x86_64