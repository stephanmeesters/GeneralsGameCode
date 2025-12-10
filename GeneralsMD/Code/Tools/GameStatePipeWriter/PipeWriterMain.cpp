#include <windows.h>
#include <cstring>
#include <iostream>

int main()
{
    constexpr char kPipeName[] = R"(\\.\pipe\my_pipe)";
    constexpr char kMessage[] = "Hello World";

    std::cout << "Waiting for pipe server at " << kPipeName << "..." << std::endl;
    if (!WaitNamedPipeA(kPipeName, 5000))
    {
        std::cerr << "WaitNamedPipeA failed: " << GetLastError() << std::endl;
        return 1;
    }

    HANDLE pipe = CreateFileA(
        kPipeName,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        std::cerr << "CreateFileA failed: " << GetLastError() << std::endl;
        return 1;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(pipe, kMessage, static_cast<DWORD>(strlen(kMessage)), &bytesWritten, nullptr))
    {
        std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
        CloseHandle(pipe);
        return 1;
    }

    FlushFileBuffers(pipe);
    CloseHandle(pipe);

    std::cout << "Sent \"" << kMessage << "\" (" << bytesWritten << " bytes)." << std::endl;
    return 0;
}
