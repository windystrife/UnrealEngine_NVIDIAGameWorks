// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	MacPlatformCompilerSetup.h: pragmas, version checks and other things for the Mac compiler
==============================================================================================*/

#pragma once

// In OS X 10.8 SDK gl3.h complains if gl.h was also included. Unfortunately, gl.h is included by Cocoa in CoreVideo.framework, so we need to disable this warning
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
