/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoWinPathUtil.h"

#include <Nv/Common/Platform/Win/NvCoWinInclude.h>

#include <stdlib.h>
#include <string.h>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

namespace nvidia {
namespace Common {

/* static */Bool WinPathUtil::exists(const SubString& pathIn)
{
	char work[_MAX_PATH];
	return (GetFileAttributesA(pathIn.storeCstr(work)) != INVALID_FILE_ATTRIBUTES);
}

/* static */Void WinPathUtil::append(const SubString& pathIn, const SubString& restIn, String& pathOut)
{
	Char path[_MAX_PATH];
	Char rest[_MAX_PATH];

	pathIn.storeCstr(path);
	PathAppendA(path, restIn.storeCstr(rest));
	
	pathOut = SubString(SubString::INIT_CSTR, path);
}

Bool WinPathUtil::isAbsolutePath(const SubString& path)
{
	if (path.getSize() >= 2)
	{
		// Strictly speaking a path starting with \ is not absolute, because it doesn't contain 
		// the drive letter, and so just takes the current drive
		// A path starting with \\ indicates a network drive
		if ((path[1] == ':') ||
			(path[0] == '\\' && path[1] == '\\'))
		{
			return true;
		}
	}
	return false;
}

/* static */void WinPathUtil::absolutePath(const SubString& path, String& absPath)
{
	if (isAbsolutePath(path))
	{
		absPath = path;
	}
	else
	{
		Char work[MAX_PATH];
		char workPath[MAX_PATH];

		// Get work path
		GetCurrentDirectoryA(MAX_PATH, workPath);
		// Concat relative
		PathAppendA(workPath, path.storeCstr(work));

		// Remove any slashes
		char* filePos = NV_NULL;
		GetFullPathNameA(workPath, MAX_PATH, work, &filePos);

		// Save out
		absPath = SubString(SubString::INIT_CSTR, work);
	}	
}

/* static */void WinPathUtil::canoncialPath(const SubString& pathIn, String& absPath)
{
	char path[_MAX_PATH];
	char work[_MAX_PATH];

	char* filePos = NV_NULL;
	GetFullPathNameA(pathIn.storeCstr(path), MAX_PATH, work, &filePos);

	absPath = SubString(SubString::INIT_CSTR, work);
}

/* static */Void WinPathUtil::getParent(const SubString& pathIn, String& pathOut)
{	
	char path[_MAX_PATH];
	char work[_MAX_PATH];

	char* filePos = NV_NULL;
	GetFullPathNameA(pathIn.storeCstr(path), MAX_PATH, work, &filePos);

	pathOut = SubString(work, IndexT(filePos - 1 - work));
}

/* static */SubString WinPathUtil::getExtension(const SubString& pathIn)
{
	const Char* start = pathIn.begin();
	for (const Char* end = pathIn.end() - 1; end > start; --end)
	{
		switch (*end)
		{
			case '/':
			case '\\': 
			{
				return SubString();
			}
			case '.':
			{
				return SubString(SubString::INIT_SPAN, end + 1, pathIn.end());
			}
			default: break;
		}
	}
	return SubString();
}

/* static */SubString WinPathUtil::combine(const SubString& dirPath, const SubString& path, String& pathOut)
{
	if (isAbsolutePath(path))
	{
		return path;
	}
	pathOut = dirPath;
	if (pathOut.getSize() > 0 && !isSeparator(pathOut.getLast()))
	{
		pathOut.concat('/');
	}
	if (path.getSize() > 0 && isSeparator(path[0]))
	{
		pathOut.concat(path.tail(1));
	}
	else
	{
		pathOut.concat(path);
	}
		
	return pathOut;
}

/* static */String WinPathUtil::combine(const SubString& dirPath, const SubString& path)
{	
	String newPath;
	combine(dirPath, path, newPath);
	return newPath;
}

} // namespace Common 
} // namespace nvidia

