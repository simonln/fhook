#ifndef __FHOOK_H__
#define __FHOOK_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * init hook environment
*/
void fhook_init(void);

/**
 * hook function
 * @param func: source function adderss
 * @param stub_func: destination function address
 * @retval
 *      0: ok
 *      1: failed
 *      2: set memory protect to w+r+x faild
*/
int fhook_replace(void *func, void* stub_func);
int fhook_restore(void* func);
int fhook_restore_all(void);


#ifdef __cplusplus
}
#endif

#endif