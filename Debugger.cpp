#include "Debugger.h"
#include <stdio.h>
#include <process.h>
#include <assert.h>
#include "../include/Disasm/Decode2Asm.h"


#ifndef _WIN64
#define ip Eip
#else
#define ip Rip
#endif

CDebugger::CDebugger()
    : m_hDbgThread(INVALID_HANDLE_VALUE)
    , m_hExitEvent(NULL)
    , m_bSingleStep(FALSE)
    , m_dwProcessId(0)
{

}


CDebugger::~CDebugger()
{
    ShutDown();
}

//���Գ����Ƿ�����
bool CDebugger::IsRunning()
{
    if (m_dwProcessId == 0)
    {
        return false;
    }

    HANDLE aryHandle[] = { 
        m_hExitEvent,
        m_hDbgThread
    };

    DWORD dwRet = WaitForMultipleObjects(
        sizeof(aryHandle) / sizeof(aryHandle[0]),
        aryHandle,
        false,
        0);

    return dwRet == WAIT_TIMEOUT;
}

//��ʼ��
bool CDebugger::Start(PFNDebugNotify pfnNotify)
{
    assert(!IsRunning());
    assert(pfnNotify != NULL);

    m_pfnNotify = pfnNotify;

    //�˳��¼�
    m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (m_hExitEvent == NULL)
    {
        goto FAIL_LABLE;
    }

    //���������߳�
    m_hDbgThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        DebugThreadProc,
        this,
        0,
        NULL);

    if (m_hDbgThread == INVALID_HANDLE_VALUE)
    {
        goto FAIL_LABLE;
    }

    return true;

FAIL_LABLE:

    ShutDown();

    return false;
}


//��ͣ
bool CDebugger::Suspend()
{
    HPROCESS hProcess(m_dwProcessId);

    if (hProcess == NULL)
    {
        return false;
    }

    return DebugBreakProcess(hProcess) == TRUE;
}

//��ֹ
void CDebugger::ShutDown()
{
    //����������У���֪ͨ
    if (m_hExitEvent != NULL)
    {
        SetEvent(m_hExitEvent);
    }

    //�ر��߳�
    if (m_dwProcessId != 0)
    {
        DebugActiveProcessStop(m_dwProcessId);
    }

    //�ȴ����Խ����˳�
    if (m_hDbgThread != INVALID_HANDLE_VALUE)
    {
        while (WaitForSingleObject(m_hDbgThread, 0) == WAIT_TIMEOUT)
        {
            Sleep(100);
        }

        CloseHandle(m_hDbgThread);
        m_hDbgThread = INVALID_HANDLE_VALUE;
    }

    //�ر��¼����
    if (m_hExitEvent != NULL)
    {
        CloseHandle(m_hExitEvent);
        m_hExitEvent = NULL;
    }

    m_bSingleStep = false;
}

//���ڴ�
bool CDebugger::ReadMemeory(
    LPVOID pAddr,
    LPVOID pBuff,
    SIZE_T dwLen)
{
    //�򿪽���
    HPROCESS hProcess(m_dwProcessId);

    if (hProcess == NULL)
    {
        return false;
    }

    SIZE_T dwBytes = 0;
    BOOL bRet = ReadProcessMemory(
        hProcess,
        pAddr,
        pBuff,
        dwLen,
        &dwBytes);

    if (dwBytes != dwLen)
    {
        return false;
    }

    return true;
}

//д�ڴ�
bool CDebugger::WriteMemeory(
    LPVOID pAddr,
    LPVOID pBuff,
    SIZE_T dwLen)
{
    //�򿪽���
    HPROCESS hProcess(m_dwProcessId);

    if (hProcess == NULL)
    {
        return false;
    }

    SIZE_T dwBytes = 0;
    BOOL bRet = WriteProcessMemory(
        hProcess,
        pAddr,
        pBuff,
        dwLen,
        &dwBytes);

    if (dwBytes != dwLen)
    {
        return false;
    }

    return true;
}

//�õ���һ��ģ��֮�䷵��
ENUM_RET_TYPE GetModuleHandle(MODULEENTRY32 *pModuleEntry, LPVOID pParam)
{
    *(HMODULE*)pParam = pModuleEntry->hModule;

    return ENUM_RET_TRUE;
}

//��ȡ������ڵ�
LPVOID CDebugger::GetEntryPoint()
{
    HMODULE hModule = NULL;

    //��ȡ��ģ��
    BOOL bRet = CSnapshoot::EnumModule(
        GetModuleHandle,
        m_dwProcessId,
        &hModule);

    if (!bRet)
    {
        return NULL;
    }

    //�򿪽���
    HPROCESS hProcess(m_dwProcessId);

    if (hProcess == NULL)
    {
        return false;
    }

    SIZE_T dwBytes = 0;

    IMAGE_DOS_HEADER dosHeader = { 0 };

    //��Dosͷ
    bRet = ReadProcessMemory(
        hProcess,
        hModule,
        &dosHeader,
        sizeof(dosHeader),
        &dwBytes);

    if (dwBytes != sizeof(dosHeader) ||
        dosHeader.e_magic != 0x5A4D)
    {
        return false;
    }

    IMAGE_NT_HEADERS ntHeaders = { 0 };

    //��Ntͷ
    bRet = ReadProcessMemory(
        hProcess,
        (PBYTE)hModule + dosHeader.e_lfanew,
        &ntHeaders,
        sizeof(ntHeaders),
        &dwBytes);

    if (dwBytes != sizeof(ntHeaders) ||
        ntHeaders.Signature != 0x00004550)
    {
        return NULL;
    }

    return (PBYTE)hModule + ntHeaders.OptionalHeader.AddressOfEntryPoint;
}

//��û��ָ��
bool CDebugger::GetAsmCode(
    LPVOID pAddr,
    char *pszAsmCode,
    char *pszOpcode,
    DWORD *pdwCodeLen)
{
    //�򿪽���
    HPROCESS hProcess(m_dwProcessId);

    if (hProcess == NULL)
    {
        return false;
    }

    //������������
    SIZE_T dwBytes = 0;
    BYTE szCode[MAX_BIN_CODE] = { 0 };

    BOOL bRet = ReadProcessMemory(
        hProcess,
        pAddr,
        szCode,
        sizeof(szCode),
        &dwBytes);

    //�����ʧ���ˣ����Ը�һ���ڴ�����
    if (dwBytes == 0)
    {
        DWORD dwProtect = 0;

        //�ĳɿɶ���
        bRet = VirtualProtectEx(
            hProcess,
            pAddr,
            sizeof(szCode),
            PAGE_READWRITE,
            &dwProtect);

        if (!bRet)
        {
            return false;
        }

        //������
        ReadProcessMemory(
            hProcess,
            pAddr,
            szCode,
            sizeof(szCode),
            &dwBytes);

        //��ԭԭ���ı�������
        bRet = VirtualProtectEx(
            hProcess,
            pAddr,
            sizeof(szCode),
            dwProtect,
            &dwProtect);

        if (!bRet || dwBytes == 0)
        {
            return false;
        }
    }

    //����������ָ��
    AdjustBinary(szCode, pAddr, dwBytes);

    //��ʾ�����ָ��
    Decode2AsmOpcode(
        szCode,
        pszAsmCode,
        pszOpcode,
        (UINT*)pdwCodeLen,
        (UINT)pAddr);

    return true;
}

//��һ��ϵ�
bool CDebugger::SetSoftBreakPoint(LPVOID pAddr, int nType)
{
    CBreakPointPtr pBp = new CSoftBreakPoint(pAddr, nType);

    if (pBp == NULL)
    {
        return false;
    }

    if (!pBp->Set(m_dwProcessId))
    {
        return false;
    }

    m_lstBp.AddTail(pBp);

    return true;
}

//����Ӳ���ϵ�
bool CDebugger::SetHardBreakPoint(
    LPVOID pAddr,
    SIZE_T dwSize,
    int nOperate,
    int nStatus)
{
    DWORD dwIndex = CHardBreakPoint::FindFreeReg(&m_lstHbp);

    if (dwIndex >= 4)
    {
        return false;
    }

    CBreakPointPtr pHbp = new CHardBreakPoint(
        dwIndex, pAddr, dwSize, nOperate, nStatus);

    if (pHbp == NULL)
    {
        return false;
    }

    if (!pHbp->Set(m_dwProcessId))
    {
        return false;
    }

    m_lstHbp.AddTail(pHbp);

    return true;
}

//�����ڴ�ϵ�
bool CDebugger::SetMemBreakPoint(
    LPVOID pAddr,
    SIZE_T dwLen,
    int nOperate,
    int nStatus)
{
    CBreakPointList lstMbp;
    
    CBreakPointList::iterator itBp = m_lstMbp.begin();

    //������һ��ԭ���ı�������
    for (; itBp != m_lstMbp.end(); itBp++)
    {
        CBreakPointPtr pBp = *itBp;

        if (pBp->IsSet())
        {
            pBp->Reset(m_dwProcessId);
            lstMbp.AddTail(pBp);
        }
    }

    //ˢ��һ���ڴ�����
    RefreshMemInfo();

    //��ԭԭ�е��ڴ�ϵ�
    lstMbp.Set(m_dwProcessId);

    CBreakPointPtr pMbp = new CMemBreakPoint(
        pAddr, dwLen, &m_lstMemInfo, nOperate, nStatus);

    if (pMbp == NULL)
    {
        return false;
    }

    //�����ڴ�ϵ�
    if (!pMbp->Set(m_dwProcessId))
    {
        return false;
    }

    //��ӵ��ڴ�ϵ��б���
    m_lstMbp.AddTail(pMbp);

    return true;
}

//�Ƴ��ϵ�
bool CDebugger::RemoveBreakPoint(CBreakPointList& lstBp, int nIndex)
{
    //�õ�ָ���ϵ�
    POSITION pos = lstBp.GetPosition(nIndex);

    if (pos != NULL)
    {
        CBreakPointPtr pBp = lstBp.GetAt(pos);

        if (pBp->IsSet())
        {
            pBp->Reset(m_dwProcessId);
        }

        //�Ƴ��ָ�����
        m_lstRecover.Remove(pBp);
        lstBp.RemoveAt(pos);

        return true;
    }

    return false;
}

//�Ƴ��ϵ�
bool CDebugger::RemoveBreakPoint(CBreakPointList& lstBp, IBreakPoint *pBp)
{
    if (pBp != NULL)
    {
        if (pBp->IsSet())
        {
            pBp->Reset(m_dwProcessId);
        }

        //�Ƴ��ָ�����
        m_lstRecover.Remove(pBp);

        return lstBp.Remove(pBp);
    }

    return false;
}


//���õ�������
bool CDebugger::SetStepInto(LPDEBUG_EVENT pDbgEvt, bool bNotify)
{
    //���߳�
    HTHREAD hThread(pDbgEvt->dwThreadId);

    if (hThread == NULL)
    {
        return false;
    }

    //��ȡ�̻߳���
    CONTEXT Context = { 0 };
    Context.ContextFlags |= CONTEXT_ALL;
    BOOL bRet = GetThreadContext(hThread, &Context);

    if (!bRet)
    {
        return false;
    }

    //���õ�������
    SetStepInto(&Context, bNotify);

    bRet = SetThreadContext(hThread, &Context);

    return bRet != FALSE;
}


//���õ�������
bool CDebugger::SetStepInto(CONTEXT *pContext, bool bNotify)
{
    //ֱ���õ���
    pContext->EFlags |= 0x100;

    if (bNotify)
    {
        m_bSingleStep = TRUE;
    }

    return true;
}


//���õ�������
bool CDebugger::SetStepOver(LPDEBUG_EVENT pDbgEvt)
{
    //���߳�
    HTHREAD hThread(pDbgEvt->dwThreadId);

    if (hThread == NULL)
    {
        return false;
    }

    //��ȡ�̻߳���
    CONTEXT Context = { 0 };
    Context.ContextFlags |= CONTEXT_ALL;
    BOOL bRet = GetThreadContext(hThread, &Context);

    if (!bRet)
    {
        return false;
    }

    //���õ�������
    SetStepOver(&Context);

    bRet = SetThreadContext(hThread, &Context);

    return bRet != FALSE;
}


bool CDebugger::SetStepOver(CONTEXT *pContext)
{
    //�ж��ǵ������뻹�ǵ�������
    DWORD dwCodeLen = 0;

    if (IsStepOver(m_dwProcessId, (LPVOID)pContext->ip, &dwCodeLen))
    {
        //����������һ��ָ���¶ϵ�
        SetSoftBreakPoint((LPVOID)(pContext->ip + dwCodeLen), BREAKPOINT_ONCE);
    }
    else
    {
        //ֱ���õ���
        pContext->EFlags |= 0x100;
        m_bSingleStep = TRUE;
    }

    return true;
}


//�Ƿ���Ҫ��������
bool CDebugger::IsStepOver(DWORD dwPid, LPVOID pAddr, PDWORD pdwCodeLen)
{
    //�򿪽���
    HPROCESS hProcess(dwPid);

    if (hProcess == NULL)
    {
        return false;
    }

    //������������
    SIZE_T dwBytes = 0;
    BYTE szCode[MAX_BIN_CODE] = { 0 };

    BOOL bRet = ReadProcessMemory(
        hProcess,
        pAddr,
        szCode,
        sizeof(szCode),
        &dwBytes);

    if (dwBytes == 0)
    {
        return false;
    }

    //����������ָ��
    AdjustBinary(szCode, pAddr, dwBytes);

    char szAsmCode[MAX_ASM_CODE] = { 0 };
    char szOpcode[MAX_ASM_CODE] = { 0 };

    //��ʾ�����ָ��
    Decode2AsmOpcode(
        szCode,
        szAsmCode,
        szOpcode,
        (UINT*)pdwCodeLen,
        (UINT)pAddr);

    //�жϵ�ǰָ���Ƿ�Ϊcall
    return (strstr(szAsmCode, "call") != NULL);
}


//����������ָ��
void CDebugger::AdjustBinary(PBYTE pbtCode, LPVOID pAddr, SIZE_T nCount)
{
    CBreakPointList::iterator itBp = m_lstBp.begin();

    for (; itBp != m_lstBp.end(); itBp++)
    {
        CSoftBreakPoint *pBp = (CSoftBreakPoint*)(IBreakPoint*)*itBp;

        //����ڷ�Χ��
        if (pBp->IsSet() &&
            pBp->GetAddress() >= pAddr &&
            pBp->GetAddress() < (PBYTE)pAddr + nCount)
        {
            int nIndex = (int)((PBYTE)pBp->GetAddress() - (PBYTE)pAddr);

            //�����ָ����ͬ
            if (pbtCode[nIndex] == pBp->GetBpCode())
            {
                pbtCode[nIndex] = pBp->GetOrigCode();
            }
        }
    }

    return;
}


//ˢ���ڴ���Ϣ
void CDebugger::RefreshMemInfo()
{
    m_lstMemInfo.RemoveAll();

    HPROCESS hProcess(m_dwProcessId);

    if (hProcess == NULL)
    {
        return;
    }

    PBYTE pAddr = NULL;

    while (TRUE)
    {
        MEMORY_BASIC_INFORMATION memInfo;

        SIZE_T dwSize = VirtualQueryEx(
            hProcess,
            pAddr,
            &memInfo,
            sizeof(MEMORY_BASIC_INFORMATION));

        if (dwSize != sizeof(MEMORY_BASIC_INFORMATION))
        {
            break;
        }
        
        pAddr = (PBYTE)memInfo.BaseAddress + memInfo.RegionSize;

        if (memInfo.State != MEM_COMMIT)
        {
            continue;
        }

        m_lstMemInfo.AddTail(memInfo);
    }

    return;
}


//��������¼�
unsigned __stdcall CDebugger::DebugThreadProc(LPVOID lpParam)
{
    CDebugger *pThis = (CDebugger*)lpParam;

    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = { 0 };

    //�������Խ���
    BOOL bRet = CreateProcessW(
        NULL,                   // No module name (use command line). 
        pThis->m_strFileName,   // Command line. 
        NULL,                   // Process handle not inheritable. 
        NULL,                   // Thread handle not inheritable. 
        FALSE,                  // Set handle inheritance to FALSE. 
        DEBUG_ONLY_THIS_PROCESS,// Debug flag. 
        NULL,                   // Use parent's environment block. 
        NULL,                   // Use parent's starting directory. 
        &si,                    // Pointer to STARTUPINFO structure.
        &pi);                   // Pointer to PROCESS_INFORMATION structure.

    if (!bRet)
    {
        return 0;
    }

    pThis->m_dwProcessId = pi.dwProcessId;

    CDebugEvent dbgEvt;

    while (pThis->m_dwProcessId != 0)
    {
        //���ܵ����¼�
        BOOL bRet = WaitForDebugEvent(&dbgEvt, INFINITE);

        if (bRet)
        {
            //�ص�����
            pThis->m_pfnNotify(
                NOTIFY_DEBUGEVENT_PRE,
                pThis,
                &dbgEvt);

            dbgEvt.m_dwContinueStatus = OnDebugEvent(
                dbgEvt.dwDebugEventCode,
                pThis,
                &dbgEvt);

            //�ص�����
            pThis->m_pfnNotify(
                NOTIFY_DEBUGEVENT_POST,
                pThis,
                &dbgEvt);

            //�ύ�¼�������
            ContinueDebugEvent(
                dbgEvt.dwProcessId,
                dbgEvt.dwThreadId,
                dbgEvt.m_dwContinueStatus);
        }
        else if (!bRet && GetLastError() != ERROR_SEM_TIMEOUT)
        {
            pThis->m_pfnNotify(
                NOTIFY_ERROR,
                pThis,
                NULL);

            break;
        }
    }

    CloseHandle(pi.hProcess);

    return 0;
}


DWORD CDebugger::OnCreateProcessDebugEvent(CDebugEvent *pDbgEvt)
{
    //������һ�µ�һ��Ҫ��ϵͳ�ϵ�
    m_bSystemBp = true;

    m_pfnNotify(NOTIFY_PROCESS_CREATE, this, pDbgEvt);

    return DBG_EXCEPTION_NOT_HANDLED;
}


DWORD CDebugger::OnExitProcessDebugEvent(CDebugEvent *pDbgEvt)
{
    m_pfnNotify(NOTIFY_PROCESS_EXIT, this, pDbgEvt);

    m_dwProcessId = 0;
    
    return DBG_EXCEPTION_NOT_HANDLED;
}


DWORD CDebugger::OnExceptionBreakPoint(CDebugEvent *pDbgEvt)
{
    BOOL bFound = FALSE;
    PEXCEPTION_RECORD pExcpRec = &pDbgEvt->u.Exception.ExceptionRecord;

    POSITION pos = m_lstBp.GetHeadPosition();

    //����ܲ����ҵ�ƥ���ϵĶϵ�
    while (pos != NULL)
    {
        POSITION posCur = pos;
        CSoftBreakPoint *pBp = (CSoftBreakPoint*)
            (IBreakPoint*)m_lstBp.GetNext(pos);

        //��������ƥ������
        if (pBp->IsMatch(pExcpRec))
        {
            bFound = TRUE;

            //�����öϵ�
            pBp->Reset(pDbgEvt->dwProcessId);

            switch (pBp->GetStatus())
            {
            case BREAKPOINT_ALWAYS:
                m_lstRecover.AddTail(pBp); //��ӵ��ָ�����
                break;
            case BREAKPOINT_ONCE:
                m_lstBp.RemoveAt(posCur); //������Ƴ��˰�
                break;
            }
        }
    }

    //���û��ƥ�䵽ֱ�ӷ���
    if (!bFound)
    {
        if (m_bSystemBp)
        {
            m_bSystemBp = false;
            m_pfnNotify(NOTIFY_SYSTEM_BREAKPOINT, this, pDbgEvt);

            return DBG_CONTINUE;
        }

        return DBG_EXCEPTION_NOT_HANDLED;
    }

    //���߳�
    HTHREAD hThread(pDbgEvt->dwThreadId);

    if (hThread == NULL)
    {
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    //��ȡ�̻߳���
    CONTEXT Context = { 0 };
    Context.ContextFlags |= CONTEXT_ALL;
    BOOL bRet = GetThreadContext(hThread, &Context);

    if (!bRet)
    {
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    //�쵥���쳣
    SetStepInto(&Context, false);

    Context.ip--;

    //�����̻߳���
    bRet = SetThreadContext(hThread, &Context);

    if (!bRet)
    {
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    if (m_pLastAddr != (LPVOID)Context.ip)
    {
        m_pfnNotify(NOTIFY_BREAKPOINT, this, pDbgEvt);
        m_pLastAddr = (LPVOID)Context.ip;
    }

    return DBG_CONTINUE;
}


DWORD CDebugger::OnExceptionSingleStep(CDebugEvent *pDbgEvt)
{
    PEXCEPTION_RECORD pExepRec = &pDbgEvt->u.Exception.ExceptionRecord;

    if (m_pLastAddr != (LPVOID)pExepRec->ExceptionAddress)
    {
        //�Ȼָ�ԭ�жϵ�
        m_lstRecover.Set(pDbgEvt->dwProcessId);
        m_lstRecover.RemoveAll();
        m_pLastAddr = NULL;
    }

    //���߳�
    HTHREAD hThread(pDbgEvt->dwThreadId);

    if (hThread == NULL)
    {
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    //��ȡ�̻߳���
    CONTEXT Context = { 0 };
    Context.ContextFlags |= CONTEXT_ALL;
    BOOL bRet = GetThreadContext(hThread, &Context);

    if (!bRet)
    {
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    POSITION pos = m_lstHbp.GetHeadPosition();

    //����ܲ����ҵ�ƥ���ϵĶϵ�
    while (pos != NULL)
    {
        POSITION posCur = pos;
        CHardBreakPoint *pHbp = (CHardBreakPoint*)
            (IBreakPoint*)m_lstHbp.GetNext(pos);

        //��������ƥ������
        if (pHbp->IsMatch(&Context))
        {
            //�����öϵ�
            pHbp->Reset(&Context);

            switch (pHbp->GetStatus())
            {
            case BREAKPOINT_ALWAYS:
                m_lstRecover.AddTail(pHbp); //��ӵ��ָ�����
                break;
            case BREAKPOINT_ONCE:
                m_lstHbp.RemoveAt(posCur); //������Ƴ��˰�
                break;
            }

            //�ϲ����
            SetStepInto(&Context, false);

            //��Ҫ����֪ͨ
            m_bSingleStep = TRUE;
        }
    }

    //�����̻߳���
    bRet = SetThreadContext(hThread, &Context);

    if (!bRet)
    {
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    if (m_bSingleStep && 
        m_pLastAddr != pExepRec->ExceptionAddress)
    {
        m_bSingleStep = FALSE;
        m_pfnNotify(NOTIFY_BREAKPOINT, this, pDbgEvt);
        m_pLastAddr = pExepRec->ExceptionAddress;
    }

    return DBG_CONTINUE;
}


DWORD CDebugger::OnExceptionAccessViolation(CDebugEvent *pDbgEvt)
{
    BOOL bFound = FALSE;
    CMemBreakPoint *pMbpHit = NULL;
    PEXCEPTION_RECORD pExepRec = &pDbgEvt->u.Exception.ExceptionRecord;
    
    POSITION pos = m_lstHbp.GetHeadPosition();

    //����ܲ����ҵ�ƥ���ϵĶϵ�
    while (pos != NULL)
    {
        POSITION posCur = pos;
        CMemBreakPoint *pMbp = (CMemBreakPoint*)
            (IBreakPoint*)m_lstHbp.GetNext(pos);

        //��������ƥ������
        if (pMbp->IsMatch(pExepRec))
        {
            //�����öϵ�
            pMbp->Reset(pDbgEvt->dwProcessId);

            switch (pMbp->GetStatus())
            {
            case BREAKPOINT_ALWAYS:
                m_lstRecover.AddTail(pMbp); //��ӵ��ָ�����
                break;
            case BREAKPOINT_ONCE:
                m_lstMbp.RemoveAt(posCur); //������Ƴ��˰�
                break;
            }

            bFound = TRUE;
        }
        else if (pMbp->InRegion(pExepRec))
        {
            pMbpHit = pMbp;
        }
    }

    if (!bFound)
    {
        //���������
        if (pMbpHit != NULL)
        {
            pMbpHit->Reset(pDbgEvt->dwProcessId); //�Ȼ�ԭ�ڴ�����
            m_lstRecover.AddTail(pMbpHit); //Ȼ����ӵ��ָ�����
            SetStepInto(pDbgEvt, false); //�õ���

            return DBG_CONTINUE;
        }

        return DBG_EXCEPTION_NOT_HANDLED;
    }

    //�ϲ����
    SetStepInto(pDbgEvt, false);

    if (m_pLastAddr != pExepRec->ExceptionAddress)
    {
        m_pfnNotify(NOTIFY_BREAKPOINT, this, pDbgEvt);
        m_pLastAddr = pExepRec->ExceptionAddress;
    }

    return DBG_CONTINUE;
}