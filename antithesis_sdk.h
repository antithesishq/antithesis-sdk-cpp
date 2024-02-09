#pragma once

#include <stdio.h>
#include <string>
#include <array>
#include <source_location>

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
        constexpr size_t size() const { return N-1; }

        template<std::size_t M>
        constexpr fixed_string<N+M-1> concat( fixed_string<M> rhs ) const {
            fixed_string< (N-1) + (M-1) + 1 > ret;
            for(size_t i=0; i<N-1; i++)
                ret.contents[i] = this->contents[i];
            for(size_t i=0; i<M-1; i++)
                ret.contents[N-1+i] = rhs.contents[i];
            ret.contents[N-1 + M-1] = 0;
            return ret;
        }
    };

    static constexpr size_t string_length( const char * s ) {
        for(int l = 0; ; l++)
            if (!s[l])
                return l;
    }

    template <class File, class FuncName>
    struct src_location {
        File file_name;
        FuncName function_name;
        int line;
    };

    template <fixed_string ID, fixed_string message, src_location loc >
    struct CatalogEntry {
        [[clang::always_inline]] static inline void check_assertion(bool cond) {
            //(void)reached;  // Needed to cause registration to happen no matter what!
            if (!reached) {
                printf("The assertion with ID `%s` was reached\n", ID.c_str());
                reached = true;  // TODO: is the race OK?  If not, use a static initialization instead
            }

            if (!cond && !failed) {
                printf("The assertion with ID `%s` failed: %s\n", ID.c_str(), message.c_str());
                failed = true;   // TODO: is the race OK?
            }
        }
        [[clang::always_inline]] static inline void add_to_catalog() {
            printf("There is an assertion with ID `%s` at %s:%d in `%s` with message: `%s`\n", ID.c_str(), loc.file_name.c_str(), loc.line, loc.function_name.c_str(), message.c_str());
        }

        static inline bool reached = (CatalogEntry<ID,message,loc>::add_to_catalog(), false);
        static inline bool failed = false;
    };
}

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define FIXED_STRING_FROM_C_STR(s) (fixed_string<string_length(s)+1>::from_c_str(s))

#define ALWAYS(cond, message) ( \
    CatalogEntry< \
        fixed_string(message " in ").concat( FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()) ), \
        fixed_string(message), \
        src_location { \
            FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
            FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
            std::source_location::current().line() \
        } \
    >::check_assertion(cond) )
