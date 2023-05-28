#include "cpux86.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "io.h"
#include "memory.h"
#include "vectors.h"

#define TRACE(x...) \
	if (0) fprintf(stderr, "[cpu] " x)

CPUx86::CPUx86(Memory& oMemory, IO& oIO, Vectors& oVectors)
	: m_Memory(oMemory), m_IO(oIO), m_Vectors(oVectors)
{
}

CPUx86::~CPUx86()
{
}

void
CPUx86::Reset()
{
	m_State.m_prefix = 0;
	m_State.m_flags = 0;
	m_State.m_cs = 0xffff; m_State.m_ip = 0;
	m_State.m_ds = 0; m_State.m_es = 0; m_State.m_ss = 0;

	m_State.m_ax = 0x1234;
}

int
CPUx86::RunInstruction()
{
#define GET_MODRM \
	register uint8_t modrm = GetNextOpcode()
#define MODRM_XXX(modrm) \
	((modrm >> 3) & 7)
#define IMM8 \
	GetNextOpcode()
#define GET_IMM8 \
	register uint8_t imm = IMM8
#define IMM16 \
	(GetNextOpcode() | (uint16_t)GetNextOpcode() << 8)
#define GET_IMM16 \
	register uint16_t imm = IMM16
#define DO_COND_JUMP(cond) \
	GET_IMM8; \
	if (cond) \
		RelativeJump8(imm)

	/*
	 * The Op_... macro's follow the 80386 manual conventions (appendix F page
	 * 706)
	 *
	 * The first character contains the addressing method:
	 *
	 * - E = mod/rm follows, specified operand
	 * - G = reg field of modrm selects general register
	 *
 	 * The second character is the operand type:
	 *
	 * - v = word
	 * - b = byte
	 */

	// op Ev Gv -> Ev = op(Ev, Gv)
#define Op_EvGv(op) \
	GET_MODRM; \
	DecodeEA(modrm, m_DecodeState); \
	WriteEA16(m_DecodeState, op##16(ReadEA16(m_DecodeState), GetReg16(MODRM_XXX(modrm))))

	// op Gv Ev -> Gv = op(Gv, Ev)
#define Op_GvEv(op) \
	GET_MODRM; \
	DecodeEA(modrm, m_DecodeState); \
	uint16_t& reg = GetReg16(MODRM_XXX(modrm)); \
	reg = op##16(reg, ReadEA16(m_DecodeState))

	// Op Eb Gb -> Eb = op(Eb, Gb)
#define Op_EbGb(op) \
	GET_MODRM; \
	DecodeEA(modrm, m_DecodeState); \
	unsigned int shift; \
	uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift); \
	WriteEA8(m_DecodeState, op##8(ReadEA8(m_DecodeState), (reg >> shift) & 0xff))

	// Op Gb Eb -> Gb = op(Gb, Eb)
#define Op_GbEb(op) \
	GET_MODRM; \
	DecodeEA(modrm, m_DecodeState); \
	unsigned int shift; \
	uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift); \
	SetReg8(reg, shift, op##8((reg >> shift) & 0xff, ReadEA8(m_DecodeState)))
	

#define TODO \
	TRACE("todo\n")
#define INVALID_OPCODE \
	abort()

	uint8_t opcode = GetNextOpcode();
	TRACE("cs:ip=%04x:%04x opcode 0x%02x\n", m_State.m_cs, m_State.m_ip - 1, opcode);
	switch(opcode) {
		case 0x00: /* ADD Eb Gb */ {
			Op_EbGb(ADD);
			break;
		}
		case 0x01: /* ADD Ev Gv */ {
			Op_EvGv(ADD);
			break;
		}
		case 0x02: /* ADD Gb Eb */ {
			Op_GbEb(ADD);
			break;
		}
		case 0x03: /* ADD Gv Ev */ {
			Op_GvEv(ADD);
			break;
		}
		case 0x04: /* ADD AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | ADD8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x05: /* ADD eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = ADD16(m_State.m_ax, imm);
			break;
		}
		case 0x06: /* PUSH ES */ {
			Push16(m_State.m_es);
			break;
		}
		case 0x07: /* POP ES */ {
			m_State.m_es = Pop16();
			break;
		}
		case 0x08: /* OR Eb Gb */ {
			Op_EbGb(OR);
			break;
		}
		case 0x09: /* OR Ev Gv */ {
			Op_EvGv(OR);
			break;
		}
		case 0x0a: /* OR Gb Eb */ {
			Op_GbEb(OR);
			break;
		}
		case 0x0b: /* OR Gv Ev */ {
			Op_GvEv(OR);
			break;
		}
		case 0x0c: /* OR AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | OR8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x0d: /* OR eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = OR16(m_State.m_ax, imm);
			break;
		}
		case 0x0e: /* PUSH CS */ {
			Push16(m_State.m_cs);
			break;
		}
		case 0x0f: /* -- */ {
			Handle0FPrefix();
			break;
		}
		case 0x10: /* ADC Eb Gb */ {
			Op_EbGb(ADC);
			break;
		}
		case 0x11: /* ADC Ev Gv */ {
			Op_EvGv(ADC);
			break;
		}
		case 0x12: /* ADC Gb Eb */ {
			Op_GbEb(ADC);
			break;
		}
		case 0x13: /* ADC Gv Ev */ {
			Op_GvEv(ADC);
			break;
		}
		case 0x14: /* ADC AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | ADC8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x15: /* ADC eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = ADC16(m_State.m_ax, imm);
			break;
		}
		case 0x16: /* PUSH SS */ {
			Push16(m_State.m_ss);
			break;
		}
		case 0x17: /* POP SS */ {
			m_State.m_ss = Pop16();
			break;
		}
		case 0x18: /* SBB Eb Gb */ {
			Op_EbGb(SBB);
			break;
		}
		case 0x19: /* SBB Ev Gv */ {
			Op_EvGv(SBB);
			break;
		}
		case 0x1a: /* SBB Gb Eb */ {
			Op_GbEb(SBB);
			break;
		}
		case 0x1b: /* SBB Gv Ev */ {
			Op_GvEv(SBB);
			break;
		}
		case 0x1c: /* SBB AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | SBB8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x1d: /* SBB eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = SBB16(m_State.m_ax, imm);
			break;
		}
		case 0x1e: /* PUSH DS */ {
			Push16(m_State.m_ds);
			break;
		}
		case 0x1f: /* POP DS */ {
			m_State.m_ds = Pop16();
			break;
		}
		case 0x20: /* AND Eb Gb */ {
			Op_EbGb(AND);
			break;
		}
		case 0x21: /* AND Ev Gv */ {
			Op_EvGv(AND);
			break;
		}
		case 0x22: /* AND Gb Eb */ {
			Op_GbEb(AND);
			break;
		}
		case 0x23: /* AND Gv Ev */ {
			Op_GvEv(AND);
			break;
		}
		case 0x24: /* AND AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | AND8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x25: /* AND eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = AND16(m_State.m_ax, imm);
			break;
		}
		case 0x26: /* ES: */ {
			m_State.m_prefix |= State::PREFIX_SEG;
			m_State.m_seg_override = SEG_ES;
			break;
		}
		case 0x27: /* DAA */ {
			TODO;
			break;
		}
		case 0x28: /* SUB Eb Gb */ {
			Op_EbGb(SUB);
			break;
		}
		case 0x29: /* SUB Ev Gv */ {
			Op_EvGv(SUB);
			break;
		}
		case 0x2a: /* SUB Gb Eb */ {
			Op_GbEb(SUB);
			break;
		}
		case 0x2b: /* SUB Gv Ev */ {
			Op_GvEv(SUB);
			break;
		}
		case 0x2c: /* SUB AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | SUB8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x2d: /* SUB eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = SUB16(m_State.m_ax, imm);
			break;
		}
		case 0x2e: /* CS: */ {
			m_State.m_prefix |= State::PREFIX_SEG;
			m_State.m_seg_override = SEG_CS;
			break;
		}
		case 0x2f: /* DAS */ {
			TODO;
			break;
		}
		case 0x30: /* XOR Eb Gb */ {
			Op_EbGb(XOR);
			break;
		}
		case 0x31: /* XOR Ev Gv */ {
			Op_EvGv(XOR);
			break;
		}
		case 0x32: /* XOR Gb Eb */ {
			Op_GbEb(XOR);
			break;
		}
		case 0x33: /* XOR Gv Ev */ {
			Op_GvEv(XOR);
			break;
		}
		case 0x34: /* XOR AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | XOR8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x35: /* XOR eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = XOR16(m_State.m_ax, imm);
			break;
		}
		case 0x36: /* SS: */ {
			m_State.m_prefix |= State::PREFIX_SEG;
			m_State.m_seg_override = SEG_SS;
			break;
		}
		case 0x37: /* AAA */ {
			TODO;
			break;
		}
		case 0x38: /* CMP Eb Gb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			unsigned int shift;
			uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift);
			SUB8(ReadEA8(m_DecodeState), (reg >> shift) & 0xff);
			break;
		}
		case 0x39: /* CMP Ev Gv */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			SUB16(ReadEA16(m_DecodeState), GetReg16(MODRM_XXX(modrm)));
			break;
		}
		case 0x3a: /* CMP Gb Eb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			unsigned int shift;
			uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift);
			SUB8((reg >> shift) & 0xff, ReadEA8(m_DecodeState));
			break;
		}
		case 0x3b: /* CMP Gv Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			SUB16(GetReg16(MODRM_XXX(modrm)), ReadEA16(m_DecodeState));
			break;
		}
		case 0x3c: /* CMP AL Ib */ {
			GET_IMM8;
			SUB8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0x3d: /* CMP eAX Iv */ {
			GET_IMM16;
			SUB16(m_State.m_ax, imm);
			break;
		}
		case 0x3e: /* DS: */ {
			m_State.m_prefix |= State::PREFIX_SEG;
			m_State.m_seg_override = SEG_DS;
			break;
		}
		case 0x3f: /* AAS */ {
			TODO;
			break;
		}
		case 0x40: /* INC eAX */ {
			m_State.m_ax = INC16(m_State.m_ax);
			break;
		}
		case 0x41: /* INC eCX */ {
			m_State.m_cx = INC16(m_State.m_cx);
			break;
		}
		case 0x42: /* INC eDX */ {
			m_State.m_dx = INC16(m_State.m_dx);
			break;
		}
		case 0x43: /* INC eBX */ {
			m_State.m_bx = INC16(m_State.m_bx);
			break;
		}
		case 0x44: /* INC eSP */ {
			m_State.m_sp = INC16(m_State.m_sp);
			break;
		}
		case 0x45: /* INC eBP */ {
			m_State.m_bp = INC16(m_State.m_bp);
			break;
		}
		case 0x46: /* INC eSI */ {
			m_State.m_si = INC16(m_State.m_si);
			break;
		}
		case 0x47: /* INC eDI */ {
			m_State.m_di = INC16(m_State.m_di);
			break;
		}
		case 0x48: /* DEC eAX */ {
			m_State.m_ax = DEC16(m_State.m_ax);
			break;
		}
		case 0x49: /* DEC eCX */ {
			m_State.m_cx = DEC16(m_State.m_cx);
			break;
		}
		case 0x4a: /* DEC eDX */ {
			m_State.m_dx = DEC16(m_State.m_dx);
			break;
		}
		case 0x4b: /* DEC eBX */ {
			m_State.m_bx = DEC16(m_State.m_bx);
			break;
		}
		case 0x4c: /* DEC eSP */ {
			m_State.m_sp = DEC16(m_State.m_sp);
			break;
		}
		case 0x4d: /* DEC eBP */ {
			m_State.m_bp = DEC16(m_State.m_bp);
			break;
		}
		case 0x4e: /* DEC eSI */ {
			m_State.m_si = DEC16(m_State.m_si);
			break;
		}
		case 0x4f: /* DEC eDI */ {
			m_State.m_di = DEC16(m_State.m_di);
			break;
		}
		case 0x50: /* PUSH eAX */ {
			Push16(m_State.m_ax);
			break;
		}
		case 0x51: /* PUSH eCX */ {
			Push16(m_State.m_cx);
			break;
		}
		case 0x52: /* PUSH eDX */ {
			Push16(m_State.m_dx);
			break;
		}
		case 0x53: /* PUSH eBX */ {
			Push16(m_State.m_bx);
			break;
		}
		case 0x54: /* PUSH eSP */ {
			Push16(m_State.m_sp);
			break;
		}
		case 0x55: /* PUSH eBP */ {
			Push16(m_State.m_bp);
			break;
		}
		case 0x56: /* PUSH eSI */ {
			Push16(m_State.m_si);
			break;
		}
		case 0x57: /* PUSH eDI */ {
			Push16(m_State.m_di);
			break;
		}
		case 0x58: /* POP eAX */ {
			m_State.m_ax = Pop16();
			break;
		}
		case 0x59: /* POP eCX */ {
			m_State.m_cx = Pop16();
			break;
		}
		case 0x5a: /* POP eDX */ {
			m_State.m_dx = Pop16();
			break;
		}
		case 0x5b: /* POP eBX */ {
			m_State.m_bx = Pop16();
			break;
		}
		case 0x5c: /* POP eSP */ {
			m_State.m_sp = Pop16();
			break;
		}
		case 0x5d: /* POP eBP */ {
			m_State.m_bp = Pop16();
			break;
		}
		case 0x5e: /* POP eSI */ {
			m_State.m_si = Pop16();
			break;
		}
		case 0x5f: /* POP eDI */ {
			m_State.m_di = Pop16();
			break;
		}
		case 0x60: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x61: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x62: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x63: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x64: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x65: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x66: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x67: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x68: /* PUSH imm16 */ {
			GET_IMM16;
			Push16(imm);
			break;
		}
		case 0x69: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x6a: /* PUSH imm8 */ {
			GET_IMM8;
			int16_t imm16 = (int16_t)imm;
			Push16(imm16);
			break;
		}
		case 0x6b: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x6c: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x6d: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x6e: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x6f: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0x70: /* JO Jb */ {
			DO_COND_JUMP(FlagOverflow());
			break;
		}
		case 0x71: /* JNO Jb */ {
			DO_COND_JUMP(!FlagOverflow());
			break;
		}
		case 0x72: /* JB Jb */ {
			DO_COND_JUMP(FlagCarry());
			break;
		}
		case 0x73: /* JNB Jb */ {
			DO_COND_JUMP(!FlagCarry());
			break;
		}
		case 0x74: /* JZ Jb */ {
			DO_COND_JUMP(FlagZero());
			break;
		}
		case 0x75: /* JNZ Jb */ {
			DO_COND_JUMP(!FlagZero());
			break;
		}
		case 0x76: /* JBE Jb */ {
			DO_COND_JUMP(FlagCarry() || FlagZero());
			break;
		}
		case 0x77: /* JA Jb */ {
			DO_COND_JUMP(!FlagCarry() && !FlagZero());
			break;
		}
		case 0x78: /* JS Jb */ {
			DO_COND_JUMP(FlagSign());
			break;
		}
		case 0x79: /* JNS Jb */ {
			DO_COND_JUMP(!FlagSign());
			break;
		}
		case 0x7a: /* JPE Jb */ {
			DO_COND_JUMP(FlagParity());
			break;
		}
		case 0x7b: /* JPO Jb */ {
			DO_COND_JUMP(!FlagParity());
			break;
		}
		case 0x7c: /* JL Jb */ {
			DO_COND_JUMP(FlagSign() != FlagOverflow());
			break;
		}
		case 0x7d: /* JGE Jb */ {
			DO_COND_JUMP(FlagSign() == FlagOverflow());
			break;
		}
		case 0x7e: /* JLE Jb */ {
			DO_COND_JUMP(FlagSign() != FlagOverflow() || FlagZero());
			break;
		}
		case 0x7f: /* JG Jb */ {
			DO_COND_JUMP(!FlagZero() && FlagSign() == FlagOverflow());
			break;
		}
		case 0x80:
		case 0x82: /* GRP1 Eb Ib */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GET_IMM8;

			uint8_t val = ReadEA8(m_DecodeState);
			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: // add
					WriteEA8(m_DecodeState, ADD8(val, imm));
					break;
				case 1: // or
					WriteEA8(m_DecodeState, OR8(val, imm));
					break;
				case 2: // adc
					WriteEA8(m_DecodeState, ADC8(val, imm));
					break;
				case 3: // sbb
					WriteEA8(m_DecodeState, SBB8(val, imm));
					break;
				case 4: // and
					WriteEA8(m_DecodeState, AND8(val, imm));
					break;
				case 5: // sub
					WriteEA8(m_DecodeState, SUB8(val, imm));
					break;
				case 6: // xor
					WriteEA8(m_DecodeState, XOR8(val, imm));
					break;
				case 7: // cmp
					fprintf(stderr, ">>> val=%x imm=%x\n", val, imm);
					SUB8(val, imm);
					break;
			}
			break;
		}
		case 0x81: /* GRP1 Ev Iv */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GET_IMM16;

			uint16_t val = ReadEA16(m_DecodeState);
			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: // add
					WriteEA16(m_DecodeState, ADD16(val, imm));
					break;
				case 1: // or
					WriteEA16(m_DecodeState, OR16(val, imm));
					break;
				case 2: // adc
					WriteEA16(m_DecodeState, ADC16(val, imm));
					break;
				case 3: // sbb
					WriteEA16(m_DecodeState, SBB16(val, imm));
					break;
				case 4: // and
					WriteEA16(m_DecodeState, AND16(val, imm));
					break;
				case 5: // sub
					WriteEA16(m_DecodeState, SUB16(val, imm));
					break;
				case 6: // xor
					WriteEA16(m_DecodeState, XOR16(val, imm));
					break;
				case 7: // cmp
					SUB16(val, imm);
					break;
			}
			break;
		}
		case 0x83: /* GRP1 Ev Ib */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GET_IMM8;

			uint16_t val = ReadEA16(m_DecodeState);
			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: // add
					WriteEA16(m_DecodeState, ADD16(val, imm));
					break;
				case 1: // or
					WriteEA16(m_DecodeState, OR16(val, imm));
					break;
				case 2: // adc
					WriteEA16(m_DecodeState, ADC16(val, imm));
					break;
				case 3: // sbb
					WriteEA16(m_DecodeState, SBB16(val, imm));
					break;
				case 4: // and
					WriteEA16(m_DecodeState, AND16(val, imm));
					break;
				case 5: // sub
					WriteEA16(m_DecodeState, SUB16(val, imm));
					break;
				case 6: // xor
					WriteEA16(m_DecodeState, XOR16(val, imm));
					break;
				case 7: // cmp
					SUB16(val, imm);
					break;
			}
			break;
		}
		case 0x84: /* TEST Gb Eb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			unsigned int shift;
			uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift);
			AND8((reg >> shift) & 0xff, ReadEA8(m_DecodeState));
			break;
		}
		case 0x85: /* TEST Gv Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			AND16(GetReg16(MODRM_XXX(modrm)), ReadEA16(m_DecodeState));
			break;
		}
		case 0x86: /* XCHG Gb Eb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			unsigned int shift;
			uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift);
			uint8_t prev_reg = (reg >> shift) & 0xff;
			SetReg8(reg, shift, ReadEA8(m_DecodeState));
			WriteEA8(m_DecodeState, prev_reg);
			break;
		}
		case 0x87: /* XCHG Gv Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			uint16_t& reg = GetReg16(MODRM_XXX(modrm));
			uint16_t prev_reg = reg;
			reg = ReadEA16(m_DecodeState);
			WriteEA16(m_DecodeState, prev_reg);
			break;
		}
		case 0x88: /* MOV Eb Gb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			unsigned int shift;
			uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift);
			WriteEA8(m_DecodeState, (reg >> shift) & 0xff);
			break;
		}
		case 0x89: /* MOV Ev Gv */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			WriteEA16(m_DecodeState, GetReg16(MODRM_XXX(modrm)));
			break;
		}
		case 0x8a: /* MOV Gb Eb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			unsigned int shift;
			uint16_t& reg = GetReg8(MODRM_XXX(modrm), shift);
			SetReg8(reg, shift, ReadEA8(m_DecodeState));
			break;
		}
		case 0x8b: /* MOV Gv Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GetReg16(MODRM_XXX(modrm)) = ReadEA16(m_DecodeState);
			break;
		}
		case 0x8c: /* MOV Ew Sw */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			WriteEA16(m_DecodeState, GetSReg16(MODRM_XXX(modrm)));
			break;
		}
		case 0x8d: /* LEA Gv M */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GetReg16(MODRM_XXX(modrm)) = GetAddrEA16(m_DecodeState);
			break;
		}
		case 0x8e: /* MOV Sw Ew */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GetSReg16(MODRM_XXX(modrm)) = ReadEA16(m_DecodeState);
			break;
		}
		case 0x8f: /* POP Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			WriteEA16(m_DecodeState, Pop16());
			break;
		}
		case 0x90: /* XCHG eAX eAX (NOP) */
		case 0x91: /* XCHG eCX eAX */
		case 0x92: /* XCHG eDX eAX */
		case 0x93: /* XCHG eBX eAX */
		case 0x94: /* XCHG eSP eAX */
		case 0x95: /* XCHG eBP eAX */
		case 0x96: /* XCHG eSI eAX */
		case 0x97: /* XCHG eDI eAX */ {
			uint16_t& reg = GetReg16(opcode - 0x90);
			uint16_t prev_ax = m_State.m_ax;
			m_State.m_ax = reg;
			reg = prev_ax;
			break;
		}
		case 0x98: /* CBW */ {
			if (m_State.m_ax & 0x80)
				m_State.m_ax = 0xff80 | m_State.m_ax & 0x7f;
			else
				m_State.m_ax = m_State.m_ax & 0x7f;
			break;
		}
		case 0x99: /* CWD */ {
			if (m_State.m_ax & 0x8000)
				m_State.m_dx = 0xffff;
			else
				m_State.m_dx = 0xffff;
			break;
		}
		case 0x9a: /* CALL Ap */ {
			uint16_t ip = IMM16;
			uint16_t cs = IMM16;
			Push16(m_State.m_cs);
			Push16(m_State.m_ip);
			m_State.m_cs = cs;
			m_State.m_ip = ip;
			break;
		}
		case 0x9b: /* WAIT */ {
			TODO; /* XXX Do we need this? */
			break;
		}
		case 0x9c: /* PUSHF */ {
			Push16(m_State.m_flags | State::FLAG_ON);
			break;
		}
		case 0x9d: /* POPF */ {
			m_State.m_flags = Pop16() | State::FLAG_ON;
			break;
		}
		case 0x9e: /* SAHF */ {
			m_State.m_flags = (m_State.m_flags & 0xff00) | (m_State.m_ax & 0xff00) >> 8;
			break;
		}
		case 0x9f: /* LAHF */ {
			m_State.m_ax = (m_State.m_ax & 0xff) | ((m_State.m_flags | State::FLAG_ON) & 0xff) << 8;
			break;
		}
		case 0xa0: /* MOV AL Ob */ {
			GET_IMM16;
			int seg = HandleSegmentOverride(SEG_DS);
			m_State.m_ax = (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(GetSReg16(seg), imm));
			break;
		}
		case 0xa1: /* MOV eAX Ov */ {
			GET_IMM16;
			int seg = HandleSegmentOverride(SEG_DS);
			m_State.m_ax = m_Memory.ReadWord(MakeAddr(GetSReg16(seg), imm));
			break;
		}
		case 0xa2: /* MOV Ob AL */ {
			GET_IMM16;
			int seg = HandleSegmentOverride(SEG_DS);
			m_Memory.WriteByte(MakeAddr(GetSReg16(seg), imm), m_State.m_ax & 0xff);
			break;
		}
		case 0xa3: /* MOV Ov eAX */ {
			GET_IMM16;
			int seg = HandleSegmentOverride(SEG_DS);
			m_Memory.WriteWord(MakeAddr(GetSReg16(seg), imm), m_State.m_ax);
			break;
		}
		case 0xa4: /* MOVSB */ {
			int delta = FlagDirection() ? -1 : 1;
			int seg = HandleSegmentOverride(SEG_DS);
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					m_Memory.WriteByte(
					 MakeAddr(m_State.m_es, m_State.m_di),
					 m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si))
					);
					m_State.m_si += delta; m_State.m_di += delta;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				m_Memory.WriteByte(
				 MakeAddr(m_State.m_es, m_State.m_di),
				 m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si))
				);
				m_State.m_si += delta; m_State.m_di += delta;
			}
			break;
		}
		case 0xa5: /* MOVSW */ {
			int delta = FlagDirection() ? -2 : 2;
			int seg = HandleSegmentOverride(SEG_DS);
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					m_Memory.WriteWord(
					 MakeAddr(m_State.m_es, m_State.m_di),
					 m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si))
					);
					m_State.m_si += delta; m_State.m_di += delta;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				m_Memory.WriteWord(
				 MakeAddr(m_State.m_es, m_State.m_di),
				 m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si))
				);
				m_State.m_si += delta; m_State.m_di += delta;
			}
			break;
		}
		case 0xa6: /* CMPSB */ {
			int delta = FlagDirection() ? -1 : 1;
			int seg = HandleSegmentOverride(SEG_DS);
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				bool break_on_zf = (m_State.m_prefix & (State::PREFIX_REPNZ)) != 0;
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					SUB8(m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)),
							 m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
					m_State.m_si += delta; m_State.m_di += delta;
					if (FlagZero() == break_on_zf)
						break;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				SUB8(m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)),
				     m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
				m_State.m_si += delta; m_State.m_di += delta;
			}
			break;
		}
		case 0xa7: /* CMPSW */ {
			int delta = FlagDirection() ? -2 : 2;
			int seg = HandleSegmentOverride(SEG_DS);
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				bool break_on_zf = (m_State.m_prefix & (State::PREFIX_REPNZ)) != 0;
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					SUB16(m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si)),
						 	  m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
					m_State.m_si += delta; m_State.m_di += delta;
					if (FlagZero() == break_on_zf)
						break;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				SUB16(m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si)),
				      m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
				m_State.m_si += delta; m_State.m_di += delta;
			}
			break;
		}
		case 0xa8: /* TEST AL Ib */ {
			GET_IMM8;
			AND8(m_State.m_ax & 0xff, imm);
			break;
		}
		case 0xa9: /* TEST eAX Iv */ {
			GET_IMM16;
			AND16(m_State.m_ax, imm);
			break;
		}
		case 0xaa: /* STOSB */ {
			int delta = FlagDirection() ? -1 : 1;
			uint8_t value = m_State.m_ax & 0xff;
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					m_Memory.WriteByte(MakeAddr(m_State.m_es, m_State.m_di), value);
					m_State.m_di += delta;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				m_Memory.WriteByte(MakeAddr(m_State.m_es, m_State.m_di), value);
				m_State.m_di += delta;
			}
			break;
		}
		case 0xab: /* STOSW */ {
			int delta = FlagDirection() ? -2 : 2;
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					m_Memory.WriteWord(MakeAddr(m_State.m_es, m_State.m_di), m_State.m_ax);
					m_State.m_di += delta;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				m_Memory.WriteWord(MakeAddr(m_State.m_es, m_State.m_di), m_State.m_ax);
				m_State.m_di += delta;
			}
			break;
		}
		case 0xac: /* LODSB */ {
			int seg = HandleSegmentOverride(SEG_DS);
			m_State.m_ax = (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(GetSReg16(seg), m_State.m_si));
			if (FlagDirection())
				m_State.m_si--;
			else
				m_State.m_si++;
			break;
		}
		case 0xad: /* LODSW */ {
			int seg = HandleSegmentOverride(SEG_DS);
			m_State.m_ax = m_Memory.ReadWord(MakeAddr(GetSReg16(seg), m_State.m_si));
			if (FlagDirection())
				m_State.m_si -= 2;
			else
				m_State.m_si += 2;
			break;
		}
		case 0xae: /* SCASB */ {
			int delta = FlagDirection() ? -1 : 1;
			uint8_t val = m_State.m_ax & 0xff;
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				bool break_on_zf = (m_State.m_prefix & (State::PREFIX_REPNZ)) != 0;
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					SUB8(val, m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
					m_State.m_di += delta;
					if (FlagZero() == break_on_zf)
						break;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				SUB8(val, m_Memory.ReadByte(MakeAddr(m_State.m_es, m_State.m_di)));
				m_State.m_di += delta;
			}
			break;
		}
		case 0xaf: /* SCASW */ {
			int delta = FlagDirection() ? -2 : 2;
			if (m_State.m_prefix & (State::PREFIX_REPZ | State::PREFIX_REPNZ)) {
				bool break_on_zf = (m_State.m_prefix & (State::PREFIX_REPNZ)) != 0;
				while (m_State.m_cx != 0) {
					m_State.m_cx--;
					SUB16(m_State.m_ax, m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
					m_State.m_di += delta;
					if (FlagZero() == break_on_zf)
						break;
				}
				m_State.m_prefix &= ~(State::PREFIX_REPZ | State::PREFIX_REPNZ);
			} else {
				SUB16(m_State.m_ax, m_Memory.ReadWord(MakeAddr(m_State.m_es, m_State.m_di)));
				m_State.m_di += delta;
			}
			break;
		}
		case 0xb0: /* MOV AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | imm;
			break;
		}
		case 0xb1: /* MOV CL Ib */ {
			GET_IMM8;
			m_State.m_cx = (m_State.m_cx & 0xff00) | imm;
			break;
		}
		case 0xb2: /* MOV DL Ib */ {
			GET_IMM8;
			m_State.m_dx = (m_State.m_dx & 0xff00) | imm;
			break;
		}
		case 0xb3: /* MOV BL Ib */ {
			GET_IMM8;
			m_State.m_bx = (m_State.m_bx & 0xff00) | imm;
			break;
		}
		case 0xb4: /* MOV AH Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff) | ((uint16_t)imm << 8);
			break;
		}
		case 0xb5: /* MOV CH Ib */ {
			GET_IMM8;
			m_State.m_cx = (m_State.m_cx & 0xff) | ((uint16_t)imm << 8);
			break;
		}
		case 0xb6: /* MOV DH Ib */ {
			GET_IMM8;
			m_State.m_dx = (m_State.m_dx & 0xff) | ((uint16_t)imm << 8);
			break;
		}
		case 0xb7: /* MOV BH Ib */ {
			GET_IMM8;
			m_State.m_bx = (m_State.m_bx & 0xff) | ((uint16_t)imm << 8);
			break;
		}
		case 0xb8: /* MOV eAX Iv */ {
			GET_IMM16;
			m_State.m_ax = imm;
			break;
		}
		case 0xb9: /* MOV eCX Iv */ {
			GET_IMM16;
			m_State.m_cx = imm;
			break;
		}
		case 0xba: /* MOV eDX Iv */ {
			GET_IMM16;
			m_State.m_dx = imm;
			break;
		}
		case 0xbb: /* MOV eBX Iv */ {
			GET_IMM16;
			m_State.m_bx = imm;
			break;
		}
		case 0xbc: /* MOV eSP Iv */ {
			GET_IMM16;
			m_State.m_sp = imm;
			break;
		}
		case 0xbd: /* MOV eBP Iv */ {
			GET_IMM16;
			m_State.m_bp = imm;
			break;
		}
		case 0xbe: /* MOV eSI Iv */ {
			GET_IMM16;
			m_State.m_si = imm;
			break;
		}
		case 0xbf: /* MOV eDI Iv */ {
			GET_IMM16;
			m_State.m_di = imm;
			break;
		}
		case 0xc0: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xc1: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xc2: /* RET Iw */ {
			GET_IMM16;
			m_State.m_ip = Pop16();
			m_State.m_sp += imm;
			break;
		}
		case 0xc3: /* RET */ {
			m_State.m_ip = Pop16();
			break;
		}
		case 0xc4: /* LES Gv Mp */
		case 0xc5: /* LDS Gv Mp */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			uint16_t& reg = GetReg16(MODRM_XXX(modrm));

			uint16_t new_off = ReadEA16(m_DecodeState);
			m_DecodeState.m_off += 2;
			uint16_t new_seg = ReadEA16(m_DecodeState);

			if (opcode == 0xc4)
				m_State.m_es = new_seg;
			else
				m_State.m_ds = new_seg;
			reg = new_off;
			break;
		}
		case 0xc6: /* MOV Eb Ib */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GET_IMM8;
			WriteEA8(m_DecodeState, imm);
			break;
		}
		case 0xc7: /* MOV Ev Iv */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);
			GET_IMM16;
			WriteEA16(m_DecodeState, imm);
			break;
		}
		case 0xc8: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xc9: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xca: /* RETF Iw */ {
			GET_IMM16;
			m_State.m_ip = Pop16();
			m_State.m_cs = Pop16();
			m_State.m_sp += imm;
			break;
		}
		case 0xcb: /* RETF */ {
			m_State.m_ip = Pop16();
			m_State.m_cs = Pop16();
			break;
		}
		case 0xcc: /* INT 3 */ {
			HandleInterrupt(INT_BREAKPOINT);
			break;
		}
		case 0xcd: /* INT Ib */ {
			GET_IMM8;
			HandleInterrupt(imm);
			break;
		}
		case 0xce: /* INTO */ {
			if (!FlagOverflow())
				HandleInterrupt(INT_OVERFLOW);
			break;
		}
		case 0xcf: /* IRET */ {
			m_State.m_ip = Pop16();
			m_State.m_cs = Pop16();
			m_State.m_flags = Pop16();
			break;
		}
		case 0xd0: /* GRP2 Eb 1 */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			unsigned int op = MODRM_XXX(modrm);
			uint8_t val = ReadEA8(m_DecodeState);
			switch(op) {
				case 0: // rol
					val = ROL8(val, 1);
					break;
				case 1: // ror
					val = ROR8(val, 1);
					break;
				case 2: // rcl
					val = RCL8(val, 1);
					break;
				case 3: // rcr
					val = RCR8(val, 1);
					break;
				case 4: // shl
					val = SHL8(val, 1);
					break;
				case 5: // shr
					val = SHR8(val, 1);
					break;
				case 6: // undefined
					INVALID_OPCODE;
					break;
				case 7: // sar
					val = SAR8(val, 1);
					break;
			}
			WriteEA8(m_DecodeState, val);
			break;
		}
		case 0xd1: /* GRP2 Ev 1 */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			unsigned int op = MODRM_XXX(modrm);
			uint16_t val = ReadEA16(m_DecodeState);
			switch(op) {
				case 0: // rol
					val = ROL16(val, 1);
					break;
				case 1: // ror
					val = ROR16(val, 1);
					break;
				case 2: // rcl
					val = RCL16(val, 1);
					break;
				case 3: // rcr
					val = RCR16(val, 1);
					break;
				case 4: // shl
					val = SHL16(val, 1);
					break;
				case 5: // shr
					val = SHR16(val, 1);
					break;
				case 6: // undefined
					INVALID_OPCODE;
					break;
				case 7: // sar
					val = SAR16(val, 1);
					break;
			}
			WriteEA16(m_DecodeState, val);
			break;
		}
		case 0xd2: /* GRP2 Eb CL */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			unsigned int op = MODRM_XXX(modrm);
			uint8_t val = ReadEA8(m_DecodeState);
			uint8_t cnt = m_State.m_cx & 0xff;
			switch(op) {
				case 0: // rol
					val = ROL8(val, cnt);
					break;
				case 1: // ror
					val = ROR8(val, cnt);
					break;
				case 2: // rcl
					val = RCL8(val, cnt);
					break;
				case 3: // rcr
					val = RCR8(val, cnt);
					break;
				case 4: // shl
					val = SHL8(val, cnt);
					break;
				case 5: // shr
					val = SHR8(val, cnt);
					break;
				case 6: // undefined
					INVALID_OPCODE;
					break;
				case 7: // sar
					val = SAR8(val, cnt);
					break;
			}
			WriteEA8(m_DecodeState, val);
			break;
		}
		case 0xd3: /* GRP2 Ev CL */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			unsigned int op = MODRM_XXX(modrm);
			uint16_t val = ReadEA16(m_DecodeState);
			uint8_t cnt = m_State.m_cx & 0xff;
			switch(op) {
				case 0: // rol
					val = ROL16(val, cnt);
					break;
				case 1: // ror
					val = ROR16(val, cnt);
					break;
				case 2: // rcl
					val = RCL16(val, cnt);
					break;
				case 3: // rcr
					val = RCR16(val, cnt);
					break;
				case 4: // shl
					val = SHL16(val, cnt);
					break;
				case 5: // shr
					val = SHR16(val, cnt);
					break;
				case 6: // undefined
					INVALID_OPCODE;
					break;
				case 7: // sar
					val = SAR16(val, cnt);
					break;
			}
			WriteEA16(m_DecodeState, val);
			break;
			break;
		}
		case 0xd4: /* AAM I0 */ {
			TODO;
			break;
		}
		case 0xd5: /* AAD I0 */ {
			TODO;
			break;
		}
		case 0xd6: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xd7: /* XLAT */ {
			int seg = HandleSegmentOverride(SEG_DS);
			m_State.m_ax = (m_State.m_ax & 0xff00) | m_Memory.ReadByte(MakeAddr(seg, m_State.m_bx + m_State.m_ax & 0xff));
			break;
		}
		case 0xd8: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xd9: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xda: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xdb: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xdc: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xdd: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xde: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xdf: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xe0: /* LOOPNZ Jb */ {
			m_State.m_cx--;
			DO_COND_JUMP(!FlagZero() && m_State.m_cx != 0);
			break;
		}
		case 0xe1: /* LOOPZ Jb */ {
			m_State.m_cx--;
			DO_COND_JUMP(FlagZero() && m_State.m_cx != 0);
			break;
		}
		case 0xe2: /* LOOP Jb */ {
			m_State.m_cx--;
			DO_COND_JUMP(m_State.m_cx != 0);
			break;
		}
		case 0xe3: /* JCXZ Jb */ {
			DO_COND_JUMP(m_State.m_cx == 0);
			break;
		}
		case 0xe4: /* IN AL Ib */ {
			GET_IMM8;
			m_State.m_ax = (m_State.m_ax & 0xff00) | m_IO.In8(imm);
			break;
		}
		case 0xe5: /* IN eAX Ib */ {
			GET_IMM8;
			m_State.m_ax = m_IO.In16(imm);
			break;
		}
		case 0xe6: /* OUT Ib AL */ {
			GET_IMM8;
			m_IO.Out8(imm, m_State.m_ax & 0xff);
			break;
		}
		case 0xe7: /* OUT Ib eAX */ {
			GET_IMM16;
			m_IO.Out16(imm, m_State.m_ax & 0xff);
			break;
		}
		case 0xe8: /* CALL Jv */ {
			GET_IMM16;
			Push16(m_State.m_ip);
			RelativeJump16(imm);
			break;
		}
		case 0xe9: /* JMP Jv */ {
			GET_IMM16;
			RelativeJump16(imm);
			break;
		}
		case 0xea: /* JMP Ap */ {
			m_State.m_ip = IMM16;
			m_State.m_cs = IMM16;
			break;
		}
		case 0xeb: /* JMP Jb */ {
			DO_COND_JUMP(true);
			break;
		}
		case 0xec: /* IN AL DX */ {
			m_State.m_ax = (m_State.m_ax & 0xff00) | m_IO.In8(m_State.m_dx);
			break;
		}
		case 0xed: /* IN eAX DX */ {
			m_State.m_ax = m_IO.In16(m_State.m_dx);
			break;
		}
		case 0xee: /* OUT DX AL */ {
			m_IO.Out8(m_State.m_dx, m_State.m_ax & 0xff);
			break;
		}
		case 0xef: /* OUT DX eAX */ {
			m_IO.Out16(m_State.m_dx, m_State.m_ax);
			break;
		}
		case 0xf0: /* LOCK */ {
			TODO; /* XXX Should we? */
			break;
		}
		case 0xf1: /* -- */ {
			INVALID_OPCODE;
			break;
		}
		case 0xf2: /* REPNZ */ {
			m_State.m_prefix |= State::PREFIX_REPNZ;
			break;
		}
		case 0xf3: /* REPZ */ {
			m_State.m_prefix |= State::PREFIX_REPZ;
			break;
		}
		case 0xf4: /* HLT */ {
			TODO;
			break;
		}
		case 0xf5: /* CMC */ {
			m_State.m_flags ^= State::FLAG_CF;
			break;
		}
		case 0xf6: /* GRP3a Eb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: /* TEST Eb Ib */ {
					GET_IMM8;
					AND8(ReadEA8(m_DecodeState), imm);
					break;
				}
				case 1: /* invalid */
					INVALID_OPCODE;
					break;
				case 2: /* NOT */
					WriteEA8(m_DecodeState, 0xFF - ReadEA8(m_DecodeState));
					break;
				case 3: /* NEG */
					WriteEA8(m_DecodeState, SUB8(0, ReadEA8(m_DecodeState)));
					break;
				case 4: /* MUL */
					MUL8(ReadEA8(m_DecodeState));
					break;
				case 5: /* IMUL */
					IMUL8(ReadEA8(m_DecodeState));
					break;
				case 6: /* DIV */
					DIV8(ReadEA8(m_DecodeState));
					break;
				case 7: /* IDIV */
					IDIV8(ReadEA8(m_DecodeState));
					break;
			}
			break;
		}
		case 0xf7: /* GRP3b Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: /* TEST Eb Iw */ {
					GET_IMM16;
					AND16(ReadEA16(m_DecodeState), imm);
					break;
				}
				case 1: /* invalid */
					INVALID_OPCODE;
					break;
				case 2: /* NOT */
					WriteEA16(m_DecodeState, 0xFFFF - ReadEA16(m_DecodeState));
					break;
				case 3: /* NEG */
					WriteEA16(m_DecodeState, SUB16(0, ReadEA16(m_DecodeState)));
					break;
				case 4: /* MUL */
					MUL16(ReadEA16(m_DecodeState));
					break;
				case 5: /* IMUL */
					IMUL16(ReadEA16(m_DecodeState));
					break;
				case 6: /* DIV */
					DIV16(ReadEA16(m_DecodeState));
					break;
				case 7: /* IDIV */
					IDIV16(ReadEA16(m_DecodeState));
					break;
			}
			break;
		}
		case 0xf8: /* CLC */ {
			m_State.m_flags &= ~State::FLAG_CF;
			break;
		}
		case 0xf9: /* STC */ {
			m_State.m_flags |= State::FLAG_CF;
			break;
		}
		case 0xfa: /* CLI */ {
			m_State.m_flags &= ~State::FLAG_IF;
			break;
		}
		case 0xfb: /* STI */ {
			m_State.m_flags |= State::FLAG_IF;
			break;
		}
		case 0xfc: /* CLD */ {
			m_State.m_flags &= ~State::FLAG_DF;
			break;
		}
		case 0xfd: /* STD */ {
			m_State.m_flags |= State::FLAG_DF;
			break;
		}
		case 0xfe: /* GRP4 Eb */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			uint8_t val = ReadEA8(m_DecodeState);
			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: // inc
					WriteEA8(m_DecodeState, INC8(val));
					break;
				case 1: // dec
					WriteEA8(m_DecodeState, DEC8(val));
					break;
				default: // invalid
					INVALID_OPCODE;
					break;
			}
			break;
		}
		case 0xff: /* GRP5 Ev */ {
			GET_MODRM;
			DecodeEA(modrm, m_DecodeState);

			uint16_t val = ReadEA16(m_DecodeState);
			unsigned int op = MODRM_XXX(modrm);
			switch(op) {
				case 0: /* INC eV */
					WriteEA16(m_DecodeState, INC16(val));
					break;
				case 1: /* DEC eV */
					WriteEA16(m_DecodeState, DEC16(val));
					break;
				case 2: /* CALL Ev */
					Push16(m_State.m_ip);
					m_State.m_ip = val;
					break;
				case 3: /* CALL Ep */ {
					Push16(m_State.m_cs);
					Push16(m_State.m_ip);
					m_State.m_ip = val;
					m_DecodeState.m_off += 2;
					m_State.m_cs = ReadEA16(m_DecodeState);
					break;
				}
				case 4: /* JMP Ev */
					m_State.m_ip = val;
					break;
				case 5: /* JMP Ep */ {
					m_State.m_ip = val;
					m_DecodeState.m_off += 2;
					m_State.m_cs = ReadEA16(m_DecodeState);
					break;
				}
				case 6: /* PUSH Ev */
					Push16(val);
					break;
				case 7: /* undefined */
					INVALID_OPCODE;
					break;
			}
			break;
		}
	}
}

void
CPUx86::Handle0FPrefix()
{
	uint8_t opcode = GetNextOpcode();
	TRACE("cs:ip=%04x:%04x opcode 0x0f 0x%02x\n", m_State.m_cs, m_State.m_ip - 1, opcode);
	switch(opcode) {
		case 0x34: /* SYSENTER - (ab)used for interrupt dispatch */ {
			GET_IMM8;
			m_Vectors.Invoke(*this, imm);
			break;
		}
		default: /* undefined */
			INVALID_OPCODE;
			break;
	}
}

uint8_t
CPUx86::GetNextOpcode()
{
	return m_Memory.ReadByte(MakeAddr(m_State.m_cs, m_State.m_ip++));
}

CPUx86::addr_t
CPUx86::MakeAddr(uint16_t seg, uint16_t off)
{
	return ((addr_t)seg << 4) + (addr_t)off;
}

void
CPUx86::DecodeEA(uint8_t modrm, DecodeState& oState)
{
	uint8_t mod = (modrm & 0xc0) >> 6;
	uint8_t rm = modrm & 7;
	oState.m_reg = MODRM_XXX(modrm);

	switch(mod) {
		case 0: /* DISP=0*, disp-low and disp-hi are absent */ {
			if (rm == 6) {
				// except if mod==00 and rm==110, then EA = disp-hi; disp-lo
				GET_IMM16;
				oState.m_type = DecodeState::T_MEM;
				oState.m_seg = HandleSegmentOverride(SEG_DS);
				oState.m_off = imm;
				oState.m_disp = 0;
				return;
			} else {
				oState.m_disp = 0;
			}
			break;
		}
		case 1: /* DISP=disp-low sign-extended to 16-bits, disp-hi absent */ {
			GET_IMM8;
			oState.m_disp = (addr_t)((int16_t)imm);
			break;
		}
		case 2: /* DISP=disp-hi:disp-lo */ {
			GET_IMM16;
			oState.m_disp = imm;
			break;
		}
		case 3: /* rm treated as reg field */ {
			oState.m_disp = 0;
		}
	}

	if (mod != 3) {
		oState.m_type = DecodeState::T_MEM;
		switch(rm) {
			case 0: // (bx) + (si) + disp
				oState.m_seg = HandleSegmentOverride(SEG_DS);
				oState.m_off = m_State.m_bx + m_State.m_si;
				break;
			case 1: // (bx) + (di) + disp
				oState.m_seg = HandleSegmentOverride(SEG_DS);
				oState.m_off = m_State.m_bx + m_State.m_di;
				break;
			case 2: // (bp) + (si) + disp
				oState.m_seg = HandleSegmentOverride(SEG_SS);
				oState.m_off = m_State.m_bp + m_State.m_si;
				break;
			case 3: // (bp) + (di) + disp
				oState.m_seg = HandleSegmentOverride(SEG_SS);
				oState.m_off = m_State.m_bp + m_State.m_di;
				break;
			case 4: // (si) + disp
				oState.m_seg = HandleSegmentOverride(SEG_DS);
				oState.m_off = m_State.m_si;
				break;
			case 5: // (di) + disp
				oState.m_seg = HandleSegmentOverride(SEG_DS);
				oState.m_off = m_State.m_di;
				break;
			case 6: // (bp) + disp
				oState.m_seg = HandleSegmentOverride(SEG_SS);
				oState.m_off = m_State.m_bp;
				break;
			case 7: // (bx) + disp
				oState.m_seg = HandleSegmentOverride(SEG_DS);
				oState.m_off = m_State.m_bx;
				break;
		}
	} else /* mod == 3, r/m treated as reg field */ {
		oState.m_type = DecodeState::T_REG;
		oState.m_reg = rm;
	}
}

uint16_t
CPUx86::GetAddrEA16(const DecodeState& oState)
{
	if (oState.m_type == DecodeState::T_REG)
		return GetReg16(oState.m_reg);

	return oState.m_off + oState.m_disp;
}

uint16_t
CPUx86::ReadEA16(const DecodeState& oState)
{
	if (oState.m_type == DecodeState::T_REG) {
		return GetReg16(oState.m_reg);
	}

	uint16_t seg_base = 0;
	switch(oState.m_seg) {
		case SEG_ES: seg_base = m_State.m_es; break;
		case SEG_CS: seg_base = m_State.m_cs; break;
		case SEG_DS: seg_base = m_State.m_ds; break;
		case SEG_SS: seg_base = m_State.m_ss; break;
		default: assert(0); // what's this?
	}

	TRACE("read(16) @ %x:%x -> %x\n", seg_base, oState.m_off + oState.m_disp,
		m_Memory.ReadWord(MakeAddr(seg_base, oState.m_off + oState.m_disp)));
	return m_Memory.ReadWord(MakeAddr(seg_base, oState.m_off + oState.m_disp));
}

void
CPUx86::WriteEA16(const DecodeState& oState, uint16_t value)
{
	if (oState.m_type == DecodeState::T_REG) {
		GetReg16(oState.m_reg) = value;
		return;
	}

	uint16_t seg_base = 0;
	switch(oState.m_seg) {
		case SEG_ES: seg_base = m_State.m_es; break;
		case SEG_CS: seg_base = m_State.m_cs; break;
		case SEG_DS: seg_base = m_State.m_ds; break;
		case SEG_SS: seg_base = m_State.m_ss; break;
		default: assert(0); // what's this?
	}

	TRACE("write(16) @ %x:%x val %x\n", seg_base, oState.m_off + oState.m_disp, value);
	m_Memory.WriteWord(MakeAddr(seg_base, oState.m_off + oState.m_disp), value);
}

uint8_t
CPUx86::ReadEA8(const DecodeState& oState)
{
	if (oState.m_type == DecodeState::T_REG) {
		unsigned int shift;
		uint16_t& v = GetReg8(oState.m_reg, shift);
		return (v >> shift) & 0xff;
	}

	uint16_t seg_base = 0;
	switch(oState.m_seg) {
		case SEG_ES: seg_base = m_State.m_es; break;
		case SEG_CS: seg_base = m_State.m_cs; break;
		case SEG_DS: seg_base = m_State.m_ds; break;
		case SEG_SS: seg_base = m_State.m_ss; break;
		default: assert(0); // what's this?
	}

	TRACE("read(8) @ %x:%x\n", seg_base, oState.m_off + oState.m_disp);
	return m_Memory.ReadByte(MakeAddr(seg_base, oState.m_off + oState.m_disp));
}

void
CPUx86::WriteEA8(const DecodeState& oState, uint8_t val)
{
	if (oState.m_type == DecodeState::T_REG) {
		unsigned int shift;
		uint16_t& v = GetReg8(oState.m_reg, shift);
		SetReg8(v, shift, val);
		return;
	}

	uint16_t seg_base = 0;
	switch(oState.m_seg) {
		case SEG_ES: seg_base = m_State.m_es; break;
		case SEG_CS: seg_base = m_State.m_cs; break;
		case SEG_DS: seg_base = m_State.m_ds; break;
		case SEG_SS: seg_base = m_State.m_ss; break;
		default: assert(0); // what's this?
	}

	TRACE("write(8) @ %x:%x val %\n", seg_base, oState.m_off + oState.m_disp, val);
	m_Memory.WriteByte(MakeAddr(seg_base, oState.m_off + oState.m_disp), val);
}

uint16_t&
CPUx86::GetReg16(int n)
{	
	switch(n) {
		case 0: return m_State.m_ax;
		case 1: return m_State.m_cx;
		case 2: return m_State.m_dx;
		case 3: return m_State.m_bx;
		case 4: return m_State.m_sp;
		case 5: return m_State.m_bp;
		case 6: return m_State.m_si;
		case 7: return m_State.m_di;
	}
	assert(0); // what's this?
}

uint16_t&
CPUx86::GetSReg16(int n)
{
	switch(n) {
		case SEG_ES: return m_State.m_es;
		case SEG_CS: return m_State.m_cs;
		case SEG_SS: return m_State.m_ss;
		case SEG_DS: return m_State.m_ds;
	}
	assert(0); // what's this?
}

uint16_t&
CPUx86::GetReg8(int n, unsigned int& shift)
{	
	shift = (n > 3) ? 8 : 0;
	switch(n & 3) {
		case 0: return m_State.m_ax;
		case 1: return m_State.m_cx;
		case 2: return m_State.m_dx;
		case 3: return m_State.m_bx;
	}

	/* NOTREACHED */
}

void
CPUx86::SetReg8(uint16_t& reg, unsigned int shift, uint8_t val)
{
	if (shift > 0) {
		reg = (reg & 0x00ff) | (val << 8);
	} else {
		reg = (reg & 0xff00) | val;
	}
}

void
CPUx86::Push16(uint16_t value)
{
	m_Memory.WriteWord(MakeAddr(m_State.m_ss, m_State.m_sp - 2), value);
	m_State.m_sp -= 2;
}

uint16_t
CPUx86::Pop16()
{
	uint16_t value = m_Memory.ReadWord(MakeAddr(m_State.m_ss, m_State.m_sp));
	m_State.m_sp += 2;
	return value;
}

void
CPUx86::State::Dump()
{
	fprintf(stderr, "  ax=%04x bx=%04x cx=%04x dx=%04x si=%04x di=%04x bp=%04x flags=%04x\n",
	 m_ax, m_bx, m_cx, m_dx, m_si, m_di, m_bp, m_flags);
	fprintf(stderr, "  cs:ip=%04x:%04x ds=%04x es=%04x ss:sp=%04x:%04x\n",
	 m_cs, m_ip, m_ds, m_es, m_ss, m_sp);
}

void
CPUx86::Dump()
{
	m_State.Dump();
}

uint8_t
CPUx86::OR8(uint8_t a, uint8_t b)
{
	uint8_t op1 = a | b;
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP8(op1);
	return op1;
}

uint16_t
CPUx86::OR16(uint16_t a, uint16_t b)
{
	uint16_t op1 = a | b;
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP16(op1);
	return op1;
}

uint8_t
CPUx86::AND8(uint8_t a, uint8_t b)
{
	uint8_t op1 = a & b;
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP8(op1);
	return op1;
}

uint16_t
CPUx86::AND16(uint16_t a, uint16_t b)
{
	uint16_t op1 = a & b;
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP16(op1);
	return op1;
}

uint8_t
CPUx86::XOR8(uint8_t a, uint8_t b)
{
	uint8_t op1 = a ^ b;
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP8(op1);
	return op1;
}

uint16_t
CPUx86::XOR16(uint16_t a, uint16_t b)
{
	uint16_t op1 = a ^ b;
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP16(op1);
	return op1;
}

void
CPUx86::SetFlagsArith8(uint8_t a, uint8_t b, uint16_t res)
{
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP8(res);
	if (res & 0xff00)
		m_State.m_flags |= State::FLAG_CF;
	if ((a ^ res) & (a ^ b) & 0x80)
		m_State.m_flags |= State::FLAG_OF;
	if ((a ^ b ^ res) & 0x10)
		m_State.m_flags |= State::FLAG_AF;
}

void
CPUx86::SetFlagsArith16(uint16_t a, uint16_t b, uint32_t res)
{
	m_State.m_flags &= ~(State::FLAG_OF | State::FLAG_SF | State::FLAG_ZF | State::FLAG_PF | State::FLAG_CF);
	SetFlagsSZP16(res);
	if (res & 0xffff0000)
		m_State.m_flags |= State::FLAG_CF;
	if ((a ^ res) & (a ^ b) & 0x8000)
		m_State.m_flags |= State::FLAG_OF;
	if ((a ^ b ^ res) & 0x10)
		m_State.m_flags |= State::FLAG_AF;
}

uint8_t
CPUx86::ADD8(uint8_t a, uint8_t b)
{
	uint16_t res = a + b;
	SetFlagsArith8(a, b, res);
	return res & 0xff;
}

uint16_t
CPUx86::ADD16(uint16_t a, uint16_t b)
{
	uint32_t res = a + b;
	SetFlagsArith16(a, b, res);
	return res & 0xffff;
}

uint8_t
CPUx86::ADC8(uint8_t a, uint8_t b)
{
	uint16_t res = a + b + FlagCarry() ? 1 : 0;
	SetFlagsArith8(a, b, res);
	return res & 0xff;
}

uint16_t
CPUx86::ADC16(uint16_t a, uint16_t b)
{
	uint32_t res = a + b + FlagCarry() ? 1 : 0;
	SetFlagsArith16(a, b, res);
	return res & 0xffff;
}

uint8_t
CPUx86::SUB8(uint8_t a, uint8_t b)
{
	uint16_t res = a - b;
	SetFlagsArith8(a, b, res);
	return res & 0xff;
}

uint16_t
CPUx86::SUB16(uint16_t a, uint16_t b)
{
	uint32_t res = a - b;
	SetFlagsArith16(a, b, res);
	return res & 0xffff;
}

uint8_t
CPUx86::SBB8(uint8_t a, uint8_t b)
{
	uint16_t res = a - b - FlagCarry() ? 1 : 0;
	SetFlagsArith8(a, b, res);
	return res & 0xff;
}

uint16_t
CPUx86::SBB16(uint16_t a, uint16_t b)
{
	uint32_t res = a - b - FlagCarry() ? 1 : 0;
	SetFlagsArith16(a, b, res);
	return res & 0xffff;
}

uint8_t
CPUx86::INC8(uint8_t a)
{
	bool carry = FlagCarry();
	uint8_t res = ADD8(a, 1);
	if (carry)
		m_State.m_flags |= State::FLAG_CF;
	else
		m_State.m_flags &= ~State::FLAG_CF;
	return res;
}

uint16_t
CPUx86::INC16(uint16_t a)
{
	bool carry = FlagCarry();
	uint16_t res = ADD16(a, 1);
	if (carry)
		m_State.m_flags |= State::FLAG_CF;
	else
		m_State.m_flags &= ~State::FLAG_CF;
	return res;
}

uint8_t
CPUx86::DEC8(uint8_t a)
{
	bool carry = FlagCarry();
	uint8_t res = SUB8(a, 1);
	if (carry)
		m_State.m_flags |= State::FLAG_CF;
	else
		m_State.m_flags &= ~State::FLAG_CF;
	return res;
}

uint16_t
CPUx86::DEC16(uint16_t a)
{
	bool carry = FlagCarry();
	uint16_t res = SUB16(a, 1);
	if (carry)
		m_State.m_flags |= State::FLAG_CF;
	else
		m_State.m_flags &= ~State::FLAG_CF;
	return res;
}

#define EMIT_ROL(BITS) \
uint##BITS##_t \
CPUx86::ROL##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	uint8_t cnt = n % BITS; \
	if (cnt > 0) { \
		v = (v << cnt) | (v >> (BITS - cnt)); \
		SetFlag(State::FLAG_CF, v & 1); \
	} \
	if (n == 1) \
		SetFlag(State::FLAG_OF, ((v & (1 << (BITS - 1))) ^ (FlagCarry() ? 1 : 0))); \
}

#define EMIT_ROR(BITS) \
uint##BITS##_t \
CPUx86::ROR##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	uint8_t cnt = n % BITS; \
	if (cnt > 0) { \
		v = (v >> cnt) | (v << (BITS - cnt)); \
		SetFlag(State::FLAG_CF, v & (1 << (BITS - 1))); \
	} \
	if (n == 1) \
		SetFlag(State::FLAG_OF, (v & (1 << (BITS - 1))) ^ (v & (1 << (BITS - 2)))); \
}

#define EMIT_RCL(BITS) \
uint##BITS##_t \
CPUx86::RCL##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	uint8_t cnt = (n & 0x1f) % (BITS + 1); \
	if (cnt > 0) { \
		uint8_t tmp = (v << cnt) | ((FlagCarry() ? 1 : 0) << (cnt - 1)) | (v >> ((BITS + 1) - cnt)); \
		SetFlag(State::FLAG_CF, (v >> (BITS - cnt)) & 1); \
		v = tmp; \
	} \
	if (n == 1) \
		SetFlag(State::FLAG_OF, ((v & (1 << (BITS - 1))) ^ (FlagCarry() ? 1 : 0))); \
	return v; \
}

#define EMIT_RCR(BITS) \
uint##BITS##_t \
CPUx86::RCR##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	if (n == 1) \
		SetFlag(State::FLAG_OF, ((v & (1 << (BITS - 1))) ^ (FlagCarry() ? 1 : 0))); \
	uint8_t cnt = (n & 0x1f) % (BITS + 1); \
	if (cnt == 0) \
		return v; \
	uint8_t tmp = (v >> cnt) | ((FlagCarry() ? 1 : 0)) << (BITS - cnt) | (v << ((BITS + 1) - cnt)); \
	SetFlag(State::FLAG_CF, (v >> (cnt - 1) & 1)); \
	return tmp; \
}

#define EMIT_SHL(BITS) \
uint##BITS##_t \
CPUx86::SHL##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	uint8_t cnt = n & 0x1f; \
	if (cnt < BITS) { \
		if (cnt > 0) \
			SetFlag(State::FLAG_CF, v & (1 << (BITS - cnt))); \
		return v << cnt; \
	} else { \
		SetFlag(State::FLAG_CF, false); \
		return 0; \
	} \
}

#define EMIT_SHR(BITS) \
uint##BITS##_t \
CPUx86::SHR##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	uint8_t cnt = n & 0x1f; \
	if (cnt < BITS) { \
		if (cnt > 0) \
			SetFlag(State::FLAG_CF, v & (1 << cnt)); \
		v >>= cnt; \
	} else { \
		v = 0; \
		SetFlag(State::FLAG_CF, false); \
	} \
	if (n == 1) \
		SetFlag(State::FLAG_OF, (v & (1 << (BITS - 1))) ^ (v & (1 << (BITS - 2)))); \
	SetFlagsSZP##BITS(v); \
	return v; \
}

#define EMIT_SAR(BITS) \
uint##BITS##_t \
CPUx86::SAR##BITS(uint##BITS##_t v, uint8_t n) \
{ \
	uint8_t cnt = n & 0x1f; \
	if (cnt > 0) \
		SetFlag(State::FLAG_CF, v & (1 << cnt)); \
	if (cnt < BITS) { \
		if (v & (1 << (BITS - 1))) { \
			v = (v >> cnt) | (0xff << (BITS - cnt)); \
		} else { \
			v >>= cnt; \
		} \
	} else /* cnt >= BITS */ { \
		if (v & (1 << (BITS - 1))) { \
			SetFlag(State::FLAG_CF, true); \
			v = (1 << BITS) - 1; \
		} else { \
			SetFlag(State::FLAG_CF, false); \
			v = 0; \
		} \
	} \
	if (n == 1) \
		SetFlag(State::FLAG_OF, 0); \
	SetFlagsSZP##BITS(v); \
	return v; \
}

EMIT_ROL(8)
EMIT_ROL(16)
EMIT_ROR(8)
EMIT_ROR(16)
EMIT_RCL(8)
EMIT_RCL(16)
EMIT_RCR(8)
EMIT_RCR(16)
EMIT_SHL(8)
EMIT_SHL(16)
EMIT_SHR(8)
EMIT_SHR(16)
EMIT_SAR(8)
EMIT_SAR(16)

void
CPUx86::SetFlagsSZP8(uint8_t n)
{
	if (n & 0x80)
		m_State.m_flags |= State::FLAG_SF;
	if (n == 0)
		m_State.m_flags |= State::FLAG_ZF;
	register uint8_t pf = ~(
	 (n & 0x80) ^ (n & 0x40) ^ (n & 0x20) ^ 
	 (n & 0x10) ^ (n & 0x08) ^ (n & 0x04) ^ 
	 (n & 0x02) ^ (n & 0x01)
	);
	if (pf & 1)
		m_State.m_flags |= State::FLAG_PF;
}

void
CPUx86::SetFlag(uint16_t flag, bool set)
{
	if (set)
		m_State.m_flags |= flag;
	else
		m_State.m_flags &= ~flag;
}

void
CPUx86::SetFlagsSZP16(uint16_t n)
{
	if (n & 0x8000)
		m_State.m_flags |= State::FLAG_SF;
	if (n == 0)
		m_State.m_flags |= State::FLAG_ZF;
	register uint8_t pf = ~(
	 (n & 0x80) ^ (n & 0x40) ^ (n & 0x20) ^ 
	 (n & 0x10) ^ (n & 0x08) ^ (n & 0x04) ^ 
	 (n & 0x02) ^ (n & 0x01)
	);
	if (pf & 1)
		m_State.m_flags |= State::FLAG_PF;
}

void
CPUx86::MUL8(uint8_t a)
{
	m_State.m_flags &= ~(State::FLAG_CF | State::FLAG_OF);
	m_State.m_ax = (m_State.m_ax & 0xff) * (uint16_t)a;
	if (m_State.m_ax >= 0x100)
		m_State.m_flags |= (State::FLAG_CF | State::FLAG_OF);
}

void
CPUx86::MUL16(uint16_t a)
{
	m_State.m_flags &= ~(State::FLAG_CF | State::FLAG_OF);
	m_State.m_dx = (m_State.m_ax * a) >> 16;
	m_State.m_ax = (m_State.m_ax * a) & 0xffff;
	if (m_State.m_dx != 0)
		m_State.m_flags |= (State::FLAG_CF | State::FLAG_OF);
}

void
CPUx86::IMUL8(uint8_t a)
{
	m_State.m_flags &= ~(State::FLAG_CF | State::FLAG_OF);
	m_State.m_ax = (m_State.m_ax & 0xff) * (uint16_t)a;
	uint8_t ah = (m_State.m_ax & 0xff00) >> 8;
	SetFlag((State::FLAG_CF | State::FLAG_OF), ah == 0 || ah == 0xff);
}

void
CPUx86::IMUL16(uint16_t a)
{
	m_State.m_flags &= ~(State::FLAG_CF | State::FLAG_OF);
	uint32_t res = (uint32_t)m_State.m_ax * (uint32_t)a;
	m_State.m_dx = res >> 16;
	m_State.m_ax = res & 0xffff;
	SetFlag((State::FLAG_CF | State::FLAG_OF), m_State.m_dx == 0 || m_State.m_dx == 0xffff);
}

void
CPUx86::DIV8(uint8_t a)
{
	if (a == 0)
		SignalInterrupt(0);
	if ((m_State.m_ax / a) > 0xff)
		SignalInterrupt(0);
	uint16_t ax = m_State.m_ax;
	m_State.m_ax =
	 	((ax % a) & 0xff) << 8 |
		((ax / a) & 0xff);
}

void
CPUx86::DIV16(uint16_t a)
{
	if (a == 0)
		SignalInterrupt(INT_DIV_BY_ZERO);
	uint32_t v = (m_State.m_dx << 16) | m_State.m_ax;
	if ((v / a) > 0xffff)
		SignalInterrupt(INT_DIV_BY_ZERO);
	m_State.m_ax = (v / a) & 0xffff;
	m_State.m_dx = (v % a) & 0xffff;
}

void
CPUx86::IDIV8(uint8_t a)
{
	if (a == 0)
		SignalInterrupt(0);
	int8_t res = m_State.m_ax / a;
	if (((int16_t)m_State.m_ax > 0 && res > 0x7f) ||
	    ((int16_t)m_State.m_ax < 0 && res < 0x80))
		SignalInterrupt(INT_DIV_BY_ZERO);
	uint16_t ax = m_State.m_ax;
	m_State.m_ax =
	 	((ax % a) & 0xff) << 8 |
		((ax / a) & 0xff);
}

void
CPUx86::IDIV16(uint16_t a)
{
	if (a == 0)
		SignalInterrupt(INT_DIV_BY_ZERO);
	int32_t v = (m_State.m_dx << 16) | m_State.m_ax;
	if ((v > 0 && (v / a) > 0x7ffff) ||
	    (v < 0 && (v / a) < 0x80000))
		SignalInterrupt(INT_DIV_BY_ZERO);
	m_State.m_ax = (v / a) & 0xffff;
	m_State.m_dx = (v % a) & 0xffff;
}

void
CPUx86::SignalInterrupt(uint8_t no)
{
	abort(); // TODO
}

void
CPUx86::HandleInterrupt(uint8_t no)
{
	addr_t addr = MakeAddr(0, no * 4);
	uint16_t off = m_Memory.ReadWord(addr + 0);

	// Push flags and return address
	Push16(m_State.m_flags | State::FLAG_ON);
	Push16(m_State.m_cs);
	Push16(m_State.m_ip);

	// Transfer control to interrupt
	m_State.m_cs = m_Memory.ReadWord(addr + 2);
	m_State.m_ip = m_Memory.ReadWord(addr + 0);

	TRACE("HandleInterrupt(): no=%x -> %04x:%04x\n", no, m_State.m_cs, m_State.m_ip);

	// XXX Kludge
	if (m_State.m_cs == 0 && m_State.m_ip == 0)
		abort();
	

#if 0
	/* XXX KLUDGE */
	if (no == 0x10 && m_State.m_ax >> 8 == 0xf) {
		/* Video: get video mode; we return 80x25x16 */
		m_State.m_ax = 3;
		return;
	}

	/* XXX BEUN */
	if (no == 0x20) {
		m_State.m_ip -= 2;
		return;
	}

	if (no == 0x16) {
		uint16_t ah = (m_State.m_ax & 0xff00) >> 8;
		switch(ah) {
			case 1: // check for key
				SetFlag(State::FLAG_ZF, true);
				break;
			default:
				TODO;
		}
		return;
	}
#endif
}

void
CPUx86::RelativeJump8(uint8_t n)
{
	if (n & 0x80)
		m_State.m_ip -= 0x100 - n;
	else
		m_State.m_ip += (uint16_t)n;
}

void
CPUx86::RelativeJump16(uint16_t n)
{
	if (n & 0x8000)
		m_State.m_ip -= 0x10000 - n;
	else
		m_State.m_ip += n;
}

int
CPUx86::HandleSegmentOverride(int def)
{
	if ((m_State.m_prefix & State::PREFIX_SEG) == 0)
		return def;
	m_State.m_prefix &= ~State::PREFIX_SEG;
	return m_State.m_seg_override;
}

/* vim:set ts=2 sw=2: */
