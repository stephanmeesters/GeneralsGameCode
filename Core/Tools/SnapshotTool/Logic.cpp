#include "Logic.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <tchar.h>
#include <zconf.h>

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "Lib/BaseTypeCore.h"

bool GameStateLogic::LoadSnapshotFromFile(const CString &path, CString &errorMessage)
{
    Clear();

    CStringA pathA(path);
    std::ifstream input(pathA.GetString(), std::ios::binary);
    if (!input)
    {
        errorMessage = _T("Failed to open file.");
        return false;
    }

    std::vector<Byte> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    if (bytes.empty())
    {
        errorMessage = _T("File is empty.");
        return false;
    }

    const std::vector<size_t> chunkOffsets = FindChunkOffsets(bytes);

    GameStateSnapshot loaded;
    for (size_t offset : chunkOffsets)
    {
        XferLoadBuffer xfer;
        xfer.open(_T("save"), bytes);
        xfer.skip(static_cast<Int>(offset));

        AsciiString token;
        xfer.xferAsciiString(&token);
        if (token.isEmpty())
        {
            xfer.close();
            continue;
        }

        int blockSize = xfer.beginBlock();

        auto it = SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.find(token.str());
        if (it != SnapshotSchema::SNAPSHOT_BLOCK_SCHEMAS.end())
        {
            std::string serialized = SerializeSnapshot(xfer, it->second);
            BuildStateFromSerialized(loaded, CString(it->first.data()), serialized);
        }
        else
        {
            xfer.skip(blockSize);
        }
        xfer.close();

        // if (token.compareNoCase(SAVE_FILE_EOF) == 0)
        // {
        //     break;
        // }
    }

    m_state = loaded;
    return true;
}

void GameStateLogic::Clear()
{
    m_state.objects.clear();
}

std::vector<size_t> GameStateLogic::FindChunkOffsets(const std::vector<Byte> &bytes) {
    const std::string chunkTag = "CHUNK_";
    std::vector<size_t> chunkOffsets;
    for (size_t i = 0; i + chunkTag.size() <= bytes.size(); ++i)
    {
        if (std::equal(chunkTag.begin(), chunkTag.end(), bytes.begin() + static_cast<long>(i)))
        {
            chunkOffsets.push_back(i > 0 ? i - 1 : i);
        }
    }
    return chunkOffsets;
}

std::string GameStateLogic::SerializeSnapshot(XferLoadBuffer &xfer, SnapshotSchemaView schema)
{
    std::ostringstream out;
    bool first = true;
    for (const SnapshotSchemaField &field : schema)
    {
        if (!first)
        {
            out << "\n";
        }
        first = false;

        out << field.name << ": ";
        std::string_view type(field.type);
        if (type == "UnsignedByte")
        {
            UnsignedByte value{};
            xfer.xferUnsignedByte(&value);
            out << static_cast<unsigned>(value);
        }
        else if (type == "Byte")
        {
            Byte value{};
            xfer.xferByte(&value);
            out << static_cast<int>(value);
        }
        else if (type == "Bool")
        {
            Bool value{};
            xfer.xferBool(&value);
            out << (value ? "true" : "false");
        }
        else if (type == "Short")
        {
            Short value{};
            xfer.xferShort(&value);
            out << value;
        }
        else if (type == "UnsignedShort")
        {
            UnsignedShort value{};
            xfer.xferUnsignedShort(&value);
            out << value;
        }
        else if (type == "Int")
        {
            Int value{};
            xfer.xferInt(&value);
            out << value;
        }
        else if (type == "UnsignedInt")
        {
            UnsignedInt value{};
            xfer.xferUnsignedInt(&value);
            out << value;
        }
        else if (type == "Int64")
        {
            Int64 value{};
            xfer.xferInt64(&value);
            out << static_cast<long long>(value);
        }
        else if (type == "Real")
        {
            Real value{};
            xfer.xferReal(&value);
            out << value;
        }
        else if (type == "AsciiString")
        {
            AsciiString value;
            xfer.xferAsciiString(&value);
            out << value.str();
        }
        else if (type == "UnicodeString")
        {
            UnicodeString unicodeValue;
            xfer.xferUnicodeString(&unicodeValue);
            AsciiString asciiValue;
            asciiValue.translate(unicodeValue);
            out << asciiValue.str();
        }
        else if (type == "BlockSize")
        {
            Int blockSize = xfer.beginBlock();
            out << blockSize;
        }
        else if (type == "EndBlock")
        {
            xfer.endBlock();
            out << "<end-block>";
        }
        else
        {
            out << "<unknown>";
        }
    }
    return out.str();
}

void GameStateLogic::BuildStateFromSerialized(GameStateSnapshot &target, const CString &blockName, const std::string &serialized)
{
    GameObject object;
    object.name = blockName;

    std::istringstream lines(serialized);
    std::string line;
    while (std::getline(lines, line))
    {
        CStringA lineA(line.c_str());
        int colon = lineA.Find(':');
        if (colon == -1)
        {
            continue;
        }
        CString name = CString(lineA.Left(colon)).Trim();
        CString value = CString(lineA.Mid(colon + 1)).Trim();
        object.properties.push_back(Property{name, value});
    }

    target.objects.push_back(object);
}
