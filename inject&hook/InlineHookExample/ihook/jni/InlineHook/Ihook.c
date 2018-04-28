#include "Ihook.h"

/**
 * �޸�ҳ���ԣ��ĳɿɶ���д��ִ��
 * @param   pAddress   ��Ҫ�޸�������ʼ��ַ
 * @param   size       ��Ҫ�޸�ҳ���Եĳ��ȣ�byteΪ��λ
 * @return  bool       �޸��Ƿ�ɹ�
 */
bool ChangePageProperty(void *pAddress, size_t size)
{
    bool bRet = false;
    
    if(pAddress == NULL)
    {
        LOGI("change page property error.");
        return bRet;
    }
    
    //���������ҳ����������ʼ��ַ
    unsigned long ulPageSize = sysconf(_SC_PAGESIZE);
    int iProtect = PROT_READ | PROT_WRITE | PROT_EXEC;
    unsigned long ulNewPageStartAddress = (unsigned long)(pAddress) & ~(ulPageSize - 1);
    long lPageCount = (size / ulPageSize) + 1;
    
    long l = 0;
    while(l < lPageCount)
    {
        //����mprotect��ҳ����
        int iRet = mprotect((const void *)(ulNewPageStartAddress), ulPageSize, iProtect);
        if(-1 == iRet)
        {
            LOGI("mprotect error:%s", strerror(errno));
            return bRet;
        }
        l++; 
    }
    
    return true;
}

/**
 * ͨ��/proc/$pid/maps����ȡģ���ַ
 * @param   pid                 ģ�����ڽ���pid���������������̣�����С��0��ֵ����-1
 * @param   pszModuleName       ģ������
 * @return  void*               ģ���ַ�������򷵻�0
 */
void * GetModuleBaseAddr(pid_t pid, char* pszModuleName)
{
    FILE *pFileMaps = NULL;
    unsigned long ulBaseValue = 0;
    char szMapFilePath[256] = {0};
    char szFileLineBuffer[1024] = {0};

    //pid�жϣ�ȷ��maps�ļ�
    if (pid < 0)
    {
        snprintf(szMapFilePath, sizeof(szMapFilePath), "/proc/self/maps");
    }
    else
    {
        snprintf(szMapFilePath, sizeof(szMapFilePath),  "/proc/%d/maps", pid);
    }

    pFileMaps = fopen(szMapFilePath, "r");
    if (NULL == pFileMaps)
    {
        return (void *)ulBaseValue;
    }

    //ѭ������maps�ļ����ҵ���Ӧģ�飬��ȡ��ַ��Ϣ
    while (fgets(szFileLineBuffer, sizeof(szFileLineBuffer), pFileMaps) != NULL)
    {
        if (strstr(szFileLineBuffer, pszModuleName))
        {
            char *pszModuleAddress = strtok(szFileLineBuffer, "-");
            if (pszModuleAddress)
            {
                ulBaseValue = strtoul(pszModuleAddress, NULL, 16);

                if (ulBaseValue == 0x8000)
                    ulBaseValue = 0;

                break;
            }
        }
    }
    fclose(pFileMaps);
    return (void *)ulBaseValue;
}

/**
 * arm��inline hook������Ϣ���ݣ�����ԭ�ȵ�opcodes��
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               ��ʼ����Ϣ�Ƿ�ɹ�
 */
bool InitArmHookInfo(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    if(pstInlineHook == NULL)
    {
        LOGI("pstInlineHook is null");
        return bRet;
    }
    
    memcpy(pstInlineHook->szbyBackupOpcodes, pstInlineHook->pHookAddr, 8);
    
    return true;
}

/**
 * ����ihookstub.s�е�shellcode����׮����ת��pstInlineHook->onCallBack�����󣬻ص��Ϻ���
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               inlinehook׮�Ƿ���ɹ�
 */
bool BuildStub(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        
        void *p_shellcode_start_s = &_shellcode_start_s;
        void *p_shellcode_end_s = &_shellcode_end_s;
        void *p_hookstub_function_addr_s = &_hookstub_function_addr_s;
        void *p_old_function_addr_s = &_old_function_addr_s;

        size_t sShellCodeLength = p_shellcode_end_s - p_shellcode_start_s;
        //mallocһ���µ�stub����
        void *pNewShellCode = malloc(sShellCodeLength);
        if(pNewShellCode == NULL)
        {
            LOGI("shell code malloc fail.");
            break;
        }
        memcpy(pNewShellCode, p_shellcode_start_s, sShellCodeLength);
        //����stub����ҳ���ԣ��ĳɿɶ���д��ִ��
        if(ChangePageProperty(pNewShellCode, sShellCodeLength) == false)
        {
            LOGI("change shell code page property fail.");
            break;
        }

        //������ת���ⲿstub����ȥ
        void **ppHookStubFunctionAddr = pNewShellCode + (p_hookstub_function_addr_s - p_shellcode_start_s);
        *ppHookStubFunctionAddr = pstInlineHook->onCallBack;
        
        //�����ⲿstub�������������ת�ĺ�����ַָ�룬��������Ϻ������µ�ַ
        pstInlineHook->ppOldFuncAddr  = pNewShellCode + (p_old_function_addr_s - p_shellcode_start_s);
            
        //���shellcode��ַ��hookinfo�У����ڹ���hook��λ�õ���תָ��
        pstInlineHook->pStubShellCodeAddr = pNewShellCode;

        bRet = true;
        break;
    }
    
    return bRet;
}

/**
 * ���첢���ARM��32����תָ���Ҫ�ⲿ��֤�ɶ���д����pCurAddress����8��bytes��С
 * @param  pCurAddress      ��ǰ��ַ��Ҫ������תָ���λ��
 * @param  pJumpAddress     Ŀ�ĵ�ַ��Ҫ�ӵ�ǰλ������ȥ�ĵ�ַ
 * @return                  ��תָ���Ƿ���ɹ�
 */
bool BuildArmJumpCode(void *pCurAddress , void *pJumpAddress)
{
    bool bRet = false;
    while(1)
    {
        if(pCurAddress == NULL || pJumpAddress == NULL)
        {
            LOGI("address null.");
            break;
        }        
        //LDR PC, [PC, #-4]
        //addr
        //LDR PC, [PC, #-4]��Ӧ�Ļ�����Ϊ��0xE51FF004
        //addrΪҪ��ת�ĵ�ַ������תָ�ΧΪ32λ������32λϵͳ��˵��Ϊȫ��ַ��ת��
        //���湹��õ���תָ�ARM��32λ������ָ��ֻ��Ҫ8��bytes��
        BYTE szLdrPCOpcodes[8] = {0x04, 0xF0, 0x1F, 0xE5};
        //��Ŀ�ĵ�ַ��������תָ���λ��
        memcpy(szLdrPCOpcodes + 4, &pJumpAddress, 4);
        
        //������õ���תָ��ˢ��ȥ
        memcpy(pCurAddress, szLdrPCOpcodes, 8);
        cacheflush(*((uint32_t*)pCurAddress), 8, 0);
        
        bRet = true;
        break;
    }
    return bRet;
}

/**
 * ���챻inline hook�ĺ���ͷ����ԭԭ����ͷ+������ת
 * ���ǿ�����ת���ɣ�ͬʱ���stub shellcode�е�oldfunction��ַ��hookinfo�����old������ַ
 * ���ʵ��û��ָ���޸����ܣ�����HOOK��λ��ָ����漰PC����Ҫ�ض���ָ��
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               ԭ���������Ƿ�ɹ�
 */
bool BuildOldFunction(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        
        //8��bytes���ԭ����opcodes������8��bytes�����ת��hook���������תָ��
        void * pNewEntryForOldFunction = malloc(16);
        if(pNewEntryForOldFunction == NULL)
        {
            LOGI("new entry for old function malloc fail.");
            break;
        }
        
        if(ChangePageProperty(pNewEntryForOldFunction, 16) == false)
        {
            LOGI("change new entry page property fail.");
            break;
        }
        
        //������û���޸�ָ�ֱ�ӿ������ɣ��������߿���������λ������ָ���޸�����
        memcpy(pNewEntryForOldFunction, pstInlineHook->szbyBackupOpcodes, 8);
        //�����תָ��
        if(BuildArmJumpCode(pNewEntryForOldFunction + 8, pstInlineHook->pHookAddr + 8) == false)
        {
            LOGI("build jump opcodes for new entry fail.");
            break;
        }
        //���shellcode��stub�Ļص���ַ
        *(pstInlineHook->ppOldFuncAddr) = pNewEntryForOldFunction;
        
        bRet = true;
        break;
    }
    
    return bRet;
}
    
/**
 * ��ҪHOOK��λ�ã�������ת����ת��shellcode stub��
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               ԭ����תָ���Ƿ���ɹ�
 */
bool RebuildHookTarget(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null");
            break;
        }
        //�޸�ԭλ�õ�ҳ���ԣ���֤��д
        if(ChangePageProperty(pstInlineHook->pHookAddr, 8) == false)
        {
            LOGI("change page property error.");
            break;
        }
        //�����תָ��
        if(BuildArmJumpCode(pstInlineHook->pHookAddr, pstInlineHook->pStubShellCodeAddr) == false)
        {
            LOGI("build jump opcodes for new entry fail.");
            break;
        }
        bRet = true;
        break;
    }
    
    return bRet;
}

/**
 * ARM�µ�inlinehook
 * @param  pstInlineHook inlinehook��Ϣ
 * @return               inlinehook�Ƿ����óɹ�
 */
bool HookArm(INLINE_HOOK_INFO* pstInlineHook)
{
    bool bRet = false;
    
    while(1)
    {
        if(pstInlineHook == NULL)
        {
            LOGI("pstInlineHook is null.");
            break;
        }

        //����ARM��inline hook�Ļ�����Ϣ
        if(InitArmHookInfo(pstInlineHook) == false)
        {
            LOGI("Init Arm HookInfo fail.");
            break;
        }
        
        //����stub�������Ǳ���Ĵ���״̬��ͬʱ��ת��Ŀ�꺯����Ȼ����ת��ԭ����
        //��ҪĿ���ַ������stub��ַ��ͬʱ����oldָ���������� 
        if(BuildStub(pstInlineHook) == false)
        {
            LOGI("BuildStub fail.");
            break;
        }
        
        //�����ع�ԭ����ͷ���������޸�ָ�������ת�ص�ԭ��ַ��
        //��Ҫԭ������ַ
        if(BuildOldFunction(pstInlineHook) == false)
        {
            LOGI("BuildOldFunction fail.");
            break;
        }
        
        //������дԭ����ͷ��������ʵ��inline hook�����һ������д��ת
        //��Ҫcacheflush����ֹ����
        if(RebuildHookTarget(pstInlineHook) == false)
        {
            LOGI("RebuildHookAddress fail.");
            break;
        }
        bRet = true;
        break;
    }

    return bRet;
}
