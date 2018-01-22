#pragma once

#include "BreakPoint.h"


class CSoftBreakPoint
    : public IBreakPoint
{
public:
    CSoftBreakPoint(LPVOID pAddr, int nStatus = BREAKPOINT_ALWAYS);
    virtual ~CSoftBreakPoint();

public:
    virtual int GetType();
    virtual int GetStatus();
    virtual int GetOperate();
    virtual LPVOID GetAddress();

    virtual bool IsSet();
    virtual bool Set(DWORD dwPid);
    virtual bool Reset(DWORD dwPid);

    bool IsMatch(PEXCEPTION_RECORD pExcpRec);
    BYTE GetBpCode();
    BYTE GetOrigCode();

protected:
    INT    m_nFlag;
    BOOL   m_bSet;       //�Ƿ�����
    LPVOID m_pAddr;      //�ϵ��ַ
    BYTE   m_btOrigCode; //ԭָ��
    BYTE   m_btBpCode;   //�ж�ָ��
};
