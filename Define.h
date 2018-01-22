#pragma once

#include <Windows.h>
#include <assert.h>
#include "../Common.h"

class CDebugger;

const int MAX_BIN_CODE = 0x20; //һ��������ָ����󳤶ȣ��ݶ���
const int MAX_ASM_CODE = 0x40; //һ�����ָ���ַ������ݶ���


//�������ص�����
typedef enum emDEBUG_NOTIFY_TYPE
{
    NOTIFY_DEBUGEVENT_PRE,
    NOTIFY_DEBUGEVENT_POST,
    NOTIFY_PROCESS_CREATE,
    NOTIFY_PROCESS_EXIT,
    NOTIFY_BREAKPOINT,
    NOTIFY_SYSTEM_BREAKPOINT,
    NOTIFY_ERROR
}DEBUG_NOTIFY_TYPE;


//�����¼�
class CDebugEvent : public DEBUG_EVENT
{
public:
    CDebugEvent() { memset(this, 0, sizeof(CDebugEvent)); }
    ~CDebugEvent() { }

public:
    DWORD m_dwContinueStatus;
};


//���Իص�����
typedef void(*PFNDebugNotify)(DEBUG_NOTIFY_TYPE type, CDebugger *pDebugger, CDebugEvent *pDbgEvt);


//switch...case...���ݵ����¼�������Ӧ����
#define BEGIN_DEBUG_EVENT_MAP() static DWORD OnDebugEvent(DWORD dwEvtCode, CDebugger *pDebugger, CDebugEvent *pDbgEvt) { switch (dwEvtCode) {
#define DECLARE_DEBUG_EVENT(excpCode, pfnHandle) case excpCode: return pDebugger->pfnHandle(pDbgEvt);
#define END_DEBUG_EVENT_MAP() default: return DBG_EXCEPTION_NOT_HANDLED; } }


//switch...case...�����쳣���͵�����Ӧ����
#define BEGIN_EXCEPTION_MAP() DWORD OnExceptionDebugEvent(CDebugEvent *pDbgEvt) { switch (pDbgEvt->u.Exception.ExceptionRecord.ExceptionCode) {
#define DECLARE_EXCEPTION(excpCode, pfnHandle) case excpCode: return pfnHandle(pDbgEvt);
#define END_EXCEPTION_MAP() default: return DBG_EXCEPTION_NOT_HANDLED; } }