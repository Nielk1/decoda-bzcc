#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"
//#include "../Frontend/DebugFrontend.h"
#include "DebugFrontend.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <unordered_set>

#ifdef _MSC_VER
#define OS_WINDOWS 1
#endif

namespace {

    class DecodaDAP {
    public:
        DecodaDAP() : m_vm(0) {}

        bool launch(const char* exe, const char* args, const char* cwd, const char* symbols, bool debug, bool breakOnStart) {
            return DebugFrontend::Get().Start(exe, args, cwd, symbols, debug, breakOnStart);
        }

        void continueExec() {
            DebugFrontend::Get().Continue(m_vm);
        }

        void pauseExec() {
            DebugFrontend::Get().Break(m_vm);
        }

        void stepOver() {
            DebugFrontend::Get().StepOver(m_vm);
        }

        void stepInto() {
            DebugFrontend::Get().StepInto(m_vm);
        }

        void setBreakpoint(unsigned int scriptIndex, unsigned int line) {
            DebugFrontend::Get().ToggleBreakpoint(m_vm, scriptIndex, line);
        }

        void removeAllBreakpoints() {
            DebugFrontend::Get().RemoveAllBreakPoints(m_vm);
        }

        unsigned int getNumStackFrames() {
            return DebugFrontend::Get().GetNumStackFrames();
        }

        DebugFrontend::StackFrame getStackFrame(unsigned int i) {
            return DebugFrontend::Get().GetStackFrame(i);
        }

        DebugFrontend::Script* getScript(unsigned int scriptIndex) {
            return DebugFrontend::Get().GetScript(scriptIndex);
        }

        unsigned int m_vm; // For now, always 0
    };

}  // anonymous namespace

int main(int, char* []) {
#ifdef OS_WINDOWS
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    auto session = dap::Session::create();

    const dap::integer threadId = 100;
    const dap::integer frameId = 200;
    const dap::integer variablesReferenceId = 300;
    const dap::integer sourceReferenceId = 400;

    dap::Event configured;
    dap::Event terminate;

    DecodaDAP decoda;

    session->registerHandler([](const dap::InitializeRequest&) {
        dap::InitializeResponse response;
        response.supportsConfigurationDoneRequest = true;
        return response;
    });

    session->registerSentHandler(
        [&](const dap::ResponseOrError<dap::InitializeResponse>&) {
            session->send(dap::InitializedEvent());
        });

    session->registerHandler([&](const dap::ThreadsRequest&) {
        dap::ThreadsResponse response;
        dap::Thread thread;
        thread.id = threadId;
        thread.name = "TheThread";
        response.threads.push_back(thread);
        return response;
    });

    session->registerHandler([&](const dap::ScopesRequest& request)
        -> dap::ResponseOrError<dap::ScopesResponse> {
            if (request.frameId != frameId) {
                return dap::Error("Unknown frameId '%d'", int(request.frameId));
            }

            dap::Scope scope;
            scope.name = "Locals";
            scope.presentationHint = "locals";
            scope.variablesReference = variablesReferenceId;

            dap::ScopesResponse response;
            response.scopes.push_back(scope);
            return response;
        });

    session->registerHandler([&](const dap::SetExceptionBreakpointsRequest&) {
        return dap::SetExceptionBreakpointsResponse();
    });

    session->registerHandler([&](const dap::LaunchRequest& request)->dap::ResponseOrError<dap::LaunchResponse> {
        // Parse request arguments (customize as needed)
        std::string exe = request.arguments["program"].asString();
        std::string args = request.arguments["args"].asString();
        std::string cwd = request.arguments["cwd"].asString();
        std::string symbols = request.arguments["symbols"].asString();
        bool debug = !request.noDebug.value(false);
        //bool breakOnStart = false; // or from request
        bool breakOnStart = args.contains("breakOnStart") ? args["breakOnStart"].asBool() : false; // custom?

        if (!decoda.launch(exe.c_str(), args.c_str(), cwd.c_str(), symbols.c_str(), debug, breakOnStart)) {
            return dap::Error("Failed to launch target application");
        }
        return dap::LaunchResponse();
    });

    session->registerHandler([&](const dap::AttachRequest& request)->dap::ResponseOrError<dap::AttachResponse> {
        unsigned int pid = request.arguments["processId"].asUInt();
        std::string symbols = request.arguments["symbols"].asString();

        if (!DebugFrontend::Get().Attach(pid, symbols.c_str())) {
            return dap::Error("Failed to attach to process");
        }
        return dap::AttachResponse();
    });

    session->registerHandler([&](const dap::ContinueRequest&) {
        decoda.continueExec();
        return dap::ContinueResponse();
    });

    session->registerHandler([&](const dap::PauseRequest&) {
        decoda.pauseExec();
        return dap::PauseResponse();
    });

    session->registerHandler([&](const dap::NextRequest&) {
        decoda.stepOver();
        return dap::NextResponse();
    });

    session->registerHandler([&](const dap::StepInRequest&) {
        decoda.stepInto();
        return dap::StepInResponse();
    });

    session->registerHandler([&](const dap::SetBreakpointsRequest& request) {
        decoda.removeAllBreakpoints();
        for (const auto& bp : request.breakpoints.value({})) {
            unsigned int scriptIndex = 0; // TODO: lookup by file name/path
            decoda.setBreakpoint(scriptIndex, bp.line);
        }
        return dap::SetBreakpointsResponse();
    });

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

    session->registerHandler([&](const dap::DisconnectRequest& request) {
        if (request.terminateDebuggee.value(false)) {
            terminate.fire();
        }
        return dap::DisconnectResponse();
    });

    session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
        configured.fire();
        return dap::ConfigurationDoneResponse();
    });

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