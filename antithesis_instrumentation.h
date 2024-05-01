#pragma once

// This header file contains the Antithesis C++ SDK, which enables C++ applications to integrate with the [Antithesis platform].
//
// Documentation for the SDK is found at https://antithesis.com/docs/using_antithesis/sdk/cpp_sdk.html.
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <cstdint>


// If the libvoidstar(determ) library is present, 
// pass thru trace_pc_guard related callbacks to it
typedef void (*trace_pc_guard_init_fn)(uint32_t *start, uint32_t *stop);
typedef void (*trace_pc_guard_fn)(uint32_t *guard);

static trace_pc_guard_init_fn trace_pc_guard_init = nullptr;
static trace_pc_guard_fn trace_pc_guard = nullptr;
static bool did_check_libvoidstar = false;
static bool has_libvoidstar = false;

inline void message_out(const char *msg) {
  write(1, msg, strlen(msg));
  return;
}

inline void load_libvoidstar() {
    constexpr const char* LIB_PATH = "/usr/lib/libvoidstar.so";
    if (did_check_libvoidstar) {
      return;
    }
    did_check_libvoidstar = true;
    void* shared_lib = dlopen(LIB_PATH, RTLD_NOW);
    if (!shared_lib) {
        message_out("Can not load the Antithesis native library\n");
        return;
    }

    void* trace_pc_guard_init_sym = dlsym(shared_lib, "__sanitizer_cov_trace_pc_guard_init");
    if (!trace_pc_guard_init_sym) {
        message_out("Can not forward calls to libvoidstar for __sanitizer_cov_trace_pc_guard_init\n");
        return;
    }

    void* trace_pc_guard_sym = dlsym(shared_lib, "__sanitizer_cov_trace_pc_guard");
    if (!trace_pc_guard_sym) {
        message_out("Can not forward calls to libvoidstar for __sanitizer_cov_trace_pc_guard\n");
        return;
    }

    trace_pc_guard_init = reinterpret_cast<trace_pc_guard_init_fn>(trace_pc_guard_init_sym);
    trace_pc_guard = reinterpret_cast<trace_pc_guard_fn>(trace_pc_guard_sym);
    has_libvoidstar = true;
}

// The following symbols are indeed reserved identifiers, since we're implementing functions defined
// in the compiler runtime. Not clear how to get Clang on board with that besides narrowly suppressing
// the warning in this case. The sample code on the CoverageSanitizer documentation page fails this 
// warning!
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
extern "C" void __sanitizer_cov_trace_pc_guard_init(uint32_t *start, uint32_t *stop) {
    message_out("SDK forwarding to libvoidstar for __sanitizer_cov_trace_pc_guard_init()\n");
    if (!did_check_libvoidstar) {
        load_libvoidstar();
    }
    if (has_libvoidstar) {
        trace_pc_guard_init(start, stop);
    }
    return;
}

extern "C" void __sanitizer_cov_trace_pc_guard( uint32_t *guard ) {
    if (has_libvoidstar) {
        trace_pc_guard(guard);
    } else {
        if (guard) {
          *guard = 0;
        }
    }
    return;
}
#pragma clang diagnostic pop
