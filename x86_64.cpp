#include "x86_64.h"

namespace x86_64 {
std::vector<u8> encode(Instruction instruction) {
  std::vector<u8> buf;
  for (const auto& encoding : instruction.mnemonic) {
    bool found_encoding = true;
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
      if (operand_type == Operand_Type::Memory && operand_encoding_type == Operand_Encoding_Type::Register_Memory) {
        continue;
      }
      if (operand_type == Operand_Type::Immediate_8 && operand_encoding_type == Operand_Encoding_Type::Immediate_8) {
        continue;
      }
      if (operand_type == Operand_Type::Immediate_32 && operand_encoding_type == Operand_Encoding_Type::Immediate_32) {
        continue;
      }
      found_encoding = false;
    }

    if (!found_encoding) {
      continue;
    }

    bool requires_mod_r_m = false;
    u8 reg_or_op_code     = 0;
    u8 rex_byte           = 0;
    u8 r_m                = 0;
    u8 mod                = static_cast<u8>(MOD::Register_Adressing);
    bool requires_sib     = false;
    u8 sib_byte           = 0;
    i32 displacement      = 0;
    bool has_displacement = false;
    for (size_t i = 0; i < instruction.operands.size(); ++i) {
      Operand& operand                    = instruction.operands[i];
      Operand_Encoding_Type encoding_type = encoding.operand_encoding_type[i];
      if (operand.type == Operand_Type::Register) {
        rex_byte |= static_cast<u8>(REX::W);
        if (encoding_type == Operand_Encoding_Type::Register) {
          assert(encoding.extension_type != Instruction_Extension_Type::Op_Code);
          reg_or_op_code = static_cast<u8>(operand.reg);
        }
      }
      if (encoding_type == Operand_Encoding_Type::Register_Memory) {
        requires_mod_r_m = true;
        if (operand.type == Operand_Type::Register) {
          r_m = static_cast<u8>(operand.reg);
          mod = static_cast<u8>(MOD::Register_Adressing);
        } else {
          assert(operand.type == Operand_Type::Memory);
          r_m              = static_cast<u8>(operand.memory.base);
          displacement     = operand.memory.displacement;
          has_displacement = true;
          if (displacement == 0) {
            mod = static_cast<u8>(MOD::Displacement_0);
          } else if (displacement >= -128 && displacement <= 127) {
            mod = static_cast<u8>(MOD::Displacement_8);
          } else {
            mod = static_cast<u8>(MOD::Displacement_32);
          }
          if (r_m == static_cast<u8>(Reg::RSP)) {
            requires_sib = true;
            sib_byte     = (static_cast<u8>(SIB_Scale::_1) << 6) | (r_m << 3) | r_m;
          }
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

    if (requires_mod_r_m) {
      u8 modrm = (mod << 6) | (reg_or_op_code << 3) | r_m;
      buf.push_back(modrm);
    }

    if (requires_sib) {
      buf.push_back(sib_byte);
    }

    // Write the displacement
    if (has_displacement) {
      if (mod == static_cast<u8>(MOD::Displacement_8)) {
        buf.push_back(displacement);
      } else if (mod == static_cast<u8>(MOD::Displacement_32)) {
        buf.push_back(displacement >> 0);
        buf.push_back(displacement >> 8);
        buf.push_back(displacement >> 16);
        buf.push_back(displacement >> 24);
      }
    }

    // Write the immediate
    for (const auto& operand : instruction.operands) {
      if (operand.type == Operand_Type::Immediate_8) {
        buf.push_back(operand.imm8);
      }
      if (operand.type == Operand_Type::Immediate_32) {
        buf.push_back(operand.imm32 >> 0);
        buf.push_back(operand.imm32 >> 8);
        buf.push_back(operand.imm32 >> 16);
        buf.push_back(operand.imm32 >> 24);
      }
    }
    return buf;
  }
  unreachable();
}

Operand imm8(u8 value) { return {.type = Operand_Type::Immediate_8, .imm8 = value}; }
Operand imm32(u32 value) { return {.type = Operand_Type::Immediate_32, .imm32 = value}; }
Operand mem(Reg b, u8 s, u8 d) {
  return {.type = Operand_Type::Memory, .memory = {.base = b, .scale = s, .displacement = d}};
}
Operand stack(u8 offset) { return mem(Reg::RSP, 0, offset); }

std::vector<u8> sub_rsp_imm_8(u8 value) { return encode({SUB, {RSP, imm8(value)}}); }

std::vector<u8> add_rsp_imm_8(u8 value) { return encode({ADD, {RSP, imm8(value)}}); }

std::vector<u8> mov_stack_offset_imm32(u8 offset, u32 value) {
  return encode({MOV, {stack(offset), imm32(value)}});
  //   return {REXW, MOV_IMM_X, 0x04, offset, expand_imm32(imm)};
}

std::vector<u8> add_rax_from_stack_offset(u8 offset) { 
    return encode({ADD, {RAX, stack(offset)}});
//    return {REXW, ADD_MEM_X, 0x04, offset};
 }

std::vector<u8> mov_rax_from_stack_offset(u8 offset) { return {REXW, MOV_MEM_X, 0x04, offset}; }

}  // namespace x86_64