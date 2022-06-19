#pragma once

#ifdef _WIN32
#ifdef API_IMPLEMENT
#define API_EXPORT  __declspec(dllexport)
#else
#define API_EXPORT  __declspec(dllimport)
#endif
#else
#define API_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

API_EXPORT int add_sum(int a, int b);
API_EXPORT double add_double(double a, double b);


#ifdef __cplusplus
}
#endif