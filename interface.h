// Copyright (c) 2009-present, the hvdos developers. All Rights Reserved.
// Read LICENSE.txt for licensing information.
//
// hvdos - a simple DOS emulator based on the OS X 10.10 Hypervisor.framework

extern uint64_t rreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg);
extern void wreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg, uint64_t v);

#define readMem8(a) _memory[a]
#define writeMem8(a, v) do { _memory[a] = v; } while (0)

#define AX ((uint16_t)rreg(_vcpu, HV_X86_RAX))
#define BX ((uint16_t)rreg(_vcpu, HV_X86_RBX))
#define CX ((uint16_t)rreg(_vcpu, HV_X86_RCX))
#define DX ((uint16_t)rreg(_vcpu, HV_X86_RDX))

#define pc ((uint16_t)rreg(_vcpu, HV_X86_RIP))
#define DS rreg(_vcpu, HV_X86_DS)
#define ES rreg(_vcpu, HV_X86_ES)

#define FLAGS ((uint16_t)rreg(_vcpu, HV_X86_RFLAGS))

#define AL ((uint16_t)rreg(_vcpu, HV_X86_RAX) & 0xFF)
#define AH ((uint16_t)rreg(_vcpu, HV_X86_RAX) >> 8)
#define BL ((uint16_t)rreg(_vcpu, HV_X86_RBX) & 0xFF)
#define BH ((uint16_t)rreg(_vcpu, HV_X86_RBX) >> 8)
#define CL ((uint16_t)rreg(_vcpu, HV_X86_RCX) & 0xFF)
#define CH ((uint16_t)rreg(_vcpu, HV_X86_RCX) >> 8)
#define DL ((uint16_t)rreg(_vcpu, HV_X86_RDX) & 0xFF)
#define DH ((uint16_t)rreg(_vcpu, HV_X86_RDX) >> 8)

#define SET_AX(v) wreg(_vcpu, HV_X86_RAX, v)
#define SET_BX(v) wreg(_vcpu, HV_X86_RBX, v)
#define SET_CX(v) wreg(_vcpu, HV_X86_RCX, v)
#define SET_DX(v) wreg(_vcpu, HV_X86_RDX, v)

#define SET_DS(v) wreg(_vcpu, HV_X86_DS, v)
#define SET_ES(v) wreg(_vcpu, HV_X86_ES, v)

#define SET_AL(v) wreg(_vcpu, HV_X86_RAX, (AX & 0xFF00) | v)
#define SET_AH(v) wreg(_vcpu, HV_X86_RAX, (AX & 0xFF) | (v << 8))
#define SET_BL(v) wreg(_vcpu, HV_X86_RBX, (BX & 0xFF00) | v)
#define SET_BH(v) wreg(_vcpu, HV_X86_RBX, (BX & 0xFF) | (v << 8))
#define SET_CL(v) wreg(_vcpu, HV_X86_RCX, (CX & 0xFF00) | v)
#define SET_CH(v) wreg(_vcpu, HV_X86_RCX, (CX & 0xFF) | (v << 8))
#define SET_DL(v) wreg(_vcpu, HV_X86_RDX, (DX & 0xFF00) | v)
#define SET_DH(v) wreg(_vcpu, HV_X86_RDX, (DX & 0xFF) | (v << 8))

#define SETC(v) wreg(_vcpu, HV_X86_RFLAGS, (FLAGS & 0xFFFE) | v)
