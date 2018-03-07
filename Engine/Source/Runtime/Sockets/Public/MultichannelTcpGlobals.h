// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

SOCKETS_API DECLARE_LOG_CATEGORY_EXTERN(LogMultichannelTCP, Log, All);


/** Magic number used to verify packet header **/
enum
{
	MultichannelMagic = 0xa692339f
};
