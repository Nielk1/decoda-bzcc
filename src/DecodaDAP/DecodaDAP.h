#pragma once

#include <windows.h>
//#include <WinDef.h>

#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include "StlUtility.h"
#include "CriticalSection.h"
#include "CriticalSectionLock.h"
#include "Protocol.h"
#include "Channel.h"
//#include "LineMapper.h"

#include "MutexEvent.h"

#include <imagehlp.h>
#include <tlhelp32.h>

#include <wincrypt.h>
#include <vector>
#include <fstream>

class DecodaDAP {
private:
    struct ExeInfo
    {
        unsigned long   entryPoint;
        bool            managed;
        bool            i386;
    };

    struct Script
    {
        std::string     name;       // Identifying name of the script (usually a file name)
        std::string     source;     // Source code for the script
        CodeState       state;
        //LineMapper      lineMapper; // Current mapping from lines in the local file to backend script lines.
    };

    struct StackFrame
    {
        unsigned int    scriptIndex;
        unsigned int    line;
        std::string     function;
        unsigned int    vm;
    };

    enum State
    {
        State_Inactive,         // Not debugging.
        State_Running,          // Debugging a program is is currently running.
        State_Broken,           // Debugging a program is is currently break point.
    };

public:
    std::unique_ptr<dap::Session> session;

private:
    DWORD                       m_processId = 0;
public:
    HANDLE                      m_process = NULL;

private:
    Channel                     m_eventChannel;
    HANDLE                      m_eventThread = NULL;

    Channel                     m_commandChannel;

    mutable CriticalSection     m_criticalSection;
    std::vector<Script*>        m_scripts;

    std::vector<StackFrame>     m_stackFrames;

    State                       m_state;


public:
    std::unordered_map<unsigned int, std::string> m_vmNames;
    std::unordered_map<int, Script*> m_virtualSources;

public:
    DecodaDAP() : m_vm(0) {}

    bool Start(const char* command, const char* commandArguments, const char* currentDirectory, const char* symbolsDirectory, bool debug, bool startBroken);
    void Stop(bool kill);

private:
    bool StartProcessAndRunToEntry(LPCSTR exeFileName, LPSTR commandLine, LPCSTR directory, PROCESS_INFORMATION& processInfo);
    bool GetExeInfo(LPCSTR fileName, ExeInfo& info) const;
    void MessageEvent(const std::string& message, MessageType type);
    void OutputError(DWORD error);
    bool InitializeBackend(const char* symbolsDirectory);
    bool GetIsBeingDebugged(DWORD processId);
    char* RemoteStrDup(HANDLE process, const char* string);
    bool ExecuteRemoteKernelFuntion(HANDLE process, const char* functionName, LPVOID param, DWORD& exitCode);
    bool InjectDll(DWORD processId, const char* dllFileName);
    bool GetStartupDirectory(char* path, int maxPathLength);
    bool ProcessInitialization(const char* symbolsDirectory);
    void EventThreadProc();
    std::string MakeValidFileName(const std::string& name);
    HWND GetProcessWindow(DWORD processId) const;

    /**
     * Static version of the event handler thread entry point. This just
     * forwards to the non-static version.
     */
    static DWORD WINAPI StaticEventThreadProc(LPVOID param);

public:
    bool Attach(unsigned int processId, const char* symbolsDirectory);

    void Continue(unsigned int vm);
    void Break(unsigned int vm);
    void StepOver(unsigned int vm);
    void StepInto(unsigned int vm);
    void StepOut(unsigned int vm);
    bool Evaluate(unsigned int vm, std::string expression, unsigned int stackLevel, std::string& result);

    void ToggleBreakpoint(unsigned int vm, unsigned int scriptIndex, unsigned int line);
    void RemoveAllBreakPoints();
    void SetBreakpoint(HANDLE p_process, LPVOID entryPoint, bool set, BYTE* data) const;

    unsigned int GetNumStackFrames() const;
    const StackFrame GetStackFrame(unsigned int i) const;

    Script* GetScript(unsigned int scriptIndex);

    unsigned int m_vm; // For now, always 0
};