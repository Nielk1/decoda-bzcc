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

#include <unordered_set>
#include <unordered_map>

#include <filesystem>

#include "tinyxml.h"

namespace dap {

    class AttachRequestEx : public AttachRequest {
    public:
        optional<dap::integer> processId;
        optional<std::string> symbols;
        optional<dap::array<dap::string>> luaWorkspaceLibrary;
    };

    DAP_STRUCT_TYPEINFO_EXT(AttachRequestEx,
        AttachRequest,
        "attach", // This is the wire protocol name for the AttachRequest
        DAP_FIELD(processId, "processId"),
        DAP_FIELD(symbols, "symbols"),
        DAP_FIELD(luaWorkspaceLibrary, "luaWorkspaceLibrary"));


    class LaunchRequestEx : public LaunchRequest {
    public:
        optional<dap::string> program;
        optional<dap::string> args;
        optional<dap::string> cwd;
        optional<dap::string> symbols;
        optional<dap::boolean> breakOnStart;
        optional<dap::array<dap::string>> luaWorkspaceLibrary;
    };

    DAP_STRUCT_TYPEINFO_EXT(LaunchRequestEx,
        LaunchRequest,
        "launch", // This is the wire protocol name for the LaunchRequest
        DAP_FIELD(program, "program"),
        DAP_FIELD(args, "args"),
        DAP_FIELD(cwd, "cwd"),
        DAP_FIELD(symbols, "symbols"),
        DAP_FIELD(breakOnStart, "breakOnStart"),
        DAP_FIELD(luaWorkspaceLibrary, "luaWorkspaceLibrary"));

}  // namespace dap


class DecodaDAP {
private:
    struct ExeInfo
    {
        unsigned long   entryPoint;
        bool            managed;
        bool            i386;
    };

    //struct Script
    //{
    //    std::string     name;       // Identifying name of the script (usually a file name)
    //    std::string     source;     // Source code for the script
    //    CodeState       state;
    //    //LineMapper      lineMapper; // Current mapping from lines in the local file to backend script lines.
    //
    //    dap::Source  sourceInfo; // DAP source info
    //    std::vector<unsigned int>   breakpoints; // breakpoint lines
    //};

    struct ScriptBreakpoint
    {
        bool desireActive;
        dap::SourceBreakpoint dap;
        std::unordered_map<unsigned int, bool> VmIsActiveMap; // current state of breakpoint in a VM
        std::string condition;

        ScriptBreakpoint() : desireActive(false) {}
        ScriptBreakpoint(dap::SourceBreakpoint dap) : dap(dap), desireActive(false) {}
    };

    struct ScriptData
    {
        std::string name; // name of the script, generally filename
        std::unordered_map<unsigned int, unsigned int> indexMap; // what script indexes there are and what VM index it is loaded for (allow cleaning when VMs are gone)

        std::unordered_map<int64_t, ScriptBreakpoint> breakpoints; // set of breakpoint lines


        std::string     source;     // Source code for the script
        CodeState       state;
        dap::Source  sourceInfo; // DAP source info


        ScriptData() : name("") {}
        ScriptData(const std::string& scriptName) : name(scriptName) {}
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

    //std::vector<Script*>        m_scripts;
    std::vector<StackFrame>     m_stackFrames;

    std::unordered_map<std::string, ScriptData> m_scriptData;
    std::vector<std::string>                    m_scriptIndexes;

    State                       m_state;

    unsigned int m_step_until_under_depth = 0;

public:
    std::unordered_map<int, std::vector<dap::Variable>> variableStore;
    int StoreVariables(const std::vector<dap::Variable>& vars);
private:
    int nextVariableReference = 1;

public:
    dap::Source GetDapSource(int scriptIndex);

public:
    std::unordered_map<unsigned int, std::string> m_vmNames;
    //std::unordered_map<int, Script*> m_virtualSources;
    std::unordered_map<std::string, dap::string> sourceMap;

public:
    DecodaDAP() : m_vm(0) {}

    bool Start(const char* command, const char* commandArguments, const char* currentDirectory, const char* symbolsDirectory, bool debug, bool startBroken);
    void Stop(bool kill);

private:
    bool StartProcessAndRunToEntry(LPCSTR exeFileName, LPSTR commandLine, LPCSTR directory, PROCESS_INFORMATION& processInfo);
    bool GetExeInfo(LPCSTR fileName, ExeInfo& info) const;
    void MessageEvent(const std::string& message, MessageType type) const;
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
    dap::SetBreakpointsResponse HandleSetBreakpointsRequest(const dap::SetBreakpointsRequest& request);

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

    bool BreakpointIsActive(dap::string name, dap::SourceBreakpoint breakpoint);
    void SetBreakpointsForScript(dap::Source source, dap::array<dap::SourceBreakpoint> breakpoints, dap::array<dap::Breakpoint>& breakpointsOut);

    // used for internal tracking
    void ApplyScriptBreakpoint(unsigned int vm, unsigned int scriptIndex, dap::integer line);

    // used for system breakpoints
    void SetBreakpoint(HANDLE p_process, LPVOID entryPoint, bool set, BYTE* data) const;

    unsigned int GetNumStackFrames() const;
    const StackFrame GetStackFrame(unsigned int i) const;

    bool PerformAutoStep(unsigned int vm);

    //Script* GetScript(unsigned int scriptIndex);

    unsigned int m_vm; // For now, always 0
};