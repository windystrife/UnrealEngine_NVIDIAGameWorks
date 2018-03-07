/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoDxIncludeHandler.h"

#include <Nv/Common/NvCoMemoryAllocator.h>
#include <Nv/Common/Platform/Win/NvCoWinPathUtil.h>

//#include "Shlwapi.h"
//#pragma comment(lib, "Shlwapi.lib")

namespace nvidia {
namespace Common { 

DxIncludeHandler::DxIncludeHandler()
{
}

DxIncludeHandler::DxIncludeHandler(const Array<SubString>& includePaths)
{
	addPaths(includePaths);
}

Result DxIncludeHandler::findAndReadFile(const SubString& path, String& pathOut, void** dataOut, UINT* sizeOut)
{
	const IndexT numPaths = m_paths.getSize();
	for (IndexT i = 0; i < numPaths; i++)
	{
		WinPathUtil::append(m_paths[i], path, pathOut);
		Result res = readFile(pathOut, (void**)dataOut, sizeOut);
		if (NV_SUCCEEDED(res)) return res;
	}
	// Didn't find it
	return NV_FAIL;
}

void DxIncludeHandler::addPath(const SubString& path)
{
	if (m_paths.indexOf(path) < 0)
	{
		m_paths.pushBack(path);
	}
}

void DxIncludeHandler::addPaths(const SubString* paths, IndexT numPaths)
{
	for (Int i = 0; i < numPaths; i++)
	{
		addPath(paths[i]);
	}
}

void DxIncludeHandler::addPaths(const Array<SubString>& paths)
{
	addPaths(paths.begin(), paths.getSize());
}

void DxIncludeHandler::addRelativePath(const SubString& relPathIn)
{
	String absPath;
	WinPathUtil::absolutePath(relPathIn, absPath);
	m_paths.expandOne().swap(absPath);
}

void DxIncludeHandler::addPathFromFile(const SubString& filePath)
{
	String work;
	WinPathUtil::getParent(filePath, work);
	m_paths.expandOne().swap(work);
}

void DxIncludeHandler::pushLocalPathFromFile(const SubString& filePath)
{
	String work;
	WinPathUtil::getParent(filePath, work);
	m_foundStack.expandOne().swap(work);
}

LONGLONG _getFileSize(HANDLE fileHandle)
{
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	FILE_STANDARD_INFO fileInfo;
	if (!GetFileInformationByHandleEx(fileHandle, FileStandardInfo, &fileInfo, sizeof(fileInfo)))
	{
		return 0;
	}
	return fileInfo.EndOfFile.QuadPart;
#else
	LARGE_INTEGER fileSizeStruct;
	GetFileSizeEx(fileHandle, &fileSizeStruct);
	return fileSizeStruct.QuadPart;
#endif
}

static Result _readFile(HANDLE fileHandle, void** dataOut, UINT* sizeOut)
{
	// Work out the size
	LONGLONG fileSize = _getFileSize(fileHandle);
	void* mem = MemoryAllocator::getInstance()->simpleAllocate(NvSizeT(fileSize));
	if (!mem)
	{
		return E_FAIL;
	}
	DWORD numRead;
	ReadFile(fileHandle, mem, DWORD(fileSize), &numRead, NV_NULL);
	*dataOut = mem;
	*sizeOut = UINT(fileSize);
	return NV_OK;
}

/* static */Result DxIncludeHandler::readFile(const SubString& pathIn, void** dataOut, UINT* sizeOut)
{
	Char path[MAX_PATH];

	*dataOut = NV_NULL;
	*sizeOut = 0;
	HANDLE fileHandle = CreateFileA(pathIn.storeCstr(path), GENERIC_READ, FILE_SHARE_READ, NV_NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NV_NULL);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		return E_FAIL;
	}
	Result res = _readFile(fileHandle, dataOut, sizeOut);
	CloseHandle(fileHandle);
	return res;
}

HRESULT NV_MCALL DxIncludeHandler::Open(D3D_INCLUDE_TYPE includeType, LPCSTR fileName, LPCVOID parentData, LPCVOID* dataOut, UINT* numBytesOut)
{
	NV_UNUSED(parentData)

	switch (includeType)
	{ 
		case D3D_INCLUDE_LOCAL:
		{
			if (m_foundStack.getSize())
			{
				String path;
				WinPathUtil::append(m_foundStack.back(), SubString(SubString::INIT_CSTR, fileName), path);

				HRESULT res = readFile(path, (void**)dataOut, numBytesOut);
				if (NV_SUCCEEDED(res))
				{
					pushLocalPathFromFile(path);
					return res;
				}
			}
			// fall thru..
		}
		default:
		case D3D_INCLUDE_SYSTEM:
		{
			String foundPath;
			HRESULT hr = findAndReadFile(SubString(SubString::INIT_CSTR, fileName), foundPath, (void**)dataOut, numBytesOut);
			if (NV_SUCCEEDED(hr))
			{
				// Push the path stack
				pushLocalPathFromFile(foundPath);
			}
			return hr;
		}
	}
}

HRESULT NV_MCALL DxIncludeHandler::Close(LPCVOID data)
{
	if (data)
	{
		MemoryAllocator::getInstance()->simpleDeallocate(data);
	}
	popLocalPath();
	return S_OK;
}

} // namespace Common 
} // namespace nvidia