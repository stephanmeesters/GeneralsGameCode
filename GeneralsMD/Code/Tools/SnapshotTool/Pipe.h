#pragma once

#include <atomic>
#include <thread>

namespace SnapshotTool {
    class GameStateLogic;

    class SnapshotPipeServer {
    public:
        SnapshotPipeServer(GameStateLogic &logic);

        ~SnapshotPipeServer();

        void Start(bool initiallyActive);

        void Stop();

        void SetRecording(bool active);

    private:
        void Run();

        void WakePipe();

        GameStateLogic &m_logic;
        std::atomic<bool> m_stop{false};
        std::atomic<bool> m_recording{false};
        std::thread m_thread;
    };
} // namespace SnapshotTool
