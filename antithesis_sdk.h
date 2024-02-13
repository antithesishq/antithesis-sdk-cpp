#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <array>
#include <source_location>
#include <map>
#include <variant>
#include <sstream>
#include <iomanip>

namespace antithesis {
    struct AssertionState {
        uint8_t false_not_seen : 1;
        uint8_t true_not_seen : 1;
        uint8_t rest : 6;

        AssertionState() : false_not_seen(true), true_not_seen(true), rest(0)  {}
    };

    struct JSON;

    typedef std::variant<std::string, bool, int, double, const char*, JSON> ValueType;

    struct JSON : std::map<std::string, ValueType> {
        JSON( std::initializer_list<std::pair<const std::string, ValueType>> args) : std::map<std::string, ValueType>(args) {}
    };

    static std::ostream& operator<<(std::ostream& out, const JSON& details);
    static std::ostream& operator<<(std::ostream& out, const ValueType& value) {
        std::visit([&](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                out << std::quoted(arg);
            } else if constexpr (std::is_same_v<T, bool>) {
                out << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, double>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, const char*>) {
                out << std::quoted(arg);
            } else if constexpr (std::is_same_v<T, JSON>) {
                if (arg.empty()) {
                    out << "null";
                } else {
                    out << arg;
                }
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        }, value);

        return out;
    }

    static std::ostream& operator<<(std::ostream& out, const JSON& details) {
        out << "{ ";

        bool first = true;
        for (auto [key, value] : details) {
            if (!first) {
                out << ", ";
            }
            out << std::quoted(key) << ": " << value;
            first = false;
        }

        out << " }";
        return out;
    }

    enum AssertType {
        EVERY,
        SOME,
        NONE
    };

    inline constexpr const char* get_assert_type(AssertType type) {
        switch (type) {
            case EVERY: return "every";
            case SOME: return "some";
            case NONE: return "none";
        }
    }

    static const uint8_t MUST_HIT_FLAG = 0x4;

    inline constexpr uint8_t get_assertion_config(AssertType type, bool must_hit) {
        return static_cast<uint8_t>(type) | (must_hit ? MUST_HIT_FLAG : 0);
    }
    inline constexpr std::pair<AssertType, bool> from_assertion_config(uint8_t config) {
        AssertType type = static_cast<AssertType>(config & (MUST_HIT_FLAG - 1));
        bool must_hit = config & MUST_HIT_FLAG;
        return std::make_pair(type, must_hit);
    }

    struct LocationInfo {
        const char* class_name;
        const char* function_name;
        const char* file_name;
        const int line;
        const int column;

        JSON to_json() const {
            return JSON{
                {"classname", class_name},
                {"function", function_name},
                {"filename", file_name},
                {"line", line},
                {"columnn", column},
            };
        }
    };

    std::string make_key(const char* message, const LocationInfo& location_info) {
        // TODO: revisit with better keys
        std::ostringstream out;
        out << location_info.file_name << "|" << location_info.line << "|" << location_info.column;
        return out.str();
    }

    void assert_impl(const char* message, bool cond, const JSON& details, const LocationInfo& location_info,
                    bool hit, bool must_hit, bool expecting, const char* assert_type) {
        std::string id = make_key(message, location_info);

        JSON assertion{
            {"antithesis_assert", JSON{
                {"hit", hit},
                {"must_hit", must_hit},
                {"assert_type", assert_type},
                {"expecting", expecting},
                {"category", ""},
                {"message", message},
                {"condition", cond},
                {"id", id},
                {"location", location_info.to_json()},
                {"details", details},
            }}
        };
        std::ostringstream out;
        out << assertion;
        printf("%s\n", out.str().c_str());
    }

    void assert_raw(const char* message, bool cond, const JSON& details, 
                            const char* class_name, const char* function_name, const char* file_name, const int line, const int column,     
                            bool hit, bool must_hit, bool expecting, const char* assert_type) {
        LocationInfo location_info{ class_name, function_name, file_name, line, column };
        assert_impl(message, cond, details, location_info, hit, must_hit, expecting, assert_type);
    }

    struct Assertion {
        AssertionState state;
        AssertType type;
        bool must_hit;
        const char* message;
        LocationInfo location;

        Assertion(const char* message, AssertType type, bool must_hit, LocationInfo&& location) : 
            state(), type(type), must_hit(must_hit), message(message), location(std::move(location)) { 
            this->add_to_catalog();
        }

        void add_to_catalog() const {
            const bool condition = (type == NONE ? true : false);
            const bool hit = false;
            const char* assert_type = get_assert_type(type);
            const bool expecting = true;
            assert_impl(message, condition, {}, location, hit, must_hit, expecting, assert_type);
        }

        [[clang::always_inline]] inline void check_assertion(bool cond, const JSON& details) {
            if (__builtin_expect(state.false_not_seen || state.true_not_seen, false)) {
                check_assertion_internal(cond, details);
            }
        }

        private:
        void check_assertion_internal(bool cond, const JSON& details) {
            bool emit = false;
            if (!cond && state.false_not_seen) {
                emit = true;
                state.false_not_seen = false;   // TODO: is the race OK?
            }

            if (cond && state.true_not_seen) {
                emit = true;
                state.true_not_seen = false;   // TODO: is the race OK?
            }
            
            if (emit) {
                const bool hit = true;
                const char* assert_type = get_assert_type(type);
                const bool expecting = true;
                assert_impl(message, cond, details, location, hit, must_hit, expecting, assert_type);
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

    template <uint8_t config, fixed_string message, fixed_string file_name, fixed_string function_name, int line, int column>
    struct CatalogEntry {
        [[clang::always_inline]] static inline antithesis::Assertion create() {
            antithesis::LocationInfo location{ "", function_name.c_str(), file_name.c_str(), line, column };
            const auto [type, must_hit] = antithesis::from_assertion_config(config);
            return antithesis::Assertion(message.c_str(), type, must_hit, std::move(location));
        }

        static inline antithesis::Assertion assertion = create();
    };
}

#define FIXED_STRING_FROM_C_STR(s) (fixed_string<string_length(s)+1>::from_c_str(s))

#define ANTITHESIS_ASSERT_RAW(type, must_hit, cond, message, ...) ( \
    CatalogEntry< \
        antithesis::get_assertion_config(type, must_hit), \
        fixed_string(message), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
        std::source_location::current().line(), \
        std::source_location::current().column() \
    >::assertion.check_assertion(cond, (antithesis::JSON(__VA_ARGS__)) ) )

#define ALWAYS(cond, message, ...) ANTITHESIS_ASSERT_RAW(antithesis::EVERY, true, cond, message, __VA_ARGS__)
#define ALWAYS_OR_UNREACHABLE(cond, message, ...) ANTITHESIS_ASSERT_RAW(antithesis::EVERY, false, cond, message, __VA_ARGS__)
#define SOMETIMES(cond, message, ...) ANTITHESIS_ASSERT_RAW(antithesis::SOME, true, cond, message, __VA_ARGS__)
#define REACHABLE(message, ...) ANTITHESIS_ASSERT_RAW(antithesis::NONE, true, true, message, __VA_ARGS__)
#define UNREACHABLE(message, ...) ANTITHESIS_ASSERT_RAW(antithesis::NONE, false, true, message, __VA_ARGS__)

