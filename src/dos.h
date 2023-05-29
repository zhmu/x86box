#ifndef __DOS_H__
#define __DOS_H__

#include <fstream>
#include <stdint.h>
#include "xcallablevector.h"

#define PACKED __attribute__((aligned(1)))

class CPUx86;
class Memory;
class Vectors;
class XFile;

//! \brief Implements MS-DOS compatible services
class DOS : public XCallableVector
{
  public:
    DOS(CPUx86& oCPU, Memory& oMemory, Vectors& oVectors);

    enum ErrorCode {
        E_SUCCESS = 0,
        E_INVALID_FN,
        E_FILE_NOT_FOUND,
        E_PATH_NOT_FOUND,
        E_TOO_MANY_OPEN_FILES,
        E_ACCESS_DENIED,
        E_INVALID_HANDLE,
        E_MCB_DESTROYED,
        E_INSUFFICIENT_MEMORY,
        E_INVALID_MCB_ADDRESS,
        E_INVALID_ENVIRONMENT,
        E_INVALID_FORMAT,
        E_INVALID_ACCESS_MODE,
        E_INVALID_DATA,
        E_RESERVED_0E,
        E_INVALID_DRIVE,
        E_REMOVE_CURRENT_DIR,
        E_NOT_SAME_DEVICE,
        E_NO_MORE_FILES,
        E_WRITE_PROTECTED,
        E_UNKNOWN_UNIT,
        E_DRIVE_NOT_READY,
        E_UNKNOWN_COMMAND,
        E_CRC,
        E_BAD_REQUEST_LENGTH,
        E_SEEK,
        E_UNKNOWN_MEDIA_TYPE,
        E_SECTOR_NOT_FOUND,
        E_OUT_OF_PAPER,
        E_WRITE_FAULT,
        E_READ_FAULT,
        E_GENERAL_FAILURE,
        E_SHARING_VIOLATION,
        E_LOCK_VIOLATION,
        E_INVALID_DISK_CHANGE,
        E_FCB_UNAVAILABLE,
        E_SHARING_BUFFER_OVERFLOW,
        E_RESERVED_25
    };

    void Reset();
    ErrorCode LoadEXE(std::ifstream& ifs);

    void InvokeVector(uint8_t no, CPUx86& oCPU, CPUx86::State& oState);

  protected:
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

    void CreatePSP(struct ProgramSegmentPrefix& oPSP);

  private:
    //! \brief Memory we manage
    Memory& m_Memory;

    //! \brief CPU we attempt to control
    CPUx86& m_CPU;

    //! \brief Vectors object
    Vectors& m_Vectors;
};

#endif /* __DOS_H__ */
