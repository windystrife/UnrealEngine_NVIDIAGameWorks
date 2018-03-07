/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_WIN_PATH_H
#define NV_CO_WIN_PATH_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoString.h>
#include <Nv/Common/NvCoMemoryAllocator.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/// Windows implementation of memory allocator
struct WinPathUtil
{
	static Bool exists(const SubString& path);

		/// True if is a separator
	static Bool isSeparator(Char c) { return c == '/' || c == '\\'; }

		/// True if it's an absolute path
	static Bool isAbsolutePath(const SubString& path);
		/// Return path and rest appended in pathOut
	static void append(const SubString& path, const SubString& rest, String& pathOut);

		/// Get as an absolute path
	static void absolutePath(const SubString& path, String& absPathOut);

		/// Simplifies path (for example removes . and .. parts). Note not canonical in the strict 
		/// sense as two paths that are to the same file, may not produce an identical path using this method.
	static void canoncialPath(const SubString& pathIn, String& pathOut);

		/// Get the parent path
	static Void getParent(const SubString& pathIn, String& pathOut);

		/// Gets the substring (or empty if not found)
	static SubString getExtension(const SubString& pathIn);

		/// Return the combined path. If path is absolute just returns it.
	static String combine(const SubString& dirPath, const SubString& path);
		/// Use pathOut to store constructed string. 
	static SubString combine(const SubString& dirPath, const SubString& path, String& pathOut);
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_WIN_PATH_H