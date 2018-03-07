// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START

#ifdef PI
# undef PI
# include "Leap.h"
# define PI (3.1415926535897932f)
#else
# include "Leap.h"
#endif

THIRD_PARTY_INCLUDES_END
