/*
** Copyright 2026 TheSuperHackers
** SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "WWLib/ffactory.h"
#include <map>
#include <string>

// A tool-only archive adapter: the regular W3D DDS/TGA loader reads bounded
// RawFileClass views of BIG entries, without initializing the game filesystem.
class BigTextureFileFactory : public FileFactoryClass
{
public:
	BigTextureFileFactory();
	virtual ~BigTextureFileFactory() override;
	bool Add_Archive(const char* path, std::string& error);
	bool Contains(const char* name) const;
	virtual FileClass* Get_File(const char* name) override;
	virtual void Return_File(FileClass* file) override;

private:
	struct Entry
	{
		std::string archive;
		int offset;
		int size;
	};
	static std::string Normalize(const char* name);
	std::map<std::string, Entry> entries;
	FileFactoryClass* previousFactory;
	BigTextureFileFactory(const BigTextureFileFactory&) = delete;
	BigTextureFileFactory& operator=(const BigTextureFileFactory&) = delete;
};
