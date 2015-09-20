#include <intrin.h>

#include "cpuid.h"

int __get_cpuid(unsigned int level, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
    int cpuinfo[4];

    __cpuid(cpuinfo, level);

    if(eax) *eax = (unsigned)cpuinfo[0];
    if(ebx) *ebx = (unsigned)cpuinfo[1];
    if(ecx) *ecx = (unsigned)cpuinfo[2];
    if(edx) *edx = (unsigned)cpuinfo[3];

    return 1;
}
