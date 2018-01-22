#pragma once

#include "../Common.h"
#include "Define.h"
#include "SoftBreakPoint.h"
#include "HardBreakPoint.h"
#include "MemBreakPoint.h"

class CDebugger
{
public:
    CDebugger();
    virtual ~CDebugger();

public:
    //���Գ����Ƿ�����
    bool IsRunning();
    //��ý���id
    DWORD GetProcessID();

    //�����ļ���
    void SetFileName(CMyStringW strFileName);
    //��ȡ�ļ���
    CMyStringW GetFileName() const;

    //���ò���
    void SetParam(LPVOID pParam);
    //��ȡ����
    LPVOID GetParam() const;

    //��ʼ��
    bool Start(PFNDebugNotify pfnNotify);
    //��ͣ
    bool Suspend();
    //��ֹ
    void ShutDown();

    //���ڴ�
    bool ReadMemeory(LPVOID pAddr, LPVOID pBuff, SIZE_T dwLen);
    //д�ڴ�
    bool WriteMemeory(LPVOID pAddr, LPVOID pBuff, SIZE_T dwLen);

    //��ȡ������ڵ�
    LPVOID GetEntryPoint();

    //��û��ָ��
    bool GetAsmCode(LPVOID pAddr, char *pszAsmCode, char *pszOpcode, DWORD *pdwCodeLen);

    //��ȡһ��ϵ��б�
    CBreakPointList& GetSoftBreakPointList();
    //��ȡӲ���ϵ��б�
    CBreakPointList& GetHardBreakPointList();
    //��ȡ�ڴ�ϵ��б�
    CBreakPointList& GetMemBreakPointList();

    //����һ��ϵ�
    bool SetSoftBreakPoint(
        LPVOID pAddr,
        int nType = BREAKPOINT_ALWAYS);

    //����Ӳ���ϵ�
    bool SetHardBreakPoint(
        LPVOID pAddr, //��ַ
        SIZE_T dwLen, //��С
        int nOperate = BREAKPOINT_READ, //����
        int nStatus = BREAKPOINT_ALWAYS);

    //�����ڴ�ϵ�
    bool SetMemBreakPoint(
        LPVOID pAddr, //��ַ
        SIZE_T dwLen, //��С
        int nOperate = BREAKPOINT_ACCESS, //����
        int nStatus = BREAKPOINT_ALWAYS);

    //�Ƴ�һ��ϵ�
    bool RemoveSoftBreakPoint(int nIndex);
    bool RemoveSoftBreakPoint(IBreakPoint *pBp);
    //�Ƴ�һ��ϵ�
    bool RemoveHardBreakPoint(int nIndex);
    bool RemoveHardBreakPoint(IBreakPoint *pBp);
    //�Ƴ�һ��ϵ�
    bool RemoveMemBreakPoint(int nIndex);
    bool RemoveMemBreakPoint(IBreakPoint *pBp);
    //�Ƴ��ϵ�
    bool RemoveBreakPoint(CBreakPointList& lstBp, int nIndex);
    bool RemoveBreakPoint(CBreakPointList& lstBp, IBreakPoint *pBp);

    //�õ�������
    bool SetStepInto(LPDEBUG_EVENT pDbgEvt, bool bNotify = true);
    bool SetStepInto(CONTEXT *pContext, bool bNotify = true);

    //�õ�������
    bool SetStepOver(LPDEBUG_EVENT pDbgEvt); 
    bool SetStepOver(CONTEXT *pContext);
    bool IsStepOver(DWORD dwPid, LPVOID pAddr, PDWORD pdwCodeLen); //�Ƿ���Ҫ��������

    //����������ָ��
    void AdjustBinary(PBYTE pbtCode, LPVOID pAddr, SIZE_T nCount);

    //ˢ���ڴ���Ϣ
    void RefreshMemInfo();

protected:
    //��������¼��߳�
    static unsigned __stdcall DebugThreadProc(LPVOID lpParam);

    // ��������¼�
    BEGIN_DEBUG_EVENT_MAP()
        DECLARE_DEBUG_EVENT(CREATE_PROCESS_DEBUG_EVENT, OnCreateProcessDebugEvent)
        DECLARE_DEBUG_EVENT(EXIT_PROCESS_DEBUG_EVENT, OnExitProcessDebugEvent)
        DECLARE_DEBUG_EVENT(EXCEPTION_DEBUG_EVENT, OnExceptionDebugEvent)
    END_DEBUG_EVENT_MAP()

    // �����쳣�����¼�
    BEGIN_EXCEPTION_MAP()
        DECLARE_EXCEPTION(EXCEPTION_BREAKPOINT, OnExceptionBreakPoint)
        DECLARE_EXCEPTION(EXCEPTION_SINGLE_STEP, OnExceptionSingleStep)
        DECLARE_EXCEPTION(EXCEPTION_ACCESS_VIOLATION, OnExceptionAccessViolation)
    END_EXCEPTION_MAP()

    DWORD OnCreateProcessDebugEvent(CDebugEvent *pDbgEvt);
    DWORD OnExitProcessDebugEvent(CDebugEvent *pDbgEvt);
    DWORD OnExceptionBreakPoint(CDebugEvent *pDbgEvt);
    DWORD OnExceptionSingleStep(CDebugEvent *pDbgEvt);
    DWORD OnExceptionAccessViolation(CDebugEvent *pDbgEvt);

protected:
    LPVOID              m_pParam;       //����λ�������Զ������
    CMyStringW          m_strFileName;  //�����ļ���
    PFNDebugNotify      m_pfnNotify;    //���Իص�����

    HANDLE              m_hDbgThread;   //�����߳�
    HANDLE              m_hExitEvent;   //�˳��¼�
    DWORD               m_dwProcessId;  //����id

    CBreakPointList     m_lstBp;        //һ��ϵ��б�
    CBreakPointList     m_lstHbp;       //Ӳ���ϵ��б�
    CBreakPointList     m_lstMbp;       //�ڴ�ϵ��б�
    CBreakPointList     m_lstRecover;   //��Ҫ�ָ��Ķϵ��б�

    CMemInfoList        m_lstMemInfo;   //�ڴ���Ϣ

    BOOL                m_bSystemBp;    //�Ƿ�ϵͳ�ϵ�
    BOOL                m_bSingleStep;  //�����ͷ���Ҫ��ʾ
    LPVOID              m_pLastAddr;
};

//���ò���
inline void CDebugger::SetParam(LPVOID pParam)
{
    m_pParam = pParam;
}

//��ȡ����
inline LPVOID CDebugger::GetParam() const
{
    return m_pParam;
}

//�����ļ���
inline void CDebugger::SetFileName(CMyStringW strFileName)
{
    ShutDown();
    m_strFileName = strFileName;
}

//��ȡ�ļ���
inline CMyStringW CDebugger::GetFileName() const
{
    return m_strFileName;
}

//��ý���id
inline DWORD CDebugger::GetProcessID()
{
    return m_dwProcessId;
}

//��ȡһ��ϵ��б�
inline CBreakPointList& CDebugger::GetSoftBreakPointList()
{
    return m_lstBp;
}

//��ȡӲ���ϵ��б�
inline CBreakPointList& CDebugger::GetHardBreakPointList()
{
    return m_lstHbp;
}

//��ȡ�ڴ�ϵ��б�
inline CBreakPointList& CDebugger::GetMemBreakPointList()
{
    return m_lstMbp;
}

//�Ƴ�һ��ϵ�
inline bool CDebugger::RemoveSoftBreakPoint(int nIndex)
{
    return RemoveBreakPoint(m_lstBp, nIndex);
}

//�Ƴ�һ��ϵ�
inline bool CDebugger::RemoveSoftBreakPoint(IBreakPoint *pBp)
{
    return RemoveBreakPoint(m_lstBp, pBp);
}

//�Ƴ�Ӳ���ϵ�
inline bool CDebugger::RemoveHardBreakPoint(int nIndex)
{
    return RemoveBreakPoint(m_lstHbp, nIndex);
}

//�Ƴ�Ӳ���ϵ�
inline bool CDebugger::RemoveHardBreakPoint(IBreakPoint *pBp)
{
    return RemoveBreakPoint(m_lstHbp, pBp);
}

//�Ƴ��ڴ�ϵ�
inline bool CDebugger::RemoveMemBreakPoint(int nIndex)
{
    return RemoveBreakPoint(m_lstMbp, nIndex);
}

//�Ƴ��ڴ�ϵ�
inline bool CDebugger::RemoveMemBreakPoint(IBreakPoint *pBp)
{
    return RemoveBreakPoint(m_lstMbp, pBp);
}