#include "DecodaDAP.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <unordered_set>

#ifdef _MSC_VER
#define OS_WINDOWS 1
#endif



unsigned int makeVariablesReference(unsigned int frameIndex, bool isGlobal) {
    return (frameIndex << 1) | (isGlobal ? 1 : 0);
}

void decodeVariablesReference(unsigned int variablesReference, unsigned int& frameIndex, bool& isGlobal) {
    frameIndex = variablesReference >> 1;
    isGlobal = (variablesReference & 1) != 0;
}


bool DecodaDAP::StartProcessAndRunToEntry(LPCSTR exeFileName, LPSTR commandLine, LPCSTR directory, PROCESS_INFORMATION& processInfo)
{

    STARTUPINFOA startUpInfo = { 0 };
    startUpInfo.cb = sizeof(startUpInfo);

    ExeInfo info;
    if (!GetExeInfo(exeFileName, info) || info.entryPoint == 0)
    {
        MessageEvent("Error: The entry point for the application could not be located", MessageType_Error);
        return false;
    }

    if (!info.i386)
    {
        MessageEvent("Error: Debugging 64-bit applications is not supported", MessageType_Error);
        return false;
    }

    DWORD flags = DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS;

    if (!CreateProcessA(NULL, commandLine, NULL, NULL, TRUE, flags, NULL, directory, &startUpInfo, &processInfo))
    {
        OutputError(GetLastError());
        return false;
    }

    // Running to the entry point currently doesn't work for managed applications, so
    // just start it up.

    if (!info.managed)
    {

        unsigned long entryPoint = info.entryPoint;

        BYTE breakPointData;
        bool done = false;

        while (!done)
        {

            DEBUG_EVENT debugEvent;
            WaitForDebugEvent(&debugEvent, INFINITE);

            DWORD continueStatus = DBG_EXCEPTION_NOT_HANDLED;

            if (debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
            {
                if (debugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP ||
                    debugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
                {

                    CONTEXT context;
                    context.ContextFlags = CONTEXT_FULL;

                    GetThreadContext(processInfo.hThread, &context);

                    if (context.Eip == entryPoint + 1)
                    {

                        // Restore the original code bytes.
                        SetBreakpoint(processInfo.hProcess, (LPVOID)entryPoint, false, &breakPointData);
                        done = true;

                        // Backup the instruction pointer so that we execute the original instruction.
                        --context.Eip;
                        SetThreadContext(processInfo.hThread, &context);

                        // Suspend the thread before we continue the debug event so that the program
                        // doesn't continue to run.
                        SuspendThread(processInfo.hThread);

                    }

                    continueStatus = DBG_CONTINUE;

                }
            }
            else if (debugEvent.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            {
                done = true;
            }
            else if (debugEvent.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
            {

                // Offset the entry point by the load address of the process.
                entryPoint += reinterpret_cast<size_t>(debugEvent.u.CreateProcessInfo.lpBaseOfImage);

                // Write a break point at the entry point of the application so that we
                // will stop when we reach that point.
                SetBreakpoint(processInfo.hProcess, reinterpret_cast<void*>(entryPoint), true, &breakPointData);

                CloseHandle(debugEvent.u.CreateProcessInfo.hFile);

            }
            else if (debugEvent.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
            {
                CloseHandle(debugEvent.u.LoadDll.hFile);
            }

            ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);

        }

    }

    DebugActiveProcessStop(processInfo.dwProcessId);
    return true;

}

bool DecodaDAP::GetExeInfo(LPCSTR fileName, ExeInfo& info) const
{

    LOADED_IMAGE loadedImage;
    if (!MapAndLoad(const_cast<PSTR>(fileName), NULL, &loadedImage, FALSE, TRUE))
    {
        return false;
    }

    // Check if this is a managed application.
    // http://www.codeguru.com/cpp/w-p/system/misc/print.php/c14001

    info.managed = false;
    if (loadedImage.FileHeader->Signature == IMAGE_NT_SIGNATURE)
    {

        DWORD netHeaderAddress =
            loadedImage.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;

        if (netHeaderAddress)
        {
            info.managed = true;
        }

    }

    info.entryPoint = loadedImage.FileHeader->OptionalHeader.AddressOfEntryPoint;
    info.i386 = loadedImage.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386;

    UnMapAndLoad(&loadedImage);

    return true;

}

void DecodaDAP::MessageEvent(const std::string& message, MessageType type)
{
    switch (type)
    {
    case MessageType_Normal: {
        dap::OutputEvent output;
        output.category = "stdout";
        output.output = message + "\n";
        session->send(output);
        break;
    }
    case MessageType_Warning:
    case MessageType_Error: {
        dap::OutputEvent output;
        output.category = "stderr";
        output.output = message + "\n";
        session->send(output);
        break;
    }
    }
}

void DecodaDAP::OutputError(DWORD error)
{
    char buffer[1024];
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer, 1024, NULL))
    {
        std::string message = "Error: ";
        message += buffer;
        MessageEvent(message, MessageType_Error);
    }

}

bool DecodaDAP::InitializeBackend(const char* symbolsDirectory)
{

    if (GetIsBeingDebugged(m_processId))
    {
        MessageEvent("Error: The process cannot be debugged because it contains hooks from a previous session", MessageType_Error);
        return false;
    }

    char eventChannelName[256];
    _snprintf(eventChannelName, 256, "Decoda.Event.%x", m_processId);

    char commandChannelName[256];
    _snprintf(commandChannelName, 256, "Decoda.Command.%x", m_processId);

    // Setup communication channel with the process that is used to receive events
    // back to the frontend.
    if (!m_eventChannel.Create(eventChannelName))
    {
        return false;
    }

    // Setup communication channel with the process that is used to send commands
    // to the backend.
    if (!m_commandChannel.Create(commandChannelName))
    {
        return false;
    }

    // Inject our debugger DLL into the process so that we can monitor from
    // inside the process's memory space.
    if (!InjectDll(m_processId, "LuaInject.dll"))
    {
        MessageEvent("Error: LuaInject.dll could not be loaded into the process", MessageType_Error);
        return false;
    }

    // Wait for the client to connect.
    m_eventChannel.WaitForConnection();

    // Read the initialization function from the event channel.

    if (!ProcessInitialization(symbolsDirectory))
    {
        MessageEvent("Error: Backend couldn't be initialized", MessageType_Error);
        return false;
    }

    m_state = State_Running;

    // Start a new thread to handle the incoming event channel.
    DWORD threadId;
    m_eventThread = CreateThread(NULL, 0, StaticEventThreadProc, this, 0, &threadId);

    return true;

}

bool DecodaDAP::GetIsBeingDebugged(DWORD processId)
{
    LPCSTR moduleFileName = "LuaInject.dll";

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (process == NULL)
    {
        return false;
    }

    bool result = false;

    DWORD exitCode;
    char* remoteFileName = RemoteStrDup(process, moduleFileName);

    if (ExecuteRemoteKernelFuntion(process, "GetModuleHandleA", remoteFileName, exitCode))
    {
        result = (exitCode != 0);
    }

    if (remoteFileName != NULL)
    {
        VirtualFreeEx(process, remoteFileName, 0, MEM_RELEASE);
        remoteFileName = NULL;
    }

    if (process != NULL)
    {
        CloseHandle(process);
    }

    return result;

}

char* DecodaDAP::RemoteStrDup(HANDLE process, const char* string)
{

    size_t length = strlen(string) + 1;
    void* remoteString = VirtualAllocEx(process, NULL, length, MEM_COMMIT, PAGE_READWRITE);

    DWORD numBytesWritten;
    WriteProcessMemory(process, remoteString, string, length, &numBytesWritten);

    return static_cast<char*>(remoteString);

}

bool DecodaDAP::ExecuteRemoteKernelFuntion(HANDLE process, const char* functionName, LPVOID param, DWORD& exitCode)
{
    HMODULE kernelModule = GetModuleHandleA("Kernel32");
    FARPROC function = GetProcAddress(kernelModule, functionName);

    if (function == NULL)
    {
        return false;
    }

    DWORD threadId;
    HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)function, param, 0, &threadId);

    if (thread != NULL)
    {
        WaitForSingleObject(thread, INFINITE);
        GetExitCodeThread(thread, &exitCode);

        CloseHandle(thread);
        return true;
    }
    else
    {
        return false;
    }
}

bool DecodaDAP::InjectDll(DWORD processId, const char* dllFileName)
{

    bool success = true;

    // Get the absolute path to the DLL.

    char fullFileName[_MAX_PATH];

    if (!GetStartupDirectory(fullFileName, _MAX_PATH))
    {
        return false;
    }

    strcat(fullFileName, dllFileName);

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (process == NULL)
    {
        return false;
    }

    HMODULE kernelModule = GetModuleHandleA("Kernel32");
    FARPROC loadLibraryProc = GetProcAddress(kernelModule, "LoadLibraryA");

    // Load the DLL.

    DWORD exitCode;
    char* remoteFileName = RemoteStrDup(process, fullFileName);

    if (!ExecuteRemoteKernelFuntion(process, "LoadLibraryA", remoteFileName, exitCode))
    {
        success = false;
    }

    HMODULE dllHandle = reinterpret_cast<HMODULE>(exitCode);

    if (dllHandle == NULL)
    {
        success = false;
    }

    /*
    // Unload the DLL.
    // This is currently not needed since the process will automatically unload
    // the DLL when it exits, however at some point in the future we may need to
    // explicitly unload it so I'm leaving the code here.

    if (dllHandle != NULL)
    {

        if (!ExecuteRemoteKernelFuntion(process, "FreeLibrary", dllHandle, exitCode))
        {
            success = false;
        }

    }
    */

    if (remoteFileName != NULL)
    {
        VirtualFreeEx(process, remoteFileName, 0, MEM_RELEASE);
        remoteFileName = NULL;
    }

    if (process != NULL)
    {
        CloseHandle(process);
    }

    return success;

}

bool DecodaDAP::GetStartupDirectory(char* path, int maxPathLength)
{

    if (!GetModuleFileNameA(NULL, path, maxPathLength))
    {
        return false;
    }

    char* lastSlash = strrchr(path, '\\');

    if (lastSlash == NULL)
    {
        return false;
    }

    // Terminate the path after the last slash.

    lastSlash[1] = 0;
    return true;

}

bool DecodaDAP::ProcessInitialization(const char* symbolsDirectory)
{
    unsigned int command;
    m_eventChannel.ReadUInt32(command);

    if (command != EventId_Initialize)
    {
        return false;
    }

    unsigned int function;
    m_eventChannel.ReadUInt32(function);

    // Call the initializtion function.

    char* remoteSymbolsDirectory = RemoteStrDup(m_process, symbolsDirectory);

    DWORD threadId;
    HANDLE thread = CreateRemoteThread(m_process, NULL, 0, (LPTHREAD_START_ROUTINE)function, remoteSymbolsDirectory, 0, &threadId);

    if (thread == NULL)
    {
        return false;
    }

    DWORD exitCode;
    WaitForSingleObject(thread, INFINITE);
    GetExitCodeThread(thread, &exitCode);

    CloseHandle(thread);
    thread = NULL;

    return exitCode != 0;
}

// do we need this thread or can the event pump just be the main program?
DWORD WINAPI DecodaDAP::StaticEventThreadProc(LPVOID param)
{
    DecodaDAP* self = static_cast<DecodaDAP*>(param);
    self->EventThreadProc();
    return 0;

}

void DecodaDAP::EventThreadProc()
{

    unsigned int eventId;

    while (m_eventChannel.ReadUInt32(eventId))
    {

        unsigned int vm;
        m_eventChannel.ReadUInt32(vm);

        //wxDebugEvent event(static_cast<EventId>(eventId), vm);

        if (eventId == EventId_CreateVM)
        {
            unsigned int newThreadId;
            m_eventChannel.ReadUInt32(newThreadId);

            dap::ThreadEvent threadStartedEvent;
            threadStartedEvent.reason = "started";
            threadStartedEvent.threadId = newThreadId;
            session->send(threadStartedEvent);
        }
        else if (eventId == EventId_DestroyVM)
        {
            unsigned int deadThreadId;
            m_eventChannel.ReadUInt32(deadThreadId);

            dap::ThreadEvent threadExitedEvent;
            threadExitedEvent.reason = "exited";
            threadExitedEvent.threadId = deadThreadId;
            session->send(threadExitedEvent);
        }
        else if (eventId == EventId_LoadScript)
        {
            CriticalSectionLock lock(m_criticalSection);

            Script* script = new Script;

            m_eventChannel.ReadString(script->name);
            m_eventChannel.ReadString(script->source);

            unsigned int codeState;
            m_eventChannel.ReadUInt32(codeState);

            script->state = static_cast<CodeState>(codeState);

            // If the debuggee does wacky things when it specifies the file name
            // we need to correct for that or it can make trying to access the
            // file bad.
            script->name = MakeValidFileName(script->name);

            unsigned int scriptIndex = m_scripts.size();
            m_scripts.push_back(script);

            // DAP: Determine if this is a real file
            dap::LoadedSourceEvent loadedEvent;
            loadedEvent.reason = "new";
            loadedEvent.source.name = script->name;

            if (IsRealFile(script->name)) {
                loadedEvent.source.path = script->name;
            }
            else {
                // Assign a unique sourceReference for virtual files
                static int nextSourceRef = 1000;
                loadedEvent.source.sourceReference = nextSourceRef;
                m_virtualSources[nextSourceRef] = script;
                ++nextSourceRef;
            }

            session->send(loadedEvent);
        }
        else if (eventId == EventId_Break)
        {
            m_state = State_Broken;

            unsigned int numStackFrames;
            m_eventChannel.ReadUInt32(numStackFrames);
            m_stackFrames.resize(numStackFrames);

            for (unsigned int i = 0; i < numStackFrames; ++i)
            {
                m_eventChannel.ReadUInt32(m_stackFrames[i].scriptIndex);

                if (m_stackFrames[i].scriptIndex != -1)
                {
                    assert(m_stackFrames[i].scriptIndex < m_scripts.size());
                }

                m_eventChannel.ReadUInt32(m_stackFrames[i].line);
                m_eventChannel.ReadString(m_stackFrames[i].function);
            }

            //if (numStackFrames > 0)
            //{
            //    event.SetScriptIndex(m_stackFrames[0].scriptIndex);
            //    event.SetLine(m_stackFrames[0].line);
            //}

            // DAP: Send stopped event
            dap::StoppedEvent stopped;
            stopped.reason = "breakpoint";
            stopped.threadId = vm;
            session->send(stopped);
        }
        else if (eventId == EventId_SetBreakpoint)
        {
            // TODO figure out if this is used for every breakpoint set or when breakpoints become valid/invalid due to script loading/unloading
            // if it runs when a breakpoint is added/removed than we can NOP this out since DAP handles that via the SetBreakpoints response

            unsigned int scriptIndex;
            m_eventChannel.ReadUInt32(scriptIndex);

            unsigned int line;
            m_eventChannel.ReadUInt32(line);

            unsigned int set;
            m_eventChannel.ReadUInt32(set);

            // Only send if this is an async change (not in direct response to setBreakpoints)
            dap::BreakpointEvent bpEvent;
            bpEvent.reason = "changed"; // or "new" if appropriate
            dap::Breakpoint bp;
            bp.verified = (set != 0);
            bp.line = line;
            // Optionally set bp.source if you have it
            bpEvent.breakpoint = bp;
            session->send(bpEvent);
        }
        else if (eventId == EventId_Exception)
        {
            std::string message;
            m_eventChannel.ReadString(message);

            // Send StoppedEvent with reason "exception"
            dap::StoppedEvent stopped;
            stopped.reason = "exception";
            stopped.description = message; // Optional: VS Code will show this in the UI
            stopped.threadId = vm;
            session->send(stopped);

            // Optionally, send OutputEvent for more details
            dap::OutputEvent output;
            output.category = "stderr";
            output.output = "Exception: " + message + "\n";
            session->send(output);
        }
        else if (eventId == EventId_LoadError)
        {
            std::string message;
            m_eventChannel.ReadString(message);

            // Send OutputEvent to notify the client of the load error
            dap::OutputEvent output;
            output.category = "stderr";
            output.output = "LoadError: " + message + "\n";
            session->send(output);
        }
        else if (eventId == EventId_Message)
        {

            unsigned int type;
            m_eventChannel.ReadUInt32(type);

            std::string message;
            m_eventChannel.ReadString(message);

            switch (type)
            {
            case MessageType_Normal: {
                dap::OutputEvent output;
                output.category = "stdout";
                output.output = message + "\n";
                session->send(output);
                break;
            }
            case MessageType_Warning:
            case MessageType_Error: {
                dap::OutputEvent output;
                output.category = "stderr";
                output.output = message + "\n";
                session->send(output);
                break;
            }
            }
        }
        else if (eventId == EventId_SessionEnd)
        {
            // Backends shouldn't send this.
            assert(0);
            continue;
        }
        else if (eventId == EventId_NameVM)
        {
            std::string message;
            m_eventChannel.ReadString(message);

            // Store the VM name associated with the VM/thread id
            //m_vmNames[vm] = vmName;

            // Send OutputEvent to notify the client of the VM name
            dap::OutputEvent output;
            output.category = "console";
            output.output = "VM Name: " + message + "\n";
            session->send(output);
        }

        // Dispatch the message to the UI.
        //if (m_eventHandler != NULL)
        //{
        //    m_eventHandler->AddPendingEvent(event);
        //}

    }

    // Send the exit event message to the UI.
    //if (m_eventHandler != NULL)
    //{
    //    wxDebugEvent event(static_cast<EventId>(EventId_SessionEnd), 0);
    //    m_eventHandler->AddPendingEvent(event);        
    //}

}

std::string DecodaDAP::MakeValidFileName(const std::string& name)
{
    std::string result;

    for (unsigned int i = 0; i < name.length(); ++i)
    {
        if (name[i] == ':')
        {
            if (i + 1 == name.length() || !GetIsSlash(name[i + 1]))
            {
                result += '_';
                continue;
            }
        }

        result += name[i];

    }

    return result;
}

HWND DecodaDAP::GetProcessWindow(DWORD processId) const
{
    HWND hWnd = FindWindowEx(NULL, NULL, NULL, NULL);
    while (hWnd != NULL)
    {
        if (GetParent(hWnd) == NULL && GetWindowTextLength(hWnd) > 0 && IsWindowVisible(hWnd))
        {
            DWORD windowProcessId;
            GetWindowThreadProcessId(hWnd, &windowProcessId);
            if (windowProcessId == processId)
            {
                // Found a match.
                break;
            }
        }
        hWnd = GetWindow(hWnd, GW_HWNDNEXT);
    }
    return hWnd;
}











void DecodaDAP::Continue(unsigned int vm)
{
    HWND hwnd = GetProcessWindow(m_processId);
    if (::IsWindow(hwnd))
    {
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }

    m_state = State_Running;
    m_commandChannel.WriteUInt32(CommandId_Continue);
    m_commandChannel.WriteUInt32(vm);
    m_commandChannel.Flush();
}

void DecodaDAP::Break(unsigned int vm)
{
    m_commandChannel.WriteUInt32(CommandId_Break);
    m_commandChannel.WriteUInt32(vm);
    m_commandChannel.Flush();
}

void DecodaDAP::StepOver(unsigned int vm)
{
    m_state = State_Running;
    m_commandChannel.WriteUInt32(CommandId_StepOver);
    m_commandChannel.WriteUInt32(vm);
    m_commandChannel.Flush();
}

void DecodaDAP::StepInto(unsigned int vm)
{
    m_state = State_Running;
    m_commandChannel.WriteUInt32(CommandId_StepInto);
    m_commandChannel.WriteUInt32(vm);
    m_commandChannel.Flush();
}

void DecodaDAP::StepOut(unsigned int vm) {
    unsigned int initialDepth = GetNumStackFrames();
    if (initialDepth <= 1) {
        // Already at the top level, nothing to step out of.
        Continue(vm);
        return;
    }
    while (true) {
        StepOver(vm);
        unsigned int currentDepth = GetNumStackFrames();
        if (currentDepth < initialDepth || currentDepth == 0) {
            break;
        }
        // TODO Optionally: add a timeout or max step count to avoid infinite loops.
    }
}

bool DecodaDAP::Evaluate(unsigned int vm, std::string expression, unsigned int stackLevel, std::string& result)
{
    if (vm == 0)
    {
        return false;
    }

    m_commandChannel.WriteUInt32(CommandId_Evaluate);
    m_commandChannel.WriteUInt32(vm);
    m_commandChannel.WriteString(expression);
    m_commandChannel.WriteUInt32(stackLevel);
    m_commandChannel.Flush();

    unsigned int success;
    m_commandChannel.ReadUInt32(success);
    m_commandChannel.ReadString(result);

    return success != 0;
}

void DecodaDAP::ToggleBreakpoint(unsigned int vm, unsigned int scriptIndex, unsigned int line)
{
    m_commandChannel.WriteUInt32(CommandId_ToggleBreakpoint);
    m_commandChannel.WriteUInt32(vm);
    m_commandChannel.WriteUInt32(scriptIndex);
    m_commandChannel.WriteUInt32(line);
    m_commandChannel.Flush();
}

void DecodaDAP::RemoveAllBreakPoints(unsigned int vm)
{
    m_commandChannel.WriteUInt32(CommandId_DeleteAllBreakpoints);
    m_commandChannel.WriteUInt32(0);
    m_commandChannel.Flush();
}

unsigned int DecodaDAP::GetNumStackFrames() const
{
    return m_stackFrames.size();
}

const DecodaDAP::StackFrame& DecodaDAP::GetStackFrame(unsigned int i) const
{
    return m_stackFrames[i];
}










bool DecodaDAP::Start(const char* command, const char* commandArguments, const char* currentDirectory, const char* symbolsDirectory, bool debug, bool startBroken)
{
    //Stop(false);

    STARTUPINFOA startUpInfo = { 0 };
    startUpInfo.cb = sizeof(startUpInfo);

    char commandLine[8191];
    _snprintf(commandLine, sizeof(commandLine), "\"%s\" %s", command, commandArguments);

    // If no directory was specified, then use the directory from the exe.

    std::string directory = TrimSpaces(currentDirectory);

    if (directory.empty())
    {
        directory = GetDirectory(command);
    }

    PROCESS_INFORMATION processInfo;

    if (debug)
    {
        if (!StartProcessAndRunToEntry(command, commandLine, directory.c_str(), processInfo))
        {
            return false;
        }
    }
    else
    {

        if (!CreateProcessA(NULL, commandLine, NULL, NULL, TRUE, 0, NULL, directory.c_str(), &startUpInfo, &processInfo))
        {
            OutputError(GetLastError());
            return false;
        }

        // We're not debugging, so no need to proceed.
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return true;

    }

    DWORD exitCode;

    if (GetExitCodeProcess(processInfo.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
    {
        MessageEvent("The process has terminated unexpectedly", MessageType_Error);
        return false;
    }

    m_process = processInfo.hProcess;
    m_processId = processInfo.dwProcessId;

    if (!InitializeBackend(symbolsDirectory))
    {
        Stop(true);
        return false;
    }

    if (startBroken)
    {
        Break(0);
    }

    // Now that our initialization is complete, let the process run.
    ResumeThread(processInfo.hThread);
    CloseHandle(processInfo.hThread);

    return true;
}

void DecodaDAP::Stop(bool kill)
{
    if (m_state != State_Inactive)
    {
        m_commandChannel.WriteUInt32(CommandId_Detach);
        m_commandChannel.WriteBool(!kill);
        m_commandChannel.Flush();
    }

    // Close the channel. This will cause the thread to exit since reading from the
    // channel will fail. Perhaps a little bit hacky.
    m_eventChannel.Destroy();
    m_commandChannel.Destroy();

    // Store the handle to the process, since when the thread exists it will close the
    // handle.
    HANDLE hProcess = NULL;
    DuplicateHandle(GetCurrentProcess(), m_process, GetCurrentProcess(), &hProcess, 0, TRUE, DUPLICATE_SAME_ACCESS);

    if (m_eventThread != NULL)
    {
        // Wait for the thread to exit.
        WaitForSingleObject(m_eventThread, INFINITE);

        CloseHandle(m_eventThread);
        m_eventThread = NULL;
    }

    if (kill)
    {
        TerminateProcess(hProcess, 0);
    }

    CloseHandle(hProcess);
}





int main(int, char* []) {
#ifdef OS_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    session = dap::Session::create();

    dap::Event configured;
    dap::Event terminate;

    DecodaDAP decoda;

    // Sent first to initialize the debug adapter.
    session->registerHandler([](const dap::InitializeRequest&) {
        dap::InitializeResponse response;
        response.supportsConfigurationDoneRequest = true;
        return response;
    });

    session->registerSentHandler([&](const dap::ResponseOrError<dap::InitializeResponse>&) {
        session->send(dap::InitializedEvent());
    });


    // Start debugging a new process.
    session->registerHandler([&](const dap::LaunchRequest& request)
        -> dap::ResponseOrError<dap::LaunchResponse> {
        // Parse request arguments (customize as needed)
        std::string exe = request.arguments["program"].asString();
        std::string args = request.arguments["args"].asString();
        std::string cwd = request.arguments["cwd"].asString();
        std::string symbols = request.arguments["symbols"].asString();
        bool debug = !request.noDebug.value(false);
        //bool breakOnStart = false; // or from request
        bool breakOnStart = args.contains("breakOnStart") ? args["breakOnStart"].asBool() : false; // custom?

        if (!decoda.Start(exe.c_str(), args.c_str(), cwd.c_str(), symbols.c_str(), debug, breakOnStart)) {
            return dap::Error("Failed to launch target application");
        }
        return dap::LaunchResponse();
    });

    // Attach to an existing process.
    session->registerHandler([&](const dap::AttachRequest& request)
        -> dap::ResponseOrError<dap::AttachResponse> {
        unsigned int pid = request.arguments["processId"].asUInt();
        std::string symbols = request.arguments["symbols"].asString();

        if (!DebugFrontend::Get().Attach(pid, symbols.c_str())) {
            return dap::Error("Failed to attach to process");
        }
        return dap::AttachResponse();
    });

    // End the debug session.
    session->registerHandler([&](const dap::DisconnectRequest& request) {
        if (request.terminateDebuggee.value(false)) {
            terminate.fire();
        }
        return dap::DisconnectResponse();
    });


    // Set breakpoints in a source file.
    session->registerHandler([&](const dap::SetBreakpointsRequest& request) {
        decoda.removeAllBreakpoints();
        for (const auto& bp : request.breakpoints.value({})) {
            unsigned int scriptIndex = 0; // TODO: lookup by file name/path
            decoda.setBreakpoint(scriptIndex, bp.line);
        }
        return dap::SetBreakpointsResponse();
    });

    // Set exception breakpoints.
    // setExceptionBreakpoints

    // Notify that the client has finished configuration (breakpoints, etc.).
    session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
        configured.fire();
        return dap::ConfigurationDoneResponse();
    });
    
    // Request the list of threads (VMs).
    // threads
    session->registerHandler([&](const dap::ThreadsRequest&) {
        dap::ThreadsResponse response;
        unsigned int numVMs = decoda.getNumVMs();
        for (unsigned int i = 0; i < numVMs; ++i) {
            dap::Thread thread;
            thread.id = i + 1; // or get actual thread ID from decoda
            thread.name = "VM " + std::to_string(i + 1);
            response.threads.push_back(thread);
        }
        return response;
    });
    
    // Request the call stack for a thread.
    session->registerHandler([&](const dap::StackTraceRequest& request)
        -> dap::ResponseOrError<dap::StackTraceResponse> {
        dap::StackTraceResponse response;
        unsigned int numFrames = decoda.getNumStackFrames();
        for (unsigned int i = 0; i < numFrames; ++i) {
            auto frame = decoda.getStackFrame(i);
            dap::StackFrame dapFrame;
            dapFrame.id = i + 1;
            dapFrame.name = frame.function;
            dapFrame.line = frame.line;
            dapFrame.source.name = decoda.getScript(frame.scriptIndex)->name;
            dapFrame.source.path = decoda.getScript(frame.scriptIndex)->name;
            response.stackFrames.push_back(dapFrame);
        }
        return response;
    });
    
    // Request the list of variable scopes for a stack frame.
    // scopes
    session->registerHandler([&](const dap::ScopesRequest& request)
        -> dap::ResponseOrError<dap::ScopesResponse> {
            dap::ScopesResponse response;

            // Map DAP frameId to Decoda stack frame index
            unsigned int frameIndex = static_cast<unsigned int>(request.frameId - 1);
            if (frameIndex >= DebugFrontend::Get().GetNumStackFrames()) {
                return dap::Error("Invalid frameId");
            }

            // Locals scope
            dap::Scope locals;
            locals.name = "Locals";
            locals.presentationHint = "locals";
            locals.variablesReference = makeVariablesReference(frameIndex, /*isGlobal=*/false);
            response.scopes.push_back(locals);

            // Globals scope (optional)
            dap::Scope globals;
            globals.name = "Globals";
            globals.presentationHint = "globals";
            globals.variablesReference = makeVariablesReference(frameIndex, /*isGlobal=*/true);
            response.scopes.push_back(globals);

            return response;
        });
    
    session->registerHandler([&](const dap::SetExceptionBreakpointsRequest&) {
        return dap::SetExceptionBreakpointsResponse();
    });
    
    // Request the variables for a scope.
    // variables
    session->registerHandler([&](const dap::VariablesRequest& request)
        -> dap::ResponseOrError<dap::VariablesResponse> {
            dap::VariablesResponse response;
            unsigned int frameIndex;
            bool isGlobal;
            decodeVariablesReference(request.variablesReference, frameIndex, isGlobal);
            if (frameIndex >= decoda.getNumStackFrames()) {
                return dap::Error("Invalid variablesReference");
            }
            std::vector<DebugFrontend::Variable> vars;
            if (isGlobal) {
                decoda.getGlobalVariables(frameIndex, vars);
            } else {
                decoda.getLocalVariables(frameIndex, vars);
            }
            for (const auto& var : vars) {
                dap::Variable dapVar;
                dapVar.name = var.name;
                dapVar.value = var.value;
                dapVar.variablesReference = 0; // or set if var is complex
                response.variables.push_back(dapVar);
            }
            return response;
        });

    // Continue execution.
    session->registerHandler([&](const dap::ContinueRequest&) {
        decoda.continueExec();
        return dap::ContinueResponse();
    });
    
    // Step over.
    session->registerHandler([&](const dap::NextRequest&) {
        decoda.stepOver();
        return dap::NextResponse();
    });
    
    // Step into.
    session->registerHandler([&](const dap::StepInRequest&) {
        decoda.stepInto();
        return dap::StepInResponse();
    });
    
    // Step out.
    // stepOut
    session->registerHandler([&](const dap::StepOutRequest&) {
        // simulate step out with multiple step overs
        decoda.stepOut();
        return dap::StepOutResponse();
    });
    
    // Pause execution.
    session->registerHandler([&](const dap::PauseRequest&) {
        decoda.pauseExec();
        return dap::PauseResponse();
    });
    
    // Request the source code for a file or virtual source.
    // TODO handle unknown source file
    session->registerHandler([&](const dap::SourceRequest& request) -> dap::ResponseOrError<dap::SourceResponse> {
        auto it = m_virtualSources.find(request.sourceReference);
        if (it != m_virtualSources.end()) {
            dap::SourceResponse response;
            response.content = it->second->source;
            return response;
        }
        return dap::Error("Unknown sourceReference");
    });
    
    // Evaluate an expression in the current context.
    // evaluate
    session->registerHandler([&](const dap::EvaluateRequest& request)
        -> dap::ResponseOrError<dap::EvaluateResponse> {
        dap::EvaluateResponse response;
        std::string result;
        if (decoda.evaluateExpression(request.expression, result)) {
            response.result = result;
            response.variablesReference = 0; // or set if result is complex
            return response;
        }
        return dap::Error("Failed to evaluate expression");
    });

    // (Optional) Set breakpoints on function entry.
    // setFunctionBreakpoints

    // (Optional) Restart the debuggee.
    // restart

    // (Optional) Get information about the current exception.
    // exceptionInfo
















    std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
    std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
    session->bind(in, out);

    configured.wait();

    dap::ThreadEvent threadStartedEvent;
    threadStartedEvent.reason = "started";
    threadStartedEvent.threadId = threadId;
    session->send(threadStartedEvent);

    terminate.wait();

    return 0;
}