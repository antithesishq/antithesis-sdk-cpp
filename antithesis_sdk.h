#pragma once

#include <cstdio>
#include <string>
#include <array>
#include <source_location>

namespace antithesis {
    struct Assertion {
        bool failed;
        bool reached;
        const char* message;
        const char* function_name;
        const char* file_name;
        const int line;
        std::string id;

        Assertion(const char* message, const char* function_name, const char* file_name, const int line) : 
            failed(false), reached(false), message(message), function_name(function_name), file_name(file_name), line(line), id(std::string(message) + " in " + function_name) {
            this->add_to_catalog();
        }

        void add_to_catalog() const {
            printf("There is an assertion with ID `%s` at %s:%d in `%s` with message: `%s`\n", id.c_str(), file_name, line, function_name, message);
        }

        [[clang::always_inline]] inline void check_assertion(bool cond) {
            //(void)reached;  // Needed to cause registration to happen no matter what!
            if (!reached) {
                printf("The assertion with ID `%s` was reached\n", id.c_str());
                reached = true;  // TODO: is the race OK?  If not, use a static initialization instead
            }

            if (!cond && !failed) {
                printf("The assertion with ID `%s` failed: %s\n", id.c_str(), message);
                failed = true;   // TODO: is the race OK?
            }
        }
    };
}

namespace {
    template <std::size_t N>
    struct fixed_string {
        std::array<char, N> contents;
        constexpr fixed_string() {
            for(int i=0; i<N; i++) contents[i] = 0;
        }
        constexpr fixed_string( const char (&arr)[N] )
        {
            for(int i=0; i<N; i++) contents[i] = arr[i];
        }
        static constexpr fixed_string from_c_str( const char* foo ) {
            fixed_string<N> it;
            for(int i=0; i<N && foo[i]; i++)
                it.contents[i] = foo[i];
            return it;
        }
        const char* c_str() const { return contents.data(); }
    };

    static constexpr size_t string_length( const char * s ) {
        for(int l = 0; ; l++)
            if (!s[l])
                return l;
    }

    template <fixed_string message, fixed_string file_name, fixed_string function_name, int line>
    struct CatalogEntry {
        [[clang::always_inline]] static inline antithesis::Assertion create() {
            return antithesis::Assertion(message.c_str(), function_name.c_str(), file_name.c_str(), line);
        }

        static inline antithesis::Assertion assertion = create();
    };
}

#define FIXED_STRING_FROM_C_STR(s) (fixed_string<string_length(s)+1>::from_c_str(s))

#define ALWAYS(cond, message) ( \
    CatalogEntry< \
        fixed_string(message), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
        std::source_location::current().line() \
    >::assertion.check_assertion(cond) )