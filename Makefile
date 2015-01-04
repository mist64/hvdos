all:
	clang++ -std=c++11 -framework Hypervisor -o hvdos DOSKernel.cpp hvdos.c
	