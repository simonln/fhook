#include <stdio.h>
#include <string.h>
#include <iostream>

#include "fhook.h"
#include "testlib.h"

using namespace std;

int local_func(int a, int b)
{
    printf("local_func\n");
    return a + b;
}

int mock_local_func(int a, int b)
{
    printf("mock_local_func\n");
    return (a + b + 1);
}

int mock_add_sum(int a, int b)
{
    printf("mock_add_sum\n");
    return (a + b + 1);
}

const char * mock_strstr ( const char * str1, const char * str2 )
{
    printf("mock_strstr\n");
    return str1;
}

int mock_printf_stub(const char* format, ...)
{
    cout << "I am printf_stub" << endl;
    return 0;
}

int main()
{
    fhook_init();
    // local function
    fhook_replace(local_func, mock_local_func);
    local_func(1, 2);

    // shared library function
    fhook_replace(add_sum, mock_add_sum);
    add_sum(1, 2);

    // stdlib
    fhook_replace(printf, mock_printf_stub);
    printf("test is test is\n");

    fhook_restore(printf);

    double x = add_double(1.0, 2.0);
    printf("add_double(1.0, 2.0), result: 4.0, real %lf\n", x);

    return 0;
}