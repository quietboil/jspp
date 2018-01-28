#ifndef __TEST_H
#define __TEST_H

#define check(cond) if (!(cond)) return __LINE__

void test(int (*test_case)(void), const char * test_name);

extern int num_tests_passed;
extern int num_tests_failed;

#endif
