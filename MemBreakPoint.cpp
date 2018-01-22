#include "MemBreakPoint.h"


CMemBreakPoint::CMemBreakPoint(LPVOID pAddr, SIZE_T dwLen,
    CMemInfoList *plstMemInfo, int nType, int nStatus)
    : m_nFlag(nType | nStatus)
    , m_bSet(FALSE)
    , m_pAddr(pAddr)
    , m_dwLen(dwLen)
    , m_dwProtect(PAGE_NOACCESS)
    , m_pLstMemInfo(plstMemInfo)
{

}


CMemBreakPoint::~CMemBreakPoint()
{
}


int CMemBreakPoint::GetType()
{
    return BREAKPOINT_MEMORY;
}


int CMemBreakPoint::GetStatus()
{
    return BREAKPOINT_STATUS(m_nFlag);
}


int CMemBreakPoint::GetOperate()
{
    return BREAKPOINT_OPERATE(m_nFlag);
}


LPVOID CMemBreakPoint::GetAddress()
{
    return m_pAddr;
}


bool CMemBreakPoint::Set(DWORD dwPid)
{
    m_lstMemProtect.RemoveAll();

    //�򿪽���
    HPROCESS hProcess(dwPid);

    if (hProcess == NULL)
    {
        return false;
    }

    //����ϵ���ʼλ�õ�����λ��
    SIZE_T dwBegin = (SIZE_T)m_pAddr & 0xFFFFF000;
    SIZE_T dwEnd = ((SIZE_T)m_pAddr + m_dwLen + 0x00000FFF) & 0xFFFFF000;

    CMemInfoList::iterator itList = m_pLstMemInfo->begin();

    for (; itList != m_pLstMemInfo->end(); itList++)
    {
        MEMORY_BASIC_INFORMATION& memInfo = *itList;

        //�ڴ���ʼ��ַ��������ַ
        SIZE_T dwMemBegin = (SIZE_T)memInfo.BaseAddress;
        SIZE_T dwMemEnd = dwMemBegin + memInfo.RegionSize;

        //�������������ܲ����غ�
        if (dwBegin >= dwMemEnd || dwEnd < dwMemBegin)
        {
            continue;
        }

        //�����������
        dwMemBegin = max(dwBegin, dwMemBegin);
        dwMemEnd = min(dwEnd, dwMemEnd);

        MEMORY_PROTECT_INFO protectInfo;;
        protectInfo.m_pAddr = (LPVOID)dwMemBegin;
        protectInfo.m_dwLen = dwMemEnd - dwMemBegin;
        protectInfo.m_pMemInfo = &memInfo;

        //ֱ���޸��ڴ�����
        DWORD dwProtect = 0;
        BOOL bRet = VirtualProtectEx(
            hProcess,
            protectInfo.m_pAddr,
            protectInfo.m_dwLen,
            m_dwProtect,
            &dwProtect);

        if (!bRet)
        {
            Reset(dwPid);
            return false;
        }

        //���浽����
        m_lstMemProtect.AddTail(protectInfo);
    }

    m_bSet = TRUE;

    return true;
}


bool CMemBreakPoint::Reset(DWORD dwPid)
{
    //�򿪽���
    HPROCESS hProcess(dwPid);

    if (hProcess == NULL)
    {
        return false;
    }

    CMemProtectList::iterator itList = m_lstMemProtect.begin();

    for (; itList != m_lstMemProtect.end(); itList++)
    {
        MEMORY_PROTECT_INFO& protectInfo = *itList;

        //�޸ĳ�ԭ��������
        DWORD dwProtect = 0;
        VirtualProtectEx(
            hProcess,
            protectInfo.m_pAddr,
            protectInfo.m_dwLen,
            protectInfo.m_pMemInfo->Protect,
            &dwProtect);
    }

    m_lstMemProtect.RemoveAll();

    m_bSet = FALSE;

    return true;
}


bool CMemBreakPoint::IsSet()
{
    return IS_BREAKPOINT_FORBIDDEN(m_nFlag) && m_bSet == TRUE;
}


bool CMemBreakPoint::InRegion(PEXCEPTION_RECORD pExepRec)
{
    SIZE_T dwAddr = (SIZE_T)pExepRec->ExceptionInformation[1]; //��������쳣�ĵ�ַ

    CMemProtectList::iterator itList = m_lstMemProtect.begin();

    for (; itList != m_lstMemProtect.end(); itList++)
    {
        MEMORY_PROTECT_INFO& protectInfo = *itList;

        if (dwAddr >= (SIZE_T)protectInfo.m_pAddr &&
            dwAddr < (SIZE_T)protectInfo.m_pAddr + protectInfo.m_dwLen)
        {
            return true;
        }
    }

    return false;
}


bool CMemBreakPoint::IsMatch(PEXCEPTION_RECORD pExcpRec)
{
    if (!IsSet())
    {
        return false;
    }

    SIZE_T dwAddr = (SIZE_T)pExcpRec->ExceptionInformation[1]; //��������쳣�ĵ�ַ
    SIZE_T dwType = (SIZE_T)pExcpRec->ExceptionInformation[0]; //��������쳣������

    if (dwAddr < (SIZE_T)m_pAddr ||
        dwAddr >= (SIZE_T)m_pAddr + m_dwLen)
    {
        return false;
    }

    switch (BREAKPOINT_OPERATE(m_nFlag))
    {
    case BREAKPOINT_WRITE:
        return dwType == 1;
    case BREAKPOINT_ACCESS:
        return true;
    default:
        return false;
    }
}


SIZE_T CMemBreakPoint::GetLength()
{
    return m_dwLen;
}


DWORD CMemBreakPoint::GetProtect()
{
    return m_dwProtect;
}


CMemProtectList& CMemBreakPoint::GetProtectList()
{
    return m_lstMemProtect;
}