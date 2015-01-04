// Copyright (c) 2009-present, the libcpu developers. All Rights Reserved.
// Read LICENSE.txt for licensing information.

#ifndef __DOSKernel_h
#define __DOSKernel_h

#include <string>
#include <map>
#include <vector>
#include <Hypervisor/hv_vmx.h>

class DOSKernel {
public:
    enum {
        STATUS_HANDLED,
        STATUS_STOP,
        STATUS_UNHANDLED,
        STATUS_UNSUPPORTED,
        STATUS_NORETURN
    };

private:
    char                *_memory;
    hv_vcpuid_t          _vcpu;
    std::map <int, int>  _fdtable;
    std::vector <bool>   _fdbits;
    uint16_t             _dta;
    int                  _exitStatus;

public:
    DOSKernel(char *memory, hv_vcpuid_t vcpu, int argc, char **argv);
    ~DOSKernel();

public:
    int dispatch(uint8_t IntNo);

private:
    int int20();
    int int21();

private:
    int int21Func02();
    int int21Func08();
    int int21Func09();
    int int21Func0A();
    int int21Func0C();
    int int21Func0E();
    int int21Func19();
    int int21Func1A();
    int int21Func25();
    int int21Func26();
    int int21Func30();
    int int21Func33();
    int int21Func35();
    int int21Func3C();
    int int21Func3D();
    int int21Func3E();
    int int21Func3F();
    int int21Func40();
    int int21Func41();
    int int21Func42();
    int int21Func43();
    int int21Func4C();
    int int21Func4E();
    int int21Func4F();
    int int21Func57();

private:
    int getDOSError() const;
    void makePSP(uint16_t seg, int argc, char **argv);

private:
    void flushConsoleInput();
    int internalGetChar(bool Echo);

private:
    int allocFD(int HostFD);
    void deallocFD(int FD);

    int findFD(int FD);

private:
    void writeMem(uint16_t const &Address, void const *Bytes,
            size_t Length);
    std::string readString(uint16_t const &Address, size_t Length);
    std::string readCString(uint16_t const &Address,
            char Terminator = '\0');

};

#endif  // !__DOSKernel_h
