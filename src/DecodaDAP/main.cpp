#include "dap/session.h"
#include "Frontend/DebugFrontend.h"

int main(int argc, char* argv[]) {
    // Initialize the DebugFrontend
    DebugFrontend& frontend = DebugFrontend::Get();

    // Create the DAP session
    auto session = dap::Session::create();

    // Register handlers for DAP requests
    // (Handlers would be defined in DecodaDAP.cpp)

    // Start the DAP server
    session->bind(dap::file(stdin, false), dap::file(stdout, false));

    // Main event loop
    while (true) {
        // Process incoming requests
        session->process();
    }

    // Cleanup
    DebugFrontend::Destroy();
    return 0;
}