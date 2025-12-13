// #include "PreRTS.h"

#include <windows.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Lib/BaseType.h"
#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
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

std::string serialize(XferLoadBuffer &xfer, SnapshotSchemaView schema) {
    std::ostringstream out;
    bool first = true;
    for (const SnapshotSchemaField &field: schema) {
        if (!first) {
            out << "\n";
        }
        first = false;

        out << field.name << ": ";
        std::string_view type(field.type);
        if (type == "UnsignedByte") {
            UnsignedByte value{};
            xfer.xferUnsignedByte(&value);
            out << static_cast<unsigned>(value);
        } else if (type == "Byte") {
            Byte value{};
            xfer.xferByte(&value);
            out << static_cast<int>(value);
        } else if (type == "Bool") {
            Bool value{};
            xfer.xferBool(&value);
            out << (value ? "true" : "false");
        } else if (type == "Short") {
            Short value{};
            xfer.xferShort(&value);
            out << value;
        } else if (type == "UnsignedShort") {
            UnsignedShort value{};
            xfer.xferUnsignedShort(&value);
            out << value;
        } else if (type == "Int") {
            Int value{};
            xfer.xferInt(&value);
            out << value;
        } else if (type == "UnsignedInt") {
            UnsignedInt value{};
            xfer.xferUnsignedInt(&value);
            out << value;
        } else if (type == "Int64") {
            Int64 value{};
            xfer.xferInt64(&value);
            out << static_cast<long long>(value);
        } else if (type == "Real") {
            Real value{};
            xfer.xferReal(&value);
            out << value;
        } else if (type == "AsciiString") {
            AsciiString value;
            xfer.xferAsciiString(&value);
            out << value.str();
        } else if (type == "UnicodeString") {
            UnicodeString unicodeValue;
            xfer.xferUnicodeString(&unicodeValue);
            AsciiString asciiValue;
            asciiValue.translate(unicodeValue);
            out << asciiValue.str();
        } else if (type == "BlockSize") {
            Int blockSize = xfer.beginBlock();
            out << blockSize;
        } else if (type == "EndBlock") {
            xfer.endBlock();
            out << "<end-block>";
        } else {
            out << "<unknown>";
        }
    }
    return out.str();
}

int main() {
    std::ifstream input("D:/00000016.sav", std::ios::binary);
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
        std::cout << "Block size: " << blockSize << std::endl;

        // serialize block
        if (!SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.contains(token.str())) {
            //BAD
            break;
        }
        SnapshotSchemaView view = SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.at(token.str());
        std::string serialized = serialize(xfer, view);
        std::cout << serialized << std::endl;
        xfer.endBlock();

        break;

        if (token.compareNoCase(SAVE_FILE_EOF) == 0) {
            break;
        }
    }
    xfer.close();

    const std::string chunkTag = "CHUNK_";
    std::vector<int> chunkOffsets;
    for (size_t i = 0; i + chunkTag.size() <= bytes.size(); ++i) {
        if (std::equal(chunkTag.begin(), chunkTag.end(), bytes.begin() + static_cast<long>(i))) {
            chunkOffsets.push_back(static_cast<int>(i - 1));
        }
    }

    for (auto offset: chunkOffsets) {
        xfer.open("buffer", bytes);
        xfer.skip(offset);

        AsciiString value;
        xfer.xferAsciiString(&value);
        std::cout << value.str() << std::endl;

        xfer.close();
    }

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
