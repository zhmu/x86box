#ifndef __DOS_H__
#define __DOS_H__

#include <fstream>
#include <stdint.h>
#include "xcallablevector.h"

#define PACKED __attribute__((aligned(1)))

class CPUx86;
class Memory;
class Vectors;

//! \brief Implements MS-DOS compatible services
class DOS : public XCallableVector
{
  public:
    DOS(CPUx86& oCPU, Memory& oMemory, Vectors& oVectors);

    enum class ErrorCode : uint8_t {
        Success = 0,
        InvalidFunction,
        FileNotFound,
        PathNotFound,
        TooManyOpenFiles,
        AccessDenied,
        InvalidHandle,
        MCBDestroyed,
        InsufficientMemory,
        InvalidMCBAddress,
        InvalidEnvironment,
        InvalidFormat,
        InvalidAccessMode,
        InvalidData,
        Reserved0E,
        InvalidDrive,
        RemoveCurrentDir,
        NotSameDevice,
        NoMoreFiles,
        WriteProtected,
        UnknownUnit,
        DriveNotReady,
        UnknownCommand,
        CRC,
        BadRequestLength,
        Seek,
        UnknownMediaType,
        SectorNotFound,
        OutOfPaper,
        WriteFault,
        ReadFault,
        GeneralFailure,
        SharingViolation,
        LockViolation,
        InvalidDiskChange,
        FCBUnavailable,
        SharingBufferOverflow,
        Reserved25
    };

    void Reset();
    ErrorCode LoadEXE(std::ifstream& ifs);

    void InvokeVector(uint8_t no, CPUx86& oCPU, CPUx86::State& oState);

  private:
    Memory& m_Memory;
    CPUx86& m_CPU;
    Vectors& m_Vectors;
};

#endif /* __DOS_H__ */
