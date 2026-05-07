# Changelog

## 0.4.7 - 2026-02-13

Simplify numeric guidance internals. This fixes a bug where certain numeric comparisons could produce incorrect guidance results.

## 0.4.6 - 2026-02-09

Reduce verbosity of guidance tracking.

## 0.4.5 - 2025-01-03

Fix `ALWAYS_SOME` and `SOMETIMES_ALL` macro implementations.

## 0.4.4 - 2024-12-13

Add polyfill macros for `NO_ANTITHESIS_SDK` mode, which let you override the behavior of assertion macros when the SDK is compiled out, instead of them silently doing nothing.

Change `check_assertion` to handle boolean conversions better.

## 0.4.3 - 2024-11-12

JSON string escaping fix for ASCII control characters 0x00-0x1F.

Fixing compilation issues with SOMETIMES_ALL and ALWAYS_SOME.

The `details` argument is not optional in all assertion macros.

## 0.4.2 - 2024-10-10

Improvements to JSON handling

## 0.4.0 - 2024-07-01

Adding guidance-based assertions. These are both assertions and guidance for the fuzzer to explore your program more effectively.

## 0.3.1 - 2024-06-06

Dedup catalog entries.

## 0.3.0 - 2024-05-10

Instrumentation is now in a separate file: antithesis_instrumentation.h. 
Instrumentation supports both C++ and C. (The remainder of the SDK is C++-only.)

## 0.2.3 - 2024-04-25
Adding `inline` to __sanitizer_cov_trace_pc_guard_init and __sanitizer_cov_trace_pc_guard.

## 0.2.2 - 2024-03-29

Supporting coverage instrumentation via __sanitizer_cov_trace_pc_guard_init and __sanitizer_cov_trace_pc_guard.

Change to dlopen-based loading of libvoidstar.so
