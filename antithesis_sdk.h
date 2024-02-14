#pragma once

#ifdef NO_ANTITHESIS_SDK

#define ALWAYS(cond, message, ...)
#define ALWAYS_OR_UNREACHABLE(cond, message, ...)
#define SOMETIMES(cond, message, ...)
#define REACHABLE(message, ...)
#define UNREACHABLE(message, ...)

#else
#include <cstdio>
#include <cstdint>
#include <string>
#include <array>
#include <source_location>
#include <map>
#include <variant>
#include <sstream>
#include <iomanip>
#include <dlfcn.h>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <random>

namespace antithesis {
    constexpr const char* const ERROR_LOG_LINE_PREFIX = "[* antithesis-sdk-cpp *]";
    constexpr const char* LIB_PATH = "/usr/lib/libvoidstar.so";

    struct LibHandler {
        virtual ~LibHandler() = default;
        virtual void output(const char* message) const = 0;
        virtual uint64_t random() = 0;
    };

    struct AntithesisHandler : LibHandler {
        void output(const char* message) const override {
            fuzz_json_data(message, strlen(message));
            fuzz_flush();
        }

        uint64_t random() override {
            return fuzz_get_random();
        }

        static std::unique_ptr<AntithesisHandler> create() {
            void* shared_lib = dlopen(LIB_PATH, RTLD_NOW);
            if (!shared_lib) {
                error("Can not load the Antithesis native library");
                return nullptr;
            }

            void* fuzz_json_data = dlsym(shared_lib, "fuzz_json_data");
            if (!fuzz_json_data) {
                error("Can access symbol fuzz_json_data");
                return nullptr;
            }

            void* fuzz_flush = dlsym(shared_lib, "fuzz_flush");
            if (!fuzz_flush) {
                error("Can access symbol fuzz_flush");
                return nullptr;
            }

            void* fuzz_get_random = dlsym(shared_lib, "fuzz_get_random");
            if (!fuzz_get_random) {
                error("Can access symbol fuzz_get_random");
                return nullptr;
            }

            return std::unique_ptr<AntithesisHandler>(new AntithesisHandler(
                reinterpret_cast<fuzz_json_data_t>(fuzz_json_data),
                reinterpret_cast<fuzz_flush_t>(fuzz_flush),
                reinterpret_cast<fuzz_get_random_t>(fuzz_get_random)));
        }

    private:
        typedef void (*fuzz_json_data_t)( const char* message, size_t length );
        typedef void (*fuzz_flush_t)();
        typedef uint64_t (*fuzz_get_random_t)();


        fuzz_json_data_t fuzz_json_data;
        fuzz_flush_t fuzz_flush;
        fuzz_get_random_t fuzz_get_random;

        AntithesisHandler(fuzz_json_data_t fuzz_json_data, fuzz_flush_t fuzz_flush, fuzz_get_random_t fuzz_get_random) :
            fuzz_json_data(fuzz_json_data), fuzz_flush(fuzz_flush), fuzz_get_random(fuzz_get_random) {}

        static void error(const char* message) {
            fprintf(stderr, "%s %s: %s\n", ERROR_LOG_LINE_PREFIX, message, dlerror());
        }
    };

    struct LocalHandler : LibHandler{
        ~LocalHandler() {
            if (file != nullptr) {
                fclose(file);
            }
        }

        void output(const char* message) const override {
            if (file != nullptr) {
                fprintf(file, "%s\n", message);
            }
        }

        uint64_t random() override {
            return distribution(gen);
        }

        static std::unique_ptr<LocalHandler> create() {
            return std::unique_ptr<LocalHandler>(new LocalHandler(create_internal()));
        }
    private:
        static constexpr const char* LOCAL_OUTPUT_ENVIRONMENT_VARIABLE = "ANTITHESIS_SDK_LOCAL_OUTPUT";

        FILE* file;
        std::random_device device;
        std::mt19937_64 gen;
        std::uniform_int_distribution<unsigned long long> distribution;

        LocalHandler(FILE* file): file(file), device(), gen(device()), distribution() {
        }

        // If `localOutputEnvVar` is set to a non-empty path, attempt to open that path and truncate the file
        // to serve as the log file of the local handler.
        // Otherwise, we don't have a log file, and logging is a no-op in the local handler.
        static FILE* create_internal() {
            const char* path = std::getenv(LOCAL_OUTPUT_ENVIRONMENT_VARIABLE);
            if (!path || !path[0]) {
                return nullptr;
            }

            // Open the file for writing (create if needed and possible) and truncate it
            FILE* file = fopen(path, "w");
            if (file == nullptr) {
                fprintf(stderr, "%s Failed to open path %s: %s\n", ERROR_LOG_LINE_PREFIX, path, strerror(errno));
                return nullptr;
            }
            int ret = fchmod(fileno(file), 0644);
            if (ret != 0) {
                fprintf(stderr, "%s Failed to set permissions for path %s: %s\n", ERROR_LOG_LINE_PREFIX, path, strerror(errno));
                return nullptr;
            }

            return file;
        }
    };

    std::unique_ptr<LibHandler> init() {
        struct stat stat_buf;
        if (stat(LIB_PATH, &stat_buf) == 0) {
            std::unique_ptr<LibHandler> tmp = AntithesisHandler::create();
            if (!tmp) {
                fprintf(stderr, "%s Failed to create handler for Antithesis library\n", ERROR_LOG_LINE_PREFIX);
                exit(-1);
            }
            return tmp;
        } else {
            return LocalHandler::create();
        }
    }

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
        static auto lib_handler = init();

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
        lib_handler->output(out.str().c_str());
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
#endif
