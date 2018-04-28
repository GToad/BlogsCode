#include <stdio.h>
#include <Android/log.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

//constructor������Խ��������嵽init�ڣ�so���������Զ�����
void ModifyIBored() __attribute__((constructor));

#define LOG_TAG "GSLab"
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args);

/**
 * �޸�ҳ���ԣ��ĳɿɶ���д��ִ��
 * @param   ulAddress   ��Ҫ�޸�������ʼ��ַ
 * @param   size     ��Ҫ�޸�ҳ���Եĳ��ȣ�byteΪ��λ
 * @return  bool    �޸��Ƿ�ɹ�
 */
bool ChangePageProperty(unsigned long ulAddress, size_t size)
{
    bool bRet = false;
    
    //���������ҳ����������ʼ��ַ
    unsigned long ulPageSize = sysconf(_SC_PAGESIZE);
    int iProtect = PROT_READ | PROT_WRITE | PROT_EXEC;
    unsigned long ulNewPageStartAddress = (long)(ulAddress) & ~(ulPageSize - 1);
    long lPageCount = (size / ulPageSize) + 1;
    
    for(long l = 0; l < lPageCount; l++)
    {
        //����mprotect��ҳ����
        int iRet = mprotect((const void *)(ulNewPageStartAddress), ulPageSize, iProtect);
        if(-1 == iRet)
        {
            bRet = false;
            LOGI("mprotect error:%s", strerror(errno));
            return bRet;
        }
    }
    bRet = true;
    return bRet;
}

/**
 * ��ȫ���ڴ����ݣ����Ǹ�ҳ����
 * @param   ulAddress   Ҫ�����ڴ���ʼ��ַ
 * @param   size        Ҫ�����ڴ��С��byteΪ��λ
 * @param   pszOutBuffer    ��Ž������Ҫ�ⲿ��֤���С��С��size
 * @return  bool    �����Ƿ�ɹ�
 */
bool SafeMemRead(unsigned long ulAddress, size_t size, char *pszOutBuffer)
{
    bool bRet = false;
    while(1)
    {
        if(NULL == pszOutBuffer)
        {
            LOGI("pszOutBuffer is null");
            break;
        }
        
        if(false == ChangePageProperty(ulAddress, size))
        {
            LOGI("ChangePageProperty error");
            break;
        }
        
        //ֱ�ӿ����ڴ�
        memcpy((void*)pszOutBuffer, (void*)ulAddress, size);
        LOGI("SafeMemRead success");
        bRet = true;
        break;
    }
    return bRet;
}

/**
 * ��ȫд�ڴ����ݣ����Ǹ�ҳ����
 * @param   ulAddress   �޸ĵ��ڴ���ʼ��ַ
 * @param   size        �޸ĵ��ڴ��С,byteΪ��λ
 * @param   pszBuffer   Ҫд������ݣ���Ҫ�ⲿ��֤��ɴ�С��С��size
 * @return  bool    �޸��Ƿ�ɹ�
 */
bool SafeMemWrite(unsigned long ulAddress, size_t size, char *pszBuffer)
{
    bool bRet = false;
    while(1)
    {
        if(NULL == pszBuffer)
        {
            LOGI("pszBuffer is null");
            break;
        }
        
        if(false == ChangePageProperty(ulAddress, size))
        {
            LOGI("ChangePageProperty error");
            break;
        }
        
        //ֱ�ӿ����ڴ�
        memcpy((void*)ulAddress, (void*)pszBuffer, size);
        LOGI("SafeMemWrite success");
        bRet = true;
        break;
    }
    return bRet;
}

/**
 * ͨ��/proc/$pid/maps����ȡģ���ַ
 * @param   pid     ģ�����ڽ���pid���������������̣�����С��0��ֵ����-1
 * @param   pszModuleName       ģ������
 * @return  void*   ģ���ַ�������򷵻�0
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
    LOGI("module %s baseAddr:%x", pszModuleName, (uint32_t)ulBaseValue);
    return (void *)ulBaseValue;
}


/**
 * ��so�����ؾ������������޸�DEMO APP��ֱ��patch�����д��תʵ�ֹ���
 */
void ModifyIBored()
{
    LOGI("In ModifyIBored.");
    char szBackupOpcodes[8] = {0};
    
    char szTargetModule[10] = "IHook.so";   //��Ӧģ����
    unsigned long ulJudgeOpcodeOffset = 0x00000CBC;     //�õ�ַ��Ϊ�޸ĵ���ת��ģ��ƫ�Ƶ�ַ
    unsigned long ulIBoredSoBaseAddr = (unsigned long)GetModuleBaseAddr(-1, szTargetModule);
    
    if(ulIBoredSoBaseAddr == 0)
    {
        LOGI("get module base address error.")
        return;
    }
    
    //��ȡԭ����opcodes�ڴ汣��
    if(SafeMemRead(ulIBoredSoBaseAddr + ulJudgeOpcodeOffset, 8, szBackupOpcodes))
    {
        for(int i = 0; i < 8; i++)
        {
            LOGI("Opcode[%02d]:0x%2X", i, szBackupOpcodes[i]);
        }
    }
    
     //MOV R3, R3ָ�NOP���ã�����ָ��д��ָ��λ�ã��޸���ת�ж�
    char szNullOpcodes[12] = {0x03, 0x30, 0xA0, 0xE1, 0x03, 0x30, 0xA0, 0xE1, 0x03, 0x30, 0xA0, 0xE1};
    if(SafeMemWrite(ulIBoredSoBaseAddr + ulJudgeOpcodeOffset, 8, szNullOpcodes))
    {
        LOGI("patch success.")
    }    
}
