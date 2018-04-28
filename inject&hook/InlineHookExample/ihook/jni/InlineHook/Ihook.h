#include <stdio.h>
#include <Android/log.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <stdbool.h>

#ifndef BYTE
#define BYTE unsigned char
#endif

#define OPCODEMAXLEN 8      //inline hook����Ҫ��opcodes��󳤶�

#define LOG_TAG "GSLab"
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args);

extern unsigned long _shellcode_start_s;
extern unsigned long _shellcode_end_s;
extern unsigned long _hookstub_function_addr_s;
extern unsigned long _old_function_addr_s;

//hook����Ϣ
typedef struct tagINLINEHOOKINFO{
    void *pHookAddr;                //hook�ĵ�ַ
    void *pStubShellCodeAddr;            //����ȥ��shellcode stub�ĵ�ַ
    void (*onCallBack)(struct pt_regs *);       //�ص���������ת��ȥ�ĺ�����ַ
    void ** ppOldFuncAddr;             //shellcode �д��old function�ĵ�ַ
    BYTE szbyBackupOpcodes[OPCODEMAXLEN];    //ԭ����opcodes
} INLINE_HOOK_INFO;

bool ChangePageProperty(void *pAddress, size_t size);

extern void * GetModuleBaseAddr(pid_t pid, char* pszModuleName);

bool InitArmHookInfo(INLINE_HOOK_INFO* pstInlineHook);

bool BuildStub(INLINE_HOOK_INFO* pstInlineHook);

bool BuildArmJumpCode(void *pCurAddress , void *pJumpAddress);

bool BuildOldFunction(INLINE_HOOK_INFO* pstInlineHook);

bool RebuildHookTarget(INLINE_HOOK_INFO* pstInlineHook);

extern bool HookArm(INLINE_HOOK_INFO* pstInlineHook);

