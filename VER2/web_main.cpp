#define INTEGRATED_MAIN_NO_MAIN
#include "integrated_main.cpp"
#include "local_web.h"

#include <windows.h>

namespace {
BOOL WINAPI onConsoleClose(DWORD) {
    auditLog("SYSTEM_SHUTDOWN", "Web interface stopped");
    return FALSE;
}
}

int main() {
    try {
        AppState app;
        std::string error;
        if (loadApp(app, error) != 0) {
            std::cerr << "Startup error: " << error << '\n';
            return 1;
        }
        auditLog("SYSTEM_STARTED", "Web interface started");
        SetConsoleCtrlHandler(onConsoleClose, TRUE);
        return serveLocalWeb([&](const std::string& method, const std::string& path,
                                 const std::string& body) {
            return handleWebApi(app, method, path, body);
        });
    } catch (const std::exception& error) {
        std::cerr << "Fatal application error: " << error.what() << '\n';
        return 1;
    }
}
