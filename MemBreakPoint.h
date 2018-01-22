#pragma once

#include "BreakPoint.h"

//�ڴ汣������
typedef struct _MEMORY_PROTECT_INFO {
    LPVOID m_pAddr;
    SIZE_T m_dwLen;
    MEMORY_BASIC_INFORMATION *m_pMemInfo;
}MEMORY_PROTECT_INFO, *PMEMORY_PROTECT_INFO;

//�ڴ���������
typedef CMyList<MEMORY_BASIC_INFORMATION> CMemInfoList;

//�ڴ汣����������
typedef CMyList<MEMORY_PROTECT_INFO> CMemProtectList;


class CMemBreakPoint
    : public IBreakPoint
{
public:
    CMemBreakPoint(LPVOID pAddr, SIZE_T dwLen,
        CMemInfoList *plstMemInfo, int nType, int nStatus);

   virtual ~CMemBreakPoint();

public:
    virtual int GetType();
    virtual int GetStatus();
    virtual int GetOperate();
    virtual LPVOID GetAddress();

    virtual bool IsSet();
    virtual bool Set(DWORD dwPid);
    virtual bool Reset(DWORD dwPid);

    bool InRegion(PEXCEPTION_RECORD pExepRec);
    bool IsMatch(PEXCEPTION_RECORD pExcpRec);
    SIZE_T GetLength();
    DWORD GetProtect();
    CMemProtectList& GetProtectList();

protected:
    INT             m_nFlag;
    BOOL            m_bSet;          //�Ƿ����öϵ�
    LPVOID          m_pAddr;         //�ϵ��ַ
    SIZE_T          m_dwLen;         //�ϵ��С
    DWORD           m_dwProtect;     //��������
    CMemInfoList   *m_pLstMemInfo;   //�ڴ���Ϣ����
    CMemProtectList m_lstMemProtect; //�ڴ汣������
};