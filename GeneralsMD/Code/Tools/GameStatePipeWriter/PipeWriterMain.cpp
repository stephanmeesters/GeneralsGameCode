// #include "PreRTS.h"

#include <windows.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include "Common/XferLoadBuffer.h"

#include "GeneratedSnapshotSchema.h"

// Globals expected by GameEngine/GameText when linking without the full game bootstrap.
HINSTANCE ApplicationHInstance = NULL;
HWND ApplicationHWnd = NULL;
const Char *g_strFile = "data\\Generals.str";
const Char *g_csfFile = "data\\%s\\Generals.csf";

extern "C" char *TheCurrentIgnoreCrashPtr = nullptr;

extern "C" void DebugCrash(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

std::string serialize(const SnapshotSchemaField& field) {

}

int main() {
    std::ifstream input("d:\\buffer.b", std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open buffer file" << std::endl;
        return 1;
    }

    std::vector bytes(
        (std::istreambuf_iterator(input)),
        (std::istreambuf_iterator<char>()));
    input.close();

    static const Char *SAVE_FILE_EOF = "SG_EOF";

    XferLoadBuffer xfer;
    xfer.open("buffer", bytes);
    while (true) {
        AsciiString token;
        xfer.xferAsciiString(&token);
        std::cout << token.str() << std::endl;

        // assume block
        int blockSize = xfer.beginBlock(); // size not used in load?

        // serialize

        if (token.compareNoCase(SAVE_FILE_EOF) == 0) {
            break;
        }
    }
    xfer.close();

    return 0;

    // constexpr char kPipeName[] = R"(\\.\pipe\my_pipe)";
    // constexpr char kMessage[] = "Hello World";
    //
    // std::cout << "Waiting for pipe server at " << kPipeName << "..." << std::endl;
    // if (!WaitNamedPipeA(kPipeName, 5000))
    // {
    //     std::cerr << "WaitNamedPipeA failed: " << GetLastError() << std::endl;
    //     return 1;
    // }
    //
    // HANDLE pipe = CreateFileA(
    //     kPipeName,
    //     GENERIC_WRITE,
    //     0,
    //     nullptr,
    //     OPEN_EXISTING,
    //     FILE_ATTRIBUTE_NORMAL,
    //     nullptr);
    // if (pipe == INVALID_HANDLE_VALUE)
    // {
    //     std::cerr << "CreateFileA failed: " << GetLastError() << std::endl;
    //     return 1;
    // }
    //
    // DWORD bytesWritten = 0;
    // if (!WriteFile(pipe, kMessage, static_cast<DWORD>(strlen(kMessage)), &bytesWritten, nullptr))
    // {
    //     std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
    //     CloseHandle(pipe);
    //     return 1;
    // }
    //
    // FlushFileBuffers(pipe);
    // CloseHandle(pipe);
    //
    // std::cout << "Sent \"" << kMessage << "\" (" << bytesWritten << " bytes)." << std::endl;
    // return 0;
}
