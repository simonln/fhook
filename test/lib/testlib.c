
#define API_IMPLEMENT
#include "testlib.h"
#include <stdio.h>


static int local_add_sum(int a, int b, int c, int d, int e)
{
    return (a + b + c + d + e );
}
API_EXPORT int add_sum(int a, int b)
{
    printf("add_sum: %d, %d\n", a, b);
    int tmp = a + b;
    tmp += 10;
    tmp = local_add_sum(1, 2, 3, 4, tmp);
    return tmp;
}
API_EXPORT double add_double(double a, double b)
{
    printf("add_double: %lf %lf\n", a, b);
    double tmp = a + b;
    tmp += 1.0;
    return tmp;
}