/*
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "Precompiled/BasePrecomp.h"
#include "WWLib/always.h"
#include "BigTextureFileFactory.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

namespace
{
// RawFileClass::Raw_Seek in this tree returns fseek's status, which breaks
// biased entry streams (Read queries Seek(0) before every read). Keep the
// correction local to this tool, and report entry-relative file positions.
class BigEntryFile : public RawFileClass
{
public:
	BigEntryFile(const char* archive, int offset, int size) : RawFileClass(archive)
	{
		Bias(offset, size);
	}

	virtual int Seek(int offset, int origin = SEEK_CUR) override
	{
		if (!Is_Open()) return -1;
		FILE* handle = static_cast<FILE*>(Get_File_Handle());
		long long position = offset;
		if (origin == SEEK_CUR) {
			const long current = ftell(handle);
			if (current < 0) return -1;
			position += current - BiasStart;
		} else if (origin == SEEK_END) {
			position += BiasLength;
		} else if (origin != SEEK_SET) {
			return -1;
		}
		if (position < 0) position = 0;
		if (position > BiasLength) position = BiasLength;
		if (fseek(handle, static_cast<long>(BiasStart + position), SEEK_SET) != 0) return -1;
		const long result = ftell(handle);
		return result < 0 ? -1 : static_cast<int>(result - BiasStart);
	}
};

unsigned int Big_Endian(const unsigned char* bytes)
{
	return (static_cast<unsigned int>(bytes[0]) << 24) |
		(static_cast<unsigned int>(bytes[1]) << 16) |
		(static_cast<unsigned int>(bytes[2]) << 8) | bytes[3];
}
}

BigTextureFileFactory::BigTextureFileFactory() : previousFactory(_TheFileFactory)
{
	_TheFileFactory = this;
}

BigTextureFileFactory::~BigTextureFileFactory()
{
	_TheFileFactory = previousFactory;
}

std::string BigTextureFileFactory::Normalize(const char* name)
{
	std::string result(name);
	for (size_t i = 0; i < result.size(); ++i) {
		if (result[i] == '/') result[i] = '\\';
		if (result[i] >= 'A' && result[i] <= 'Z') result[i] += 'a' - 'A';
	}
	return result;
}

bool BigTextureFileFactory::Add_Archive(const char* path, std::string& error)
{
	FILE* archive = fopen(path, "rb");
	if (!archive) {
		error = std::string("Cannot open BIG archive: ") + path;
		return false;
	}
	unsigned char header[16];
	bool valid = fseek(archive, 0, SEEK_END) == 0;
	const long length = ftell(archive);
	valid = valid && length >= 16 && fseek(archive, 0, SEEK_SET) == 0 &&
		fread(header, 1, sizeof(header), archive) == sizeof(header);
	unsigned int count = 0;
	unsigned int directoryEnd = 0;
	if (valid) {
		count = Big_Endian(header + 8);
		directoryEnd = Big_Endian(header + 12);
		valid = (memcmp(header, "BIGF", 4) == 0 || memcmp(header, "BIG4", 4) == 0) &&
			directoryEnd >= 16 && directoryEnd <= static_cast<unsigned long>(length) &&
			count <= (directoryEnd - 16) / 9;
	}
	std::map<std::string, Entry> additions;
	for (unsigned int i = 0; valid && i < count; ++i) {
		unsigned char bounds[8];
		const long position = ftell(archive);
		valid = position >= 0 && static_cast<unsigned long>(position) + 8 < directoryEnd &&
			fread(bounds, 1, sizeof(bounds), archive) == sizeof(bounds);
		if (!valid) break;
		const unsigned int offset = Big_Endian(bounds);
		const unsigned int size = Big_Endian(bounds + 4);
		std::string name;
		bool terminated = false;
		while (valid && name.size() < 4096 && ftell(archive) < directoryEnd) {
			const int byte = fgetc(archive);
			if (byte == EOF) break;
			if (byte == 0) { terminated = true; break; }
			name += static_cast<char>(byte);
		}
		valid = terminated && !name.empty() && offset >= directoryEnd &&
			offset <= static_cast<unsigned long>(length) && size <= static_cast<unsigned long>(length) - offset &&
			offset <= INT_MAX && size <= INT_MAX;
		if (valid) {
			Entry entry;
			entry.archive = path;
			entry.offset = static_cast<int>(offset);
			entry.size = static_cast<int>(size);
			additions[Normalize(name.c_str())] = entry;
		}
	}
	fclose(archive);
	if (!valid) {
		error = std::string("Invalid or unsupported BIG archive (requires offsets below 2 GiB): ") + path;
		return false;
	}
	// Explicit command-line ordering: later archives override earlier ones.
	for (std::map<std::string, Entry>::const_iterator it = additions.begin(); it != additions.end(); ++it) {
		entries[it->first] = it->second;
	}
	return true;
}

bool BigTextureFileFactory::Contains(const char* name) const
{
	return entries.find(Normalize(name)) != entries.end();
}

FileClass* BigTextureFileFactory::Get_File(const char* name)
{
	std::map<std::string, Entry>::const_iterator found = entries.find(Normalize(name));
	if (found == entries.end()) return new RawFileClass(name);
	return new BigEntryFile(found->second.archive.c_str(), found->second.offset, found->second.size);
}

void BigTextureFileFactory::Return_File(FileClass* file)
{
	delete file;
}
