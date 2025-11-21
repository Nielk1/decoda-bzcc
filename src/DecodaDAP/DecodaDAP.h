#pragma once

#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"

//#include "../Frontend/DebugFrontend.h"
#include "DebugFrontend.h"

#include "StlUtility.h"
#include "CriticalSectionLock.h"

#include <imagehlp.h>
#include <tlhelp32.h>

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
        LineMapper      lineMapper; // Current mapping from lines in the local file to backend script lines.
    };

    struct StackFrame
    {
        unsigned int    scriptIndex;
        unsigned int    line;
        std::string     function;
    };

    enum State
    {
        State_Inactive,         // Not debugging.
        State_Running,          // Debugging a program is is currently running.
        State_Broken,           // Debugging a program is is currently break point.
    };

    std::unique_ptr<dap::Session> session;

    DWORD                       m_processId = 0;
    HANDLE                      m_process = NULL;

    Channel                     m_eventChannel;
    HANDLE                      m_eventThread = NULL;

    Channel                     m_commandChannel;

    mutable CriticalSection     m_criticalSection;
    std::vector<Script*>        m_scripts;

    std::vector<StackFrame>     m_stackFrames;

    State                       m_state;

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
    DWORD WINAPI StaticEventThreadProc(LPVOID param);
    void EventThreadProc();
    std::string MakeValidFileName(const std::string& name);
    HWND GetProcessWindow(DWORD processId) const;

public:

    void Continue(unsigned int vm);
    void Break(unsigned int vm);
    void StepOver(unsigned int vm);
    void StepInto(unsigned int vm);
    void StepOut(unsigned int vm);
    bool Evaluate(unsigned int vm, std::string expression, unsigned int stackLevel, std::string& result);

    void ToggleBreakpoint(unsigned int vm, unsigned int scriptIndex, unsigned int line);
    void RemoveAllBreakPoints(unsigned int vm);

    unsigned int GetNumStackFrames() const
    const StackFrame GetStackFrame(unsigned int i) const

    DebugFrontend::Script* getScript(unsigned int scriptIndex) {
        return DebugFrontend::Get().GetScript(scriptIndex);
    }

    unsigned int m_vm; // For now, always 0
};