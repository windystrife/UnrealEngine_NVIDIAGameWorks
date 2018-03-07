// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-AnimPhys stream
struct CORE_API FAnimPhysObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded,
		// convert animnode look at to use just default axis instead of enum, which doesn't do much
		ConvertAnimNodeLookAtAxis,
		// Change FKSphylElem and FKBoxElem to use Rotators not Quats for easier editing
		BoxSphylElemsUseRotators,
		// Change thumbnail scene info and asset import data to be transactional
		ThumbnailSceneInfoAndAssetImportDataAreTransactional,
		// Enabled clothing masks rather than painting parameters directly
		AddedClothingMaskWorkflow,
		// Remove UID from smart name serialize, it just breaks determinism 
		RemoveUIDFromSmartNameSerialize,
		// Convert FName Socket to FSocketReference and added TargetReference that support bone and socket
		CreateTargetReference,
		// Tune soft limit stiffness and damping coefficients
		TuneSoftLimitStiffnessAndDamping,
		// Fix possible inf/nans in clothing particle masses
		FixInvalidClothParticleMasses,
		// Moved influence count to cached data
		CacheClothMeshInfluences,
		// Remove GUID from Smart Names entirely + remove automatic name fixup
		SmartNameRefactorForDeterministicCooking,
		// rename the variable and allow individual curves to be set
		RenameDisableAnimCurvesToAllowAnimCurveEvaluation,
		// link curve to LOD, so curve metadata has to include LODIndex
		AddLODToCurveMetaData,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FAnimPhysObjectVersion() {}
};
