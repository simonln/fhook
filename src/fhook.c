#include "fhook.h"

#ifdef _WIN32 
//windows
#include <windows.h>
#include <processthreadsapi.h>
#else
//linux
#include <unistd.h>
#include <sys/mman.h>
#endif

#include <string.h>
#include <stddef.h>

/**********************************************************
                  replace function
**********************************************************/
#ifdef _WIN32 
#define CACHEFLUSH(addr, size) FlushInstructionCache(GetCurrentProcess(), addr, size)
#else
#define CACHEFLUSH(addr, size) __builtin___clear_cache(addr, addr + size)
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define CODESIZE        (16U)
#define CODESIZE_MIN    (16U)
#define CODESIZE_MAX    CODESIZE
// ldr x9, +8 
// br x9 
// addr 
#define REPLACE_FAR(fn, fn_stub)                    \
    ((uint32_t*)fn)[0] = 0x58000040 | 9;            \
    ((uint32_t*)fn)[1] = 0xd61f0120 | (9 << 5);     \
    *(long long *)(fn + 8) = (long long )fn_stub;   \
    CACHEFLUSH((char *)fn, CODESIZE);
#define REPLACE_NEAR(fn, fn_stub)        REPLACE_FAR(fn, fn_stub)

#elif defined(__arm__) || defined(_M_ARM)

#define CODESIZE        (8U)
#define CODESIZE_MIN    (8U)
#define CODESIZE_MAX    CODESIZE
// ldr pc, [pc, #-4]
#define REPLACE_FAR(fn, fn_stub)            \
    ((uint32_t*)fn)[0] = 0xe51ff004;        \
    ((uint32_t*)fn)[1] = (uint32_t)fn_stub; \
    CACHEFLUSH((char *)fn, CODESIZE);
#define REPLACE_NEAR(fn, fn_stub)           REPLACE_FAR(fn, fn_stub)

#elif defined(__mips64)

#define CODESIZE        (80U)
#define CODESIZE_MIN    (80U)
#define CODESIZE_MAX    CODESIZE
//MIPS has no PC pointer, so you need to manually enter and exit the stack
//120000ce0:  67bdffe0    daddiu  sp, sp, -32  //enter the stack
//120000ce4:  ffbf0018    sd  ra, 24(sp)
//120000ce8:  ffbe0010    sd  s8, 16(sp)
//120000cec:  ffbc0008    sd  gp, 8(sp)
//120000cf0:  03a0f025    move    s8, sp

//120000d2c:  03c0e825    move    sp, s8  //exit the stack
//120000d30:  dfbf0018    ld  ra, 24(sp)
//120000d34:  dfbe0010    ld  s8, 16(sp)
//120000d38:  dfbc0008    ld  gp, 8(sp)
//120000d3c:  67bd0020    daddiu  sp, sp, 32
//120000d40:  03e00008    jr  ra

#define REPLACE_FAR(fn, fn_stub)    \
    ((uint32_t *)fn)[0] = 0x67bdffe0;\
    ((uint32_t *)fn)[1] = 0xffbf0018;\
    ((uint32_t *)fn)[2] = 0xffbe0010;\
    ((uint32_t *)fn)[3] = 0xffbc0008;\
    ((uint32_t *)fn)[4] = 0x03a0f025;\
    *(uint16_t *)(fn + 20) = (long long)fn_stub >> 32;\
    *(fn + 22) = 0x19;\
    *(fn + 23) = 0x24;\
    ((uint32_t *)fn)[6] = 0x0019cc38;\
    *(uint16_t *)(fn + 28) = (long long)fn_stub >> 16;\
    *(fn + 30) = 0x39;\
    *(fn + 31) = 0x37;\
    ((uint32_t *)fn)[8] = 0x0019cc38;\
    *(uint16_t *)(fn + 36) = (long long)fn_stub;\
    *(fn + 38) = 0x39;\
    *(fn + 39) = 0x37;\
    ((uint32_t *)fn)[10] = 0x0320f809;\
    ((uint32_t *)fn)[11] = 0x00000000;\
    ((uint32_t *)fn)[12] = 0x00000000;\
    ((uint32_t *)fn)[13] = 0x03c0e825;\
    ((uint32_t *)fn)[14] = 0xdfbf0018;\
    ((uint32_t *)fn)[15] = 0xdfbe0010;\
    ((uint32_t *)fn)[16] = 0xdfbc0008;\
    ((uint32_t *)fn)[17] = 0x67bd0020;\
    ((uint32_t *)fn)[18] = 0x03e00008;\
    ((uint32_t *)fn)[19] = 0x00000000;\
    CACHEFLUSH((char *)fn, CODESIZE);
#define REPLACE_NEAR(fn, fn_stub)       REPLACE_FAR(fn, fn_stub)

#elif defined(__thumb__) || defined(_M_THUMB)
    #error "Thumb is not supported"
#else //__i386__ _x86_64__  _M_IX86 _M_X64

#define CODESIZE        (13U)
#define CODESIZE_MIN    (5U)
#define CODESIZE_MAX    CODESIZE
//13 byte(jmp m16:64)
//movabs $0x102030405060708,%r11
//jmpq   *%r11
#define REPLACE_FAR(fn, fn_stub)             \
    *fn = 0x49;                                 \
    *(fn + 1) = 0xbb;                           \
    *(long long *)(fn + 2) = (long long)fn_stub;\
    *(fn + 10) = 0x41;                          \
    *(fn + 11) = 0xff;                          \
    *(fn + 12) = 0xe3;                          \
    //CACHEFLUSH((char *)fn, CODESIZE);

//5 byte(jmp rel32)
#define REPLACE_NEAR(fn, fn_stub)                           \
    *fn = 0xE9;                                             \
    *(int *)(fn + 1) = (int)(fn_stub - fn - CODESIZE_MIN);  \
    //CACHEFLUSH((char *)fn, CODESIZE);

#endif

// the max counter of function
#define MAX_STUB_FUNC       (1024)

typedef struct func_stub
{
    void *fn;
    unsigned char code_buf[CODESIZE];
    int far_jmp;
}func_stub_t;


#ifdef _WIN32
//LLP64
static long long s_pagesize;
#else
//LP64
static long s_pagesize;
#endif

static int s_index = 0;
static func_stub_t s_func_stubs[MAX_STUB_FUNC];

void fhook_init(void)
{
#ifdef _WIN32
    SYSTEM_INFO sys_info;  
    GetSystemInfo(&sys_info);
    s_pagesize = sys_info.dwPageSize;
#else
    s_pagesize = sysconf(_SC_PAGE_SIZE);
#endif       

    if (s_pagesize < 0)
    {
        // default vaule
        s_pagesize = 4096;
    }
    memset(s_func_stubs, 0, MAX_STUB_FUNC * sizeof(func_stub_t));
    s_index = 0;
}

static char *get_pageof(char* addr)
{ 
#ifdef _WIN32
    return (char *)((unsigned long long)addr & ~(s_pagesize - 1));
#else
    return (char *)((unsigned long)addr & ~(s_pagesize - 1));
#endif   
}

static int is_distanceof(char* addr, char* addr_stub)
{
    ptrdiff_t diff = addr_stub >= addr ? addr_stub - addr : addr - addr_stub;
    if((sizeof(addr) > 4) && (((diff >> 31) - 1) > 0))
    {
        return 1;
    }
    return 0;
}
int fhook_replace(void *func, void *mock)
{
    if (!func || !mock || func == mock)
    {
        return 1;
    }
    
    // reset hook
    fhook_restore(func);

    func_stub_t *pstub = &s_func_stubs[s_index];
    pstub->fn = func;

    if(is_distanceof(func, mock))
    {
        pstub->far_jmp = 1;
        memcpy(pstub->code_buf, func, CODESIZE_MAX);
    }
    else
    {
        pstub->far_jmp = 0;
        memcpy(pstub->code_buf, func, CODESIZE_MIN);
    }

#ifdef _WIN32
    DWORD lpflOldProtect;
    if(0 == VirtualProtect(get_pageof(pstub->fn), s_pagesize * 2, PAGE_EXECUTE_READWRITE, &lpflOldProtect))
#else
    if (-1 == mprotect(get_pageof(pstub->fn), s_pagesize * 2, PROT_READ | PROT_WRITE | PROT_EXEC))
#endif       
    {
        return 2;
    }

    if(pstub->far_jmp)
    {
        REPLACE_FAR((char*)func, (char*)mock);
    }
    else
    {
        REPLACE_NEAR((char*)func, (char*)mock);
    }


#ifdef _WIN32
    if(0 == VirtualProtect(get_pageof(pstub->fn), s_pagesize * 2, PAGE_EXECUTE_READ, &lpflOldProtect))
#else
    if (-1 == mprotect(get_pageof(pstub->fn), s_pagesize * 2, PROT_READ | PROT_EXEC))
#endif     
    {
        return 2;
    }
    // stub ok
    s_index++;
    return 0;
}

static int find_func_stub(void *func)
{
    int ret = -1;
    for(int i = 0; i < s_index; i++)
    {
        if (s_func_stubs[i].fn == func)
        {
            ret = i;
            break;
        }
    }
    return ret;
}
static void delete_stub_node(int index)
{
    if (index < 0)
    {
        return;
    }
    if (s_index == index)
    {
        s_index--;
        if (s_index < 0)
        {
            s_index = 0;
            return;
        }
    }
    else
    {
        memcpy(s_func_stubs + index, s_func_stubs + s_index, sizeof(func_stub_t));
        s_index--;
    }
    return;
}

static int restore_hook(int index)
{
    if (index < 0 || index >= s_index)
    {
        return 1;
    }
    func_stub_t *pstub = &s_func_stubs[index];
    
#ifdef _WIN32
    DWORD lpflOldProtect;
    if(0 == VirtualProtect(get_pageof(pstub->fn), s_pagesize * 2, PAGE_EXECUTE_READWRITE, &lpflOldProtect))
#else
    if (-1 == mprotect(get_pageof(pstub->fn), s_pagesize * 2, PROT_READ | PROT_WRITE | PROT_EXEC))
#endif       
    {
        return 2;
    }

    if(pstub->far_jmp)
    {
        memcpy(pstub->fn, pstub->code_buf, CODESIZE_MAX);
    }
    else
    {
        memcpy(pstub->fn, pstub->code_buf, CODESIZE_MIN);
    }

#if defined(__aarch64__) || defined(_M_ARM64)
            CACHEFLUSH(pstub->fn, CODESIZE);
#elif defined(__arm__) || defined(_M_ARM)
            CACHEFLUSH(pstub->fn, CODESIZE);
#elif defined(__mips64)
            CACHEFLUSH(pstub->fn, CODESIZE);
#else //__i386__ _x86_64__  _M_IX86 _M_X64
            //CACHEFLUSH(pstub->fn, CODESIZE);
#endif

#ifdef _WIN32
    if(0 == VirtualProtect(get_pageof(pstub->fn), s_pagesize * 2, PAGE_EXECUTE_READ, &lpflOldProtect))
#else
    if (-1 == mprotect(get_pageof(pstub->fn), s_pagesize * 2, PROT_READ | PROT_EXEC))
#endif     
    {
        return 2;
    }
    return 0;
}
int fhook_restore(void *func)
{
    if (!func)
    {
        return 1;
    }
    int index = find_func_stub(func);
    if (index == -1)
    {
        return 1;
    }
    int ret = restore_hook(index);
    // delete current node
    delete_stub_node(index);
    return ret;
}

int fhook_restore_all(void)
{
    int count = s_index;
    int ret = 0;
    for(int i = count - 1; i >= 0; i--)
    {
        ret |= restore_hook(i);
        delete_stub_node(i);
    }
    return ret;
}