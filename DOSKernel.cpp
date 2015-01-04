// Copyright (c) 2009-present, the libcpu developers. All Rights Reserved.
// Read LICENSE.txt for licensing information.

#include "DOSKernel.h"
#include "interface.h"

#include <cerrno>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//#define DEBUG 1

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define MK_FP(SEG, OFF) (((SEG) << 4) + (OFF))


// TODO Make this list
#define DOS_EBADF  EBADF
#define DOS_ENFILE ENFILE

namespace {

enum  {
    ATTR_ARCHIVE      = (1 << 5),
    ATTR_DIRECTORY    = (1 << 4),
    ATTR_VOLUME_LABEL = (1 << 3),
    ATTR_SYSTEM       = (1 << 2),
    ATTR_HIDDEN       = (1 << 1),
    ATTR_READONLY     = (1 << 0)
};

#pragma pack(push, 1)

struct FindData {
    uint8_t  Unknown[21];
    uint8_t  Attributes;
    uint16_t FileTime;
    uint16_t FileDate;
    uint32_t FileSize;
    char     FileName[13];
};

#pragma pack(pop)


#pragma pack(push, 1)
struct PSP {
    uint8_t  CPMExit[2];
    uint16_t FirstFreeSegment;
    uint8_t  Reserved1;
    uint8_t  CPMCall5Compat[5];
    uint32_t OldTSRAddress;
    uint32_t OldBreakAddress;
    uint32_t CriticalErrorHandlerAddress;
    uint16_t CallerPSPSegment;
    uint8_t  JobFileTable[20];
    uint16_t EnvironmentSegment;
    uint32_t INT21SSSP;
    uint16_t JobFileTableSize;
    uint32_t JobFileTablePointer;
    uint32_t PreviousPSP;
    uint32_t Reserved2;
    uint16_t DOSVersion;
    uint8_t  Reserved3[14];
    uint8_t  DOSFarCall[3];
    uint16_t Reserved4;
    uint8_t  ExtendedFCB1[7];
    uint8_t  FCB1[16];
    uint8_t  FCB2[20];
    uint8_t  CommandLineLength;
    char     CommandLine[127];
};
#pragma pack(pop)

// convert backslashes to slashes
static inline void
ConvertSlashes(std::string &S)
{
    std::transform(S.begin(), S.begin(), S.end(),
            [](char C) { return (C == '\\') ? '/' : C; });
}

static inline uint8_t
ModeToAttribute(uint16_t Mode)
{
    uint8_t Attribute = 0;

    if (Mode & S_IFDIR) {
        Attribute |= ATTR_DIRECTORY;
    }
    if ((Mode & S_IRUSR) == 0) {
        Attribute |= ATTR_READONLY;
    }

    return Attribute;
}

}

DOSKernel::DOSKernel(char *memory, hv_vcpuid_t vcpu, int argc, char **argv) :
    _memory    (memory),
    _vcpu      (vcpu),
    _dta       (0),
    _exitStatus(0)
{
    _fdbits.resize(256);

    _fdtable[0] = 0, _fdbits[0] = true;
    _fdtable[1] = 1, _fdbits[1] = true;
    _fdtable[2] = 2, _fdbits[2] = true;

    // Initialize PSP
    makePSP(0, argc, argv);
}

DOSKernel::~DOSKernel()
{
}

int DOSKernel::
dispatch(uint8_t IntNo)
{
    switch (IntNo) {
        case 0x20: return int20();
        case 0x21: return int21();
        default:   break;
    }
    return STATUS_UNHANDLED;
}

int DOSKernel::
int20()
{
    _exitStatus = 0;
    return STATUS_STOP;
}

int DOSKernel::
int21()
{
#ifdef DEBUG
    std::fprintf(stderr, "\n[%04x] INT 21/AH=%02Xh\n", pc, AH);
#endif

    switch (AH) {
        case 0x02: return int21Func02();
        case 0x08: return int21Func08();
        case 0x09: return int21Func09();
        case 0x0A: return int21Func0A();
        case 0x0C: return int21Func0C();
        case 0x0E: return int21Func0E();
        case 0x19: return int21Func19();
        case 0x1A: return int21Func1A();
        case 0x25: return int21Func25();
        case 0x26: return int21Func26();
        case 0x30: return int21Func30();
        case 0x33: return int21Func33();
        case 0x35: return int21Func35();
        case 0x3C: return int21Func3C();
        case 0x3D: return int21Func3D();
        case 0x3E: return int21Func3E();
        case 0x3F: return int21Func3F();
        case 0x40: return int21Func40();
        case 0x41: return int21Func41();
        case 0x42: return int21Func42();
        case 0x43: return int21Func43();
        case 0x4C: return int21Func4C();
        case 0x4E: return int21Func4E();
        case 0x4F: return int21Func4F();
        case 0x57: return int21Func57();
        default:   break;
    }

    std::fprintf(stderr, "Unknown interrupt 0x21/0x%02X at %s:%d\n",
            AH, __FILE__, __LINE__);
    return STATUS_UNSUPPORTED;
}

// DOS 1+ - WRITE CHARACTER TO STANDARD OUTPUT
int DOSKernel::
int21Func02()
{
    putchar(DL);
    SET_AL(DL);
    return STATUS_HANDLED;
}

// DOS 1+ - CHARACTER INPUT WITHOUT ECHO
int DOSKernel::
int21Func08()
{
    SET_AL(internalGetChar(0));
    return STATUS_HANDLED;
}

// DOS 1+ - WRITE STRING TO STANDARD OUTPUT
int DOSKernel::
int21Func09()
{
    std::string S(readCString(MK_FP(DS, DX), '$'));
    fwrite(S.data(), 1, S.size(), stdout);

    SET_AL('$');

    return STATUS_HANDLED;
}

// DOS 1+ - BUFFERED INPUT
int DOSKernel::
int21Func0A()
{
    uint32_t abs = MK_FP(DS, DX);
    char *addr = &_memory[abs];
    getline(&addr, NULL, stdin);

    return STATUS_HANDLED;
}

// DOS 1+ - FLUSH BUFFER AND READ STANDARD INPUT
int DOSKernel::
int21Func0C()
{
    flushConsoleInput();

    switch (AL) {
        case 0x01:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x0a:
            SET_AH(AL);
            int21();
            SET_AH(0x0c);
            break;

        default:
            break;
    }

    return STATUS_HANDLED;
}

// DOS 1+ - SELECT DEFAULT DRIVE
int DOSKernel::
int21Func0E()
{
    SET_AL(DL + 'A');
    return STATUS_HANDLED;
}

// DOS 1+ - GET CURRENT DEFAULT DRIVE
int DOSKernel::
int21Func19()
{
    SET_AL(0);
    return STATUS_HANDLED;
}

// DOS 1+ - SET DISK TRANSFER AREA ADDRESS
int DOSKernel::
int21Func1A()
{
    _dta = DX;
    return STATUS_HANDLED;
}

void DOSKernel::
makePSP(uint16_t seg, int argc, char **argv)
{
    uint32_t abs = MK_FP(seg, 0);

    struct PSP *PSP = (struct PSP *)(&_memory[abs]);

    // CPMExit: INT 20h
    PSP->CPMExit[0] = 0xcd;
    PSP->CPMExit[1] = 0x20;

    // DOS Far Call: INT 21h + RETF
    PSP->DOSFarCall[0] = 0xcd;
    PSP->DOSFarCall[1] = 0x21;
    PSP->DOSFarCall[2] = 0xcb;

    // first FSB = empty file name
    PSP->FCB1[0] = 0x01;
    PSP->FCB1[1] = 0x20;

    uint8_t c = 0;
    int j = 0;
    for (int i = 2; i < argc && c < 0x7E; i++) {
        j = 0;
        PSP->CommandLine[c++] = ' ';
        while (argv[i][j] && c < 0x7E)
            PSP->CommandLine[c++] = argv[i][j++];
    }
    PSP->CommandLine[c] = 0x0D;
    PSP->CommandLineLength = c;
}

// DOS 1+ - SET INTERRUPT VECTOR
int DOSKernel::
int21Func25()
{
#ifdef DEBUG
    std::fprintf(stderr, "[%04x] SET INTERRUPT VECTOR: 0x%02x to 0x%04x:0x%04x\n", pc, AL, DS, DX);
#endif
    return STATUS_HANDLED;
}

// DOS 1+ - CREATE NEW PROGRAM SEGMENT PREFIX
int DOSKernel::
int21Func26()
{
    makePSP(DS, 0, 0);
    return STATUS_HANDLED;
}

// DOS 2+ - GET DOS VERSION
int DOSKernel::
int21Func30()
{
    SET_AL(7);
    SET_AH(0);
    return STATUS_HANDLED;
}

// DOS 2+ - EXTENDED BREAK CHECKING
int DOSKernel::
int21Func33()
{
    static bool extended_break_checking = false;
    switch (AL) {
        case 0:
            SET_DL(extended_break_checking);
            break;
        case 1:
            extended_break_checking = DL;
            break;
        default:
            std::fprintf(stderr, "Unknown subfunction 0x21/0x33/0x%02X at %s:%d\n",
                         AL, __FILE__, __LINE__);
    }
    return STATUS_HANDLED;
}

// DOS 2+ - GET INTERRUPT VECTOR
int DOSKernel::
int21Func35()
{
#ifdef DEBUG
    std::fprintf(stderr, "\nGET INTERRUPT VECTOR: 0x%02x\n", AL);
#endif
    SET_ES(0);
    SET_BX(0);
    return STATUS_HANDLED;
}

// DOS 2+ - CREAT - CREATE OR TRUNCATE FILE
int DOSKernel::
int21Func3C()
{
    std::string FN(readCString(MK_FP(DS, DX)));

#if DEBUG
    std::fprintf(stderr, "\ncreat: %s\n", FN.c_str());
#endif

    // TODO we ignore attributes
    int HostFD = ::open(FN.c_str(), O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0777);
    if (HostFD < 0) {
        SETC(1);
        SET_AX(getDOSError());
    } else {
        int FD = allocFD(HostFD);
        if (FD < 0) {
            ::close(HostFD);
            SETC(1);
            SET_AX(DOS_ENFILE);
        } else {
            SETC(0);
            SET_AX(FD);
        }
    }

    return STATUS_HANDLED;
}

// DOS 2+ - OPEN - OPEN EXISTING FILE
int DOSKernel::
int21Func3D()
{
    std::string FN(readCString(MK_FP(DS, DX)));

#if DEBUG
    std::fprintf(stderr, "\nopen: %s\n", FN.c_str());
#endif

    // oflag is compatible!
    int HostFD = ::open(FN.c_str(), (AL & 3) | O_BINARY);
    if (HostFD < 0) {
        SETC(1);
        SET_AX(getDOSError());
    } else {
        int FD = allocFD(HostFD);
        if (FD < 0) {
            ::close(HostFD);
            SETC(1);
            SET_AX(DOS_ENFILE);
        } else {
            SETC(0);
            SET_AX(FD);
        }
    }

    return STATUS_HANDLED;
}

// DOS 2+ - CLOSE - CLOSE FILE
int DOSKernel::
int21Func3E()
{
    int FD     = BX;
    int HostFD = findFD(FD);
    if (HostFD < 0) {
        SETC(1);
        SET_AX(DOS_EBADF);
    } else {
        deallocFD(FD);
        if (::close(HostFD) < 0) {
            SETC(1);
            SET_AX(getDOSError());
        } else {
            SETC(0);
        }
    }

    return STATUS_HANDLED;
}

// DOS 2+ - READ - READ FROM FILE OR DEVICE
int DOSKernel::
int21Func3F()
{
    int FD = findFD(BX);
    if (FD < 0) {
        SETC(1);
        SET_AX(DOS_EBADF);
        return STATUS_HANDLED;
    }

    char Buffer[64 * 1024];
    ssize_t ReadCount = ::read(FD, Buffer, CX);
    if (ReadCount < 0) {
        SETC(1);
        SET_AX(getDOSError());
    } else {
        writeMem(MK_FP(DS, DX), Buffer, ReadCount);
        SETC(0);
        SET_AX(ReadCount);
    }

    return STATUS_HANDLED;
}

// DOS 2+ - WRITE - WRITE TO FILE OR DEVICE
int DOSKernel::
int21Func40()
{
    int FD = findFD(BX);
    if (FD < 0) {
        SETC(1);
        SET_AX(DOS_EBADF);
        return STATUS_HANDLED;
    }

    std::string B(readString(MK_FP(DS, DX), CX));

    ssize_t WriteCount = ::write(FD, B.data(), B.size());
    if (WriteCount < 0) {
        SETC(1);
        SET_AX(getDOSError());
    } else {
        SETC(0);
        SET_AX(WriteCount);
    }

    return STATUS_HANDLED;
}

// DOS 2+ - UNLINK - DELETE FILE
int DOSKernel::
int21Func41()
{
    std::string FN(readCString(MK_FP(DS, DX)));

    // TODO
#if DEBUG
    std::fprintf(stderr, "\nUNIMPL del: %s\n", FN.c_str());
#endif

    return STATUS_HANDLED;
}

// DOS 2+ - LSEEK - SET CURRENT FILE POSITION
int DOSKernel::
int21Func42()
{
    int FD = findFD(BX);
    if (FD < 0) {
        SETC(1);
        SET_AX(DOS_EBADF);
        return STATUS_HANDLED;
    }

    off_t NewOffset = lseek(FD, (CX << 16) | DX, AL);

#if 0
    std::fprintf(stderr, "\n%" PRIx64 ": lseek(%d, 0x%x, %d) = %" PRId64 "\n",
            pc.value(), BX, (CX << 16) | DX, AL,
            static_cast <int64_t> (NewOffset));
#endif

    if (NewOffset < 0) {
        SETC(1);
        SET_AX(getDOSError());
    } else {
        SETC(0);
        SET_DX(NewOffset >> 16);
        SET_AX(NewOffset & 0xffff);
    }
    return STATUS_HANDLED;
}

// DOS 2+ - GET/SET FILE ATTRIBUTES
int DOSKernel::
int21Func43()
{
    std::string FN;
    struct stat ST;

    switch (AL) {
        case 0x00: // GET FILE ATTRIBUTES
            FN = readCString(MK_FP(DS, DX));
            ConvertSlashes(FN);

            if (::stat(FN.c_str(), &ST) != 0) {
                SETC(1);
                SET_AX(getDOSError());
                return STATUS_HANDLED;
            }

            SETC(0);
            SET_CX(ModeToAttribute(ST.st_mode));
            break;

        case 0x01: // SET FILE ATTRIBUTES
#if DEBUG
            std::fprintf(stderr, "\nUNIMPL SetFileAttributes: 0x%02X, %s\n",
                    CX, readCString(MK_FP(DS, DX)).c_str());
#endif
            SETC(0);
            break;

        default:
            std::fprintf(stderr, "Unknown GetSetFileAttributes "
                    "subfunction: 0x%02X\n", AL);
            return STATUS_UNSUPPORTED;
    }

    return STATUS_HANDLED;
}

// DOS 2+ - EXIT - TERMINATE WITH RETURN CODE
int DOSKernel::
int21Func4C()
{
    _exitStatus = AL;
    return STATUS_STOP;
}

// DOS 2+ - FINDFIRST - FIND FIRST MATCHING FILE
int DOSKernel::
int21Func4E()
{
    std::string FileSpec(readCString(MK_FP(DS, DX)));
    ConvertSlashes(FileSpec);

    if (CX & ATTR_VOLUME_LABEL) {
#if DEBUG
        std::fprintf(stderr, "\nUNIMPL findfirst volume label: %s\n",
                FileSpec.c_str());
#endif
        SETC(1);
        SET_AX(0x12); // no more files
        return STATUS_HANDLED;
    }

    if (FileSpec.find_first_of("?*") != std::string::npos) {
#if DEBUG
        std::fprintf(stderr, "\nUNIMPL findfirst wildcards: %s\n",
                FileSpec.c_str());
#endif
        SETC(1);
        SET_AX(0x12); // no more files
        return STATUS_HANDLED;
    }

    struct stat ST;
    if (::stat(FileSpec.c_str(), &ST)) {
        SETC(1);
        SET_AX(getDOSError());
        return STATUS_HANDLED;
    }

    if ((ST.st_mode) == S_IFDIR && !(CX & ATTR_DIRECTORY)) {
        // found a directory but program hasn't requested directories
        SETC(1);
        SET_AX(0x12); // no more files
        return STATUS_HANDLED;
    }


    FindData FD;

    std::memset(&FD, 0, sizeof(FD));

    FD.Attributes = ModeToAttribute(ST.st_mode);
    FD.FileTime   = 0; // TODO
    FD.FileDate   = 0; // TODO
    FD.FileSize   = ST.st_size;

    size_t FNPos = FileSpec.rfind('/');
    if (FNPos != std::string::npos) {
        FNPos++;
    } else {
        FNPos = 0;
    }
    std::strncpy(FD.FileName, &FileSpec[FNPos], sizeof(FD.FileName) - 1);

    writeMem(MK_FP(DS, _dta), &FD, sizeof(FD));

    SETC(0);
    return STATUS_HANDLED;
}

// DOS 2+ - FINDNEXT - FIND NEXT MATCHING FILE
int DOSKernel::
int21Func4F()
{
    // TODO
#if DEBUG
    std::fprintf(stderr, "\nUNIMPL FindNext\n");
#endif
    SET_AX(0x12); // no more files
    SETC(1);

    return STATUS_HANDLED;
}

// DOS 2+ - GET FILE'S LAST-WRITTEN DATE AND TIME
int DOSKernel::
int21Func57()
{
    // TODO
#if DEBUG
    std::fprintf(stderr, "\nUNIMPL datetime\n");
#endif
    SETC(0);

    return STATUS_HANDLED;
}

int DOSKernel::
getDOSError() const
{
    // TODO translate
    return errno;
}

void DOSKernel::
flushConsoleInput()
{
    fflush(stdout);
}

int DOSKernel::
internalGetChar(bool Echo)
{
    return getchar();
}

int DOSKernel::
allocFD(int HostFD)
{
    auto I = std::find(_fdbits.begin(), _fdbits.end(), false);
    if (I == _fdbits.end())
        return -1;

    int FD = I - _fdbits.begin();
    _fdbits[FD] = true;
    _fdtable.insert(std::make_pair(FD, HostFD));
    return FD;
}

void DOSKernel::
deallocFD(int FD)
{
    if (FD < 3)
        return;

    _fdbits[FD] = false;
    _fdtable.erase(FD);
}

int DOSKernel::
findFD(int FD)
{
    if (FD < 0)
        return -1;

    auto I = _fdtable.find(FD);
    return (I != _fdtable.end()) ? I->second : -1;
}


void DOSKernel::
writeMem(uint16_t const &Address, void const *Bytes, size_t Length)
{
    auto             D = Address;
    uint8_t const   *B = reinterpret_cast <uint8_t const *> (Bytes);

    while (Length-- != 0) {
        writeMem8(D, *B);
        D++, B++;
    }
}

std::string DOSKernel::
readString(uint16_t const &Address, size_t Length)
{
    std::string     Result;
    auto            S = Address;

    while (Length-- != 0) {
        Result += static_cast <char> (readMem8(S));
        S++;
    }

    return Result;
}

std::string DOSKernel::
readCString(uint16_t const &Address, char Terminator)
{
    std::string     Result;
    auto            S = Address;

    for (;;) {
        char C = readMem8(S);
        if (C == Terminator)
            break;

        Result += C, S++;
    }

    return Result;
}
