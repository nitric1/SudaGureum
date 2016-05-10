#include <stdint.h>

#ifndef SSIZE_MAX
#define SSIZE_MAX INTPTR_MAX
#endif

#ifndef SSIZE_MIN
#define SSIZE_MIN INTPTR_MIN
#endif

typedef intptr_t ssize_t;
