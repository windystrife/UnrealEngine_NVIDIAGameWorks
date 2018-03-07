/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoDxDebugUtil.h"

#include <Nv/Common/NvCoLogger.h>

namespace nvidia {
namespace Common {

/* static */Result DxDebugUtil::getDebugInterface(ComPtr<IDXGIDebug>& debugOut)
{
	HMODULE module = GetModuleHandleA("Dxgidebug.dll");
	if (module)
	{
		//WINAPI
		typedef HRESULT(WINAPI *FuncType)(REFIID riid, void **ppDebug);
		FARPROC funcAddr = GetProcAddress(module, "DXGIGetDebugInterface");

		FuncType debugFunc = (FuncType)funcAddr;
		if (debugFunc)
		{
			return debugFunc(__uuidof(IDXGIDebug), (Void**)debugOut.writeRef());			
		}
	}
	return NV_FAIL;
}

} // namespace Common
} // namespace nvidia
