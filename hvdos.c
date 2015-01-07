// Copyright (c) 2009-present, the hvdos developers. All Rights Reserved.
// Read LICENSE.txt for licensing information.
//
// hvdos - a simple DOS emulator based on the OS X 10.10 Hypervisor.framework

#include <stdlib.h>
#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>
#include "vmcs.h"
#include "interface.h"
#include "DOSKernel.h"

//#define DEBUG 1

/* read GPR */
uint64_t
rreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg)
{
	uint64_t v;

	if (hv_vcpu_read_register(vcpu, reg, &v)) {
		abort();
	}

	return v;
}

/* write GPR */
void
wreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg, uint64_t v)
{
	if (hv_vcpu_write_register(vcpu, reg, v)) {
		abort();
	}
}

/* read VMCS field */
static uint64_t
rvmcs(hv_vcpuid_t vcpu, uint32_t field)
{
	uint64_t v;

	if (hv_vmx_vcpu_read_vmcs(vcpu, field, &v)) {
		abort();
	}

	return v;
}

/* write VMCS field */
static void
wvmcs(hv_vcpuid_t vcpu, uint32_t field, uint64_t v)
{
	if (hv_vmx_vcpu_write_vmcs(vcpu, field, v)) {
		abort();
	}
}

/* desired control word constrained by hardware/hypervisor capabilities */
static uint64_t
cap2ctrl(uint64_t cap, uint64_t ctrl)
{
	return (ctrl | (cap & 0xffffffff)) & (cap >> 32);
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: hvdos [com file]\n");
                exit(1);
    }

	/* create a VM instance for the current task */
	if (hv_vm_create(HV_VM_DEFAULT)) {
		abort();
	}

	/* get hypervisor enforced capabilities of the machine, (see Intel docs) */
	uint64_t vmx_cap_pinbased, vmx_cap_procbased, vmx_cap_procbased2, vmx_cap_entry;
	if (hv_vmx_read_capability(HV_VMX_CAP_PINBASED, &vmx_cap_pinbased)) {
		abort();
	}
	if (hv_vmx_read_capability(HV_VMX_CAP_PROCBASED, &vmx_cap_procbased)) {
		abort();
	}
	if (hv_vmx_read_capability(HV_VMX_CAP_PROCBASED2, &vmx_cap_procbased2)) {
		abort();
	}
	if (hv_vmx_read_capability(HV_VMX_CAP_ENTRY, &vmx_cap_entry)) {
		abort();
	}

	/* allocate some guest physical memory */
#define VM_MEM_SIZE (1 * 1024 * 1024)
	void *vm_mem;
	if (!(vm_mem = valloc(VM_MEM_SIZE))) {
		abort();
	}
	/* map a segment of guest physical memory into the guest physical address
	 * space of the vm (at address 0) */
	if (hv_vm_map(vm_mem, 0, VM_MEM_SIZE, HV_MEMORY_READ | HV_MEMORY_WRITE
		| HV_MEMORY_EXEC))
	{
		abort();
	}

	/* create a vCPU instance for this thread */
	hv_vcpuid_t vcpu;
	if (hv_vcpu_create(&vcpu, HV_VCPU_DEFAULT)) {
		abort();
	}

	/* vCPU setup */
#define VMCS_PRI_PROC_BASED_CTLS_HLT           (1 << 7)
#define VMCS_PRI_PROC_BASED_CTLS_CR8_LOAD      (1 << 19)
#define VMCS_PRI_PROC_BASED_CTLS_CR8_STORE     (1 << 20)

	/* set VMCS control fields */
    wvmcs(vcpu, VMCS_PIN_BASED_CTLS, cap2ctrl(vmx_cap_pinbased, 0));
    wvmcs(vcpu, VMCS_PRI_PROC_BASED_CTLS, cap2ctrl(vmx_cap_procbased,
                                                   VMCS_PRI_PROC_BASED_CTLS_HLT |
                                                   VMCS_PRI_PROC_BASED_CTLS_CR8_LOAD |
                                                   VMCS_PRI_PROC_BASED_CTLS_CR8_STORE));
	wvmcs(vcpu, VMCS_SEC_PROC_BASED_CTLS, cap2ctrl(vmx_cap_procbased2, 0));
	wvmcs(vcpu, VMCS_ENTRY_CTLS, cap2ctrl(vmx_cap_entry, 0));
	wvmcs(vcpu, VMCS_EXCEPTION_BITMAP, 0xffffffff);
	wvmcs(vcpu, VMCS_CR0_MASK, 0x60000000);
	wvmcs(vcpu, VMCS_CR0_SHADOW, 0);
	wvmcs(vcpu, VMCS_CR4_MASK, 0);
	wvmcs(vcpu, VMCS_CR4_SHADOW, 0);
	/* set VMCS guest state fields */
	wvmcs(vcpu, VMCS_GUEST_CS_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_CS_LIMIT, 0xffff);
	wvmcs(vcpu, VMCS_GUEST_CS_ACCESS_RIGHTS, 0x9b);
	wvmcs(vcpu, VMCS_GUEST_CS_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_DS_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_DS_LIMIT, 0xffff);
	wvmcs(vcpu, VMCS_GUEST_DS_ACCESS_RIGHTS, 0x93);
	wvmcs(vcpu, VMCS_GUEST_DS_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_ES_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_ES_LIMIT, 0xffff);
	wvmcs(vcpu, VMCS_GUEST_ES_ACCESS_RIGHTS, 0x93);
	wvmcs(vcpu, VMCS_GUEST_ES_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_FS_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_FS_LIMIT, 0xffff);
	wvmcs(vcpu, VMCS_GUEST_FS_ACCESS_RIGHTS, 0x93);
	wvmcs(vcpu, VMCS_GUEST_FS_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_GS_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_GS_LIMIT, 0xffff);
	wvmcs(vcpu, VMCS_GUEST_GS_ACCESS_RIGHTS, 0x93);
	wvmcs(vcpu, VMCS_GUEST_GS_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_SS_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_SS_LIMIT, 0xffff);
	wvmcs(vcpu, VMCS_GUEST_SS_ACCESS_RIGHTS, 0x93);
	wvmcs(vcpu, VMCS_GUEST_SS_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_LDTR_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_LDTR_LIMIT, 0);
	wvmcs(vcpu, VMCS_GUEST_LDTR_ACCESS_RIGHTS, 0x10000);
	wvmcs(vcpu, VMCS_GUEST_LDTR_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_TR_SELECTOR, 0);
	wvmcs(vcpu, VMCS_GUEST_TR_LIMIT, 0);
	wvmcs(vcpu, VMCS_GUEST_TR_ACCESS_RIGHTS, 0x83);
	wvmcs(vcpu, VMCS_GUEST_TR_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_GDTR_LIMIT, 0);
	wvmcs(vcpu, VMCS_GUEST_GDTR_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_IDTR_LIMIT, 0);
	wvmcs(vcpu, VMCS_GUEST_IDTR_BASE, 0);

	wvmcs(vcpu, VMCS_GUEST_CR0, 0x20);
	wvmcs(vcpu, VMCS_GUEST_CR3, 0x0);
	wvmcs(vcpu, VMCS_GUEST_CR4, 0x2000);

	/* initialize DOS emulation */
	DOSKernel Kernel((char *)vm_mem, vcpu, argc, argv);

	/* read COM file at 0x100 */
	FILE *f = fopen(argv[1], "r");
	fread((char *)vm_mem + 0x100, 1, 64 * 1024, f);
	fclose(f);

	/* set up GPRs, start at COM file entry point */
	wreg(vcpu, HV_X86_RIP, 0x100);
	wreg(vcpu, HV_X86_RFLAGS, 0x2);
	wreg(vcpu, HV_X86_RSP, 0x0);

	/* vCPU run loop */
	int stop = 0;
	do {
		if (hv_vcpu_run(vcpu)) {
			abort();
		}
		/* handle VMEXIT */
		uint64_t exit_reason = rvmcs(vcpu, VMCS_EXIT_REASON);

		switch (exit_reason) {
			case EXIT_REASON_EXCEPTION: {
				uint8_t interrupt_number = rvmcs(vcpu, VMCS_IDT_VECTORING_INFO) & 0xFF;
				int Status = Kernel.dispatch(interrupt_number);
				switch (Status) {
					case DOSKernel::STATUS_HANDLED:
						wreg(vcpu, HV_X86_RIP, rreg(vcpu, HV_X86_RIP) + 2);
						break;
					case DOSKernel::STATUS_UNSUPPORTED:
					case DOSKernel::STATUS_STOP:
						stop = 1;
						break;
					case DOSKernel::STATUS_NORETURN:
						// The kernel changed the PC.
						break;
					default:
						break;
				}
				break;
			}
			case EXIT_REASON_EXT_INTR:
				/* VMEXIT due to host interrupt, nothing to do */
#if DEBUG
				printf("IRQ\n");
#endif
				break;
			case EXIT_REASON_HLT:
				/* guest executed HLT */
#if DEBUG
				printf("HLT\n");
#endif
				stop = 1;
				break;
			case EXIT_REASON_EPT_FAULT:
				/* disambiguate between EPT cold misses and MMIO */
				/* ... handle MMIO ... */
				break;
	 		/* ... many more exit reasons go here ... */
			default:
				printf("unhandled VMEXIT (%llu)\n", exit_reason);

				stop = 1;
		}
	} while (!stop);

	/*
	 * optional clean-up
	 */

	/* destroy vCPU */
	if (hv_vcpu_destroy(vcpu)) {
		abort();
	}

	/* unmap memory segment at address 0 */
	if (hv_vm_unmap(0, VM_MEM_SIZE)) {
		abort();
	}
	/* destroy VM instance of this task */
	if (hv_vm_destroy()) {
		abort();
	}

	free(vm_mem);

	return 0;
}
