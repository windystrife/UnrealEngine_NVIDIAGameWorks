/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX_INCLUDE_HANDER_H
#define NV_CO_DX_INCLUDE_HANDER_H

#include <Nv/Common/NvCoComTypes.h>

#include <Nv/Common/NvCoString.h>
#include <Nv/Common/Container/NvCoArray.h>

#include <d3dcommon.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

/*! 
\brief A fairly simple implementation of the ID3DInclude interface. This interface is often used when compiling shaders, as it 
is used for finding files that are specified as includes in shader code. 
This implementation allows for setting multiple search paths that will be searched for when looking for the requested file. It also
takes into account if an include is a system wide or local include as specified by the D3D_INCLUDE_TYPE includeType parameter.
*/
class DxIncludeHandler : public ID3DInclude
{
public:
	
	enum { MAX_PATHS = 10 };

		/// ID3DInclude 
	NV_NO_THROW HRESULT NV_MCALL Open(D3D_INCLUDE_TYPE includeType, LPCSTR fileName, LPCVOID parentData, LPCVOID* dataOut, UINT* numBytesOut) NV_OVERRIDE;
	NV_NO_THROW HRESULT NV_MCALL Close(LPCVOID data) NV_OVERRIDE;

		/// Add a path to search
	void addPath(const SubString& path);
	void addRelativePath(const SubString& relPath);
	void addPathFromFile(const SubString& filePath);
	void addPaths(const SubString* paths, IndexT numPaths);
	void addPaths(const Array<SubString>& paths);

		/// Local path
	void pushLocalPathFromFile(const SubString& filePath);
	void popLocalPath() { m_foundStack.popBack(); }
		/// Ctor
	DxIncludeHandler();
	DxIncludeHandler(const Array<SubString>& includePaths);

		/// Attempts to find a file and if found reads it, returning it through dataOut and sizeOut
		/// The data is allocated through the MemoryAllocator::getInstance and should be freed with simpleDeallocate
	Result findAndReadFile(const SubString& path, String& pathOut, void** dataOut, UINT* sizeOut);

		/// Read the contents of a file. Data stored in data (allocated by MemoryAllocator), deallocate with simpleDeallocate()
	static Result readFile(const SubString& path, void** dataOut, UINT* sizeOut);

protected:
	
	Array<String> m_paths;
	Array<String> m_foundStack;
};

} // namespace Common
} // namespace nvidia

 /** @} */

#endif // NV_CO_DX_INCLUDE_HANDER_H
