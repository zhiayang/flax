// file.cpp
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

#include "lexer.h"
#include "errors.h"
#include "frontend.h"

namespace frontend
{
	struct FileInnards
	{
		lexer::TokenList tokens;
		util::string_view fileContents;
		util::FastInsertVector<util::string_view> lines;
		std::vector<size_t> importIndices;

		bool didLex = false;
		bool isLexing = false;
	};

	static std::unordered_map<std::string, FileInnards> fileList;

	static FileInnards& readFileIfNecessary(const std::string& fullPath)
	{
		// break early if we can
		{
			auto it = fileList.find(fullPath);
			if(it != fileList.end() && it->second.didLex)
				return it->second;
		}


		util::string_view fileContents;
		{
			fileContents = platform::readEntireFile(fullPath);
		}



		// split into lines
		bool crlf = false;
		util::FastInsertVector<util::string_view> rawlines;

		{
			util::string_view view = fileContents;

			bool first = true;
			while(true)
			{
				size_t ln = 0;

				if(first || crlf)
				{
					ln = view.find("\r\n");
					if(ln != util::string_view::npos && first)
						crlf = true;
				}

				if((!first && !crlf) || (first && !crlf && ln == util::string_view::npos))
					ln = view.find('\n');

				first = false;

				if(ln != util::string_view::npos)
				{
					new (rawlines.getNextSlotAndIncrement()) util::string_view(view.data(), ln + (crlf ? 2 : 1));
					view.remove_prefix(ln + (crlf ? 2 : 1));
				}
				else
				{
					break;
				}
			}

			// account for the case when there's no trailing newline, and we still have some stuff stuck in the view.
			if(!view.empty())
			{
				new (rawlines.getNextSlotAndIncrement()) util::string_view(view.data(), view.length());
			}
		}


		Location pos;
		FileInnards& innards = fileList[fullPath];
		if(innards.isLexing)
		{
			warn("attempting to lex file while file is already being lexed, stop it");
			return innards;
		}

		{
			pos.fileID = getFileIDFromFilename(fullPath);

			innards.fileContents = std::move(fileContents);
			innards.lines = std::move(rawlines);
			innards.isLexing = true;
		}

		lexer::TokenList& ts = innards.tokens;
		{
			size_t curLine = 0;
			size_t curOffset = 0;

			bool flag = true;
			size_t i = 0;

			do {
				auto type = lexer::getNextToken(innards.lines, &curLine, &curOffset, innards.fileContents, pos, ts.getNextSlotAndIncrement(), crlf);

				flag = (type != lexer::TokenType::EndOfFile);

				if(type == lexer::TokenType::Import)
					innards.importIndices.push_back(i);

				else if(type == lexer::TokenType::Invalid)
					error(pos, "invalid token");

				i++;

			} while(flag);

			ts[ts.size() - 1].loc.len = 0;
		}

		innards.didLex = true;
		innards.isLexing = false;
		return innards;
	}


	lexer::TokenList& getFileTokens(const std::string& fullPath)
	{
		return readFileIfNecessary(fullPath).tokens;
	}

	std::string getFileContents(const std::string& fullPath)
	{
		return util::to_string(readFileIfNecessary(fullPath).fileContents);
	}


	static std::vector<std::string> fileNames { "null" };
	static std::unordered_map<std::string, size_t> existingNames;
	const std::string& getFilenameFromID(size_t fileID)
	{
		iceAssert(fileID > 0 && fileID < fileNames.size());
		return fileNames[fileID];
	}

	size_t getFileIDFromFilename(const std::string& name)
	{
		if(existingNames.find(name) != existingNames.end())
		{
			return existingNames[name];
		}
		else
		{
			fileNames.push_back(name);
			existingNames[name] = fileNames.size() - 1;

			return fileNames.size() - 1;
		}
	}

	const util::FastInsertVector<util::string_view>& getFileLines(size_t id)
	{
		std::string fp = getFilenameFromID(id);
		return readFileIfNecessary(fp).lines;
	}

	const std::vector<size_t>& getImportTokenLocationsForFile(const std::string& filename)
	{
		return fileList[filename].importIndices;
	}




	std::string getPathFromFile(const std::string& path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(0, sep);

		return ret;
	}

	std::string getFilenameFromPath(const std::string& path)
	{
		std::string ret;

		size_t sep = path.find_last_of("\\/");
		if(sep != std::string::npos)
			ret = path.substr(sep + 1);

		return ret;
	}


	std::string getFullPathOfFile(const std::string& partial)
	{
		std::string full = platform::getFullPath(partial);
		if(full.empty())
			error("nonexistent file %s", partial.c_str());

		return full;
	}

	std::string removeExtensionFromFilename(const std::string& name)
	{
		auto i = name.find_last_of(".");
		return name.substr(0, i);
	}
}













