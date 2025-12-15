#include "Pipe.h"

#include <winsock2.h>
#include <windows.h>

#include "Logic.h"

namespace SnapshotTool {
    static const wchar_t kPipeName[] = L"\\\\.\\pipe\\SnapshotToolPipe";

    SnapshotPipeServer::SnapshotPipeServer(GameStateLogic &logic)
        : m_logic(logic) {
    }

    SnapshotPipeServer::~SnapshotPipeServer() {
        Stop();
    }

    void SnapshotPipeServer::Start(bool initiallyActive) {
        m_stop = false;
        m_recording = initiallyActive;
        m_thread = std::thread([this]() { Run(); });
    }

    void SnapshotPipeServer::Stop() {
        m_stop = true;
        WakePipe();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void SnapshotPipeServer::SetRecording(bool active) {
        m_recording = active;
    }

    void SnapshotPipeServer::WakePipe() {
        HANDLE pipe = CreateFileW(
            kPipeName,
            GENERIC_READ,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe);
        }
    }

    void SnapshotPipeServer::Run() {
        while (!m_stop) {
            HANDLE pipe = CreateNamedPipeW(
                kPipeName,
                PIPE_ACCESS_INBOUND,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,
                0,
                0,
                0,
                nullptr);
            if (pipe == INVALID_HANDLE_VALUE) {
                return;
            }

            BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (connected && !m_stop) {
                std::vector<Byte> buffer;
                Byte chunk[4096];
                DWORD bytesRead = 0;
                while (!m_stop && ReadFile(pipe, chunk, sizeof(chunk), &bytesRead, nullptr) && bytesRead > 0) {
                    buffer.insert(buffer.end(), chunk, chunk + bytesRead);
                }

                if (!buffer.empty() && m_recording) {
                    m_logic.OnPipeMessage(buffer);
                }
            }

            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
    }
} // namespace SnapshotTool
