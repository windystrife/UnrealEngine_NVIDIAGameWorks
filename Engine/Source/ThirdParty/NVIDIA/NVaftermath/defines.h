/*
* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

// Library stuff...
#define GFSDK_Aftermath_PFN typedef GFSDK_Aftermath_Result

#ifdef EXPORTS
#define GFSDK_Aftermath_API __declspec(dllexport) GFSDK_Aftermath_Result
#else
#define GFSDK_Aftermath_API GFSDK_Aftermath_Result
#endif

#ifndef GFSDK_Aftermath_WITH_DX11
#define GFSDK_Aftermath_WITH_DX11 0
#endif

#ifndef GFSDK_Aftermath_WITH_DX12
#define GFSDK_Aftermath_WITH_DX12 0
#endif