#include "Logic.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <vector>
#include <string_view>
#include <string>
#include <unordered_map>
#include <windows.h>
#include <tchar.h>
#include <thread>

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"

namespace SnapshotTool {
    GameStateLogic::GameStateLogic() = default;

    GameStateLogic::~GameStateLogic() {
        Stop();
    }

    void GameStateLogic::Start() {
        if (m_worker.joinable()) {
            return;
        }
        m_stop = false;
        m_worker = std::thread([this]() { WorkerLoop(); });
    }

    void GameStateLogic::Stop() {
        m_stop = true;
        m_queueCV.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    bool GameStateLogic::LoadSnapshotFromFile(const CString &path, CString &errorMessage) {
        CStringA pathA(path);
        std::ifstream input(pathA.GetString(), std::ios::binary);
        if (!input) {
            errorMessage = _T("Failed to open file.");
            return false;
        }

        std::vector<Byte> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        input.close();
        if (bytes.empty()) {
            errorMessage = _T("File is empty.");
            return false;
        }

        EnqueueSnapshot(bytes);
        return true;
    }

    bool GameStateLogic::LoadReplayFromFile(const CString &path, CString &errorMessage) {
        const DWORD replayAttrs = GetFileAttributes(path);
        if (replayAttrs == INVALID_FILE_ATTRIBUTES || (replayAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            errorMessage.Format(_T("Replay file not found: %s"), path.GetString());
            return false;
        }

        CString replayName = path;
        int slash = replayName.ReverseFind(_T('\\'));
        int fslash = replayName.ReverseFind(_T('/'));
        int cut = (std::max)(slash, fslash);
        if (cut >= 0) {
            replayName = replayName.Mid(cut + 1);
        }

        TCHAR modulePath[MAX_PATH] = {0};
        DWORD len = GetModuleFileName(nullptr, modulePath, MAX_PATH);
        if (len == 0 || len == MAX_PATH) {
            errorMessage = _T("Failed to locate current executable directory.");
            return false;
        }
        CString exeDir(modulePath);
        int lastSlash = exeDir.ReverseFind(_T('\\'));
        int lastForwardSlash = exeDir.ReverseFind(_T('/'));
        int dirCut = (std::max)(lastSlash, lastForwardSlash);
        if (dirCut >= 0) {
            exeDir = exeDir.Left(dirCut + 1);
        }
        CString exePath = exeDir + _T("generalszh.exe");

        const DWORD exeAttrs = GetFileAttributes(exePath);
        if (exeAttrs == INVALID_FILE_ATTRIBUTES || (exeAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            errorMessage.Format(_T("Expected Generals executable at \"%s\""), exePath.GetString());
            return false;
        }

        CString cmdLine;
        cmdLine.Format(_T("\"%s\" -jobs 4 -headless -replay \"%s\""), exePath.GetString(), replayName.GetString());

        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        LPTSTR cmdBuffer = cmdLine.GetBuffer();
        BOOL launched = CreateProcess(
            nullptr,
            cmdBuffer,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            exeDir.IsEmpty() ? nullptr : exeDir.GetString(),
            &si,
            &pi);
        cmdLine.ReleaseBuffer();

        if (!launched) {
            DWORD err = GetLastError();
            errorMessage.Format(_T("Failed to launch \"%s\" (error %lu)."), exePath.GetString(),
                                static_cast<unsigned long>(err));
            return false;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }

    void GameStateLogic::EnqueueSnapshot(const std::vector<Byte> &bytes) {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_queue.push_back(bytes);
        }
        m_queueCV.notify_one();
    }

    bool GameStateLogic::LoadSnapshot(const std::vector<Byte> &bytes, CString &errorMessage) {
        EnqueueSnapshot(bytes);
        return true;
    }

    bool GameStateLogic::SaveStateToFile(const CString &path, CString &errorMessage) const {
        CStringA pathA(path);
        std::ofstream output(pathA.GetString(), std::ios::binary);
        if (!output) {
            errorMessage = _T("Failed to open file for writing.");
            return false;
        }

        output << SerializeStateToText();
        if (!output.good()) {
            errorMessage = _T("Failed to write snapshot text.");
            return false;
        }

        return true;
    }

    void GameStateLogic::Clear() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_state.objects.clear();
        m_stateDirty = true;
    }

    void GameStateLogic::OnPipeMessage(const std::vector<Byte> &bytes) {
        EnqueueSnapshot(bytes);
    }

    bool GameStateLogic::ConsumeState(GameStateSnapshot &out) {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (!m_stateDirty) {
            return false;
        }
        out = m_state;
        m_stateDirty = false;
        return true;
    }

    std::vector<size_t> GameStateLogic::FindChunkOffsets(const std::vector<Byte> &bytes) {
        const std::string chunkTag = "CHUNK_";
        std::vector<size_t> chunkOffsets;
        for (size_t i = 0; i + chunkTag.size() <= bytes.size(); ++i) {
            if (std::equal(chunkTag.begin(), chunkTag.end(), bytes.begin() + static_cast<long>(i))) {
                chunkOffsets.push_back(i > 0 ? i - 1 : i);
            }
        }
        return chunkOffsets;
    }

    namespace {
        struct LoopFrame {
            size_t bodyStart;
            size_t endIndex;
            long long remaining;
            long long total;
            std::string counterName;
            long long currentIndex;
        };

        size_t findLoopEnd(SnapshotSchemaView schema, size_t startIndex) {
            size_t depth = 0;
            for (size_t i = startIndex + 1; i < schema.size(); ++i) {
                std::string_view loopType(schema[i].type);
                if (loopType == "LoopStart") {
                    ++depth;
                } else if (loopType == "LoopEnd") {
                    if (depth == 0) {
                        return i;
                    }
                    --depth;
                }
            }
            return schema.size();
        }

        std::string buildPropertyName(const std::string &prefix, const std::vector<LoopFrame> &loopStack,
                                      const std::string &fieldName) {
            std::ostringstream oss;
            for (const LoopFrame &frame: loopStack) {
                oss << frame.counterName << "[" << frame.currentIndex << "].";
            }
            if (!prefix.empty()) {
                oss << prefix;
                if (prefix.back() != '.') {
                    oss << ".";
                }
            }
            oss << fieldName;
            return oss.str();
        }

        void SerializeSnapshotInternal(XferLoadBuffer &xfer, SnapshotSchemaView schema,
                                       std::vector<Property> &properties,
                                       std::vector<std::string> &warnings,
                                       const std::string &prefix,
                                       std::unordered_map<std::string, long long> &numericValues,
                                       std::vector<LoopFrame> &loopStack) {
            size_t index = 0;
            while (index < schema.size()) {
                const SnapshotSchemaField &field = schema[index];
                const std::string_view type(field.type);
                const std::string fieldName = field.name ? field.name : "";

                if (type == "LoopStart") {
                    const size_t endIndex = findLoopEnd(schema, index);
                    if (endIndex == schema.size()) {
                        std::ostringstream warn;
                        warn << "Unmatched LoopStart for '" << fieldName << "'";
                        warnings.push_back(warn.str());
                        break;
                    }

                    long long count = 0;
                    const auto foundCount = numericValues.find(fieldName);
                    if (foundCount != numericValues.end()) {
                        count = foundCount->second;
                    } else {
                        std::ostringstream warn;
                        warn << "LoopStart for '" << fieldName << "' has no recorded count value";
                        warnings.push_back(warn.str());
                    }

                    if (count <= 0) {
                        index = endIndex + 1;
                        continue;
                    }

                    loopStack.push_back({index + 1, endIndex, count, count, fieldName, 0});
                    index = index + 1;
                    continue;
                }

                if (type == "LoopEnd") {
                    if (loopStack.empty()) {
                        warnings.push_back("Encountered LoopEnd without matching LoopStart");
                        ++index;
                        continue;
                    }

                    LoopFrame &frame = loopStack.back();
                    frame.remaining -= 1;
                    if (frame.remaining > 0) {
                        frame.currentIndex += 1;
                        index = frame.bodyStart;
                    } else {
                        loopStack.pop_back();
                        ++index;
                    }
                    continue;
                }

                const std::string propertyName = buildPropertyName(prefix, loopStack, fieldName);
                std::ostringstream valueStream;
                bool handled = true;

                auto recordNumeric = [&](long long value) {
                    if (!fieldName.empty()) {
                        numericValues[fieldName] = value;
                    }
                };

                const auto nestedSchema = SnapshotSchema::SCHEMAS.find(std::string(type));
                if (nestedSchema != SnapshotSchema::SCHEMAS.end()) {
                    std::vector<Property> nestedProps;
                    std::string nestedPrefix = prefix.empty() ? fieldName : prefix + "." + fieldName;
                    std::unordered_map<std::string, long long> nestedNumericValues;
                    SerializeSnapshotInternal(xfer, nestedSchema->second, nestedProps, warnings, nestedPrefix,
                                              nestedNumericValues, loopStack);
                    properties.insert(properties.end(),
                                      std::make_move_iterator(nestedProps.begin()),
                                      std::make_move_iterator(nestedProps.end()));
                    handled = false;
                } else if (type == "UnsignedByte") {
                    UnsignedByte v{};
                    xfer.xferUnsignedByte(&v);
                    valueStream << static_cast<unsigned>(v);
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "Byte") {
                    Byte v{};
                    xfer.xferByte(&v);
                    valueStream << static_cast<int>(v);
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "Bool") {
                    Bool v{};
                    xfer.xferBool(&v);
                    valueStream << (v ? "true" : "false");
                    recordNumeric(v ? 1 : 0);
                } else if (type == "Short") {
                    Short v{};
                    xfer.xferShort(&v);
                    valueStream << v;
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "UnsignedShort") {
                    UnsignedShort v{};
                    xfer.xferUnsignedShort(&v);
                    valueStream << v;
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "Int") {
                    Int v{};
                    xfer.xferInt(&v);
                    valueStream << v;
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "UnsignedInt") {
                    UnsignedInt v{};
                    xfer.xferUnsignedInt(&v);
                    valueStream << v;
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "Int64") {
                    Int64 v{};
                    xfer.xferInt64(&v);
                    valueStream << static_cast<long long>(v);
                    recordNumeric(static_cast<long long>(v));
                } else if (type == "Real") {
                    Real v{};
                    xfer.xferReal(&v);
                    valueStream << v;
                } else if (type == "AsciiString") {
                    AsciiString v;
                    xfer.xferAsciiString(&v);
                    valueStream << v.str();
                } else if (type == "UnicodeString") {
                    UnicodeString unicodeValue;
                    xfer.xferUnicodeString(&unicodeValue);
                    AsciiString asciiValue;
                    asciiValue.translate(unicodeValue);
                    valueStream << asciiValue.str();
                } else if (type == "BlockSize") {
                    Int blockSize = xfer.beginBlock();
                    valueStream << blockSize;
                    recordNumeric(static_cast<long long>(blockSize));
                } else if (type == "EndBlock") {
                    xfer.endBlock();
                    valueStream << "<end-block>";
                } else {
                    valueStream << "<unknown>";
                    std::ostringstream warning;
                    warning << "Unknown field type '" << type << "' for field '" << fieldName << "'";
                    warnings.push_back(warning.str());
                    handled = false;
                }

                if (handled) {
                    Property prop;
                    prop.name = CString(propertyName.c_str());
                    prop.type = CString(field.type);
                    CStringA valueA(valueStream.str().c_str());
                    prop.value = CString(valueA);
                    properties.push_back(prop);
                }

                ++index;
            }
        }
    } // namespace

    void GameStateLogic::SerializeSnapshot(XferLoadBuffer &xfer, SnapshotSchemaView schema,
                                           std::vector<Property> &properties,
                                           std::vector<std::string> &warnings,
                                           const std::string &prefix) {
        std::unordered_map<std::string, long long> numericValues;
        std::vector<LoopFrame> loopStack;
        SerializeSnapshotInternal(xfer, schema, properties, warnings, prefix, numericValues, loopStack);
    }

    bool GameStateLogic::ParseSnapshot(const std::vector<Byte> &bytes, GameStateSnapshot &outState,
                                       CString &errorMessage) {
        const std::vector<size_t> chunkOffsets = FindChunkOffsets(bytes);

        GameStateSnapshot loaded;
        for (size_t offset: chunkOffsets) {
            XferLoadBuffer xfer;
            xfer.open(_T("save"), bytes);
            xfer.skip(static_cast<Int>(offset));

            AsciiString token;
            xfer.xferAsciiString(&token);
            if (token.isEmpty()) {
                xfer.close();
                continue;
            }

            int blockSize = xfer.beginBlock();
            const size_t blockStart = xfer.tell();

            auto it = SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.find(token.str());
            if (it != SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.end()) {
                std::vector<std::string> warnings;
                std::vector<Property> properties;
                SerializeSnapshot(xfer, it->second, properties, warnings);
                const size_t blockEnd = xfer.tell();
                const size_t consumedBytes = blockEnd >= blockStart ? blockEnd - blockStart : 0;
                const size_t expectedBytes = blockSize >= 0 ? static_cast<size_t>(blockSize) : 0;

                if (expectedBytes != consumedBytes) {
                    std::ostringstream mismatch;
                    mismatch << "Block size mismatch: expected " << expectedBytes << " bytes, parsed "
                            << consumedBytes;
                    warnings.push_back(mismatch.str());
                }

                CString debugInfo;
                debugInfo.Format(
                    _T("Expected block bytes: %d\r\nProcessed bytes: %zu\r\nMatch: %s"),
                    blockSize,
                    consumedBytes,
                    expectedBytes == consumedBytes ? _T("yes") : _T("NO"));
                if (!warnings.empty()) {
                    debugInfo += _T("\r\nWarnings:");
                    for (const std::string &warning: warnings) {
                        CStringA warningA(warning.c_str());
                        debugInfo += _T("\r\n- ");
                        debugInfo += CString(warningA);
                    }
                }

                BuildStateFromSerialized(loaded, CString(it->first.data()), properties, debugInfo);
            } else {
                xfer.skip(blockSize);
            }
            xfer.close();
        }

        outState = loaded;
        return true;
    }

    void GameStateLogic::BuildStateFromSerialized(GameStateSnapshot &target, const CString &blockName,
                                                  const std::vector<Property> &properties, const CString &debugInfo) {
        GameObject object;
        object.name = blockName;
        object.debugInfo = debugInfo;
        object.properties = properties;
        target.objects.push_back(std::move(object));
    }

    std::string GameStateLogic::SerializeStateToText() const {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        std::ostringstream out;
        for (const GameObject &obj: m_state.objects) {
            out << obj.name.GetString() << "\n";
            for (const Property &prop: obj.properties) {
                out << "  " << prop.name.GetString() << ": " << prop.value.GetString() << "\n";
            }
            out << "\n";
        }
        return out.str();
    }

    void GameStateLogic::WorkerLoop() {
        while (true) {
            std::vector<Byte> payload;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCV.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
                if (m_stop && m_queue.empty()) {
                    break;
                }
                payload = std::move(m_queue.front());
                m_queue.pop_front();
            }

            GameStateSnapshot parsed;
            CString error;
            if (ParseSnapshot(payload, parsed, error)) {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = std::move(parsed);
                m_stateDirty = true;
            }
        }
    }
} // namespace SnapshotTool
