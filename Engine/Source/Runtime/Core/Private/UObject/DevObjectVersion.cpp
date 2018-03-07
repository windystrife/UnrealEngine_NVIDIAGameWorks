// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "UObject/DevObjectVersion.h"
#include "Logging/LogMacros.h"
#include "UObject/BlueprintsObjectVersion.h"
#include "UObject/BuildObjectVersion.h"
#include "UObject/CoreObjectVersion.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/MobileObjectVersion.h"
#include "UObject/NetworkingObjectVersion.h"
#include "UObject/OnlineObjectVersion.h"
#include "UObject/PhysicsObjectVersion.h"
#include "UObject/PlatformObjectVersion.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/SequencerObjectVersion.h"
#include "UObject/VRObjectVersion.h"
#include "UObject/GeometryObjectVersion.h"
#include "UObject/AnimPhysObjectVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogDevObjectVersion, Log, All);

#if !UE_BUILD_SHIPPING
static TArray<FGuid> GDevVersions;
#endif
FDevVersionRegistration::FDevVersionRegistration(FGuid InKey, int32 Version, FName InFriendlyName)
: FCustomVersionRegistration(InKey, Version, InFriendlyName)
{
#if !UE_BUILD_SHIPPING
	GDevVersions.Add(InKey);
#endif
}
void FDevVersionRegistration::DumpVersionsToLog()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogDevObjectVersion, Log, TEXT("Number of dev versions registered: %d"), GDevVersions.Num());
	for (FGuid& Guid : GDevVersions)
	{
		const FCustomVersion* Version = FCustomVersionContainer::GetRegistered().GetVersion(Guid);
		check(Version);
		UE_LOG(LogDevObjectVersion, Log, TEXT("  %s (%s): %d"), *Version->GetFriendlyName().ToString(), *Version->Key.ToString(EGuidFormats::DigitsWithHyphens), Version->Version);
	}
#endif
}
// Unique Blueprints Object version id
const FGuid FBlueprintsObjectVersion::GUID(0xB0D832E4, 0x1F894F0D, 0xACCF7EB7, 0x36FD4AA2);
// Register Blueprints custom version with Core
FDevVersionRegistration GRegisterBlueprintsObjectVersion(FBlueprintsObjectVersion::GUID, FBlueprintsObjectVersion::LatestVersion, TEXT("Dev-Blueprints"));

// Unique Build Object version id
const FGuid FBuildObjectVersion::GUID(0xE1C64328, 0xA22C4D53, 0xA36C8E86, 0x6417BD8C);
// Register Build custom version with Core
FDevVersionRegistration GRegisterBuildObjectVersion(FBuildObjectVersion::GUID, FBuildObjectVersion::LatestVersion, TEXT("Dev-Build"));

// Unique Core Object version id
const FGuid FCoreObjectVersion::GUID(0x375EC13C, 0x06E448FB, 0xB50084F0, 0x262A717E);
// Register Core custom version with Core
FDevVersionRegistration GRegisterCoreObjectVersion(FCoreObjectVersion::GUID, FCoreObjectVersion::LatestVersion, TEXT("Dev-Core"));

// Unique Editor Object version id
const FGuid FEditorObjectVersion::GUID(0xE4B068ED, 0xF49442E9, 0xA231DA0B, 0x2E46BB41);
// Register Editor custom version with Core
FDevVersionRegistration GRegisterEditorObjectVersion(FEditorObjectVersion::GUID, FEditorObjectVersion::LatestVersion, TEXT("Dev-Editor"));

// Unique Framework Object version id
const FGuid FFrameworkObjectVersion::GUID(0xCFFC743F, 0x43B04480, 0x939114DF, 0x171D2073);
// Register Framework custom version with Core
FDevVersionRegistration GRegisterFrameworkObjectVersion(FFrameworkObjectVersion::GUID, FFrameworkObjectVersion::LatestVersion, TEXT("Dev-Framework"));

// Unique Mobile Object version id
const FGuid FMobileObjectVersion::GUID(0xB02B49B5, 0xBB2044E9, 0xA30432B7, 0x52E40360);
// Register Mobile custom version with Core
FDevVersionRegistration GRegisterMobileObjectVersion(FMobileObjectVersion::GUID, FMobileObjectVersion::LatestVersion, TEXT("Dev-Mobile"));

// Unique Networking Object version id
const FGuid FNetworkingObjectVersion::GUID(0xA4E4105C, 0x59A149B5, 0xA7C540C4, 0x547EDFEE);
// Register Networking custom version with Core
FDevVersionRegistration GRegisterNetworkingObjectVersion(FNetworkingObjectVersion::GUID, FNetworkingObjectVersion::LatestVersion, TEXT("Dev-Networking"));

// Unique Online Object version id
const FGuid FOnlineObjectVersion::GUID(0x39C831C9, 0x5AE647DC, 0x9A449C17, 0x3E1C8E7C);
// Register Online custom version with Core
FDevVersionRegistration GRegisterOnlineObjectVersion(FOnlineObjectVersion::GUID, FOnlineObjectVersion::LatestVersion, TEXT("Dev-Online"));

// Unique Physics Object version id
const FGuid FPhysicsObjectVersion::GUID(0x78F01B33, 0xEBEA4F98, 0xB9B484EA, 0xCCB95AA2);
// Register Physics custom version with Core
FDevVersionRegistration GRegisterPhysicsObjectVersion(FPhysicsObjectVersion::GUID, FPhysicsObjectVersion::LatestVersion, TEXT("Dev-Physics"));

// Unique Platform Object version id
const FGuid FPlatformObjectVersion::GUID(0x6631380F, 0x2D4D43E0, 0x8009CF27, 0x6956A95A);
// Register Platform custom version with Core
FDevVersionRegistration GRegisterPlatformObjectVersion(FPlatformObjectVersion::GUID, FPlatformObjectVersion::LatestVersion, TEXT("Dev-Platform"));

// Unique Rendering Object version id
const FGuid FRenderingObjectVersion::GUID(0x12F88B9F, 0x88754AFC, 0xA67CD90C, 0x383ABD29);
// Register Rendering custom version with Core
FDevVersionRegistration GRegisterRenderingObjectVersion(FRenderingObjectVersion::GUID, FRenderingObjectVersion::LatestVersion, TEXT("Dev-Rendering"));

// Unique Sequencer Object version id
const FGuid FSequencerObjectVersion::GUID(0x7B5AE74C, 0xD2704C10, 0xA9585798, 0x0B212A5A);
// Register Sequencer custom version with Core
FDevVersionRegistration GRegisterSequencerObjectVersion(FSequencerObjectVersion::GUID, FSequencerObjectVersion::LatestVersion, TEXT("Dev-Sequencer"));

// Unique VR Object version id
const FGuid FVRObjectVersion::GUID(0xD7296918, 0x1DD64BDD, 0x9DE264A8, 0x3CC13884);
// Register VR custom version with Core
FDevVersionRegistration GRegisterVRObjectVersion(FVRObjectVersion::GUID, FVRObjectVersion::LatestVersion, TEXT("Dev-VR"));

// Unique Load Times version id
const FGuid FLoadTimesObjectVersion::GUID(0xC2A15278, 0xBFE74AFE, 0x6C1790FF, 0x531DF755);
// Register LoadTimes custom version with Core
FDevVersionRegistration GRegisterLoadTimesObjectVersion(FLoadTimesObjectVersion::GUID, FLoadTimesObjectVersion::LatestVersion, TEXT("Dev-LoadTimes"));


// Unique Geometry Object version id
const FGuid FGeometryObjectVersion::GUID(0x6EACA3D4, 0x40EC4CC1, 0xb7868BED, 0x9428FC5);
// Register Geometry custom version with Core
FDevVersionRegistration GRegisterGeometryObjectVersion(FGeometryObjectVersion::GUID, FGeometryObjectVersion::LatestVersion, TEXT("Private-Geometry"));

// Unique AnimPhys Object version id
const FGuid FAnimPhysObjectVersion::GUID(0x29E575DD, 0xE0A34627, 0x9D10D276, 0x232CDCEA);
// Register AnimPhys custom version with Core
FDevVersionRegistration GRegisterAnimPhysObjectVersion(FAnimPhysObjectVersion::GUID, FAnimPhysObjectVersion::LatestVersion, TEXT("Dev-AnimPhys"));