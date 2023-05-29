#include "dos.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cpux86.h"
#include "memory.h"
#include "vectors.h"

#define TRACE_INT(x...) fprintf(stderr, "[dos-int] " x)
#define TRACE_EXE(x...) fprintf(stderr, "[dos-exe] " x)

namespace
{
    struct EXEHeader {
        uint16_t eh_signature;
    #define DOS_EXEHEADER_SIGNATURE 0x5a4d /* 'MZ' */
        uint16_t eh_bytes_in_last_block;
        uint16_t eh_blocks_in_file;
        uint16_t eh_num_relocs;
        uint16_t eh_header_paragraphs;
        uint16_t eh_min_extra_paragraphs;
        uint16_t eh_max_extra_paragraphs;
        uint16_t eh_ss;
        uint16_t eh_sp;
        uint16_t eh_checksum;
        uint16_t eh_ip;
        uint16_t eh_cs;
        uint16_t eh_reloc_table_offset;
        uint16_t eh_overlay_number;
    } PACKED;

    struct EXERelocation {
        uint16_t er_offset;
        uint16_t er_segment;
    } PACKED;

    struct ProgramSegmentPrefix {
        uint8_t psp_exit[2];
        uint16_t psp_mem_size;
        uint8_t psp_unused1;
        uint8_t psp_cpm_entry;
        uint16_t psp_cpm_segment_size;
        uint32_t psp_terminate_addr;
        uint32_t psp_break_addr;
        uint32_t psp_error_addr;
        uint16_t psp_parent_seg;
        uint8_t psp_open_tab[20];
        uint16_t psp_env_seg;
        uint32_t psp_ss_sp;
        uint16_t psp_max_open;
        uint32_t psp_open_addr;
        uint32_t psp_prev_psp;
        uint8_t psp_unused2[20];
        uint8_t psp_fn_disp[3];
        uint8_t psp_unused3[9];
        uint8_t psp_fcb[36];
        uint8_t psp_arg[128];
    } PACKED;
    static_assert(sizeof(struct ProgramSegmentPrefix) == 256);

    void CreatePSP(ProgramSegmentPrefix& oPSP)
    {
        memset(&oPSP, 0, sizeof(oPSP));
        oPSP.psp_exit[0] = 0xcd;
        oPSP.psp_exit[1] = 0x20; /* INT 20 */
        oPSP.psp_fn_disp[0] = 0xcd;
        oPSP.psp_fn_disp[1] = 0x21; /* INT 21 */
        oPSP.psp_fn_disp[2] = 0xcb; /* RETN */
    }

}

DOS::DOS(CPUx86& oCPU, Memory& oMemory, Vectors& oVectors)
    : m_CPU(oCPU), m_Memory(oMemory), m_Vectors(oVectors)
{
}

void DOS::Reset()
{
    m_Vectors.Register(0x20, *this);
    m_Vectors.Register(0x21, *this);
    m_Vectors.Register(0x22, *this);
}

namespace
{
    template<typename T>
    bool ReadFromStream(std::ifstream& ifs, T& buffer)
    {
        return ifs.read(reinterpret_cast<char*>(&buffer), sizeof(T)) && ifs.gcount() == sizeof(T);
    }
}

DOS::ErrorCode DOS::LoadEXE(std::ifstream& ifs)
{
    if (!ifs.good())
        return ErrorCode::FileNotFound;

    /* Read EXE Header */
    struct EXEHeader header;
    ifs.seekg(0);
    if (!ReadFromStream(ifs, header))
        return ErrorCode::ReadFault;
    /* XXX Convert header to native format */
    if (header.eh_signature != DOS_EXEHEADER_SIGNATURE)
        return ErrorCode::InvalidFormat;
    if (header.eh_blocks_in_file >= 512)
        return ErrorCode::InvalidFormat;

    /* Determine exe size */
    uint16_t exe_size = header.eh_blocks_in_file * 512 - (512 - header.eh_bytes_in_last_block);
    exe_size -= header.eh_header_paragraphs * 16;

    /*
     * XXX Hardcode the initial EXE segment and place the BSS seg right after the
     *     exe segment for now. We assume it fits XXX
     */
    uint16_t seg = 0x600;
    uint16_t exe_seg = seg + 16;
    uint16_t bss_seg = exe_seg + (exe_size + 15) / 16;

    /* Create a PSP */
    ProgramSegmentPrefix* oPSP =
        reinterpret_cast<ProgramSegmentPrefix*>(m_Memory.GetPointer(CPUx86::MakeAddr(seg, 0), sizeof(*oPSP)));
    CreatePSP(*oPSP);

    TRACE_EXE(
        "exe_size=%u seg=0x%04x exe_seg=%04x bss_seg=%04x\n", exe_size, seg, exe_seg, bss_seg);
    TRACE_EXE(
        "cs:ip=%04x:%04x ss:sp=%04x:%04x\n", header.eh_cs, header.eh_ip, header.eh_ss,
        header.eh_sp);

    // Program data starts after the header; read it completely
    TRACE_EXE(
        "loading %u bytes from offset 0x%x -> %04x:0000 (%x)\n", exe_size,
        header.eh_header_paragraphs * 16, exe_seg, CPUx86::MakeAddr(exe_seg, 0));
    ifs.seekg(header.eh_header_paragraphs * 16);
    for (unsigned int n = 0; n < exe_size; n++) {
        uint8_t b;
        if (!ReadFromStream(ifs, b))
            return ErrorCode::ReadFault; // XXX Is this the correct error code?
        m_Memory.WriteByte(CPUx86::MakeAddr(exe_seg, n), b);
    }

    printf(">> DATA %x\n", m_Memory.ReadWord(CPUx86::MakeAddr(0x9d8, 0x814)));

    // Handle relocations
    ifs.seekg(header.eh_reloc_table_offset);
    for (unsigned int n = 0; n < header.eh_num_relocs; n++) {
        struct EXERelocation reloc;
        if (!ReadFromStream(ifs, reloc))
            return ErrorCode::ReadFault;
        /* XXX Convert relocation to native form */
        // printf("reloc [%x:%x]\n", reloc.er_segment, reloc.er_offset);
        const auto reloc_addr = CPUx86::MakeAddr(reloc.er_segment + exe_seg, reloc.er_offset);
        m_Memory.WriteWord(reloc_addr, m_Memory.ReadWord(reloc_addr) + exe_seg);
    }

    /* Update CPU fields */
    CPUx86::State& oState = m_CPU.GetState();
    oState.m_ax = 0xffff; // TODO FCB stuff
    oState.m_bx = 0;      // XXX env segment
    oState.m_cx = 0;      // XXX data segment size
    oState.m_cs = header.eh_cs + exe_seg;
    oState.m_ss = header.eh_cs + exe_seg;
    oState.m_ds = seg;
    oState.m_es = seg;
    oState.m_ip = header.eh_ip;
    oState.m_sp = header.eh_sp;

    return ErrorCode::Success;
}

void DOS::InvokeVector(uint8_t no, CPUx86& oCPU, CPUx86::State& oState)
{
    Memory& oMemory = oCPU.GetMemory();
    if (no != 0x21) {
        abort(); // XXX TODO
    }

#define GET_AL uint8_t al = oState.m_ax & 0xff;
#define GET_DL uint8_t dl = oState.m_dx & 0xff;
#define AX oState.m_ax
#define BX oState.m_bx
#define CX oState.m_cx
#define DX oState.m_dx
#define DS oState.m_ds
#define ES oState.m_es

    auto SetError = [&](const ErrorCode ec) {
        oState.m_flags |= CPUx86::State::FLAG_CF;
        oState.m_ax = static_cast<uint16_t>(ec);
    };

    /* Default to okay */
    oState.m_flags &= ~CPUx86::State::FLAG_CF;

    uint8_t ah = (oState.m_ax & 0xff00) >> 8;
    switch (ah) {
        case 0x06: /* direct console output */ {
            GET_DL;
            TRACE_INT("ah=%02x: direct console output, dl='%c'\n", ah, dl);
            break;
        }
        case 0x2c: /* get system time */ {
            TRACE_INT("ah=%02x: get system time\n", ah);
            time_t t = time(NULL);
            struct tm* tm = localtime(&t);
            CX = (tm->tm_hour << 8) | tm->tm_min;
            DX = (tm->tm_sec << 8);
            break;
        }
        case 0x25: /* set interrupt vector */ {
            GET_AL;
            TRACE_INT("ah=%02x: set interrupt vector, al=%02x ds:dx=%04x:%04x\n", ah, al, DS, DX);
            CPUx86::addr_t addr = CPUx86::MakeAddr(0, al * 4);
            oMemory.WriteWord(addr + 2, DS);
            oMemory.WriteWord(addr + 0, DX);
            break;
        }
        case 0x35: /* get interrupt vector */ {
            GET_AL;
            TRACE_INT("ah=%02x: get interrupt vector al=%02x\n", ah, al);
            CPUx86::addr_t addr = CPUx86::MakeAddr(0, al * 4);
            ES = oMemory.ReadWord(addr + 2);
            BX = oMemory.ReadWord(addr + 0);
            break;
        }
        case 0x3c: /* create/truncate file */ {
            const auto sFilename = oMemory.GetASCIIZString(CPUx86::MakeAddr(DS, DX));
            TRACE_INT(
                "ah=%02x: create/truncate file, cx=%04x ds:dx=%04x:%04x '%s'\n", ah, CX, DS, DX,
                sFilename.c_str());
            // SetError(ErrorCode::FileNotFound);
            AX = 42;
            break;
        }
        case 0x4c: /* terminate with return code */ {
            GET_AL;
            TRACE_INT("ah=%02x: terminate with return code, ah=%02x\n", ah, al);
            exit(al);
            break;
        }
        default: /* what's this? */
            TRACE_INT("unknown function ah=%02x\n", ah);
            SetError(ErrorCode::InvalidFunction);
            break;
    }
}
