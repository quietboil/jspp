#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
static void __attribute__ ((__constructor__)) enable_color_output()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */ 0x0004);
}
#endif

int num_tests_passed;
int num_tests_failed;

void test(int (*test_case)(void), const char * test_name)
{
    int failure_line = test_case();
    if (failure_line == 0) {
        num_tests_passed++;
        printf("\x1b[32mPASS\x1b[0m: %s\n", test_name);
    } else {
        num_tests_failed++;
        printf("\x1b[31mFAIL\x1b[0m: %s @ %d\n", test_name, failure_line);
    }
}
