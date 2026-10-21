#pragma once

// This header file contains the Antithesis C++ SDK, which enables C++ applications to integrate with the [Antithesis platform].
//
// Documentation for the SDK is found at https://antithesis.com/docs/using_antithesis/sdk/cpp/.

#ifndef NO_ANTITHESIS_SDK

#if __cplusplus < 202000L
    #error "The Antithesis C++ API requires C++20 or higher"
    #define NO_ANTITHESIS_SDK
#endif

#if !defined(__clang__)
    #error "The Antithesis C++ API requires a clang compiler"
    #define NO_ANTITHESIS_SDK
#endif

#if __clang_major__ < 16
    #error "The Antithesis C++ API requires clang version 16 or higher"
    #define NO_ANTITHESIS_SDK
#endif

#else

#if __cplusplus < 201700L
    #error "The Antithesis C++ API (with NO_ANTITHESIS_SDK) requires C++17 or higher"
#endif

#endif

/*****************************************************************************
 * COMMON
 *****************************************************************************/

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>
#include <utility>

namespace antithesis {
    inline const char* SDK_VERSION = "0.5.1";
    inline const char* PROTOCOL_VERSION = "1.1.0";

    struct JSON; struct JSONArray;
    typedef std::variant<JSON, std::nullptr_t, std::string, bool, char, int, unsigned, int64_t, uint64_t, float, double, const char*, JSONArray> JSONValue;

    struct JSONArray : std::vector<JSONValue> {
        using std::vector<JSONValue>::vector;

        template<typename T, typename std::enable_if<std::is_convertible<T, JSONValue>::value, bool>::type = true>
        JSONArray(std::vector<T> vals) : std::vector<JSONValue>(vals.begin(), vals.end()) {}
    };

    struct JSON : std::map<std::string, JSONValue> {
        JSON() : std::map<std::string, JSONValue>() {}
        JSON( std::initializer_list<std::pair<const std::string, JSONValue>> args) : std::map<std::string, JSONValue>(args) {}

        JSON( std::initializer_list<std::pair<const std::string, JSONValue>> args, std::vector<std::pair<const std::string, JSONValue>> more_args ) : std::map<std::string, JSONValue>(args) {
            for (auto& pair : more_args) {
                (*this)[pair.first] = pair.second;
            }
        }
    };
}


/*****************************************************************************
 * INTERNAL HELPERS: LOCAL RANDOM
 * Used in both the NO_ANTITHESIS_SDK version and when running locally
 *****************************************************************************/

#include <random>

namespace antithesis::internal::random {
    struct LocalRandom {
        std::random_device device;
        std::mt19937_64 gen;
        std::uniform_int_distribution<unsigned long long> distribution;

        LocalRandom() : device(), gen(device()), distribution() {}

        uint64_t random() {
#ifdef ANTITHESIS_RANDOM_OVERRIDE
            return ANTITHESIS_RANDOM_OVERRIDE();
#else
            return distribution(gen);
#endif
        }
    };
}

/*****************************************************************************
 * INTERNAL HELPERS: JSON
 *****************************************************************************/

#ifndef NO_ANTITHESIS_SDK

#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace antithesis::internal::json {
    template<class>
    inline constexpr bool always_false_v = false;

    // Floating-point values are serialized with std::to_chars, which
    // produces the shortest representation that round-trips exactly.
    template<typename F>
    static void write_float(std::ostream& out, F value) {
        if (!std::isfinite(value)) {
            out << (std::isnan(value) ? "\"NaN\"" : value > 0 ? "\"Infinity\"" : "\"-Infinity\"");
            return;
        }
        std::array<char, 64> buffer;
        std::to_chars_result result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        out.write(buffer.data(), result.ptr - buffer.data());
        for (const char* c = buffer.data(); c != result.ptr; c++) {
            if ((*c < '0' || *c > '9') && *c != '-') {
                return;
            }
        }
        // Preserve the floating-point type for consumers that distinguish JSON
        // integer tokens, including -0 whose sign would otherwise be lost.
        out.write(".0", 2);
    }

    static std::ostream& operator<<(std::ostream& out, const JSON& details);

    static void escaped(std::ostream& out, const char c) {
        const char HEX[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
        switch (c) {
            case '\t': out << "\\t"; break;
            case '\b': out << "\\b"; break;
            case '\n': out << "\\n"; break;
            case '\f': out << "\\f"; break;
            case '\r': out << "\\r"; break;
            case '\"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            default:
                if ('\u0000' <= c && c <= '\u001F') {
                    out << "\\u00" << HEX[(c >> 4) & 0x0F] << HEX[c & 0x0F];
                } else {
                    out << c;
                }
        }
    }

    static std::ostream& operator<<(std::ostream& out, const JSONValue& json) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                out << '"';
                for (auto c : arg) {
                    escaped(out, c);
                }
                out << '"';
            } else if constexpr (std::is_same_v<T, bool>) {
                out << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, char>) {
                out << '"';
                escaped(out, arg);
                out << '"';
            } else if constexpr (std::is_same_v<T, int>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, unsigned>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, float>) {
                write_float(out, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                write_float(out, arg);
            } else if constexpr (std::is_same_v<T, const char*>) {
                out << '"';
                for (auto str = arg; *str != '\0'; str++) {
                    escaped(out, *str);
                }
                out << '"';
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                out << "null";
            } else if constexpr (std::is_same_v<T, JSON>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, JSONArray>) {
                out << '[';
                bool first = true;
                for (auto &item : arg) {
                    if (!first) {
                        out << ',';
                    }
                    first = false;
                    out << item;
                }
                out << ']';
            } else {
                static_assert(always_false_v<T>, "non-exhaustive JSONValue visitor!");
            }
        }, json);

        return out;
    }

    static std::ostream& operator<<(std::ostream& out, const JSON& details) {
        out << '{';

        bool first = true;
        for (const auto& [key, value] : details) {
            if (!first) {
                out << ',';
            }
            out << '"';
            for (auto c : key) {
                escaped(out, c);
            }
            out << '"' << ':' << value;
            first = false;
        }

        out << '}';
        return out;
    }

    struct ObjectView {
        const JSON* object = nullptr;
        std::span<const JSON::value_type> fields;
        std::span<const JSON::value_type> additional;
    };

    template<typename F>
    static void for_each_field(const ObjectView& view, F&& visit) {
        if (view.object != nullptr && view.additional.empty()) {
            for (const auto& [key, value] : *view.object) visit(key, value);
            return;
        }
        std::vector<const JSON::value_type*> fields;
        fields.reserve((view.object != nullptr ? view.object->size() : view.fields.size()) + view.additional.size());
        // Stable sorting keeps the last added value first for each key, so
        // overwritten values are neither serialized nor scanned for warnings.
        for (auto it = view.additional.rbegin(); it != view.additional.rend(); ++it) {
            fields.push_back(&*it);
        }
        if (view.object != nullptr) {
            for (const auto& field : *view.object) fields.push_back(&field);
        } else {
            for (const auto& field : view.fields) fields.push_back(&field);
        }
        std::stable_sort(fields.begin(), fields.end(), [](const auto* left, const auto* right) {
            return left->first < right->first;
        });
        const std::string* previous = nullptr;
        for (const auto* field : fields) {
            const auto& [key, value] = *field;
            if (previous != nullptr && *previous == key) continue;
            visit(key, value);
            previous = &key;
        }
    }

    static std::ostream& operator<<(std::ostream& out, const ObjectView& view) {
        out << '{';
        bool first = true;
        for_each_field(view, [&](const std::string& key, const JSONValue& value) {
            if (!first) out << ',';
            first = false;
            out << '"';
            for (char c : key) escaped(out, c);
            out << '"' << ':' << value;
        });
        return out << '}';
    }

    struct DetailsWarnings {
        bool non_finite = false;
        bool unsafe_integer = false;

        void scan(const JSONValue& value) {
            std::visit([&](const auto& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, JSON>) {
                    scan(arg);
                } else if constexpr (std::is_same_v<T, JSONArray>) {
                    for (const auto& item : arg) scan(item);
                } else if constexpr (std::is_floating_point_v<T>) {
                    non_finite |= !std::isfinite(arg);
                } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>) {
                    unsafe_integer |= beyond_safe_range(arg);
                }
            }, value);
        }

        void scan(const JSON& object) {
            for (const auto& [key, value] : object) scan(value);
        }

        void scan(const ObjectView& view) {
            for_each_field(view, [&](const std::string&, const JSONValue& value) { scan(value); });
        }

    private:
        template<typename I>
        static bool beyond_safe_range(I n) {
            constexpr int64_t limit = int64_t{1} << 53;
            if constexpr (std::is_signed_v<I>) {
                return n >= limit || n <= -limit;
            } else {
                return n >= static_cast<uint64_t>(limit);
            }
        }
    };

    static void write_string(std::ostream& out, std::string_view value) {
        out << '"';
        for (char c : value) escaped(out, c);
        out << '"';
    }

    inline void write_value(std::ostream& out, const JSONValue& value) { out << value; }
    inline void write_value(std::ostream& out, const JSON& value) { out << value; }
    inline void write_value(std::ostream& out, const ObjectView& value) { out << value; }
    inline void write_value(std::ostream& out, const std::string& value) { write_string(out, value); }
    inline void write_value(std::ostream& out, const char* value) { write_string(out, value); }

    template<typename F> requires std::is_invocable_v<const F&, std::ostream&>
    void write_value(std::ostream& out, const F& write) { write(out); }

    struct ObjectWriter {
        std::ostream& out;
        bool first = true;

        template<typename T>
        void field(std::string_view name, const T& value) {
            if (!first) out << ',';
            first = false;
            write_string(out, name);
            out << ':';
            write_value(out, value);
        }
    };

    template<typename F>
    void write_object(std::ostream& out, F&& fields) {
        out << '{';
        ObjectWriter writer{out};
        fields(writer);
        out << '}';
    }

}

#endif

/*****************************************************************************
 * INTERNAL HELPERS: HANDLERS
 * Implementations for running locally and running in Antithesis
 *****************************************************************************/

#ifndef NO_ANTITHESIS_SDK

#include <cstdio>
#include <iostream>
#include <sstream>
#include <dlfcn.h>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


namespace antithesis::internal::handlers {
    constexpr const char* const ERROR_LOG_LINE_PREFIX = "[* antithesis-sdk-cpp *]";
    constexpr const char* LIB_PATH = "/usr/lib/libvoidstar.so";
    constexpr const char* LOCAL_OUTPUT_ENVIRONMENT_VARIABLE = "ANTITHESIS_SDK_LOCAL_OUTPUT";

    using namespace antithesis::internal::json;
    
    struct LibHandler {
        virtual ~LibHandler() = default;
        virtual void output(std::string message) const = 0;
        virtual uint64_t random() = 0;

        template<typename F>
        void output_json(F&& write) const {
            std::ostringstream out;
            write(out);
            output(std::move(out).str());
        }

        void output(const JSON& json) const {
            output_json([&](std::ostream& out) { out << json; });
        }
    };

    struct AntithesisHandler : LibHandler {
        void output(std::string message) const override {
            fuzz_json_data(message.data(), message.size());
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
                error("Can not access symbol fuzz_json_data");
                return nullptr;
            }

            void* fuzz_flush = dlsym(shared_lib, "fuzz_flush");
            if (!fuzz_flush) {
                error("Can not access symbol fuzz_flush");
                return nullptr;
            }

            void* fuzz_get_random = dlsym(shared_lib, "fuzz_get_random");
            if (!fuzz_get_random) {
                error("Can not access symbol fuzz_get_random");
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
        ~LocalHandler() override {
            if (file != nullptr) {
                fclose(file);
            }
        }

        void output(std::string message) const override {
            if (file != nullptr) {
                message.push_back('\n');
                // Using `fwrite` and a manually appended newline instead of `fprintf` because the latter will split writes in 4KB chunks,
                // whereas the former sends the whole line as a single `write(2)` syscall
                fwrite(message.data(), 1, message.size(), file);
            }
        }

        uint64_t random() override {
            thread_local antithesis::internal::random::LocalRandom random_gen;
            return random_gen.random();
        }

        static std::unique_ptr<LocalHandler> create() {
            return std::unique_ptr<LocalHandler>(new LocalHandler(create_internal()));
        }
    private:
        FILE* file;

        LocalHandler(FILE* file): file(file) {
        }

        // If `localOutputEnvVar` is set to a non-empty path, attempt to open that path for appending
        // to serve as the log file of the local handler.
        // Otherwise, we don't have a log file, and logging is a no-op in the local handler.
        static FILE* create_internal() {
            const char* path = std::getenv(LOCAL_OUTPUT_ENVIRONMENT_VARIABLE);
            if (!path || !path[0]) {
                return nullptr;
            }

            // Open the file for writing (create if needed and possible) in append mode
            FILE* file = fopen(path, "a");
            if (file == nullptr) {
                fprintf(stderr, "%s Failed to open path %s: %s\n", ERROR_LOG_LINE_PREFIX, path, strerror(errno));
                return nullptr;
            }
            // Set the buffer length to 0 so that each fwrite in output() reaches the file as a single write(2) whatever its size.
            if (setvbuf(file, nullptr, _IONBF, 0) != 0) {
                fprintf(stderr, "%s Failed to make output at %s unbuffered; records larger than the stream buffer may interleave with other writers\n", ERROR_LOG_LINE_PREFIX, path);
            }
            int ret = fchmod(fileno(file), 0644);
            if (ret != 0) {
                fprintf(stderr, "%s Failed to set permissions for path %s: %s\n", ERROR_LOG_LINE_PREFIX, path, strerror(errno));
                fclose(file);
                return nullptr;
            }

            return file;
        }
    };

    static std::unique_ptr<LibHandler> init() {
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

    inline LibHandler& get_lib_handler() {
        static LibHandler& lib_handler = *[]() {
            LibHandler* handler = init().release();

            JSON language_block{
              {"name", "C++"},
              {"version", __VERSION__}
            };

            JSON version_message{
                {"antithesis_sdk", JSON{
                    {"language", language_block},
                    {"sdk_version", SDK_VERSION},
                    {"protocol_version", PROTOCOL_VERSION}
                }
            }};
            handler->output(version_message);
            return handler;
        }();

        return lib_handler;
    }

    inline void output_warning(int code, const std::string& message, const char* context) {
        get_lib_handler().output_json([&](std::ostream& out) {
            write_object(out, [&](auto& event) {
                event.field("antithesis_warning", [&](std::ostream& out) {
                    write_object(out, [&](auto& warning) {
                        warning.field("code", code);
                        warning.field("context", context);
                        warning.field("message", message);
                    });
                });
            });
        });
    }

    inline std::atomic<bool> non_finite_reported{false};
    inline std::atomic<bool> unsafe_integer_reported{false};
    inline std::atomic<bool> non_finite_operand_reported{false};

    template<typename Details>
    inline void output_details_warnings(const Details& details, const char* context) {
        DetailsWarnings warnings;
        warnings.scan(details);
        if (warnings.non_finite && !non_finite_reported.exchange(true, std::memory_order_relaxed)) {
            output_warning(6002, std::string("C++ SDK replaced non-finite numbers in details for \"") + context
                + "\" with the strings \"NaN\", \"Infinity\" or \"-Infinity\"; later occurrences in this process are not reported", context);
        }
        if (warnings.unsafe_integer && !unsafe_integer_reported.exchange(true, std::memory_order_relaxed)) {
            output_warning(6003, std::string("C++ SDK emitted integers beyond ±2^53 in details for \"") + context
                + "\"; consumers that parse JSON numbers as doubles will round them; later occurrences in this process are not reported", context);
        }
    }

    inline void output_non_finite_guidance_warning(const char* id) {
        if (non_finite_operand_reported.exchange(true, std::memory_order_relaxed)) return;
        output_warning(6004, std::string("C++ SDK skipped numeric guidance for \"") + id
            + "\" because an operand was not finite; later occurrences in this process are not reported", id);
    }
}

#endif

/*****************************************************************************
 * INTERNAL HELPERS: Various classes related to assertions
 *****************************************************************************/

#ifndef NO_ANTITHESIS_SDK

namespace antithesis::internal::assertions {
    using namespace antithesis::internal::handlers;

    struct AssertionState {
        std::atomic<bool> false_not_seen;
        std::atomic<bool> true_not_seen;

        AssertionState() : false_not_seen(true), true_not_seen(true) {}
    };

    enum AssertionType {
        ALWAYS_ASSERTION,
        ALWAYS_OR_UNREACHABLE_ASSERTION,
        SOMETIMES_ASSERTION,
        REACHABLE_ASSERTION,
        UNREACHABLE_ASSERTION,
    };

    inline constexpr bool get_must_hit(AssertionType type) {
        switch (type) {
            case ALWAYS_ASSERTION:
            case SOMETIMES_ASSERTION: 
            case REACHABLE_ASSERTION:
                return true;
            case ALWAYS_OR_UNREACHABLE_ASSERTION: 
            case UNREACHABLE_ASSERTION: 
                return false;
        }
    }

    inline constexpr const char* get_assert_type_string(AssertionType type) {
        switch (type) {
            case ALWAYS_ASSERTION:
            case ALWAYS_OR_UNREACHABLE_ASSERTION: 
                return "always";
            case SOMETIMES_ASSERTION: 
                return "sometimes";
            case REACHABLE_ASSERTION:
            case UNREACHABLE_ASSERTION: 
                return "reachability";
        }
    }

    inline constexpr const char* get_display_type_string(AssertionType type) {
        switch (type) {
            case ALWAYS_ASSERTION: return "Always";
            case ALWAYS_OR_UNREACHABLE_ASSERTION: return "AlwaysOrUnreachable";
            case SOMETIMES_ASSERTION: return "Sometimes";
            case REACHABLE_ASSERTION: return "Reachable";
            case UNREACHABLE_ASSERTION: return "Unreachable";
        }
    }

    struct LocationInfo {
        const char* class_name;
        const char* function_name;
        const char* file_name;
        const int line;
        const int column;

        void write_json(std::ostream& out) const {
            json::write_object(out, [&](auto& object) {
                object.field("begin_column", column);
                object.field("begin_line", line);
                object.field("class", class_name);
                object.field("file", file_name);
                object.field("function", function_name);
            });
        }
    };

    inline std::string make_key([[maybe_unused]] const char* message, const LocationInfo& location_info) {
        return message;
    }

    inline void assert_impl(bool cond, const char* message, const json::ObjectView* details, const LocationInfo& location_info,
                    bool hit, bool must_hit, const char* assert_type, const char* display_type, const char* id) {
        if (hit && details != nullptr) output_details_warnings(*details, id);
        get_lib_handler().output_json([&](std::ostream& out) {
            json::write_object(out, [&](auto& event) {
                event.field("antithesis_assert", [&](std::ostream& out) {
                    json::write_object(out, [&](auto& assertion) {
                        assertion.field("assert_type", assert_type);
                        assertion.field("condition", cond);
                        if (hit && details != nullptr) assertion.field("details", *details);
                        assertion.field("display_type", display_type);
                        assertion.field("hit", hit);
                        assertion.field("id", id);
                        assertion.field("location", [&](std::ostream& out) { location_info.write_json(out); });
                        assertion.field("message", message);
                        assertion.field("must_hit", must_hit);
                    });
                });
            });
        });
    }

    struct TrackedAssertion {
        std::atomic<bool> registered{false};
        AssertionState state;
    };

    inline TrackedAssertion& tracked_assertion(const std::string& id) {
        [[clang::no_destroy]] static std::mutex mutex;
        [[clang::no_destroy]] static std::unordered_map<std::string, TrackedAssertion> tracker;
        std::lock_guard<std::mutex> lock(mutex);
        return tracker[id];
    }

    inline bool raw_assert_gate(bool cond, const char* id) {
        AssertionState& state = tracked_assertion(id).state;
        std::atomic<bool>& not_seen = cond ? state.true_not_seen : state.false_not_seen;
        return not_seen.exchange(false, std::memory_order_relaxed);
    }

    inline void assert_raw_impl(bool cond, const char* message, const JSON* details,
                            const char* class_name, const char* function_name, const char* file_name, const int line, const int column,
                            bool hit, bool must_hit, const char* assert_type, const char* display_type, const char* id,
                            std::optional<std::span<const JSON::value_type>> guidance) {
        if (hit && !raw_assert_gate(cond, id)) {
            return;
        }
        LocationInfo location_info{ class_name, function_name, file_name, line, column };
        json::ObjectView view{details, {}, guidance.value_or(std::span<const JSON::value_type>{})};
        const bool present = details != nullptr || guidance.has_value();
        assert_impl(cond, message, present ? &view : nullptr, location_info,
                    hit, must_hit, assert_type, display_type, id);
    }

    inline void assert_raw(bool cond, const char* message, const JSON& details,
                            const char* class_name, const char* function_name, const char* file_name, const int line, const int column,
                            bool hit, bool must_hit, const char* assert_type, const char* display_type, const char* id,
                            std::optional<std::span<const JSON::value_type>> guidance = std::nullopt) {
        assert_raw_impl(cond, message, &details, class_name, function_name, file_name, line, column,
                        hit, must_hit, assert_type, display_type, id, guidance);
    }

    inline void assert_raw(bool cond, const char* message,
                            const char* class_name, const char* function_name, const char* file_name, const int line, const int column,
                            bool hit, bool must_hit, const char* assert_type, const char* display_type, const char* id,
                            std::optional<std::span<const JSON::value_type>> guidance = std::nullopt) {
        assert_raw_impl(cond, message, nullptr, class_name, function_name, file_name, line, column,
                        hit, must_hit, assert_type, display_type, id, guidance);
    }

    struct Assertion {
        AssertionState state;
        AssertionType type;
        const char* message;
        LocationInfo location;

        Assertion(const char* message, AssertionType type, LocationInfo&& location) : 
            state(), type(type), message(message), location(std::move(location)) { 
            this->add_to_catalog();
        }

        void add_to_catalog() const {
            std::string id = make_key(message, location);

            if (tracked_assertion(id).registered.exchange(true, std::memory_order_relaxed)) {
                return;
            }
            const bool condition = (type == REACHABLE_ASSERTION ? true : false);
            const bool hit = false;
            const char* assert_type = get_assert_type_string(type);
            const bool must_hit = get_must_hit(type);
            const char* display_type = get_display_type_string(type);
            assert_impl(condition, message, nullptr, location, hit, must_hit, assert_type, display_type, id.c_str());
        }

        using Fields = std::initializer_list<JSON::value_type>;
        using AdditionalFields = std::span<const JSON::value_type>;

        [[clang::always_inline]] inline void check_assertion(auto&& cond)
            requires requires { static_cast<bool>(std::forward<decltype(cond)>(cond)); } {
            check_assertion_with_details(std::forward<decltype(cond)>(cond), nullptr, {}, {}, false);
        }

        [[clang::always_inline]] inline void check_assertion(auto&& cond, const JSON& details, Fields additional = {})
            requires requires { static_cast<bool>(std::forward<decltype(cond)>(cond)); } {
            check_assertion_with_details(std::forward<decltype(cond)>(cond), &details, {}, {additional.begin(), additional.size()}, true);
        }

        [[clang::always_inline]] inline void check_assertion(auto&& cond, Fields details, Fields additional = {})
            requires requires { static_cast<bool>(std::forward<decltype(cond)>(cond)); } {
            check_assertion_with_details(std::forward<decltype(cond)>(cond), nullptr, details, {additional.begin(), additional.size()}, true);
        }

        [[clang::always_inline]] inline void check_assertion(auto&& cond, Fields details,
                const std::vector<JSON::value_type>& additional)
            requires requires { static_cast<bool>(std::forward<decltype(cond)>(cond)); } {
            check_assertion_with_details(std::forward<decltype(cond)>(cond), nullptr, details, additional, true);
        }

        private:
        // Borrowed fields are consumed synchronously, and only when emitting.
        [[clang::always_inline]] inline void check_assertion_with_details(auto&& cond, const JSON* object,
                Fields fields, AdditionalFields additional, bool present) {
            if (__builtin_expect(state.false_not_seen.load(std::memory_order_relaxed)
                    || state.true_not_seen.load(std::memory_order_relaxed), false)) {
                check_assertion_internal(static_cast<bool>(std::forward<decltype(cond)>(cond)),
                    object, fields, additional, present);
            }
        }

        void check_assertion_internal(bool cond, const JSON* object, Fields fields, AdditionalFields additional, bool present) {
            // exchange() rather than read-then-write: exactly one of any
            // racing first evaluations wins each flag, so an assertion
            // cannot emit twice for one condition. The plain load in front
            // only skips the atomic store once the flag is already spent,
            // so repeated calls don't keep dirtying the cache line.
            bool emit = false;
            if (!cond && state.false_not_seen.load(std::memory_order_relaxed)
                      && state.false_not_seen.exchange(false, std::memory_order_relaxed)) {
                emit = true;
            }

            if (cond && state.true_not_seen.load(std::memory_order_relaxed)
                     && state.true_not_seen.exchange(false, std::memory_order_relaxed)) {
                emit = true;
            }

            if (emit) {
                const bool hit = true;
                const char* assert_type = get_assert_type_string(type);
                const bool must_hit = get_must_hit(type);
                const char* display_type = get_display_type_string(type);
                json::ObjectView details{object, {fields.begin(), fields.size()}, additional};
                assert_impl(cond, message, present ? &details : nullptr, location,
                    hit, must_hit, assert_type, display_type, message);
            }
        }
    };

    enum GuidepostType {
        GUIDEPOST_MAXIMIZE,
        GUIDEPOST_MINIMIZE,
        GUIDEPOST_EXPLORE,
        GUIDEPOST_ALL,
        GUIDEPOST_NONE
    };

    inline constexpr const char* get_guidance_type_string(GuidepostType type) {
        switch (type) {
            case GUIDEPOST_MAXIMIZE:
            case GUIDEPOST_MINIMIZE:
                return "numeric";
            case GUIDEPOST_ALL:
            case GUIDEPOST_NONE:
                return "boolean";
            case GUIDEPOST_EXPLORE:
                return "json";
        }
    }

    inline constexpr bool does_guidance_maximize(GuidepostType type) {
        switch (type) {
            case GUIDEPOST_MAXIMIZE:
            case GUIDEPOST_ALL:
                return true;
            case GUIDEPOST_EXPLORE:
            case GUIDEPOST_MINIMIZE:
            case GUIDEPOST_NONE:
                return false;
        }
    }

    inline constexpr double unset_extreme_gap() {
        return std::numeric_limits<double>::quiet_NaN();
    }

    inline constexpr bool guidance_improves(double gap, double current, GuidepostType type) {
        if (current != current) {
            return true;
        }
        return type == GUIDEPOST_MAXIMIZE ? gap > current : gap < current;
    }

    template <typename NumericValue>
    inline constexpr double numeric_gap(NumericValue left, NumericValue right) {
        return static_cast<double>(left) - static_cast<double>(right);
    }

    template<typename T>
    void emit_guidance(const char* id, const char* message, const LocationInfo& location, GuidepostType type, const T* data) {
        if constexpr (std::is_same_v<T, JSON>) {
            if (data != nullptr) output_details_warnings(*data, id);
        }
        get_lib_handler().output_json([&](std::ostream& out) {
            json::write_object(out, [&](auto& event) {
                event.field("antithesis_guidance", [&](std::ostream& out) {
                    json::write_object(out, [&](auto& guidance) {
                        if (data != nullptr) guidance.field("guidance_data", *data);
                        guidance.field("guidance_type", get_guidance_type_string(type));
                        guidance.field("hit", data != nullptr);
                        guidance.field("id", id);
                        guidance.field("location", [&](std::ostream& out) { location.write_json(out); });
                        guidance.field("maximize", does_guidance_maximize(type));
                        guidance.field("message", message);
                    });
                });
            });
        });
    }

    struct TrackedGuidance {
        std::atomic<double> extreme_gap{unset_extreme_gap()};
    };

    inline TrackedGuidance& tracked_guidance(const std::string& id) {
        [[clang::no_destroy]] static std::mutex mutex;
        [[clang::no_destroy]] static std::unordered_map<std::string, TrackedGuidance> tracker;
        std::lock_guard<std::mutex> lock(mutex);
        return tracker[id];
    }

    inline bool raw_guidance_gate(const char* id, GuidepostType type, double gap) {
        TrackedGuidance& gate = tracked_guidance(id);
        double current = gate.extreme_gap.load(std::memory_order_relaxed);
        while (guidance_improves(gap, current, type)) {
            if (gate.extreme_gap.compare_exchange_weak(
                    current, gap, std::memory_order_relaxed, std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }

    // These are low-level functions designed to be used by third-party
    // frameworks. Regular users of the SDK should use the guidance macros.

    template <typename NumericValue>
    inline void numeric_guidance_raw(NumericValue left, NumericValue right,
                            const char* class_name, const char* function_name, const char* file_name, const int line, const int column,
                            const char* id, const char* message, GuidepostType type, bool hit) {
        LocationInfo location{ class_name, function_name, file_name, line, column };
        if (!hit) {
            emit_guidance<JSON>(id, message, location, type, nullptr);
            return;
        }
        if (!std::isfinite(left) || !std::isfinite(right)) {
            output_non_finite_guidance_warning(id);
            return;
        }
        if (!raw_guidance_gate(id, type, numeric_gap(left, right))) {
            return;
        }
        JSON data{{"left", left}, {"right", right}};
        emit_guidance(id, message, location, type, &data);
    }

    inline void boolean_guidance_raw(const JSON& data,
                            const char* class_name, const char* function_name, const char* file_name, const int line, const int column,
                            const char* id, const char* message, GuidepostType type, bool hit) {
        LocationInfo location{ class_name, function_name, file_name, line, column };
        // Boolean guidance is never gated
        if (!hit) {
            emit_guidance<JSON>(id, message, location, type, nullptr);
            return;
        }
        emit_guidance(id, message, location, type, &data);
    }

    template <typename NumericValue, class Value=std::pair<NumericValue, NumericValue>>
    struct NumericGuidepost {
        const char* message;
        LocationInfo location;
        GuidepostType type;
        std::atomic<double> extreme_gap;
        std::mutex extreme_gap_mutex;

        NumericGuidepost(const char* message, LocationInfo&& location, GuidepostType type) :
            message(message), location(std::move(location)), type(type) {
                this->add_to_catalog();
                extreme_gap = unset_extreme_gap();
            }

        inline void add_to_catalog() {
            emit_guidance<JSON>(message, message, location, type, nullptr);
        }

        bool should_send_value(double gap) {
            return guidance_improves(gap, extreme_gap.load(std::memory_order_relaxed), this->type);
        }

        [[clang::always_inline]] inline void send_guidance(Value value) {
            if (!std::isfinite(value.first) || !std::isfinite(value.second)) {
                output_non_finite_guidance_warning(message);
                return;
            }
            double gap = numeric_gap(value.first, value.second);
            if (!should_send_value(gap)) {
                return;
            }
            std::lock_guard<std::mutex> lock(extreme_gap_mutex);
            if (should_send_value(gap)) {
                extreme_gap.store(gap, std::memory_order_relaxed);
                JSON data{{"left", value.first}, {"right", value.second}};
                emit_guidance(this->message, this->message, this->location, this->type, &data);
            }
        }
    };

    template <typename GuidanceType>
    struct BooleanGuidepost {
        const char* message;
        LocationInfo location;
        GuidepostType type;

        BooleanGuidepost(const char* message, LocationInfo&& location, GuidepostType type) :
            message(message), location(std::move(location)), type(type) {
                this->add_to_catalog();
            }

        inline void add_to_catalog() {
            emit_guidance<JSON>(message, message, location, type, nullptr);
        }

        inline virtual void send_guidance(const GuidanceType& data) {
            emit_guidance(this->message, this->message, this->location, this->type, &data);
        }
    };
}

namespace antithesis::internal {
namespace { // Anonymous namespace which is translation-unit-specific; certain symbols aren't exposed in the symbol table as a result
    enum msg_name_e { msg_name };

    template <std::size_t N>
    struct fixed_string {
        std::array<char, N> contents;
        constexpr fixed_string() {
            for(unsigned int i=0; i<N; i++) contents[i] = 0;
        }

        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

        constexpr fixed_string( const char (&arr)[N] )
        {
            for(unsigned int i=0; i<N; i++) contents[i] = arr[i];
        }

        constexpr fixed_string( const char (&arr)[N], msg_name_e )
        {
#ifdef ANTITHESIS_MAX_NAME_LEN
            static_assert(N <= ANTITHESIS_MAX_NAME_LEN + 1);
#endif
            for(unsigned int i=0; i<N; i++) contents[i] = arr[i];
        }

        static constexpr fixed_string<N> from_c_str( const char* s ) {
            fixed_string<N> it;
            for(unsigned int i=0; i<N && s[i]; i++)
                it.contents[i] = s[i];
            return it;
        }
        #pragma clang diagnostic pop

        const char* c_str() const { return contents.data(); }
    };

    template <std::size_t N>
    fixed_string( const char (&arr)[N] ) -> fixed_string<N>;

    template <std::size_t N>
    fixed_string( const char (&arr)[N], msg_name_e ) -> fixed_string<N>;

    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
    static constexpr size_t string_length( const char * s ) {
        for(int l = 0; ; l++)
            if (!s[l])
                return l;
    }
    #pragma clang diagnostic pop

    template <antithesis::internal::assertions::AssertionType type, fixed_string message, fixed_string file_name, fixed_string function_name, int line, int column>
    struct CatalogEntry {
        [[clang::always_inline]] static inline antithesis::internal::assertions::Assertion create() {
            antithesis::internal::assertions::LocationInfo location{ "", function_name.c_str(), file_name.c_str(), line, column };
            return antithesis::internal::assertions::Assertion(message.c_str(), type, std::move(location));
        }

        static inline antithesis::internal::assertions::Assertion assertion = create();
    };

    template<typename GuidanceDataType, antithesis::internal::assertions::GuidepostType type, fixed_string message, fixed_string file_name, fixed_string function_name, int line, int column>
    struct BooleanGuidanceCatalogEntry {
        [[clang::always_inline]] static inline antithesis::internal::assertions::BooleanGuidepost<GuidanceDataType> create() {
            antithesis::internal::assertions::LocationInfo location{ "", function_name.c_str(), file_name.c_str(), line, column };
            switch (type) {
                case antithesis::internal::assertions::GUIDEPOST_ALL:
                case antithesis::internal::assertions::GUIDEPOST_NONE:
                    return antithesis::internal::assertions::BooleanGuidepost<GuidanceDataType>(message.c_str(), std::move(location), type);
                default:
                    throw std::runtime_error("Can't create boolean guidepost with non-boolean type");
            }
        }
        
        static inline antithesis::internal::assertions::BooleanGuidepost<GuidanceDataType> guidepost = create();
    };

    template<typename NumericType, antithesis::internal::assertions::GuidepostType type, fixed_string message, fixed_string file_name, fixed_string function_name, int line, int column>
    struct NumericGuidanceCatalogEntry {
        [[clang::always_inline]] static inline antithesis::internal::assertions::NumericGuidepost<NumericType> create() {
            antithesis::internal::assertions::LocationInfo location{ "", function_name.c_str(), file_name.c_str(), line, column };
            switch (type) {
                case antithesis::internal::assertions::GUIDEPOST_MAXIMIZE:
                case antithesis::internal::assertions::GUIDEPOST_MINIMIZE:
                    return antithesis::internal::assertions::NumericGuidepost<NumericType>(message.c_str(), std::move(location), type);
                default:
                    throw std::runtime_error("Can't create numeric guidepost with non-numeric type");
            }
        }
        
        static inline antithesis::internal::assertions::NumericGuidepost<NumericType> guidepost = create();
    };
}
}

#endif

/*****************************************************************************
 * PUBLIC SDK: ASSERTIONS
 *****************************************************************************/

#define _NL_1(foo) { #foo, foo }
#define _NL_2(foo, ...) { #foo, foo }, _NL_1(__VA_ARGS__)
#define _NL_3(foo, ...) { #foo, foo }, _NL_2(__VA_ARGS__)
#define _NL_4(foo, ...) { #foo, foo }, _NL_3(__VA_ARGS__)
#define _NL_5(foo, ...) { #foo, foo }, _NL_4(__VA_ARGS__)
#define _NL_6(foo, ...) { #foo, foo }, _NL_5(__VA_ARGS__)
#define _NL_7(foo, ...) { #foo, foo }, _NL_6(__VA_ARGS__)
#define _NL_8(foo, ...) { #foo, foo }, _NL_7(__VA_ARGS__)
#define _NL_9(foo, ...) { #foo, foo }, _NL_8(__VA_ARGS__)
#define _NL_10(foo, ...) { #foo, foo }, _NL_9(__VA_ARGS__)

#define _ELEVENTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

#define _GET_NL(...) \
    _ELEVENTH_ARG(__VA_ARGS__, _NL_10, _NL_9, _NL_8, _NL_7, _NL_6, _NL_5, _NL_4, _NL_3, _NL_2, _NL_1)

#define NAMED_LIST(...) { _GET_NL(__VA_ARGS__)(__VA_ARGS__) }

#ifdef NO_ANTITHESIS_SDK

#ifndef ANTITHESIS_SDK_ALWAYS_POLYFILL
    #define ANTITHESIS_SDK_ALWAYS_POLYFILL(...) do {} while (0)
#endif

#ifndef ANTITHESIS_SDK_SOMETIMES_POLYFILL
    #define ANTITHESIS_SDK_SOMETIMES_POLYFILL(...) do {} while (0)
#endif

#ifndef ANTITHESIS_SDK_ALWAYS_OR_UNREACHABLE_POLYFILL
    #define ANTITHESIS_SDK_ALWAYS_OR_UNREACHABLE_POLYFILL(...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL(__VA_ARGS__)
#endif

#define ALWAYS(cond, message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL(cond, message, __VA_ARGS__)
#define ALWAYS_OR_UNREACHABLE(cond, message, ...) \
    ANTITHESIS_SDK_ALWAYS_OR_UNREACHABLE_POLYFILL(cond, message, __VA_ARGS__)
#define SOMETIMES(cond, message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL(cond, message, __VA_ARGS__)
#define REACHABLE(message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL(true, message, __VA_ARGS__)
#define UNREACHABLE(message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL(false, message, __VA_ARGS__)
#define ALWAYS_GREATER_THAN(val, threshold, message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL((val > threshold), message, __VA_ARGS__)
#define ALWAYS_GREATER_THAN_OR_EQUAL_TO(val, threshold, message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL((val >= threshold), message, __VA_ARGS__)
#define SOMETIMES_GREATER_THAN(val, threshold, message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL((val > threshold), message, __VA_ARGS__)
#define SOMETIMES_GREATER_THAN_OR_EQUAL_TO(val, threshold, message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL((val >= threshold), message, __VA_ARGS__)
#define ALWAYS_LESS_THAN(val, threshold, message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL((val < threshold), message, __VA_ARGS__)
#define ALWAYS_LESS_THAN_OR_EQUAL_TO(val, threshold, message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL((val <= threshold), message, __VA_ARGS__)
#define SOMETIMES_LESS_THAN(val, threshold, message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL((val < threshold), message, __VA_ARGS__)
#define SOMETIMES_LESS_THAN_OR_EQUAL_TO(val, threshold, message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL((val <= threshold), message, __VA_ARGS__)
#define ALWAYS_SOME(pairs, message, ...) \
    ANTITHESIS_SDK_ALWAYS_POLYFILL(([]( \
        std::initializer_list<std::pair<std::string, bool>> ps){ \
    for (auto const& pair : ps) \
        if (pair.second) return true; \
    return false; }(pairs)), message, __VA_ARGS__)
#define SOMETIMES_ALL(pairs, message, ...) \
    ANTITHESIS_SDK_SOMETIMES_POLYFILL(([]( \
        std::initializer_list<std::pair<std::string, bool>> ps){ \
    for (auto const& pair : ps) \
        if (!pair.second) return false; \
    return true; }(pairs)), message, __VA_ARGS__)

#else

#include <source_location>

#define FIXED_STRING_FROM_C_STR(s) (antithesis::internal::fixed_string<antithesis::internal::string_length(s)+1>::from_c_str(s))

#define ANTITHESIS_ASSERT_RAW(type, cond, message, ...) ( \
    antithesis::internal::CatalogEntry< \
        type, \
        antithesis::internal::fixed_string(message, antithesis::internal::msg_name), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
        std::source_location::current().line(), \
        std::source_location::current().column() \
    >::assertion.check_assertion(cond __VA_OPT__(, __VA_ARGS__)) )

#define ALWAYS(cond, message, ...) ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::ALWAYS_ASSERTION, cond, message, __VA_ARGS__)
#define ALWAYS_OR_UNREACHABLE(cond, message, ...) ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::ALWAYS_OR_UNREACHABLE_ASSERTION, cond, message, __VA_ARGS__)
#define SOMETIMES(cond, message, ...) ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::SOMETIMES_ASSERTION, cond, message, __VA_ARGS__)
#define REACHABLE(message, ...) ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::REACHABLE_ASSERTION, true, message, __VA_ARGS__)
#define UNREACHABLE(message, ...) ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::UNREACHABLE_ASSERTION, false, message, __VA_ARGS__)

#define ANTITHESIS_NUMERIC_ASSERT_RAW(name, assertion_type, guidepost_type, left, cmp, right, message, ...) \
do { \
    static_assert(std::is_same_v<decltype(left), decltype(right)>, "Values compared in " #name " must be of same type"); \
    /* The generated {left,right} pair lands in check_assertion's `additional` slot when the \
       caller passed details, and in the `details` slot otherwise; both serialize identically, \
       with the generated keys overriding any user keys of the same name. */ \
    ANTITHESIS_ASSERT_RAW(assertion_type, left cmp right, message, __VA_ARGS__ __VA_OPT__(,) {{ "left", left }, { "right", right }} ); \
    antithesis::internal::NumericGuidanceCatalogEntry< \
        decltype(left), \
        guidepost_type, \
        antithesis::internal::fixed_string(message, antithesis::internal::msg_name), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
        std::source_location::current().line(), \
        std::source_location::current().column() \
    >::guidepost.send_guidance({ left, right }); \
} while (0)

#define ALWAYS_GREATER_THAN(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(ALWAYS_GREATER_THAN, antithesis::internal::assertions::ALWAYS_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MINIMIZE, left, >, right, message, __VA_ARGS__)
#define ALWAYS_GREATER_THAN_OR_EQUAL_TO(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(ALWAYS_GREATER_THAN_OR_EQUAL_TO, antithesis::internal::assertions::ALWAYS_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MINIMIZE, left, >=, right, message, __VA_ARGS__)
#define SOMETIMES_GREATER_THAN(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(SOMETIMES_GREATER_THAN, antithesis::internal::assertions::SOMETIMES_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MAXIMIZE, left, >, right, message, __VA_ARGS__)
#define SOMETIMES_GREATER_THAN_OR_EQUAL_TO(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(SOMETIMES_GREATER_THAN_OR_EQUAL_TO, antithesis::internal::assertions::SOMETIMES_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MAXIMIZE, left, >=, right, message, __VA_ARGS__)
#define ALWAYS_LESS_THAN(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(ALWAYS_LESS_THAN, antithesis::internal::assertions::ALWAYS_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MAXIMIZE, left, <, right, message, __VA_ARGS__)
#define ALWAYS_LESS_THAN_OR_EQUAL_TO(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(ALWAYS_LESS_THAN_OR_EQUAL_TO, antithesis::internal::assertions::ALWAYS_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MAXIMIZE, left, <=, right, message, __VA_ARGS__)
#define SOMETIMES_LESS_THAN(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(SOMETIMES_LESS_THAN, antithesis::internal::assertions::SOMETIMES_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MINIMIZE, left, <, right, message, __VA_ARGS__)
#define SOMETIMES_LESS_THAN_OR_EQUAL_TO(left, right, message, ...) \
ANTITHESIS_NUMERIC_ASSERT_RAW(SOMETIMES_LESS_THAN_OR_EQUAL_TO, antithesis::internal::assertions::SOMETIMES_ASSERTION, antithesis::internal::assertions::GUIDEPOST_MINIMIZE, left, <=, right, message, __VA_ARGS__)

#define ALWAYS_SOME(pairs, message, ...) \
do { \
    ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::ALWAYS_ASSERTION, ( \
        [](std::initializer_list<std::pair<std::string, bool>> ps){ \
            for (auto const& pair : ps) \
                if (pair.second) return true; \
            return false; }(pairs)), \
        message, __VA_ARGS__ __VA_OPT__(,) pairs); \
    antithesis::internal::BooleanGuidanceCatalogEntry< \
        antithesis::JSON, \
        antithesis::internal::assertions::GUIDEPOST_NONE, \
        antithesis::internal::fixed_string(message, antithesis::internal::msg_name), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
        std::source_location::current().line(), \
        std::source_location::current().column() \
    >::guidepost.send_guidance(antithesis::JSON(pairs)); \
} while (0)

#define SOMETIMES_ALL(pairs, message, ...) \
do { \
    ANTITHESIS_ASSERT_RAW(antithesis::internal::assertions::SOMETIMES_ASSERTION, ( \
        [](std::initializer_list<std::pair<std::string, bool>> ps){ \
            for (auto const& pair : ps) \
                if (!pair.second) return false; \
            return true; }(pairs)), \
        message, __VA_ARGS__ __VA_OPT__(,) pairs); \
    antithesis::internal::BooleanGuidanceCatalogEntry< \
        antithesis::JSON, \
        antithesis::internal::assertions::GUIDEPOST_ALL, \
        antithesis::internal::fixed_string(message, antithesis::internal::msg_name), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().file_name()), \
        FIXED_STRING_FROM_C_STR(std::source_location::current().function_name()), \
        std::source_location::current().line(), \
        std::source_location::current().column() \
    >::guidepost.send_guidance(antithesis::JSON(pairs)); \
} while (0)

#endif

/*****************************************************************************
 * PUBLIC SDK: LIFECYCLE
 *****************************************************************************/

#ifdef NO_ANTITHESIS_SDK

namespace antithesis {
    inline void setup_complete() {
    }

    inline void setup_complete(const JSON& details) {
    }

    inline void send_event(const char* name, const JSON& details) {
    }
}

#else

namespace antithesis {
    inline void setup_complete() {
        internal::handlers::get_lib_handler().output_json([&](std::ostream& out) {
            internal::json::write_object(out, [&](auto& event) {
                event.field("antithesis_setup", [&](std::ostream& out) {
                    internal::json::write_object(out, [&](auto& setup) {
                        setup.field("status", "complete");
                    });
                });
            });
        });
    }

    inline void setup_complete(const JSON& details) {
        internal::handlers::output_details_warnings(details, "setup_complete");
        internal::handlers::get_lib_handler().output_json([&](std::ostream& out) {
            internal::json::write_object(out, [&](auto& event) {
                event.field("antithesis_setup", [&](std::ostream& out) {
                    internal::json::write_object(out, [&](auto& setup) {
                        setup.field("details", details);
                        setup.field("status", "complete");
                    });
                });
            });
        });
    }

    inline void send_event(const char* name, const JSON& details) {
        internal::handlers::output_details_warnings(details, name);
        internal::handlers::get_lib_handler().output_json([&](std::ostream& out) {
            internal::json::write_object(out, [&](auto& event) {
                event.field(name, details);
            });
        });
    }
}
#endif

/*****************************************************************************
 * PUBLIC SDK: RANDOM
 *****************************************************************************/

namespace antithesis {
    // Declarations that we expose
    uint64_t get_random();
}

#ifdef NO_ANTITHESIS_SDK

namespace antithesis {
    inline uint64_t get_random() {
        thread_local antithesis::internal::random::LocalRandom random_gen;
        return random_gen.random();
    }
}

#else

namespace antithesis {
    inline uint64_t get_random() {
        return antithesis::internal::handlers::get_lib_handler().random();
    }
}

#endif

namespace antithesis {
    template <typename Iter>
    Iter random_choice(Iter begin, Iter end) {
        ssize_t num_things = end - begin;
        // Returns end for an empty range, as std::max_element does.
        if (num_things == 0) {
            return end;
        }

        if (num_things == 1) {
            return begin;
        }

        uint64_t ceiling = (UINT64_MAX / num_things) * num_things;
        uint64_t uval = get_random();
        while (uval >= ceiling) {
            uval = get_random();
        }
        ssize_t index = uval % num_things;
        return begin + index;
    }
}
