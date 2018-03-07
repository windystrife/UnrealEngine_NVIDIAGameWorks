// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * This file is just used to indirectly include EngineParticleClasses.h with all of the C++ headers it depends on included first.
 */

#pragma once

#include "Misc/MonolithicHeaderBoilerplate.h"
MONOLITHIC_HEADER_BOILERPLATE()

#include "PixelFormat.h"
#include "ParticleVertexFactory.h"
#include "ConvexVolume.h"
#include "HitProxies.h"
#include "UnrealClient.h"
#include "TextureResource.h"
#include "SceneTypes.h"
#include "StaticParameterSet.h"
#include "MaterialShared.h"
#include "ShowFlags.h"
#include "EngineLogs.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "EngineDefines.h"
#include "ComponentInstanceDataCache.h"
#include "TimerManager.h"
#include "GenericOctreePublic.h"
#include "GenericOctree.h"
#include "BlendableManager.h"
#include "FinalPostProcessSettings.h"
#include "SceneInterface.h"
#include "DebugViewModeHelpers.h"
#include "SceneView.h"
#include "StaticBoundShaderState.h"
#include "BatchedElements.h"
#include "PrimitiveUniformShaderParameters.h"
#include "MeshBatch.h"
#include "SceneUtils.h"
#include "TessellationRendering.h"
#include "SceneManagement.h"
#include "MeshParticleVertexFactory.h"
#include "ParticleHelper.h"
#include "Distributions.h"
#include "ParticleEmitterInstances.h"
