#pragma once

#include <afx.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Common/XferLoadBuffer.h"
#include "Schemas/GeneralsMDSchema.h"
#include "Lib/BaseType.h"

namespace SnapshotTool {
    struct Property {
        CString name;
        CString value;
        CString type;
    };

    struct GameObject {
        CString name;
        std::vector<Property> properties;
        CString debugInfo;
    };

    struct GameStateSnapshot {
        std::vector<GameObject> objects;
    };

    class GameStateLogic {
    public:
        GameStateLogic();

        ~GameStateLogic();

        void Start();

        void Stop();

        bool LoadSnapshotFromFile(const CString &path, CString &errorMessage);

        bool LoadReplayFromFile(const CString &path, CString &errorMessage);

        bool LoadSnapshot(const std::vector<Byte> &bytes, CString &errorMessage);

        bool SaveStateToFile(const CString &path, CString &errorMessage) const;

        void Clear();

        void OnPipeMessage(const std::vector<Byte> &bytes);

        bool ConsumeState(GameStateSnapshot &out);

    private:
        void EnqueueSnapshot(const std::vector<Byte> &bytes);

        static void SerializeSnapshot(XferLoadBuffer &xfer, SnapshotSchemaView schema,
                                      std::vector<Property> &properties, std::vector<std::string> &warnings,
                                      const std::string &prefix = std::string());

        std::string SerializeStateToText() const;

        bool ParseSnapshot(const std::vector<Byte> &bytes, GameStateSnapshot &outState, CString &errorMessage);

        static void BuildStateFromSerialized(GameStateSnapshot &target, const CString &blockName,
                                             const std::vector<Property> &properties, const CString &debugInfo);

        static std::vector<size_t> FindChunkOffsets(const std::vector<Byte> &bytes);

        void WorkerLoop();

        mutable std::mutex m_stateMutex;
        GameStateSnapshot m_state;
        std::atomic<bool> m_stateDirty{false};

        std::mutex m_queueMutex;
        std::condition_variable m_queueCV;
        std::deque<std::vector<Byte> > m_queue;
        std::thread m_worker;
        std::atomic<bool> m_stop{false};
    };
} // namespace SnapshotTool
