#ifndef __CPUX86_H__
#define __CPUX86_H__

#include <stdint.h>

class IO;
class Memory;
class Vectors;

class CPUx86
{
  public:
    typedef uint32_t addr_t;

    CPUx86(Memory& oMemory, IO& oIO, Vectors& oVector);
    ~CPUx86();

    /*! \brief Fetches and executes the next instruction
     *  \returns Number of clock cycles used
     */
    void RunInstruction();

    //! \brief Resets the CPU
    void Reset();

    void Dump();

    // XXX
    void SetupForCOM(uint16_t seg)
    {
        m_State.m_cs = seg;
        m_State.m_ip = 0x100;
        m_State.m_ss = seg;
        m_State.m_sp = 0xfff8;
        m_State.m_ds = seg;
        m_State.m_es = seg;
    }

    //! \brief CPU state
    class State
    {
      public:
        uint16_t m_ax, m_cx, m_dx, m_bx, m_sp, m_bp, m_si, m_di, m_ip;
        uint16_t m_es, m_cs, m_ss, m_ds;
        uint16_t m_flags;
        uint16_t m_prefix;
        uint16_t m_seg_override;

        static const uint16_t FLAG_CF = (1 << 0);
        static const uint16_t FLAG_ON = (1 << 1); /* This flag is always set */
        static const uint16_t FLAG_PF = (1 << 2);
        static const uint16_t FLAG_AF = (1 << 4);
        static const uint16_t FLAG_ZF = (1 << 6);
        static const uint16_t FLAG_SF = (1 << 7);
        static const uint16_t FLAG_TF = (1 << 8);
        static const uint16_t FLAG_IF = (1 << 9);
        static const uint16_t FLAG_DF = (1 << 10);
        static const uint16_t FLAG_OF = (1 << 11);

        static const unsigned int PREFIX_REPZ = (1 << 0);
        static const unsigned int PREFIX_REPNZ = (1 << 1);
        static const unsigned int PREFIX_SEG = (1 << 2);

        void Dump();
    };
    State& GetState() { return m_State; }
    const State& GetState() const { return m_State; }
    Memory& GetMemory() { return m_Memory; }
    const Memory& GetMemory() const { return m_Memory; }
    IO& GetIO() { return m_IO; }
    const IO& GetIO() const { return m_IO; }

    static addr_t MakeAddr(uint16_t seg, uint16_t off);

  protected:
    void Push16(uint16_t value);
    uint16_t Pop16();

    class DecodeState
    {
      public:
        enum Type { T_REG, T_MEM };

        Type m_type;
        uint16_t m_seg, m_off;
        uint16_t m_reg;
        addr_t m_disp;
    };
    DecodeState m_DecodeState;
    void DecodeEA(uint8_t modrm, DecodeState& oState);
    uint16_t& GetReg16(int n);
    uint16_t& GetReg8(int n, unsigned int& shift);
    void SetReg8(uint16_t& reg, unsigned int shift, uint8_t val);
    uint16_t& GetSReg16(int n);
    uint16_t ReadEA16(const DecodeState& oState);
    uint16_t GetAddrEA16(const DecodeState& oState);
    void WriteEA16(const DecodeState& oState, uint16_t value);
    uint8_t ReadEA8(const DecodeState& oState);
    void WriteEA8(const DecodeState& oState, uint8_t value);

    void Handle0FPrefix();

    void HandleInterrupt(uint8_t no);
    void SignalInterrupt(uint8_t no);

    // These must be in sync with the x86 segment values (Sw)
    static const int SEG_ES = 0;
    static const int SEG_CS = 1;
    static const int SEG_SS = 2;
    static const int SEG_DS = 3;

    // Interrupt values
    static const unsigned int INT_DIV_BY_ZERO = 0;
    static const unsigned int INT_SINGLE_STEP = 1;
    static const unsigned int INT_NMI = 2;
    static const unsigned int INT_BREAKPOINT = 3;
    static const unsigned int INT_OVERFLOW = 4;

  private:
    //! \brief Retrieves the next byte from cs:ip
    uint8_t GetNextOpcode();

    //! \brief Memory we use
    Memory& m_Memory;

    //! \brief IO space we use
    IO& m_IO;

    //! \brief Current CPU state
    State m_State;

    //! \brief Interrupt vectors in use
    Vectors& m_Vectors;
};

#endif /* __CPUX86_H__ */
