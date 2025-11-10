#include "startup_testfunktion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void startup_test_entry(void);

#ifdef _MSC_VER
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (__cdecl *startup_constructor_ptr)(void) = startup_test_entry;
#ifdef _WIN64
#pragma comment(linker, "/include:startup_constructor_ptr")
#else
#pragma comment(linker, "/include:_startup_constructor_ptr")
#endif
#pragma startup startup_test_entry
#endif

typedef int (*StartupTestFunction)(char *message, size_t message_size);

typedef struct {
    const char *name;
    StartupTestFunction function;
} StartupTestCase;

static int test_memory_allocation(char *message, size_t message_size) {
    const size_t block_size = 1024U;
    void *block = malloc(block_size);
    if (block == NULL) {
        (void)snprintf(message, message_size, "Memory allocation of %lu bytes failed", (unsigned long)block_size);
        return 0;
    }

    memset(block, 0xA5, block_size);
    {
        const unsigned char expected_value = 0xA5U;
        size_t index;
        for (index = 0U; index < block_size; ++index) {
            if (((unsigned char *)block)[index] != expected_value) {
                free(block);
                (void)snprintf(message, message_size, "Memory verification failed at offset %lu", (unsigned long)index);
                return 0;
            }
        }
    }

    free(block);
    return 1;
}

static int test_arithmetic_edge_cases(char *message, size_t message_size) {
    const double numerator = 123456.0;
    const double denominator = 3.0;
    const double tolerance = 1e-9;

    if (fabs(denominator) < tolerance) {
        (void)snprintf(message, message_size, "Denominator too small for safe division");
        return 0;
    }

    {
        const double quotient = numerator / denominator;
        if (quotient <= 0.0) {
            (void)snprintf(message, message_size, "Unexpected quotient result: %.3f", quotient);
            return 0;
        }
    }

    {
        const double values[] = { 0.0, 1.0, -1.0, 1000.0, -1000.0 };
        double accumulated = 0.0;
        size_t index;
        for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
            const double next_value = values[index];
            const double new_total = accumulated + next_value;
            if (!isfinite(new_total)) {
                (void)snprintf(message, message_size, "Non-finite arithmetic result detected");
                return 0;
            }
            accumulated = new_total;
        }

        if (fabs(accumulated) > 1e6) {
            (void)snprintf(message, message_size, "Accumulated sum out of expected range: %.3f", accumulated);
            return 0;
        }
    }

    return 1;
}

static int test_string_boundaries(char *message, size_t message_size) {
    const char *input = "STARTUP-CHECK";
    char buffer[32];
    const size_t input_length = strlen(input);

    if (input_length >= sizeof(buffer)) {
        (void)snprintf(message, message_size, "Input string length %lu exceeds buffer size", (unsigned long)input_length);
        return 0;
    }

    memset(buffer, 0, sizeof(buffer));
    (void)strncpy(buffer, input, sizeof(buffer) - 1U);

    if (strncmp(buffer, input, sizeof(buffer)) != 0) {
        (void)snprintf(message, message_size, "Buffer copy mismatch detected");
        return 0;
    }

    return 1;
}

static int test_file_access(char *message, size_t message_size) {
#if defined(EINKAUFSLISTE_PFAD)
    const char *path = EINKAUFSLISTE_PFAD;
#elif defined(SHOPPING_LIST_PATH)
    const char *path = SHOPPING_LIST_PATH;
#else
    const char *path = "einkaufsliste.txt";
#endif

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        (void)snprintf(message, message_size, "Failed to open required file: %s", path);
        return 0;
    }

    {
        char buffer[64];
        const size_t bytes_read = fread(buffer, 1U, sizeof(buffer), file);
        if (ferror(file) != 0) {
            fclose(file);
            (void)snprintf(message, message_size, "Error reading from file: %s", path);
            return 0;
        }

        if (bytes_read == 0U && feof(file) == 0) {
            fclose(file);
            (void)snprintf(message, message_size, "Unable to read data from file: %s", path);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

static int test_environment_invariants(char *message, size_t message_size) {
    const char *env_variable = getenv("PATH");
    if (env_variable == NULL) {
        (void)snprintf(message, message_size, "Critical environment variable PATH is not set");
        return 0;
    }

    if (strlen(env_variable) == 0U) {
        (void)snprintf(message, message_size, "Environment variable PATH is empty");
        return 0;
    }

    return 1;
}

static void startup_execute_tests(void) {
    StartupTestCase tests[] = {
        { "Memory allocation", test_memory_allocation },
        { "Arithmetic edge cases", test_arithmetic_edge_cases },
        { "String boundary handling", test_string_boundaries },
        { "File accessibility", test_file_access },
        { "Environment invariants", test_environment_invariants }
    };

    const size_t test_count = sizeof(tests) / sizeof(tests[0]);
    size_t passed = 0U;
    size_t index;

    (void)printf("[Startup] Running system self-check (%lu tests)\n", (unsigned long)test_count);

    for (index = 0U; index < test_count; ++index) {
        char message[128];
        int result;
        message[0] = '\0';

        result = tests[index].function(message, sizeof(message));
        if (result != 0) {
            ++passed;
            (void)printf("[OK]   %s\n", tests[index].name);
        } else {
            const char *detail = (message[0] != '\0') ? message : "No details available";
            (void)printf("[FAIL] %s -- %s\n", tests[index].name, detail);
        }
    }

    if (passed == test_count) {
        (void)printf("System check passed (%lu/%lu tests).\n", (unsigned long)passed, (unsigned long)test_count);
    } else {
        (void)printf("Startup test failed (%lu/%lu tests).\n", (unsigned long)passed, (unsigned long)test_count);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
}

void startup_run_all_tests(void) {
    static int tests_already_executed = 0;
    if (tests_already_executed != 0) {
        return;
    }

    tests_already_executed = 1;
    startup_execute_tests();
}

static void startup_test_entry(void) {
    startup_run_all_tests();
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void startup_constructor(void) {
    startup_test_entry();
}
#endif

