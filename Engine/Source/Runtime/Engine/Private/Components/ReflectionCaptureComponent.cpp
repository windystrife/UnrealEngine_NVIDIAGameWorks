// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	
=============================================================================*/

#include "Components/ReflectionCaptureComponent.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Actor.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "Misc/ScopeLock.h"
#include "Components/BillboardComponent.h"
#include "Engine/CollisionProfile.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "Engine/ReflectionCapture.h"
#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatform.h"
#include "EngineModule.h"
#include "ShaderCompiler.h"
#include "LoadTimesObjectVersion.h"
#include "RenderingObjectVersion.h"
#include "Engine/SphereReflectionCapture.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Components/DrawSphereComponent.h"
#include "Components/BoxReflectionCaptureComponent.h"
#include "Engine/PlaneReflectionCapture.h"
#include "Engine/BoxReflectionCapture.h"
#include "Components/PlaneReflectionCaptureComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkyLightComponent.h"
#include "ProfilingDebugging/CookStats.h"

#if ENABLE_COOK_STATS
namespace ReflectionCaptureCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("ReflectionCapture.Usage"), TEXT(""));
	});
}
#endif

// ES3.0+ devices support seamless cubemap filtering, averaging edges will produce artifacts on those devices
#define MOBILE_AVERAGE_CUBEMAP_EDGES 0 

/** 
 * Size of all reflection captures.
 * Reflection capture derived data versions must be changed if modifying this
 */
ENGINE_API TAutoConsoleVariable<int32> CVarReflectionCaptureSize(
	TEXT("r.ReflectionCaptureResolution"),
	128,
	TEXT("Set the resolution for all reflection capture cubemaps. Should be set via project's Render Settings. Must be power of 2. Defaults to 128.\n")
	);

static int32 SanatizeReflectionCaptureSize(int32 ReflectionCaptureSize)
{
	static const int32 MaxReflectionCaptureSize = 1024;
	static const int32 MinReflectionCaptureSize = 1;

	return FMath::Clamp(ReflectionCaptureSize, MinReflectionCaptureSize, MaxReflectionCaptureSize);
}

int32 UReflectionCaptureComponent::GetReflectionCaptureSize_GameThread()
{
	return SanatizeReflectionCaptureSize(CVarReflectionCaptureSize.GetValueOnGameThread());
}

int32 UReflectionCaptureComponent::GetReflectionCaptureSize_RenderThread()
{
	return SanatizeReflectionCaptureSize(CVarReflectionCaptureSize.GetValueOnRenderThread());
}


void UReflectionCaptureComponent::ReleaseHDRData()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseHDRData,
		FReflectionCaptureFullHDR*, FullHDRData, FullHDRData,
		{
			delete FullHDRData;
		});

	FullHDRData = nullptr;
}

void UWorld::UpdateAllReflectionCaptures()
{
	if ( FeatureLevel < ERHIFeatureLevel::SM4)
	{
		UE_LOG(LogMaterial, Warning, TEXT("Update reflection captures only works with an active feature level of SM4 or greater."));
		return;
	}
	
	TArray<UPackage*> Packages;	
	for (TObjectIterator<UReflectionCaptureComponent> It; It; ++It)
	{
		UReflectionCaptureComponent* CaptureComponent = *It;

		if (ContainsActor(CaptureComponent->GetOwner()) && !CaptureComponent->IsPendingKill())
		{
			// Purge cached derived data and force an update
			CaptureComponent->SetCaptureIsDirty();			
			Packages.AddUnique(CaptureComponent->GetOutermost());
		}
	}
	for (UPackage* Package : Packages)
	{
		if (Package)
		{
			// Need to dirty capture packages for new HDR data
			Package->MarkPackageDirty();
		}
	}
	UReflectionCaptureComponent::UpdateReflectionCaptureContents(this);
	
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		USkyLightComponent* SkylightComponent = *It;
		if (ContainsActor(SkylightComponent->GetOwner()) && !SkylightComponent->IsPendingKill())
		{			
			SkylightComponent->SetCaptureIsDirty();			
		}
	}
	USkyLightComponent::UpdateSkyCaptureContents(this);
}

AReflectionCapture::AReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CaptureComponent = CreateAbstractDefaultSubobject<UReflectionCaptureComponent>(TEXT("NewReflectionComponent"));

	bCanBeInCluster = true;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			FName NAME_ReflectionCapture;
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics()
				: NAME_ReflectionCapture(TEXT("ReflectionCapture"))
				, DecalTexture(TEXT("/Engine/EditorResources/S_ReflActorIcon"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bAbsoluteScale = true;
		SpriteComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		SpriteComponent->bIsScreenSizeScaled = true;
	}

	CaptureOffsetComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("CaptureOffset"));
	if (!IsRunningCommandlet() && (CaptureOffsetComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			FName NAME_ReflectionCapture;
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics()
				: NAME_ReflectionCapture(TEXT("ReflectionCapture"))
				, DecalTexture(TEXT("/Engine/EditorResources/S_ReflActorIcon"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		CaptureOffsetComponent->Sprite = ConstructorStatics.DecalTexture.Get();
		CaptureOffsetComponent->RelativeScale3D = FVector(0.2f, 0.2f, 0.2f);
		CaptureOffsetComponent->bHiddenInGame = true;
		CaptureOffsetComponent->bAbsoluteScale = true;
		CaptureOffsetComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		CaptureOffsetComponent->bIsScreenSizeScaled = true;
	}

	if (CaptureComponent)
	{
		CaptureComponent->CaptureOffsetComponent = CaptureOffsetComponent;
	}
	
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void AReflectionCapture::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	CaptureComponent->SetCaptureIsDirty();
}
#endif // WITH_EDITOR

ASphereReflectionCapture::ASphereReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USphereReflectionCaptureComponent>(TEXT("NewReflectionComponent")))
{
	USphereReflectionCaptureComponent* SphereComponent = CastChecked<USphereReflectionCaptureComponent>(GetCaptureComponent());
	RootComponent = SphereComponent;
#if WITH_EDITORONLY_DATA
	if (GetSpriteComponent())
	{
		GetSpriteComponent()->SetupAttachment(SphereComponent);
	}
	if (GetCaptureOffsetComponent())
	{
		GetCaptureOffsetComponent()->SetupAttachment(SphereComponent);
	}
#endif	//WITH_EDITORONLY_DATA

	UDrawSphereComponent* DrawInfluenceRadius = CreateDefaultSubobject<UDrawSphereComponent>(TEXT("DrawRadius0"));
	DrawInfluenceRadius->SetupAttachment(GetCaptureComponent());
	DrawInfluenceRadius->bDrawOnlyIfSelected = true;
	DrawInfluenceRadius->bUseEditorCompositing = true;
	DrawInfluenceRadius->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SphereComponent->PreviewInfluenceRadius = DrawInfluenceRadius;

	DrawCaptureRadius = CreateDefaultSubobject<UDrawSphereComponent>(TEXT("DrawRadius1"));
	DrawCaptureRadius->SetupAttachment(GetCaptureComponent());
	DrawCaptureRadius->bDrawOnlyIfSelected = true;
	DrawCaptureRadius->bUseEditorCompositing = true;
	DrawCaptureRadius->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DrawCaptureRadius->ShapeColor = FColor(100, 90, 40);
}

#if WITH_EDITOR
void ASphereReflectionCapture::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	USphereReflectionCaptureComponent* SphereComponent = Cast<USphereReflectionCaptureComponent>(GetCaptureComponent());
	check(SphereComponent);
	const FVector ModifiedScale = DeltaScale * ( AActor::bUsePercentageBasedScaling ? 5000.0f : 50.0f );
	FMath::ApplyScaleToFloat(SphereComponent->InfluenceRadius, ModifiedScale);
	GetCaptureComponent()->SetCaptureIsDirty();
	PostEditChange();
}

void APlaneReflectionCapture::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	UPlaneReflectionCaptureComponent* PlaneComponent = Cast<UPlaneReflectionCaptureComponent>(GetCaptureComponent());
	check(PlaneComponent);
	const FVector ModifiedScale = DeltaScale * ( AActor::bUsePercentageBasedScaling ? 5000.0f : 50.0f );
	FMath::ApplyScaleToFloat(PlaneComponent->InfluenceRadiusScale, ModifiedScale);
	GetCaptureComponent()->SetCaptureIsDirty();
	PostEditChange();
}
#endif

ABoxReflectionCapture::ABoxReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBoxReflectionCaptureComponent>(TEXT("NewReflectionComponent")))
{
	UBoxReflectionCaptureComponent* BoxComponent = CastChecked<UBoxReflectionCaptureComponent>(GetCaptureComponent());
	BoxComponent->RelativeScale3D = FVector(1000, 1000, 400);
	RootComponent = BoxComponent;
#if WITH_EDITORONLY_DATA
	if (GetSpriteComponent())
	{
		GetSpriteComponent()->SetupAttachment(BoxComponent);
	}
	if (GetCaptureOffsetComponent())
	{
		GetCaptureOffsetComponent()->SetupAttachment(BoxComponent);
	}
#endif	//WITH_EDITORONLY_DATA
	UBoxComponent* DrawInfluenceBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DrawBox0"));
	DrawInfluenceBox->SetupAttachment(GetCaptureComponent());
	DrawInfluenceBox->bDrawOnlyIfSelected = true;
	DrawInfluenceBox->bUseEditorCompositing = true;
	DrawInfluenceBox->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DrawInfluenceBox->InitBoxExtent(FVector(1, 1, 1));
	BoxComponent->PreviewInfluenceBox = DrawInfluenceBox;

	UBoxComponent* DrawCaptureBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DrawBox1"));
	DrawCaptureBox->SetupAttachment(GetCaptureComponent());
	DrawCaptureBox->bDrawOnlyIfSelected = true;
	DrawCaptureBox->bUseEditorCompositing = true;
	DrawCaptureBox->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DrawCaptureBox->ShapeColor = FColor(100, 90, 40);
	DrawCaptureBox->InitBoxExtent(FVector(1, 1, 1));
	BoxComponent->PreviewCaptureBox = DrawCaptureBox;
}

APlaneReflectionCapture::APlaneReflectionCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPlaneReflectionCaptureComponent>(TEXT("NewReflectionComponent")))
{
	UPlaneReflectionCaptureComponent* PlaneComponent = CastChecked<UPlaneReflectionCaptureComponent>(GetCaptureComponent());
	PlaneComponent->RelativeScale3D = FVector(1, 1000, 1000);
	RootComponent = PlaneComponent;
#if WITH_EDITORONLY_DATA
	if (GetSpriteComponent())
	{
		GetSpriteComponent()->SetupAttachment(PlaneComponent);
	}
	if (GetCaptureOffsetComponent())
	{
		GetCaptureOffsetComponent()->SetupAttachment(PlaneComponent);
	}
#endif	//#if WITH_EDITORONLY_DATA
	UDrawSphereComponent* DrawInfluenceRadius = CreateDefaultSubobject<UDrawSphereComponent>(TEXT("DrawRadius0"));
	DrawInfluenceRadius->SetupAttachment(GetCaptureComponent());
	DrawInfluenceRadius->bDrawOnlyIfSelected = true;
	DrawInfluenceRadius->bAbsoluteScale = true;
	DrawInfluenceRadius->bUseEditorCompositing = true;
	DrawInfluenceRadius->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PlaneComponent->PreviewInfluenceRadius = DrawInfluenceRadius;

	UBoxComponent* DrawCaptureBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DrawBox1"));
	DrawCaptureBox->SetupAttachment(GetCaptureComponent());
	DrawCaptureBox->bDrawOnlyIfSelected = true;
	DrawCaptureBox->bUseEditorCompositing = true;
	DrawCaptureBox->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DrawCaptureBox->ShapeColor = FColor(100, 90, 40);
	DrawCaptureBox->InitBoxExtent(FVector(1, 1, 1));
	PlaneComponent->PreviewCaptureBox = DrawCaptureBox;
}

// Generate a new guid to force a recapture of all reflection data
// Note: changing this will cause saved capture data in maps to be discarded
// A resave of those maps will be required to guarantee valid reflections when cooking for ES2
FGuid ReflectionCaptureDDCVer(0x0c669396, 0x9cb849ae, 0x9f4120ff, 0x5812f4d3);

// Bumping this version will invalidate only encoded capture data
#define REFLECTIONCAPTURE_ENCODED_DERIVEDDATA_VER 1

FReflectionCaptureFullHDR::~FReflectionCaptureFullHDR()
{
	DEC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, CompressedCapturedData.GetAllocatedSize());
}

void FReflectionCaptureFullHDR::InitializeFromUncompressedData(const TArray<uint8>& UncompressedData, int32 InCubemapSize)
{
	CubemapSize = InCubemapSize;

	DEC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, CompressedCapturedData.GetAllocatedSize());

	int32 UncompressedSize = UncompressedData.Num() * UncompressedData.GetTypeSize();

	TArray<uint8> TempCompressedMemory;
	// Compressed can be slightly larger than uncompressed
	TempCompressedMemory.Empty(UncompressedSize * 4 / 3);
	TempCompressedMemory.AddUninitialized(UncompressedSize * 4 / 3);
	int32 CompressedSize = TempCompressedMemory.Num() * TempCompressedMemory.GetTypeSize();

	verify(FCompression::CompressMemory(
		(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory), 
		TempCompressedMemory.GetData(), 
		CompressedSize, 
		UncompressedData.GetData(), 
		UncompressedSize));

	// Note: change REFLECTIONCAPTURE_FULL_DERIVEDDATA_VER when modifying the serialization layout
	FMemoryWriter FinalArchive(CompressedCapturedData, true);
	FinalArchive << UncompressedSize;
	FinalArchive << CompressedSize;
	FinalArchive.Serialize(TempCompressedMemory.GetData(), CompressedSize);

	INC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, CompressedCapturedData.GetAllocatedSize());
}

TRefCountPtr<FReflectionCaptureUncompressedData> FReflectionCaptureFullHDR::GetUncompressedData() const
{
	// If we have serialized uncompressed data (from a cook), use it rather than uncompressing
	if (StoredUncompressedData)
	{
		return StoredUncompressedData;
	}
	else
	{
		check(CompressedCapturedData.Num() > 0);
		FMemoryReader Ar(CompressedCapturedData);

		// Note: change REFLECTIONCAPTURE_FULL_DERIVEDDATA_VER when modifying the serialization layout
		int32 UncompressedSize;
		Ar << UncompressedSize;

		int32 CompressedSize;
		Ar << CompressedSize;

		TRefCountPtr<FReflectionCaptureUncompressedData> UncompressedDataOut = new FReflectionCaptureUncompressedData(UncompressedSize);
		const uint8* SourceData = &CompressedCapturedData[Ar.Tell()];
		verify(FCompression::UncompressMemory((ECompressionFlags)COMPRESS_ZLIB, UncompressedDataOut->GetData(), UncompressedSize, SourceData, CompressedSize));
		return UncompressedDataOut;
	}
}

FColor RGBMEncode( FLinearColor Color )
{
	FColor Encoded;

	// Convert to gamma space
	Color.R = FMath::Sqrt( Color.R );
	Color.G = FMath::Sqrt( Color.G );
	Color.B = FMath::Sqrt( Color.B );

	// Range
	Color /= 16.0f;
	
	float MaxValue = FMath::Max( FMath::Max(Color.R, Color.G), FMath::Max(Color.B, DELTA) );
	
	if( MaxValue > 0.75f )
	{
		// Fit to valid range by leveling off intensity
		float Tonemapped = ( MaxValue - 0.75 * 0.75 ) / ( MaxValue - 0.5 );
		Color *= Tonemapped / MaxValue;
		MaxValue = Tonemapped;
	}

	Encoded.A = FMath::Min( FMath::CeilToInt( MaxValue * 255.0f ), 255 );
	Encoded.R = FMath::RoundToInt( ( Color.R * 255.0f / Encoded.A ) * 255.0f );
	Encoded.G = FMath::RoundToInt( ( Color.G * 255.0f / Encoded.A ) * 255.0f );
	Encoded.B = FMath::RoundToInt( ( Color.B * 255.0f / Encoded.A ) * 255.0f );

	return Encoded;
}

// Based off of CubemapGen
// https://code.google.com/p/cubemapgen/

#define FACE_X_POS 0
#define FACE_X_NEG 1
#define FACE_Y_POS 2
#define FACE_Y_NEG 3
#define FACE_Z_POS 4
#define FACE_Z_NEG 5

#define EDGE_LEFT   0	 // u = 0
#define EDGE_RIGHT  1	 // u = 1
#define EDGE_TOP    2	 // v = 0
#define EDGE_BOTTOM 3	 // v = 1

#define CORNER_NNN  0
#define CORNER_NNP  1
#define CORNER_NPN  2
#define CORNER_NPP  3
#define CORNER_PNN  4
#define CORNER_PNP  5
#define CORNER_PPN  6
#define CORNER_PPP  7

// D3D cube map face specification
//   mapping from 3D x,y,z cube map lookup coordinates 
//   to 2D within face u,v coordinates
//
//   --------------------> U direction 
//   |                   (within-face texture space)
//   |         _____
//   |        |     |
//   |        | +Y  |
//   |   _____|_____|_____ _____
//   |  |     |     |     |     |
//   |  | -X  | +Z  | +X  | -Z  |
//   |  |_____|_____|_____|_____|
//   |        |     |
//   |        | -Y  |
//   |        |_____|
//   |
//   v   V direction
//      (within-face texture space)

// Index by [Edge][FaceOrEdge]
static const int32 CubeEdgeListA[12][2] =
{
	{FACE_X_POS, EDGE_LEFT},
	{FACE_X_POS, EDGE_RIGHT},
	{FACE_X_POS, EDGE_TOP},
	{FACE_X_POS, EDGE_BOTTOM},

	{FACE_X_NEG, EDGE_LEFT},
	{FACE_X_NEG, EDGE_RIGHT},
	{FACE_X_NEG, EDGE_TOP},
	{FACE_X_NEG, EDGE_BOTTOM},

	{FACE_Z_POS, EDGE_TOP},
	{FACE_Z_POS, EDGE_BOTTOM},
	{FACE_Z_NEG, EDGE_TOP},
	{FACE_Z_NEG, EDGE_BOTTOM}
};

static const int32 CubeEdgeListB[12][2] =
{
	{FACE_Z_POS, EDGE_RIGHT },
	{FACE_Z_NEG, EDGE_LEFT  },
	{FACE_Y_POS, EDGE_RIGHT },
	{FACE_Y_NEG, EDGE_RIGHT },

	{FACE_Z_NEG, EDGE_RIGHT },
	{FACE_Z_POS, EDGE_LEFT  },
	{FACE_Y_POS, EDGE_LEFT  },
	{FACE_Y_NEG, EDGE_LEFT  },

	{FACE_Y_POS, EDGE_BOTTOM },
	{FACE_Y_NEG, EDGE_TOP    },
	{FACE_Y_POS, EDGE_TOP    },
	{FACE_Y_NEG, EDGE_BOTTOM },
};

// Index by [Face][Corner]
static const int32 CubeCornerList[6][4] =
{
	{ CORNER_PPP, CORNER_PPN, CORNER_PNP, CORNER_PNN },
	{ CORNER_NPN, CORNER_NPP, CORNER_NNN, CORNER_NNP },
	{ CORNER_NPN, CORNER_PPN, CORNER_NPP, CORNER_PPP },
	{ CORNER_NNP, CORNER_PNP, CORNER_NNN, CORNER_PNN },
	{ CORNER_NPP, CORNER_PPP, CORNER_NNP, CORNER_PNP },
	{ CORNER_PPN, CORNER_NPN, CORNER_PNN, CORNER_NNN }
};

static void EdgeWalkSetup( bool ReverseDirection, int32 Edge, int32 MipSize, int32& EdgeStart, int32& EdgeStep )
{
	if( ReverseDirection )
	{
		switch( Edge )
		{
		case EDGE_LEFT:		//start at lower left and walk up
			EdgeStart = MipSize * (MipSize - 1);
			EdgeStep = -MipSize;
			break;
		case EDGE_RIGHT:	//start at lower right and walk up
			EdgeStart = MipSize * (MipSize - 1) + (MipSize - 1);
			EdgeStep = -MipSize;
			break;
		case EDGE_TOP:		//start at upper right and walk left
			EdgeStart = (MipSize - 1);
			EdgeStep = -1;
			break;
		case EDGE_BOTTOM:	//start at lower right and walk left
			EdgeStart = MipSize * (MipSize - 1) + (MipSize - 1);
			EdgeStep = -1;
			break;
		}            
	}
	else
	{
		switch( Edge )
		{
		case EDGE_LEFT:		//start at upper left and walk down
			EdgeStart = 0;
			EdgeStep = MipSize;
			break;
		case EDGE_RIGHT:	//start at upper right and walk down
			EdgeStart = (MipSize - 1);
			EdgeStep = MipSize;
			break;
		case EDGE_TOP:		//start at upper left and walk left
			EdgeStart = 0;
			EdgeStep = 1;
			break;
		case EDGE_BOTTOM:	//start at lower left and walk left
			EdgeStart = MipSize * (MipSize - 1);
			EdgeStep = 1;
			break;
		}
	}
}

void FReflectionCaptureEncodedHDRDerivedData::GenerateFromDerivedDataSource(const FReflectionCaptureFullHDR& FullHDRData, float Brightness)
{
	const int32 NumMips = FMath::CeilLogTwo(FullHDRData.CubemapSize) + 1;

	TRefCountPtr<FReflectionCaptureUncompressedData> SourceCubemapData = FullHDRData.GetUncompressedData();

	int32 SourceMipBaseIndex = 0;
	int32 DestMipBaseIndex = 0;

	CapturedData = new FReflectionCaptureUncompressedData(SourceCubemapData->Size() * sizeof(FColor) / sizeof(FFloat16Color));

	// Note: change REFLECTIONCAPTURE_ENCODED_DERIVEDDATA_VER when modifying the encoded data layout or contents

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const int32 SourceCubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);
		const int32 DestCubeFaceBytes = MipSize * MipSize * sizeof(FColor);

		const FFloat16Color*	MipSrcData = (const FFloat16Color*)SourceCubemapData->GetData(SourceMipBaseIndex);
		FColor*					MipDstData = (FColor*)CapturedData->GetData(DestMipBaseIndex);

#if MOBILE_AVERAGE_CUBEMAP_EDGES
		// Fix cubemap seams by averaging colors across edges
		int32 CornerTable[4] =
		{
			0,
			MipSize - 1,
			MipSize * (MipSize - 1),
			MipSize * (MipSize - 1) + MipSize - 1,
		};

		// Average corner colors
		FLinearColor AvgCornerColors[8];
		memset( AvgCornerColors, 0, sizeof( AvgCornerColors ) );
		for( int32 Face = 0; Face < CubeFace_MAX; Face++ )
		{
			const FFloat16Color* FaceSrcData = MipSrcData + Face * MipSize * MipSize;

			for( int32 Corner = 0; Corner < 4; Corner++ )
			{
				AvgCornerColors[ CubeCornerList[Face][Corner] ] += FLinearColor( FaceSrcData[ CornerTable[Corner] ] );
			}
		}

		// Encode corners
		for( int32 Face = 0; Face < CubeFace_MAX; Face++ )
		{
			FColor* FaceDstData = MipDstData + Face * MipSize * MipSize;

			for( int32 Corner = 0; Corner < 4; Corner++ )
			{
				const FLinearColor LinearColor = AvgCornerColors[ CubeCornerList[Face][Corner] ] / 3.0f;
				FaceDstData[ CornerTable[Corner] ] = RGBMEncode( LinearColor * Brightness );
			}
		}

		// Average edge colors
		for( int32 EdgeIndex = 0; EdgeIndex < 12; EdgeIndex++ )
		{
			int32 FaceA = CubeEdgeListA[ EdgeIndex ][0];
			int32 EdgeA = CubeEdgeListA[ EdgeIndex ][1];

			int32 FaceB = CubeEdgeListB[ EdgeIndex ][0];
			int32 EdgeB = CubeEdgeListB[ EdgeIndex ][1];

			const FFloat16Color*	FaceSrcDataA = MipSrcData + FaceA * MipSize * MipSize;
			FColor*					FaceDstDataA = MipDstData + FaceA * MipSize * MipSize;

			const FFloat16Color*	FaceSrcDataB = MipSrcData + FaceB * MipSize * MipSize;
			FColor*					FaceDstDataB = MipDstData + FaceB * MipSize * MipSize;

			int32 EdgeStartA = 0;
			int32 EdgeStepA = 0;
			int32 EdgeStartB = 0;
			int32 EdgeStepB = 0;

			EdgeWalkSetup( false, EdgeA, MipSize, EdgeStartA, EdgeStepA );
			EdgeWalkSetup( EdgeA == EdgeB || EdgeA + EdgeB == 3, EdgeB, MipSize, EdgeStartB, EdgeStepB );

			// Walk edge
			// Skip corners
			for( int32 Texel = 1; Texel < MipSize - 1; Texel++ )       
			{
				int32 EdgeTexelA = EdgeStartA + EdgeStepA * Texel;
				int32 EdgeTexelB = EdgeStartB + EdgeStepB * Texel;

				check( 0 <= EdgeTexelA && EdgeTexelA < MipSize * MipSize );
				check( 0 <= EdgeTexelB && EdgeTexelB < MipSize * MipSize );

				const FLinearColor EdgeColorA = FLinearColor( FaceSrcDataA[ EdgeTexelA ] );
				const FLinearColor EdgeColorB = FLinearColor( FaceSrcDataB[ EdgeTexelB ] );
				const FLinearColor AvgColor = 0.5f * ( EdgeColorA + EdgeColorB );
				
				FaceDstDataA[ EdgeTexelA ] = FaceDstDataB[ EdgeTexelB ] = RGBMEncode( AvgColor * Brightness );
			}
		}
#endif // MOBILE_AVERAGE_CUBEMAP_EDGES
		
		// Encode rest of texels
		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			const int32 FaceSourceIndex = SourceMipBaseIndex + CubeFace * SourceCubeFaceBytes;
			const int32 FaceDestIndex = DestMipBaseIndex + CubeFace * DestCubeFaceBytes;
			const FFloat16Color* FaceSourceData = (const FFloat16Color*)SourceCubemapData->GetData(FaceSourceIndex);
			FColor* FaceDestData = (FColor*)CapturedData->GetData(FaceDestIndex);

			// Convert each texel from linear space FP16 to RGBM FColor
			// Note: Brightness on the capture is baked into the encoded HDR data
			// Skip edges
			const int32 SkipEdges = MOBILE_AVERAGE_CUBEMAP_EDGES ? 1 : 0;

			for( int32 y = SkipEdges; y < MipSize - SkipEdges; y++ )
			{
				for( int32 x = SkipEdges; x < MipSize - SkipEdges; x++ )
				{
					int32 TexelIndex = x + y * MipSize;
					const FLinearColor LinearColor = FLinearColor( FaceSourceData[ TexelIndex ]) * Brightness;
					FaceDestData[ TexelIndex ] = RGBMEncode( LinearColor );
				}
			}
		}

		SourceMipBaseIndex += SourceCubeFaceBytes * CubeFace_MAX;
		DestMipBaseIndex += DestCubeFaceBytes * CubeFace_MAX;
	}
}

FString FReflectionCaptureEncodedHDRDerivedData::GetDDCKeyString(const FGuid& StateId, int32 CubemapDimension)
{
	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("REFL_ENC"),
		*ReflectionCaptureDDCVer.ToString(),
		*StateId.ToString().Append("_").Append(FString::FromInt(CubemapDimension)).Append("_").Append(FString::FromInt(REFLECTIONCAPTURE_ENCODED_DERIVEDDATA_VER))
		);
}

FReflectionCaptureEncodedHDRDerivedData::~FReflectionCaptureEncodedHDRDerivedData()
{
}

TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> FReflectionCaptureEncodedHDRDerivedData::GenerateEncodedHDRData(const FReflectionCaptureFullHDR& FullHDRData, const FGuid& StateId, float Brightness)
{
	TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> EncodedHDRData = new FReflectionCaptureEncodedHDRDerivedData();
	const FString KeyString = GetDDCKeyString(StateId, FullHDRData.CubemapSize);

	COOK_STAT(auto Timer = ReflectionCaptureCookStats::UsageStats.TimeSyncWork());
	bool DDCHit = GetDerivedDataCacheRef().GetSynchronous(*KeyString, EncodedHDRData->CapturedData->GetArray() );
	if (!DDCHit)
	{
		EncodedHDRData->GenerateFromDerivedDataSource(FullHDRData, Brightness);

		if (EncodedHDRData->CapturedData->Size() > 0)
		{
			GetDerivedDataCacheRef().Put(*KeyString, EncodedHDRData->CapturedData->GetArray() );
		}
	}
	EncodedHDRData->CapturedData->UpdateMemoryTracking();
	COOK_STAT(Timer.AddHitOrMiss(DDCHit ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, EncodedHDRData->CapturedData->Size()));

	check(EncodedHDRData->CapturedData->Size() > 0);
	return EncodedHDRData;
}

/** 
 * A cubemap texture resource that knows how to upload the packed capture data from a reflection capture. 
 * @todo - support texture streaming and compression
 */
class FReflectionTextureCubeResource : public FTexture
{
public:

	FReflectionTextureCubeResource() :
		Size(0),
		NumMips(0),
		Format(PF_Unknown)
	{}

	void SetupParameters(int32 InSize, int32 InNumMips, EPixelFormat InFormat, TRefCountPtr<FReflectionCaptureUncompressedData> InSourceData)
	{
		Size = InSize;
		NumMips = InNumMips;
		Format = InFormat;
		SourceData = InSourceData;
	}

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		TextureCubeRHI = RHICreateTextureCube(Size, Format, NumMips, 0, CreateInfo);
		TextureRHI = TextureCubeRHI;

		if (SourceData)
		{
			check(SourceData->Size() > 0);

			const int32 BlockBytes = GPixelFormats[Format].BlockBytes;
			int32 MipBaseIndex = 0;

			for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				const int32 MipSize = 1 << (NumMips - MipIndex - 1);
				const int32 CubeFaceBytes = MipSize * MipSize * BlockBytes;

				for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
				{
					uint32 DestStride = 0;
					uint8* DestBuffer = (uint8*)RHILockTextureCubeFace(TextureCubeRHI, CubeFace, 0, MipIndex, RLM_WriteOnly, DestStride, false);

					// Handle DestStride by copying each row
					for (int32 Y = 0; Y < MipSize; Y++)
					{
						uint8* DestPtr = ((uint8*)DestBuffer + Y * DestStride);
						const int32 SourceIndex = MipBaseIndex + CubeFace * CubeFaceBytes + Y * MipSize * BlockBytes;
						const uint8* SourcePtr = SourceData->GetData(SourceIndex);
						FMemory::Memcpy(DestPtr, SourcePtr, MipSize * BlockBytes);
					}

					RHIUnlockTextureCubeFace(TextureCubeRHI, CubeFace, 0, MipIndex, false);
				}

				MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
			}

			if (!GIsEditor)
			{
				// Toss the source data now that we've created the cubemap
				// Note: can't do this if we ever use this texture resource in the editor and want to save the data later
				SourceData = nullptr;
			}
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
			SF_Trilinear,
			AM_Clamp,
			AM_Clamp,
			AM_Clamp
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

		INC_MEMORY_STAT_BY(STAT_ReflectionCaptureTextureMemory,CalcTextureSize(Size,Size,Format,NumMips) * 6);
	}

	virtual void ReleaseRHI() override
	{
		DEC_MEMORY_STAT_BY(STAT_ReflectionCaptureTextureMemory,CalcTextureSize(Size,Size,Format,NumMips) * 6);
		TextureCubeRHI.SafeRelease();
		FTexture::ReleaseRHI();
	}

	virtual uint32 GetSizeX() const override
	{
		return Size;
	}

	virtual uint32 GetSizeY() const override //-V524
	{
		return Size;
	}

	FTextureRHIParamRef GetTextureRHI() 
	{
		return TextureCubeRHI;
	}

private:

	int32 Size;
	int32 NumMips;
	EPixelFormat Format;
	FTextureCubeRHIRef TextureCubeRHI;

	// Source data. Note that this is owned by the cubemap
	TRefCountPtr<FReflectionCaptureUncompressedData> SourceData;
};


TArray<UReflectionCaptureComponent*> UReflectionCaptureComponent::ReflectionCapturesToUpdate;
TArray<UReflectionCaptureComponent*> UReflectionCaptureComponent::ReflectionCapturesToUpdateForLoad;
FCriticalSection UReflectionCaptureComponent::ReflectionCapturesToUpdateForLoadLock;

UReflectionCaptureComponent::UReflectionCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Brightness = 1;
	// Shouldn't be able to change reflection captures at runtime
	Mobility = EComponentMobility::Static;

	bCaptureDirty = false;
	bDerivedDataDirty = false;
	bLoadedCookedData = false;
	AverageBrightness = 1.0f;
}

void UReflectionCaptureComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	UpdatePreviewShape();

	if (ShouldRender())
	{
		GetWorld()->Scene->AddReflectionCapture(this);
	}
}

void UReflectionCaptureComponent::SendRenderTransform_Concurrent()
{	
	// Don't update the transform of a component that needs to be recaptured,
	// Otherwise the RT will get the new transform one frame before the capture
	if (!bCaptureDirty)
	{
		UpdatePreviewShape();

		if (ShouldRender())
		{
			GetWorld()->Scene->UpdateReflectionCaptureTransform(this);
		}
	}

	Super::SendRenderTransform_Concurrent();
}

void UReflectionCaptureComponent::OnRegister()
{
	Super::OnRegister();

	UWorld* World = GetWorld();
	if (World->FeatureLevel < ERHIFeatureLevel::SM4)
	{
		if (EncodedHDRDerivedData == nullptr)
		{
			World->NumInvalidReflectionCaptureComponents+= 1;
		}
	}
}

void UReflectionCaptureComponent::OnUnregister()
{
	UWorld* World = GetWorld();
	if (World->FeatureLevel < ERHIFeatureLevel::SM4)
	{
		if (EncodedHDRDerivedData == nullptr && World->NumInvalidReflectionCaptureComponents > 0)
		{
			World->NumInvalidReflectionCaptureComponents-= 1;
		}
	}

	Super::OnUnregister();
}

void UReflectionCaptureComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	GetWorld()->Scene->RemoveReflectionCapture(this);
}

void UReflectionCaptureComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Create a new guid in case this is a newly created component
	// If not, this guid will be overwritten when serialized
	FPlatformMisc::CreateGuid(StateId);

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		FScopeLock Lock(&ReflectionCapturesToUpdateForLoadLock);
		ReflectionCapturesToUpdateForLoad.AddUnique(this);
		bCaptureDirty = true; 
	}
}

void UReflectionCaptureComponent::SerializeSourceData(FArchive& Ar)
{
	if (Ar.UE4Ver() >= VER_UE4_REFLECTION_DATA_IN_PACKAGES)
	{
		if (Ar.IsSaving())
		{
			Ar << ReflectionCaptureDDCVer;
			Ar << AverageBrightness;

			int32 StartOffset = Ar.Tell();
			Ar << StartOffset;
			
			bool bValid = FullHDRData != NULL;
			Ar << bValid;

			if (FullHDRData)
			{
				Ar << FullHDRData->CubemapSize;
				Ar << FullHDRData->CompressedCapturedData;
			}

			int32 EndOffset = Ar.Tell();
			Ar.Seek(StartOffset);
			Ar << EndOffset;
			Ar.Seek(EndOffset);
		}
		else if (Ar.IsLoading())
		{
			FGuid SavedVersion;
			Ar << SavedVersion;

			if (Ar.CustomVer(FRenderingObjectVersion::GUID) >= FRenderingObjectVersion::ReflectionCapturesStoreAverageBrightness)
			{
				Ar << AverageBrightness;
			}

			int32 EndOffset = 0;
			Ar << EndOffset;

			if (SavedVersion != ReflectionCaptureDDCVer)
			{
				// Guid version of saved source data doesn't match latest, skip the data
				// The skipping is done so we don't have to maintain legacy serialization code paths when changing the format
				Ar.Seek(EndOffset);
			}
			else
			{
				bool bValid = false;
				Ar << bValid;

				if (bValid)
				{
					FullHDRData = new FReflectionCaptureFullHDR();

					if (Ar.CustomVer(FRenderingObjectVersion::GUID) >= FRenderingObjectVersion::CustomReflectionCaptureResolutionSupport)
					{
						Ar << FullHDRData->CubemapSize;
					}
					else
					{
						FullHDRData->CubemapSize = 128;
					}

					Ar << FullHDRData->CompressedCapturedData;

					INC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, FullHDRData->CompressedCapturedData.GetAllocatedSize());
				}
			}
		}
	}
}

void UReflectionCaptureComponent::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UReflectionCaptureComponent::Serialize"), STAT_ReflectionCaptureComponent_Serialize, STATGROUP_LoadTime);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FLoadTimesObjectVersion::GUID);

	Super::Serialize(Ar);

	bool bCooked = false;

	if (Ar.UE4Ver() >= VER_UE4_REFLECTION_CAPTURE_COOKING)
	{
		bCooked = Ar.IsCooking() || bLoadedCookedData;
		// Save a bool indicating whether this is cooked data
		// This is needed when loading cooked data, to know to serialize differently
		Ar << bCooked;

		// Save the cooked bool in a member so that if this object was loaded with cooked data,
		// it can be saved correctly later, such as if it needs to be duplicated.
		if (Ar.IsLoading())
		{
			bLoadedCookedData = bCooked;
		}
	}

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogMaterial, Fatal, TEXT("This platform requires cooked packages, and this reflection capture does not contain cooked data %s."), *GetName());
	}

	if (bCooked)
	{
		static FName FullHDR(TEXT("FullHDR"));
		static FName EncodedHDR(TEXT("EncodedHDR"));

		// Saving for cooking, or previously loaded cooked data
		if (Ar.IsSaving())
		{
			Ar << AverageBrightness;

			TArray<FName> Formats;

			// Get all the reflection capture formats that the target platform wants
			if (Ar.IsCooking())
			{
				// Get all the reflection capture formats that the target platform wants
				Ar.CookingTarget()->GetReflectionCaptureFormats(Formats);
			}
			else
			{
				// Get the reflection capture formats that were loaded from cooked data
				Formats = LoadedFormats;
			}

			int32 NumFormats = Formats.Num();
			Ar << NumFormats;

			for (int32 FormatIndex = 0; FormatIndex < NumFormats; FormatIndex++)
			{
				FName CurrentFormat = Formats[FormatIndex];

				Ar << CurrentFormat;

				if (CurrentFormat == FullHDR)
				{
					// FullHDRData would have been set in PostLoad during cooking
					// Can't generate it if missing, since that requires rendering the scene
					bool bValid = FullHDRData != NULL;

					Ar << bValid;

					if (FullHDRData)
					{
						Ar << FullHDRData->CubemapSize;

						// Raw data needs to be uncompressed on cooked platforms to avoid decompression hitches
						TRefCountPtr<FReflectionCaptureUncompressedData> UncompressedData = FullHDRData->GetUncompressedData();
						Ar << UncompressedData->GetArray();
					}
				}
				else
				{
					check(CurrentFormat == EncodedHDR);

					TRefCountPtr<FReflectionCaptureEncodedHDRDerivedData> EncodedHDRData;
					
					// FullHDRData would have been set in PostLoad during cooking
					// Generate temporary encoded HDR data for saving
					if (FullHDRData != NULL && Ar.IsCooking())
					{
						EncodedHDRData = FReflectionCaptureEncodedHDRDerivedData::GenerateEncodedHDRData(*FullHDRData, StateId, Brightness);
					}
					
					bool bValid = EncodedHDRData != NULL;

					Ar << bValid;

					if (bValid)
					{
						Ar << EncodedHDRData->CapturedData->GetArray();
					}
					else if (!IsTemplate())
					{
						// Temporary warning until the cooker can do scene captures itself in the case of missing DDC
						UE_LOG(LogMaterial, Warning, TEXT("Reflection capture requires encoded HDR data but none was found in the DDC!  This reflection will be black."));
						UE_LOG(LogMaterial, Warning, TEXT("Fix by resaving the map in the editor.  %s."), *GetFullName());
					}
				}
			}
		}
		else
		{
			// Loading cooked data path
			Ar << AverageBrightness;

			int32 NumFormats = 0;
			Ar << NumFormats;

			LoadedFormats.SetNum(NumFormats);

			for (int32 FormatIndex = 0; FormatIndex < NumFormats; FormatIndex++)
			{
				FName CurrentFormat;
				Ar << CurrentFormat;
				LoadedFormats[FormatIndex] = CurrentFormat;

				bool bValid = false;
				Ar << bValid;

				if (bValid)
				{
					if (CurrentFormat == FullHDR)
					{
						FullHDRData = new FReflectionCaptureFullHDR();

						Ar << FullHDRData->CubemapSize;
						if (Ar.CustomVer(FLoadTimesObjectVersion::GUID) >= FLoadTimesObjectVersion::UncompressedReflectionCapturesForCookedBuilds)
						{
							// Raw data needs to be uncompressed on cooked platforms to avoid hitches
							FullHDRData->StoredUncompressedData = new FReflectionCaptureUncompressedData();
							Ar << FullHDRData->StoredUncompressedData->GetArray();
							FullHDRData->StoredUncompressedData->UpdateMemoryTracking();
						}
						else
						{
							Ar << FullHDRData->CompressedCapturedData;
							INC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, FullHDRData->CompressedCapturedData.GetAllocatedSize());
						}
					}
					else 
					{
						check(CurrentFormat == EncodedHDR);
						EncodedHDRDerivedData = new FReflectionCaptureEncodedHDRDerivedData();
						Ar << EncodedHDRDerivedData->CapturedData->GetArray();
						EncodedHDRDerivedData->CapturedData->UpdateMemoryTracking();
					}
				}
				else if (CurrentFormat == EncodedHDR)
				{
					// Temporary warning until the cooker can do scene captures itself in the case of missing DDC
					UE_LOG(LogMaterial, Error, TEXT("Reflection capture was loaded without any valid capture data and will be black.  This can happen if the DDC was not up to date during cooking."));
					UE_LOG(LogMaterial, Error, TEXT("Load the map in the editor once before cooking to fix.  %s."), *GetFullName());
				}
			}
		}
	}
	else
	{
		SerializeSourceData(Ar);
	}
}

void UReflectionCaptureComponent::PostLoad()
{
	Super::PostLoad();

	bool bRetainAllFeatureLevelData = GIsEditor && GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4;
	bool bEncodedDataRequired = bRetainAllFeatureLevelData || (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2 || GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1);
	bool bFullDataRequired = GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4;
	const int32 ReflectionCaptureSize = UReflectionCaptureComponent::GetReflectionCaptureSize_GameThread();

	// If we're loading on a platform that doesn't require cooked data, attempt to load missing data from the DDC
	if (!FPlatformProperties::RequiresCookedData())
	{
		// if we don't have the FullHDRData then recapture it since we are on a platform that can capture it.
		if (!FullHDRData || 
			FullHDRData->CubemapSize != ReflectionCaptureSize || 
			(EncodedHDRDerivedData && FullHDRData->CubemapSize != EncodedHDRDerivedData->CalculateCubemapDimension()))
		{
			bDerivedDataDirty = true;
			delete FullHDRData;
			FullHDRData = nullptr;
			EncodedHDRDerivedData = nullptr;
		}

		// If we have full HDR data but not encoded HDR data, generate the encoded data now
		if (FullHDRData 
			&& !EncodedHDRDerivedData 
			&& bEncodedDataRequired)
		{
			EncodedHDRDerivedData = FReflectionCaptureEncodedHDRDerivedData::GenerateEncodedHDRData(*FullHDRData, StateId, Brightness);
		}
	}
	
	// Initialize rendering resources for the current feature level, and toss data only needed by other feature levels (unless in editor mode, in which all feature level data should be resident.)
	if (FullHDRData && bFullDataRequired)
	{
		// Don't need encoded HDR data for rendering on this feature level
		INC_MEMORY_STAT_BY(STAT_ReflectionCaptureMemory, FullHDRData->CompressedCapturedData.GetAllocatedSize());

		if (GMaxRHIFeatureLevel == ERHIFeatureLevel::SM4)
		{
			SM4FullHDRCubemapTexture = new FReflectionTextureCubeResource();
			SM4FullHDRCubemapTexture->SetupParameters(FullHDRData->CubemapSize, FMath::CeilLogTwo(FullHDRData->CubemapSize) + 1, PF_FloatRGBA, FullHDRData->GetCapturedDataForSM4Load() );
			BeginInitResource(SM4FullHDRCubemapTexture);
		}

		if (!bEncodedDataRequired)
		{
			EncodedHDRDerivedData = nullptr;
		}
	}

	if (EncodedHDRDerivedData && bEncodedDataRequired)
	{
		int32 EncodedCubemapSize = EncodedHDRDerivedData->CalculateCubemapDimension();

		if (EncodedCubemapSize == ReflectionCaptureSize)
		{
			// Create a cubemap texture out of the encoded HDR data
			EncodedHDRCubemapTexture = new FReflectionTextureCubeResource();
			EncodedHDRCubemapTexture->SetupParameters(EncodedCubemapSize, FMath::CeilLogTwo(EncodedCubemapSize) + 1, PF_B8G8R8A8, EncodedHDRDerivedData->CapturedData);
			BeginInitResource(EncodedHDRCubemapTexture);
		}
		else
		{
			UE_LOG(
				LogMaterial,
				Error,
				TEXT("Encoded reflection caputure resolution and project setting mismatch.\n(Project Setting: %d, Encoded Reflection Capture: %d.\nReflection cubemaps will be unavailable and cooking will fail."), 
				CVarReflectionCaptureSize.GetValueOnGameThread(), 
				EncodedCubemapSize
				);
		}
		

		// free up the full hdr data if we no longer need it.
		if (FullHDRData && !bFullDataRequired)
		{
			delete FullHDRData;
			FullHDRData = NULL;
		}
	}
}

void UReflectionCaptureComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

	// This is done on save of the package, because this capture data can only be generated by the renderer
	// So we must make efforts to ensure that it is generated in the editor, because it can't be generated during cooking when missing
	// Note: This will only work when registered
	UWorld* World = GetWorld();
	if (World)
	{
		ReadbackFromGPU(World);
	}
}

void UReflectionCaptureComponent::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		// Reset the StateId on duplication since it needs to be unique for each capture.
		// PostDuplicate covers direct calls to StaticDuplicateObject, but not actor duplication (see PostEditImport)
		FPlatformMisc::CreateGuid(StateId);
	}
}

void UReflectionCaptureComponent::UpdateDerivedData(FReflectionCaptureFullHDR* NewDerivedData)
{
#if UE_SERVER
	delete FullHDRData;
#else
	if (FullHDRData)
	{
		// Delete the derived data on the rendering thread, since the rendering thread may be reading from its contents in FScene::UpdateReflectionCaptureContents
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
			DeleteCaptureDataCommand,
			FReflectionCaptureFullHDR*, FullHDRData, FullHDRData,
		{
			delete FullHDRData;
		});
	}
#endif

	FullHDRData = NewDerivedData;
}

FReflectionCaptureProxy* UReflectionCaptureComponent::CreateSceneProxy()
{
	return new FReflectionCaptureProxy(this);
}

void UReflectionCaptureComponent::UpdatePreviewShape() 
{
	if (CaptureOffsetComponent)
	{
		CaptureOffsetComponent->RelativeLocation = CaptureOffset / GetComponentTransform().GetScale3D();
	}
}

#if WITH_EDITOR
bool UReflectionCaptureComponent::CanEditChange(const UProperty* Property) const
{
	bool bCanEditChange = Super::CanEditChange(Property);

	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UReflectionCaptureComponent, Cubemap) ||
		Property->GetFName() == GET_MEMBER_NAME_CHECKED(UReflectionCaptureComponent, SourceCubemapAngle))
	{
		bCanEditChange &= ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap;
	}

	return bCanEditChange;
}

void UReflectionCaptureComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property
		&& (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UReflectionCaptureComponent, bVisible)
			|| (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UReflectionCaptureComponent, CaptureOffset))))
	{
		SetCaptureIsDirty();
	}
}

void UReflectionCaptureComponent::PostEditImport()
{
	Super::PostEditImport(); 

	// Generate a new StateId.  This is needed to cover actor duplication through alt-drag or copy-paste.
	SetCaptureIsDirty();
}
#endif // WITH_EDITOR

void UReflectionCaptureComponent::BeginDestroy()
{
	// Deregister the component from the update queue
	if (bCaptureDirty)
	{
		FScopeLock Lock(&ReflectionCapturesToUpdateForLoadLock);
		ReflectionCapturesToUpdate.Remove(this);
		ReflectionCapturesToUpdateForLoad.Remove(this);
	}

	// Have to do this because we can't use GetWorld in BeginDestroy
	for (TSet<FSceneInterface*>::TConstIterator SceneIt(GetRendererModule().GetAllocatedScenes()); SceneIt; ++SceneIt)
	{
		FSceneInterface* Scene = *SceneIt;
		Scene->ReleaseReflectionCubemap(this);
	}

	if (SM4FullHDRCubemapTexture)
	{
		BeginReleaseResource(SM4FullHDRCubemapTexture);
	}

	if (EncodedHDRCubemapTexture)
	{
		BeginReleaseResource(EncodedHDRCubemapTexture);
	}

	// Begin a fence to track the progress of the above BeginReleaseResource being completed on the RT
	ReleaseResourcesFence.BeginFence();

	Super::BeginDestroy();
}

bool UReflectionCaptureComponent::IsReadyForFinishDestroy()
{
	// Wait until the fence is complete before allowing destruction
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.IsFenceComplete();
}

void UReflectionCaptureComponent::FinishDestroy()
{
	UpdateDerivedData(NULL);

	if (SM4FullHDRCubemapTexture)
	{
		delete SM4FullHDRCubemapTexture;
		SM4FullHDRCubemapTexture = NULL;
	}

	if (EncodedHDRCubemapTexture)
	{
		delete EncodedHDRCubemapTexture;
		EncodedHDRCubemapTexture = NULL;
	}

	Super::FinishDestroy();
}

void UReflectionCaptureComponent::SetCaptureIsDirty() 
{ 
	if (bVisible)
	{
		UpdateDerivedData(NULL);
		FPlatformMisc::CreateGuid(StateId);
		bDerivedDataDirty = true;
		ReflectionCapturesToUpdate.AddUnique(this);
		bCaptureDirty = true; 
	}
}

void ReadbackFromSM4Cubemap_RenderingThread(FRHICommandListImmediate& RHICmdList, FReflectionTextureCubeResource* SM4FullHDRCubemapTexture, FReflectionCaptureFullHDR* OutDerivedData, int32 CubemapSize)
{
	const int32 EffectiveTopMipSize = CubemapSize;
	const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

	TArray<uint8> CaptureData;
	int32 CaptureDataSize = 0;

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			CaptureDataSize += MipSize * MipSize * sizeof(FFloat16Color);
		}
	}

	CaptureData.Empty(CaptureDataSize);
	CaptureData.AddZeroed(CaptureDataSize);
	int32 MipBaseIndex = 0;

	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		check(SM4FullHDRCubemapTexture->GetTextureRHI()->GetFormat() == PF_FloatRGBA);
		const int32 MipSize = 1 << (NumMips - MipIndex - 1);
		const int32 CubeFaceBytes = MipSize * MipSize * sizeof(FFloat16Color);

		for (int32 CubeFace = 0; CubeFace < CubeFace_MAX; CubeFace++)
		{
			TArray<FFloat16Color> SurfaceData;
			// Read each mip face
			//@todo - do this without blocking the GPU so many times
			//@todo - pool the temporary textures in RHIReadSurfaceFloatData instead of always creating new ones
			RHICmdList.ReadSurfaceFloatData(SM4FullHDRCubemapTexture->GetTextureRHI(), FIntRect(0, 0, MipSize, MipSize), SurfaceData, (ECubeFace)CubeFace, 0, MipIndex);
			const int32 DestIndex = MipBaseIndex + CubeFace * CubeFaceBytes;
			uint8* FaceData = &CaptureData[DestIndex];
			check(SurfaceData.Num() * SurfaceData.GetTypeSize() == CubeFaceBytes);
			FMemory::Memcpy(FaceData, SurfaceData.GetData(), CubeFaceBytes);
		}

		MipBaseIndex += CubeFaceBytes * CubeFace_MAX;
	}

	OutDerivedData->InitializeFromUncompressedData(CaptureData, EffectiveTopMipSize);
}

void ReadbackFromSM4Cubemap(FReflectionTextureCubeResource* SM4FullHDRCubemapTexture, FReflectionCaptureFullHDR* OutDerivedData, int32 CubemapSize)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER( 
		ReadbackReflectionData,
		FReflectionTextureCubeResource*, SM4FullHDRCubemapTexture, SM4FullHDRCubemapTexture,
		FReflectionCaptureFullHDR*, OutDerivedData, OutDerivedData,
		int32, CubemapSize, CubemapSize,
	{
		ReadbackFromSM4Cubemap_RenderingThread(RHICmdList, SM4FullHDRCubemapTexture, OutDerivedData, CubemapSize);
	});

	FlushRenderingCommands();
}

void UReflectionCaptureComponent::ReadbackFromGPU(UWorld* WorldToUpdate)
{
	if (WorldToUpdate == nullptr || WorldToUpdate->Scene == nullptr)
	{
		// This can happen during autosave
		return;
	}

	if (bDerivedDataDirty && (!IsRunningCommandlet() || IsAllowCommandletRendering()) && WorldToUpdate && WorldToUpdate->FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		FReflectionCaptureFullHDR* NewDerivedData = new FReflectionCaptureFullHDR();

		if (WorldToUpdate->FeatureLevel == ERHIFeatureLevel::SM4)
		{
			if (SM4FullHDRCubemapTexture)
			{
				ensure(SM4FullHDRCubemapTexture->GetSizeX() == SM4FullHDRCubemapTexture->GetSizeY());
				ReadbackFromSM4Cubemap(SM4FullHDRCubemapTexture, NewDerivedData, int32(SM4FullHDRCubemapTexture->GetSizeX()));
			}
		}
		else
		{
			WorldToUpdate->Scene->GetReflectionCaptureData(this, *NewDerivedData);
		}

		if (NewDerivedData->CompressedCapturedData.Num() > 0)
		{
			// Update our copy in memory
			UpdateDerivedData(NewDerivedData);
		}
		else
		{
			delete NewDerivedData;
		}
	}
}

void UReflectionCaptureComponent::UpdateReflectionCaptureContents(UWorld* WorldToUpdate)
{
	if (WorldToUpdate->Scene 
		// Don't capture and read back capture contents if we are currently doing async shader compiling
		// This will keep the update requests in the queue until compiling finishes
		// Note: this will also prevent uploads of cubemaps from DDC, which is unintentional
		&& (GShaderCompilingManager == NULL || !GShaderCompilingManager->IsCompiling()))
	{
		TArray<UReflectionCaptureComponent*> WorldCombinedCaptures;

		for (int32 CaptureIndex = ReflectionCapturesToUpdate.Num() - 1; CaptureIndex >= 0; CaptureIndex--)
		{
			UReflectionCaptureComponent* CaptureComponent = ReflectionCapturesToUpdate[CaptureIndex];

			if (!CaptureComponent->GetOwner() || WorldToUpdate->ContainsActor(CaptureComponent->GetOwner()))
			{
				WorldCombinedCaptures.Add(CaptureComponent);
				ReflectionCapturesToUpdate.RemoveAt(CaptureIndex);
			}
		}

		TArray<UReflectionCaptureComponent*> WorldCapturesToUpdateForLoad;

		if (ReflectionCapturesToUpdateForLoad.Num() > 0)
		{
			FScopeLock Lock(&ReflectionCapturesToUpdateForLoadLock);
			for (int32 CaptureIndex = ReflectionCapturesToUpdateForLoad.Num() - 1; CaptureIndex >= 0; CaptureIndex--)
			{
				UReflectionCaptureComponent* CaptureComponent = ReflectionCapturesToUpdateForLoad[CaptureIndex];

				if (!CaptureComponent->GetOwner() || WorldToUpdate->ContainsActor(CaptureComponent->GetOwner()))
				{
					WorldCombinedCaptures.Add(CaptureComponent);
					WorldCapturesToUpdateForLoad.Add(CaptureComponent);
					ReflectionCapturesToUpdateForLoad.RemoveAt(CaptureIndex);
				}
			}
		}

		const auto FeatureLevel = WorldToUpdate->Scene->GetFeatureLevel();

		if (FeatureLevel == ERHIFeatureLevel::SM4)
		{
			for (int32 CaptureIndex = 0; CaptureIndex < WorldCombinedCaptures.Num(); CaptureIndex++)
			{
				UReflectionCaptureComponent* ReflectionComponent = WorldCombinedCaptures[CaptureIndex];

				if (!ReflectionComponent->SM4FullHDRCubemapTexture)
				{
					const int32 ReflectionCaptureSize = UReflectionCaptureComponent::GetReflectionCaptureSize_GameThread();

					// Create the cubemap if it wasn't already - this happens when updating a reflection capture that doesn't have valid DDC
					ReflectionComponent->SM4FullHDRCubemapTexture = new FReflectionTextureCubeResource();
					ReflectionComponent->SM4FullHDRCubemapTexture->SetupParameters(ReflectionCaptureSize, FMath::CeilLogTwo(ReflectionCaptureSize) + 1, PF_FloatRGBA, nullptr);
					BeginInitResource(ReflectionComponent->SM4FullHDRCubemapTexture);
					ReflectionComponent->MarkRenderStateDirty();
				}
			}
		}

		WorldToUpdate->Scene->AllocateReflectionCaptures(WorldCombinedCaptures);

		if (FeatureLevel >= ERHIFeatureLevel::SM4 && !FPlatformProperties::RequiresCookedData())
			{
				for (int32 CaptureIndex = 0; CaptureIndex < WorldCapturesToUpdateForLoad.Num(); CaptureIndex++)
				{
					// Save the derived data for any captures that were dirty on load
					// This allows the derived data to get cached without having to resave a map
					WorldCapturesToUpdateForLoad[CaptureIndex]->ReadbackFromGPU(WorldToUpdate);
				}
			}
	}
}

#if WITH_EDITOR
void UReflectionCaptureComponent::PreFeatureLevelChange(ERHIFeatureLevel::Type PendingFeatureLevel)
{
	if (PendingFeatureLevel == ERHIFeatureLevel::ES2 || PendingFeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		// generate encoded hdr data for ES2 preview mode.
		UWorld* World = GetWorld();
		if (World != nullptr)
		{
			// Capture full hdr derived data first.
			ReadbackFromGPU(World);
		}

		if (FullHDRData)
		{
			EncodedHDRDerivedData = FReflectionCaptureEncodedHDRDerivedData::GenerateEncodedHDRData(*FullHDRData, StateId, Brightness);
			if (EncodedHDRCubemapTexture == nullptr)
			{
				EncodedHDRCubemapTexture = new FReflectionTextureCubeResource();
			}

			int32 EncodedCubemapSize = EncodedHDRDerivedData->CalculateCubemapDimension();

			EncodedHDRCubemapTexture->SetupParameters(EncodedCubemapSize, FMath::CeilLogTwo(EncodedCubemapSize) + 1, PF_B8G8R8A8, EncodedHDRDerivedData->CapturedData);
			BeginInitResource(EncodedHDRCubemapTexture);
		}
	}
	else
	{
		EncodedHDRDerivedData = nullptr;
		if (EncodedHDRCubemapTexture)
		{
			BeginReleaseResource(EncodedHDRCubemapTexture);
			FlushRenderingCommands();
			delete EncodedHDRCubemapTexture;
			EncodedHDRCubemapTexture = nullptr;
		}

		// for >= SM4 capture should be updated.
		SetCaptureIsDirty();
	}

	if (PendingFeatureLevel == ERHIFeatureLevel::SM4)
	{
		if (SM4FullHDRCubemapTexture == nullptr)
		{
			const int32 ReflectionCaptureSize = UReflectionCaptureComponent::GetReflectionCaptureSize_GameThread();

			SM4FullHDRCubemapTexture = new FReflectionTextureCubeResource();
			SM4FullHDRCubemapTexture->SetupParameters(ReflectionCaptureSize, FMath::CeilLogTwo(ReflectionCaptureSize) + 1, PF_FloatRGBA, nullptr);
			BeginInitResource(SM4FullHDRCubemapTexture);
		}
	}
	else
	{
		// release SM4 texture
		if (SM4FullHDRCubemapTexture != nullptr)
		{
			BeginReleaseResource(SM4FullHDRCubemapTexture);
			FlushRenderingCommands();
			delete SM4FullHDRCubemapTexture;
			SM4FullHDRCubemapTexture = nullptr;
		}
	}
}
#endif // WITH_EDITOR

USphereReflectionCaptureComponent::USphereReflectionCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InfluenceRadius = 3000;
}

void USphereReflectionCaptureComponent::UpdatePreviewShape()
{
	if (PreviewInfluenceRadius)
	{
		PreviewInfluenceRadius->InitSphereRadius(InfluenceRadius);
	}

	Super::UpdatePreviewShape();
}

#if WITH_EDITOR
void USphereReflectionCaptureComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && 
		(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USphereReflectionCaptureComponent, InfluenceRadius)))
	{
		SetCaptureIsDirty();
	}
}
#endif // WITH_EDITOR

UBoxReflectionCaptureComponent::UBoxReflectionCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BoxTransitionDistance = 100;
}

void UBoxReflectionCaptureComponent::UpdatePreviewShape()
{
	if (PreviewCaptureBox)
	{
		PreviewCaptureBox->InitBoxExtent(((GetComponentTransform().GetScale3D() - FVector(BoxTransitionDistance)) / GetComponentTransform().GetScale3D()).ComponentMax(FVector::ZeroVector));
	}

	Super::UpdatePreviewShape();
}

float UBoxReflectionCaptureComponent::GetInfluenceBoundingRadius() const
{
	return (GetComponentTransform().GetScale3D() + FVector(BoxTransitionDistance)).Size();
}

#if WITH_EDITOR
void UBoxReflectionCaptureComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBoxReflectionCaptureComponent, BoxTransitionDistance))
	{
		SetCaptureIsDirty();
	}
}
#endif // WITH_EDITOR

UPlaneReflectionCaptureComponent::UPlaneReflectionCaptureComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InfluenceRadiusScale = 2;
}

void UPlaneReflectionCaptureComponent::UpdatePreviewShape()
{
	if (PreviewInfluenceRadius)
	{
		PreviewInfluenceRadius->InitSphereRadius(GetInfluenceBoundingRadius());
	}

	Super::UpdatePreviewShape();
}

float UPlaneReflectionCaptureComponent::GetInfluenceBoundingRadius() const
{
	return FVector2D(GetComponentTransform().GetScale3D().Y, GetComponentTransform().GetScale3D().Z).Size() * InfluenceRadiusScale;
}

FReflectionCaptureProxy::FReflectionCaptureProxy(const UReflectionCaptureComponent* InComponent)
{
	PackedIndex = INDEX_NONE;
	CaptureOffset = InComponent->CaptureOffset;

	const USphereReflectionCaptureComponent* SphereComponent = Cast<const USphereReflectionCaptureComponent>(InComponent);
	const UBoxReflectionCaptureComponent* BoxComponent = Cast<const UBoxReflectionCaptureComponent>(InComponent);
	const UPlaneReflectionCaptureComponent* PlaneComponent = Cast<const UPlaneReflectionCaptureComponent>(InComponent);

	// Initialize shape specific settings
	Shape = EReflectionCaptureShape::Num;
	BoxTransitionDistance = 0;

	if (SphereComponent)
	{
		Shape = EReflectionCaptureShape::Sphere;
	}
	else if (BoxComponent)
	{
		Shape = EReflectionCaptureShape::Box;
		BoxTransitionDistance = BoxComponent->BoxTransitionDistance;
	}
	else if (PlaneComponent)
	{
		Shape = EReflectionCaptureShape::Plane;
	}
	else
	{
		check(0);
	}
	
	// Initialize common settings
	Component = InComponent;
	SM4FullHDRCubemap = InComponent->SM4FullHDRCubemapTexture;
	EncodedHDRCubemap = InComponent->EncodedHDRCubemapTexture;
	SetTransform(InComponent->GetComponentTransform().ToMatrixWithScale());
	InfluenceRadius = InComponent->GetInfluenceBoundingRadius();
	Brightness = InComponent->Brightness;
	Guid = GetTypeHash( Component->GetPathName() );
	AverageBrightness = 1.0f;

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FInitReflectionProxy,
		const float*,AverageBrightness,InComponent->GetAverageBrightnessPtr(),
		FReflectionCaptureProxy*,ReflectionCaptureProxy,this,
	{
		// Only access AverageBrightness on the RT, even though they belong to the UReflectionCaptureComponent, 
		// Because FScene::UpdateReflectionCaptureContents does not block the RT so the writes could still be in flight
		ReflectionCaptureProxy->InitializeAverageBrightness(*AverageBrightness);
	});
}

void FReflectionCaptureProxy::InitializeAverageBrightness(const float& InAverageBrightness)
{
	AverageBrightness = InAverageBrightness;
}

void FReflectionCaptureProxy::SetTransform(const FMatrix& InTransform)
{
	Position = InTransform.GetOrigin();
	BoxTransform = InTransform.Inverse();

	FVector ForwardVector(1.0f,0.0f,0.0f);
	FVector RightVector(0.0f,-1.0f,0.0f);
	const FVector4 PlaneNormal = InTransform.TransformVector(ForwardVector);

	// Normalize the plane
	ReflectionPlane = FPlane(Position, FVector(PlaneNormal).GetSafeNormal());
	const FVector ReflectionXAxis = InTransform.TransformVector(RightVector);
	const FVector ScaleVector = InTransform.GetScaleVector();
	BoxScales = ScaleVector;
	// Include the owner's draw scale in the axes
	ReflectionXAxisAndYScale = ReflectionXAxis.GetSafeNormal() * ScaleVector.Y;
	ReflectionXAxisAndYScale.W = ScaleVector.Y / ScaleVector.Z;
}
