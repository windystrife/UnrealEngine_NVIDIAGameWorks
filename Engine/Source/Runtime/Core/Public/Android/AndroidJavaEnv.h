// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include <jni.h>

namespace AndroidJavaEnv
{
	// Returns the java environment
	CORE_API void InitializeJavaEnv(JavaVM* VM, jint Version, jobject GlobalThis);
	CORE_API jobject GetGameActivityThis();
	CORE_API jobject GetClassLoader();
	CORE_API JNIEnv* GetJavaEnv(bool bRequireGlobalThis = true);
	CORE_API jclass FindJavaClass(const char* name);
	CORE_API void DetachJavaEnv();
	CORE_API bool CheckJavaException();
}
