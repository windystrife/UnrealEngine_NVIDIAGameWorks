// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LMCore.h"

/** 
 * Set to 1 to allow selecting lightmap texels by holding down T and left clicking in the editor,
 * And having debug information about that texel tracked during subsequent lighting rebuilds.
 * Have to enable 'r.TexelDebugging' in the editor as well.
 */
#define ALLOW_LIGHTMAP_SAMPLE_DEBUGGING	0

#if USE_EMBREE
#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#else
typedef void* RTCDevice;
#endif

#include "LightmassScene.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "LightingMesh.h"
#include "BuildOptions.h"
#include "LightmapData.h"
#include "Mappings.h"
#include "Collision.h"
#include "Embree.h"

namespace Lightmass
{
#if USE_EMBREE
	typedef FStaticLightingAggregateMesh FStaticLightingAggregateMeshType;
#else
	typedef FDefaultAggregateMesh FStaticLightingAggregateMeshType;
#endif
}

#include "BSP.h"
#include "StaticMesh.h"
#include "FluidSurface.h"
#include "Landscape.h"

namespace Lightmass
{
	extern double GStartupTime;
}

#define WORLD_MAX			2097152.0			/* Maximum size of the world */
#define HALF_WORLD_MAX		( WORLD_MAX*0.5f )	/* Half the maximum size of the world */


