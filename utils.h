/* Bunch of small macros and the like */
#ifndef GRADS_UTILS_H_
#define GRADS_UTILS_H_

#include "math.h"
#include <stdint.h>
#include <stdio.h>

#define ALWAYS_INLINE inline __attribute__((always_inline))

extern void time_ns_store(const uint64_t id);
extern uint64_t time_ns_load(const uint64_t id);

#define PRINT_TIME(id, time_ns)                                                                                        \
    printf("TIME: %.9lfs for " #id "\n", ((double) (time_ns) / 1e9));                                                  \
    if((time_ns) && (int) log10((double) (time_ns) / 1e9) > 0) {                                                       \
        printf("TIME:   %*smmmµµµnnn\n", (int) log10((double) (time_ns) / 1e9), "");                                   \
    } else {                                                                                                           \
        printf("TIME:   mmmµµµnnn\n");                                                                                 \
    }

#define ERROR(...)                                                                                                     \
    fprintf(stderr, "ERROR: " __VA_ARGS__);                                                                            \
    exit(1);

#define UNREACHABLE()                                                                                                  \
    ERROR("Reached `UNREACHABLE()` in %s at line %d at %s %s\n", __FILE__, __LINE__, __DATE__, __TIME__);

#define TODO()                                                                                                         \
    ERROR("Tried to execute not implemented feature at line %d in file %s\n", __LINE__, __FILE__);                     \
    exit(1);

#define WARN(...)                                                                                                      \
    if(getenv("LOGGER") && getenv("LOGGER")[0] >= '1') {                                                               \
        fprintf(stderr, "WARN: " __VA_ARGS__);                                                                         \
    }

#define INFO(...)                                                                                                      \
    if(getenv("LOGGER") && getenv("LOGGER")[0] >= '2') {                                                               \
        fprintf(stderr, "INFO: " __VA_ARGS__);                                                                         \
    }

#define ASSERT_MSG(condition, ...)                                                                                     \
    if(!(condition)) {                                                                                                 \
        fprintf(stderr, "Assertion failure: " __VA_ARGS__);                                                            \
        fprintf("in %s at line %d at %s %s.\n", __FILE__, __LINE__, __DATE__, __TIME__);                               \
        exit(1);                                                                                                       \
    }

#endif
