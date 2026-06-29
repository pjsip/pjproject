#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pjsip-apps/src/pjsua/pjsua_app_legacy.c"

START_TEST(test_simple_input_security)
{
    // Invariant: simple_input() must never write beyond buffer bounds during newline removal
    const char *payloads[] = {
        "A" "\0" "B",  // Embedded null byte - fgets stops early, loop may continue
        "",             // Empty input - boundary case
        "normal_input\n" // Valid input with newline
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        const char *test_input = payloads[i];
        size_t input_len = strlen(test_input);
        
        // Create test buffer with guard bytes
        const size_t buf_size = 16;
        char *buffer = malloc(buf_size + 8);
        char *guard_start = buffer;
        char *guard_end = buffer + buf_size + 4;
        
        // Initialize guard zones
        memset(buffer, 0xAA, buf_size + 8);
        buffer += 4;  // Start buffer after guard zone
        
        // Simulate fgets behavior
        FILE *mock_stdin = fmemopen((void*)test_input, input_len, "r");
        if (!mock_stdin) continue;
        
        // Call the actual function
        pj_bool_t result = simple_input(buffer, buf_size, mock_stdin);
        
        // Verify guard zones unchanged
        ck_assert_msg(memchr(guard_start, 0xAA, 4) == guard_start + 3,
                     "Buffer underflow detected");
        ck_assert_msg(memchr(guard_end, 0xAA, 4) == guard_end + 3,
                     "Buffer overflow detected");
        
        fclose(mock_stdin);
        free(guard_start);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_simple_input_security);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}