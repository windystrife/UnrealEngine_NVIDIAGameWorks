// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME 1
#else
	#define USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME 0
#endif

