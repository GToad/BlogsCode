#include <stdio.h>
#include <Android/log.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "GSLab"
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args);

/**
 * д�ļ���ָ��ƫ�Ƽ���С
 * @param   pFile       ��Ҫд����ļ�ָ��
 * @param   ulOffsetBegin   Ҫд��λ�ã��ļ�ƫ��
 * @param   size        Ҫд��ĳ��ȣ�byteΪ��λ
 * @param   pszBuffer   д������ݣ���Ҫ�ⲿ��֤���ʴ�С��С��size
 * @return  bool        �Ƿ�д�ɹ�
 */
bool IWrite(FILE *pFile, unsigned long ulOffsetBegin, size_t size, char* pszBuffer)
{
    bool bRet = false;
    
    while(1)
    {
        if(NULL == pFile || NULL == pszBuffer)
        {
            break;
        }
        
        int iRet = fseek(pFile, ulOffsetBegin, SEEK_SET);
        if(-1 == iRet)
        {
            break;
        }
        
        for(int i = 0; i < size; i++)
        {
            fputc(pszBuffer[i], pFile);
        }
        LOGI("write success");
        bRet = true;
        break;
    }
    
    return bRet;
}

/**
 * ���ļ���ָ��ƫ�Ƽ���С
 * @param   pFile       ��Ҫ��ȡ���ļ�ָ��
 * @param   ulOffsetBegin   Ҫ��ȡλ�ã��ļ�ƫ��
 * @param   size        Ҫ��ȡ�ĳ��ȣ�byteΪ��λ
 * @param   pszOutBuffer    ��Ŷ�ȡ�������Ҫ�ⲿ��֤���С��С��size
 * @return  bool        ��ȡ�Ƿ�ɹ�
 */
bool IRead(FILE *pFile, unsigned long ulOffsetBegin, size_t size, char* pszOutBuffer)
{
    bool bRet = false;
    
    while(1)
    {
        if(pFile == NULL && pszOutBuffer == NULL)
        {
            break;
        }
        
        int iRet = fseek(pFile, ulOffsetBegin, SEEK_SET);
        if(-1 == iRet)
        {
            break;
        }
        
        fread(pszOutBuffer, 1, size, pFile);
        bRet = true;
        break;
    }
    
    return bRet;
}

/**
 * ���Ժ����������޸�IBored ndk��Ĺؼ�����uiTimeCounter��ʵ���߼��޸�
 * �������������Է���uiTimeCounter����ڶ��ף��ɶ���д��+0x4��λ��
 * @param   pszPid      demoӦ�õ�pid
 * @param   pszRWModuleBegin    ��ɶ���д�ε��ڴ�ƫ�ƣ��������������޸��߼�
 */
void ModifyIBored(const char* pszPid, const char* pszRWModuleBegin)
{
    char szMemPath[32] = {0};   //���mem�ļ�·��
    unsigned long ulRWOffsetBegin = atol(pszRWModuleBegin);
    
    sprintf(szMemPath, "/proc/%s/mem", pszPid);
    
    FILE *pmemm = fopen(szMemPath, "r+");    
    LOGI("mem path:%s;\tOffset:0x%08X", szMemPath, (unsigned int)ulRWOffsetBegin);

    if(pmemm == NULL)
    {
        LOGI("fopen error:%d", errno);
    }
    
    //��ȡԭ����uiTimeCounter���ڿɶ���д�ε�+4λ��
    char szBackupTime[4] = {0};
    if(IRead(pmemm, ulRWOffsetBegin + 4, 4, szBackupTime) == false)
    {
        LOGI("read error.");
        fclose(pmemm);
        return;
    }
    else
    {
        LOGI("Time:[0x%02X] [0x%02X] [0x%02X] [0x%02X]", szBackupTime[0], szBackupTime[1], szBackupTime[2], szBackupTime[3])
    }
    
    //��дIBored��uiTimeCounter���ĳ�0������ع�ȴ�״̬��
    char szEvilData[12] = {0x0, 0x0, 0x0, 0x0};
    if(IWrite(pmemm, ulRWOffsetBegin + 4, 4, szEvilData) == false)
    {
        LOGI("write error.");
        fclose(pmemm);
        return;
    }
    
    fclose(pmemm);
}


int main(int argc, char const *argv[])
{
    LOGI("In main");
    ModifyIBored(argv[1], argv[2]);
    
    return 0;
}