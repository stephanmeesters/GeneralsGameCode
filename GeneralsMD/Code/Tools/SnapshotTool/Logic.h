#pragma once

#include <afx.h>
#include <string>
#include <vector>

#include "Common/XferLoadBuffer.h"
#include "Schemas/GeneralsMDSchema.h"
#include "Lib/BaseType.h"

struct Property
{
    CString name;
    CString value;
};

struct GameObject
{
    CString name;
    std::vector<Property> properties;
};

struct GameStateSnapshot
{
    std::vector<GameObject> objects;
};

class GameStateLogic
{
public:
    GameStateLogic() = default;

    const GameStateSnapshot &GetState() const { return m_state; }
    bool LoadSnapshotFromFile(const CString &path, CString &errorMessage);
    void Clear();

private:

    static std::string SerializeSnapshot(XferLoadBuffer &xfer, SnapshotSchemaView schema);
    static void BuildStateFromSerialized(GameStateSnapshot &target, const CString &blockName, const std::string &serialized);
    static std::vector<size_t> FindChunkOffsets(const std::vector<Byte> &bytes);

    GameStateSnapshot m_state;
};
