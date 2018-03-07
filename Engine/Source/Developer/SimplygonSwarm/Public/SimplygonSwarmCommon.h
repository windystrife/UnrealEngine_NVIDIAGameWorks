// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RawMesh.h"
#include "MeshUtilities.h"
#include "MaterialUtilities.h"
#include "MeshBoneReduction.h"
#include "ComponentReregisterContext.h"
#include "ImageUtils.h"
THIRD_PARTY_INCLUDES_START
#include "ssf.h"
THIRD_PARTY_INCLUDES_END
#include "IImageWrapper.h"
#include "SPLInclude.h"

namespace SPL = Simplygon::SPL::v80;

#ifndef SPL_VERSION
	#define SPL_VERSION 8
#endif // !SPL_VERSION

#define SPL_CURRENT_VERSION SPL_VERSION

// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.
