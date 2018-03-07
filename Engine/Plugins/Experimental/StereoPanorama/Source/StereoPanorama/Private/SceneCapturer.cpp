// Copyright 2015 Kite & Lightning.  All rights reserved.

#include "SceneCapturer.h"
#include "StereoPanoramaManager.h"
#include "StereoPanorama.h"
#include "SceneCapturer.h"
#include "StereoCapturePawn.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "UnrealEngine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY( LogStereoPanorama );

//Rotated Grid Supersampling
const int32 maxNumSamples = 16;
struct SamplingPattern
{
    int numSamples;
    FVector2D ssOffsets[maxNumSamples];
};
const SamplingPattern g_ssPatterns[] =
{
    {
        1,
        {
            FVector2D(0, 0),
        }
    },
    {
        4,
        {
            FVector2D(0.125f, 0.625f),
            FVector2D(0.375f, 0.125f),
            FVector2D(0.625f, 0.875f),
            FVector2D(0.875f, 0.375f),
        }
    },
    {
        16,
        {
            FVector2D(0.125f, 0.125f),
            FVector2D(0.125f, 0.375f),
            FVector2D(0.125f, 0.625f),
            FVector2D(0.125f, 0.875f),
            FVector2D(0.375f, 0.125f),
            FVector2D(0.375f, 0.375f),
            FVector2D(0.375f, 0.625f),
            FVector2D(0.375f, 0.875f),
            FVector2D(0.625f, 0.125f),
            FVector2D(0.625f, 0.375f),
            FVector2D(0.625f, 0.625f),
            FVector2D(0.625f, 0.875f),
            FVector2D(0.875f, 0.125f),
            FVector2D(0.875f, 0.375f),
            FVector2D(0.875f, 0.625f),
            FVector2D(0.875f, 0.875f),
        }
    },

};

void USceneCapturer::InitCaptureComponent( USceneCaptureComponent2D* CaptureComponent, float HFov, float VFov, EStereoscopicPass InStereoPass )
{
	CaptureComponent->SetVisibility( true );
	CaptureComponent->SetHiddenInGame( false );

    CaptureComponent->CaptureStereoPass = InStereoPass;
    CaptureComponent->FOVAngle = FMath::Max( HFov, VFov );
    CaptureComponent->bCaptureEveryFrame = false;
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// NVCHANGE_BEGIN: Add VXGI
	CaptureComponent->bEnableVxgi = true;
	// NVCHANGE_END: Add VXGI

	const FName TargetName = MakeUniqueObjectName(this, UTextureRenderTarget2D::StaticClass(), TEXT("SceneCaptureTextureTarget"));
    CaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>(this, TargetName);
    //TODO: ikrimae: Not sure why the render target needs to be float to avoid banding. Seems like captures to this RT and then applies PP
    //               on top of it which causes degredation.
    CaptureComponent->TextureTarget->InitCustomFormat(CaptureWidth, CaptureHeight, PF_A16B16G16R16, false);
	CaptureComponent->TextureTarget->ClearColor = FLinearColor::Red;

	CaptureComponent->RegisterComponentWithWorld( GWorld );

	// UE4 cannot serialize an array of subobject pointers, so add these objects to the root
	CaptureComponent->AddToRoot();
}

USceneCapturer::USceneCapturer(FVTableHelper& Helper)
    : Super(Helper)
    , ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper")))
    , bIsTicking(false)
    , CapturePlayerController(NULL)
    , CaptureGameMode(NULL)
    , hAngIncrement(FStereoPanoramaManager::HorizontalAngularIncrement->GetFloat())
    , vAngIncrement(FStereoPanoramaManager::VerticalAngularIncrement->GetFloat())
    , eyeSeparation(FStereoPanoramaManager::EyeSeparation->GetFloat())
    , NumberOfHorizontalSteps((int32)(360.0f / hAngIncrement))
    , NumberOfVerticalSteps((int32)(180.0f / vAngIncrement) + 1) /* Need an extra b/c we only grab half of the top & bottom slices */
    , SphericalAtlasWidth(FStereoPanoramaManager::StepCaptureWidth->GetInt())
    , SphericalAtlasHeight(SphericalAtlasWidth / 2)
    , bForceAlpha(FStereoPanoramaManager::ForceAlpha->GetInt() != 0)
    , bEnableBilerp(FStereoPanoramaManager::EnableBilerp->GetInt() != 0)
    , SSMethod(FMath::Clamp<int32>(FStereoPanoramaManager::SuperSamplingMethod->GetInt(), 0, ARRAY_COUNT(g_ssPatterns)))
    , bOverrideInitialYaw(FStereoPanoramaManager::ShouldOverrideInitialYaw->GetInt() != 0)
    , ForcedInitialYaw(FRotator::ClampAxis(FStereoPanoramaManager::ForcedInitialYaw->GetFloat()))
    , OutputDir(FStereoPanoramaManager::OutputDir->GetString().IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("StereoPanorama") : FStereoPanoramaManager::OutputDir->GetString())
    , dbgDisableOffsetRotation(FStereoPanoramaManager::FadeStereoToZeroAtSides->GetInt() != 0)
{}

USceneCapturer::USceneCapturer()
	: ImageWrapperModule( FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName( "ImageWrapper" ) ) )
	, bIsTicking( false )
	, CapturePlayerController( NULL )
	, CaptureGameMode( NULL )
    , hAngIncrement( FStereoPanoramaManager::HorizontalAngularIncrement->GetFloat() )
    , vAngIncrement( FStereoPanoramaManager::VerticalAngularIncrement->GetFloat() )
    , eyeSeparation( FStereoPanoramaManager::EyeSeparation->GetFloat() )
    , NumberOfHorizontalSteps( ( int32 )( 360.0f / hAngIncrement ) )
    , NumberOfVerticalSteps( ( int32 )( 180.0f / vAngIncrement ) + 1 ) /* Need an extra b/c we only grab half of the top & bottom slices */
    , SphericalAtlasWidth( FStereoPanoramaManager::StepCaptureWidth->GetInt() )
    , SphericalAtlasHeight( SphericalAtlasWidth / 2)
    , bForceAlpha( FStereoPanoramaManager::ForceAlpha->GetInt() != 0 )
    , bEnableBilerp( FStereoPanoramaManager::EnableBilerp->GetInt() != 0 )
    , SSMethod( FMath::Clamp<int32>(FStereoPanoramaManager::SuperSamplingMethod->GetInt(), 0, ARRAY_COUNT(g_ssPatterns)) )
    , bOverrideInitialYaw( FStereoPanoramaManager::ShouldOverrideInitialYaw->GetInt() != 0 )
    , ForcedInitialYaw( FRotator::ClampAxis(FStereoPanoramaManager::ForcedInitialYaw->GetFloat()) )
    , OutputDir( FStereoPanoramaManager::OutputDir->GetString().IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("StereoPanorama") : FStereoPanoramaManager::OutputDir->GetString() )
    , dbgDisableOffsetRotation( FStereoPanoramaManager::FadeStereoToZeroAtSides->GetInt() != 0 )
{
    //NOTE: ikrimae: Keeping the old sampling mechanism just until we're sure the new way is always better
    dbgMatchCaptureSliceFovToAtlasSliceFov = false;

    float captureHFov = 0, captureVFov = 0;

    if (dbgMatchCaptureSliceFovToAtlasSliceFov)
    {
        //Slicing Technique 1: Match Capture Slice StripWidth to match the pixel dimensions of AtlasWidth/NumHorizSteps & s.t. stripwidth/stripheight fovs match hAngIncr & vAngIncr
        //                     Legacy technique but allows setting the strip width to match atlas slice width
        //                     Pretty wasteful and will break if CaptureHFov & hangIncr/vAngIncr diverge greatly b/c resultant texture will exceed GPU bounds
        //                     StripHeight is computed based on solving CpxV = CpxH * SpxV / SpxH
        //                                                               CpxV = CV   * SpxV / SV
        //                                                               captureVfov = 2 * atan( tan(captureHfov / 2) * (SpxV / SpxH) )
        sliceHFov = hAngIncrement;
        sliceVFov = vAngIncrement;

        //TODO: ikrimae: Also do a quick test to see if there are issues with setting fov to something really small ( < 1 degree)
        //               And it does. Current noted issues: screen space effects like SSAO, AA, SSR are all off
        //                                                  local eyeadaptation also causes problems. Should probably turn off all PostProcess effects
        //                                                  small fovs cause floating point errors in the sampling function (probably a bug b/c no thought put towards that)
        captureHFov = FStereoPanoramaManager::CaptureHorizontalFOV->GetFloat();

        ensure(captureHFov >= hAngIncrement);

        //TODO: ikrimae: In hindsight, there's no reason that strip size should be this at all. Just select a square FOV larger than hAngIncr & vAngIncr
        //               and then sample the resulting plane accordingly. Remember when updating to this to recheck the math in resample function. Might
        //               have made assumptions about capture slice dimensions matching the sample strips
        StripWidth = SphericalAtlasWidth / NumberOfHorizontalSteps;
        //The scenecapture cube won't allow horizontal & vertical fov to not match the aspect ratio so we have to compute the right dimensions here for square pixels
        StripHeight = StripWidth * FMath::Tan(FMath::DegreesToRadians(vAngIncrement / 2.0f)) / FMath::Tan(FMath::DegreesToRadians(hAngIncrement / 2.0f));

        const FVector2D slicePlaneDim = FVector2D(
            2.0f * FMath::Tan(FMath::DegreesToRadians(hAngIncrement) / 2.0f),
            2.0f * FMath::Tan(FMath::DegreesToRadians(vAngIncrement) / 2.0f));

        const float capturePlaneWidth = 2.0f * FMath::Tan(FMath::DegreesToRadians(captureHFov) / 2.0f);

        //TODO: ikrimae: This is just to let the rest of the existing code work. Sampling rate of the slice can be whatever.
        //      Ex: To match the highest sampling frequency of the spherical atlas, it should match the area of differential patch
        //      at ray direction of pixel(0,1) in the atlas

        //Need stripwidth/slicePlaneDim.X = capturewidth / capturePlaneDim.X
        CaptureWidth = capturePlaneWidth * StripWidth / slicePlaneDim.X;
        CaptureHeight = CaptureWidth * StripHeight / StripWidth;

        captureVFov = FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(captureHFov / 2.0f)) * CaptureHeight / CaptureWidth));

        //float dbgCapturePlaneDimY = 2.0f * FMath::Tan(FMath::DegreesToRadians(captureVFov) / 2.0f);
        //float dbgCaptureHeight = dbgCapturePlaneDimY * StripHeight / slicePlaneDim.Y;
    }
    else
    {
        //Slicing Technique 2: Each slice is a determined square FOV at a configured preset resolution.
        //                     Strip Width/Strip Height is determined based on hAngIncrement & vAngIncrement
        //                     Just make sure pixels/captureHFov >= pixels/hAngIncr && pixels/vAngIncr

        captureVFov = captureHFov = FStereoPanoramaManager::CaptureHorizontalFOV->GetFloat();
        sliceVFov   = sliceHFov   = captureHFov;

        ensure(captureHFov >= FMath::Max(hAngIncrement, vAngIncrement));
        
        //TODO: ikrimae: Re-do for floating point accuracy
        const FVector2D slicePlaneDim = FVector2D(
            2.0f * FMath::Tan(FMath::DegreesToRadians(hAngIncrement) / 2.0f),
            2.0f * FMath::Tan(FMath::DegreesToRadians(vAngIncrement) / 2.0f));

        const FVector2D capturePlaneDim = FVector2D(
            2.0f * FMath::Tan(FMath::DegreesToRadians(captureHFov) / 2.0f),
            2.0f * FMath::Tan(FMath::DegreesToRadians(captureVFov) / 2.0f));

        CaptureHeight = CaptureWidth = FStereoPanoramaManager::CaptureSlicePixelWidth->GetInt();

        StripWidth  = CaptureWidth  * slicePlaneDim.X / capturePlaneDim.X;
        StripHeight = CaptureHeight * slicePlaneDim.Y / capturePlaneDim.Y;

        //TODO: ikrimae: Come back and check for the actual right sampling rate
        check(StripWidth  >=  (SphericalAtlasWidth / NumberOfHorizontalSteps) && 
              StripHeight >= (SphericalAtlasHeight / NumberOfVerticalSteps));
        
        //Ensure Width/Height is always even
        StripWidth  += StripWidth & 1;
        StripHeight += StripHeight & 1;

    }

    UnprojectedAtlasWidth  = NumberOfHorizontalSteps * StripWidth;
    UnprojectedAtlasHeight = NumberOfVerticalSteps   * StripHeight;

    //NOTE: ikrimae: Ensure that the main gameview is > CaptureWidth x CaptureHeight. Bug in UE4 that won't re-alloc scene render targets to the correct size
    //               when the scenecapture component > current window render target. https://answers.unrealengine.com/questions/80531/scene-capture-2d-max-resolution.html
    //TODO: ikrimae: Ensure that r.SceneRenderTargetResizeMethod=2
    FSystemResolution::RequestResolutionChange(CaptureWidth, CaptureHeight, EWindowMode::Windowed);



	for( int CaptureIndex = 0; CaptureIndex < FStereoPanoramaManager::ConcurrentCaptures->GetInt(); CaptureIndex++ )
	{
		FString LeftCounter = FString::Printf( TEXT( "LeftEyeCaptureComponent_%04d" ), CaptureIndex );
		USceneCaptureComponent2D* LeftEyeCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>( *LeftCounter );
		InitCaptureComponent( LeftEyeCaptureComponent, captureHFov, captureVFov, EStereoscopicPass::eSSP_LEFT_EYE );
		LeftEyeCaptureComponents.Add( LeftEyeCaptureComponent );

		FString RightCounter = FString::Printf( TEXT( "RightEyeCaptureComponent_%04d" ), CaptureIndex );
		USceneCaptureComponent2D* RightEyeCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>( *RightCounter );
		InitCaptureComponent( RightEyeCaptureComponent, captureHFov, captureVFov, EStereoscopicPass::eSSP_RIGHT_EYE );
		RightEyeCaptureComponents.Add( RightEyeCaptureComponent );
	}

	CurrentStep = 0;
	TotalSteps = 0;
	FrameDescriptors = TEXT( "FrameNumber, GameClock, TimeTaken(s)" LINE_TERMINATOR );

	CaptureStep = ECaptureStep::Reset;
}

UWorld* USceneCapturer::GetTickableGameObjectWorld() const 
{
	if (LeftEyeCaptureComponents.Num() > 0)
	{
		if (LeftEyeCaptureComponents[0])
		{
			return LeftEyeCaptureComponents[0]->GetWorld();
		}
	}

	return nullptr;
}


void USceneCapturer::Reset()
{
	for( int CaptureIndex = 0; CaptureIndex < FStereoPanoramaManager::ConcurrentCaptures->GetInt(); CaptureIndex++ )
	{
		USceneCaptureComponent2D* LeftEyeCaptureComponent = LeftEyeCaptureComponents[CaptureIndex];
		USceneCaptureComponent2D* RightEyeCaptureComponent = RightEyeCaptureComponents[CaptureIndex];

		LeftEyeCaptureComponent->SetVisibility( false );
		LeftEyeCaptureComponent->SetHiddenInGame( true );
		
		// UE4 cannot serialize an array of subobject pointers, so work around the GC problems
		LeftEyeCaptureComponent->RemoveFromRoot();

		RightEyeCaptureComponent->SetVisibility( false );
		RightEyeCaptureComponent->SetHiddenInGame( true );
		
		// UE4 cannot serialize an array of subobject pointers, so work around the GC problems
		RightEyeCaptureComponent->RemoveFromRoot();
	}

	UnprojectedLeftEyeAtlas.Empty();
	UnprojectedRightEyeAtlas.Empty();
}

void USceneCapturer::SetPositionAndRotation( int32 CurrentHorizontalStep, int32 CurrentVerticalStep, int32 CaptureIndex )
{
	FRotator Rotation = StartRotation;
	Rotation.Yaw += CurrentHorizontalStep * hAngIncrement;
	Rotation.Pitch -= CurrentVerticalStep * vAngIncrement;

    Rotation = Rotation.Clamp();

	FVector Offset( 0.0f, eyeSeparation / 2.0f, 0.0f );
    if (dbgDisableOffsetRotation)
    {
        //For rendering near field objects, we don't rotate the capture components around the stereo pivot, but instead
        //around each capture component
        const auto rotAngleOffset = FRotator::ClampAxis(Rotation.Yaw - StartRotation.Yaw);
        float eyeSeparationDampeningFactor = 1.0f;
        if (rotAngleOffset <= 90.0f)
        {
            eyeSeparationDampeningFactor = FMath::Lerp(1.0f, 0.0f, rotAngleOffset / 90.0f);
        }
        else if (rotAngleOffset <= 270.0f)
        {
            eyeSeparationDampeningFactor = 0.0f;
        }
        else
        {
            eyeSeparationDampeningFactor = FMath::Lerp(0.0f, 1.0f, (rotAngleOffset - 270.0f) / 90.0f);
        }

        Offset = StartRotation.RotateVector(Offset * eyeSeparationDampeningFactor);
    }
    else
    {
        Offset = Rotation.RotateVector(Offset);
    }

	LeftEyeCaptureComponents[CaptureIndex]->SetWorldLocationAndRotation( StartLocation - Offset, Rotation );
    LeftEyeCaptureComponents[CaptureIndex]->CaptureSceneDeferred();
	RightEyeCaptureComponents[CaptureIndex]->SetWorldLocationAndRotation( StartLocation + Offset, Rotation );
    RightEyeCaptureComponents[CaptureIndex]->CaptureSceneDeferred();
}

void USceneCapturer::ValidateParameters()
{
	// Angular increment needs to be a factor of 360 to avoid seams i.e. 360 / angular increment needs to be a whole number
	if( ( int32 )( NumberOfHorizontalSteps * hAngIncrement ) != 360 )
	{
		UE_LOG( LogStereoPanorama, Warning, TEXT( "Horizontal angular step (%g) is not a factor of 360! This will lead to a seam between the start and end points" ), hAngIncrement );
	}

	if( ( int32 )( (NumberOfVerticalSteps - 1) * vAngIncrement ) != 180 )
	{
		UE_LOG( LogStereoPanorama, Warning, TEXT( "Vertical angular step (%g) is not a factor of 180! This will lead to a seam between the start and end points" ), vAngIncrement );
	}

	TotalSteps = NumberOfHorizontalSteps * NumberOfVerticalSteps;
    if( ( SphericalAtlasWidth & 1 ) != 0)
    {
        UE_LOG(LogStereoPanorama, Warning, TEXT("The Atlas Width (%d) must be even! Otherwise the Atlas height will not divide evenly."), SphericalAtlasWidth);
    }


	// The strip width needs to be an even number and a factor of the number of steps
	if( ( StripWidth & 1 ) != 0 )
	{
		UE_LOG( LogStereoPanorama, Warning, TEXT( "Strip width (%d) needs to be even to avoid bad offsets" ), StripWidth );
	}

	if( StripWidth * NumberOfHorizontalSteps != SphericalAtlasWidth )
	{
		UE_LOG( LogStereoPanorama, Warning, TEXT( "The number of horizontal steps (%d) needs to be a factor of the atlas width (%d)" ), NumberOfHorizontalSteps, SphericalAtlasWidth );
	}

    if ((StripHeight & 1) != 0)
    {
        UE_LOG(LogStereoPanorama, Warning, TEXT("Strip height (%d) needs to be even to avoid bad offsets"), StripHeight);
    }

    if (StripHeight * (NumberOfVerticalSteps - 1) != SphericalAtlasHeight)
	{
        UE_LOG(LogStereoPanorama, Warning, TEXT("The number of vertical steps (%d) needs to be a factor of the atlas height (%d)"), NumberOfVerticalSteps, SphericalAtlasHeight);
	}

    //TODO: ikrimae: Validate capturewidth & captureheight. Need to be even

	UE_LOG( LogStereoPanorama, Display, TEXT( "Stereo panoramic screenshot parameters" ) );
	UE_LOG( LogStereoPanorama, Display, TEXT( " ... capture size: %d x %d" ), CaptureWidth, CaptureHeight );
	UE_LOG( LogStereoPanorama, Display, TEXT( " ... spherical atlas size: %d x %d" ), SphericalAtlasWidth, SphericalAtlasHeight );
    UE_LOG( LogStereoPanorama, Display, TEXT( " ... intermediate atlas size: %d x %d" ), UnprojectedAtlasWidth, UnprojectedAtlasHeight );
	UE_LOG( LogStereoPanorama, Display, TEXT( " ... strip size: %d x %d" ), StripWidth, StripHeight );
	UE_LOG( LogStereoPanorama, Display, TEXT( " ... horizontal steps: %d at %g degrees" ), NumberOfHorizontalSteps, hAngIncrement );
	UE_LOG( LogStereoPanorama, Display, TEXT( " ... vertical steps: %d at %g degrees" ), NumberOfVerticalSteps, vAngIncrement );
}

void USceneCapturer::SetInitialState( int32 InStartFrame, int32 InEndFrame, FStereoCaptureDoneDelegate& InStereoCaptureDoneDelegate )
{
	if( bIsTicking )
	{
		UE_LOG( LogStereoPanorama, Warning, TEXT( "Already capturing a scene; concurrent captures are not allowed" ) );
		return;
	}

	CapturePlayerController = UGameplayStatics::GetPlayerController( GWorld, 0 );
	CaptureGameMode = UGameplayStatics::GetGameMode( GWorld );

	if( CaptureGameMode == NULL || CapturePlayerController == NULL )
	{
		UE_LOG( LogStereoPanorama, Warning, TEXT( "Missing GameMode or PlayerController" ) );
		return;
	}

	// Calculate the steps and validate they will produce good results
	ValidateParameters();

	// Setup starting criteria
    StartFrame        = InStartFrame;
    EndFrame          = InEndFrame;
	CurrentFrameCount = 0;
    CurrentStep       = 0;
    CaptureStep       = ECaptureStep::Unpause;

	Timestamp = FString::Printf( TEXT( "%s" ), *FDateTime::Now().ToString() );
	
	//SetStartPosition();

	// Create storage for atlas textures
    check( UnprojectedAtlasWidth * UnprojectedAtlasHeight <= MAX_int32 );
	UnprojectedLeftEyeAtlas.AddUninitialized(  UnprojectedAtlasWidth * UnprojectedAtlasHeight );
    UnprojectedRightEyeAtlas.AddUninitialized( UnprojectedAtlasWidth * UnprojectedAtlasHeight );

	StartTime        = FDateTime::UtcNow();
	OverallStartTime = StartTime;
	bIsTicking       = true;

    StereoCaptureDoneDelegate = InStereoCaptureDoneDelegate;
}

void USceneCapturer::CopyToUnprojAtlas( int32 CurrentHorizontalStep, int32 CurrentVerticalStep, TArray<FColor>& Atlas, TArray<FColor>& SurfaceData )
{
	int32 XOffset = StripWidth * CurrentHorizontalStep;
    int32 YOffset = StripHeight * CurrentVerticalStep;

	int32 StripSize = StripWidth * sizeof( FColor );
    for (int32 Y = 0; Y < StripHeight; Y++)
	{
        void* Destination = &Atlas[( ( Y + YOffset ) * UnprojectedAtlasWidth ) + XOffset];
		void* Source = &SurfaceData[StripWidth * Y];
		FMemory::Memcpy( Destination, Source, StripSize );
	}
}

TArray<FColor> USceneCapturer::SaveAtlas(FString Folder, const TArray<FColor>& SurfaceData)
{
	SCOPE_CYCLE_COUNTER( STAT_SPSavePNG );
	
    TArray<FColor> SphericalAtlas;
    SphericalAtlas.AddZeroed(SphericalAtlasWidth * SphericalAtlasHeight);

    const FVector2D slicePlaneDim = FVector2D(
        2.0f * FMath::Tan(FMath::DegreesToRadians(sliceHFov) / 2.0f),
        2.0f * FMath::Tan(FMath::DegreesToRadians(sliceVFov) / 2.0f));

    //For each direction,
    //    Find corresponding slice
    //    Calculate intersection of slice plane
    //    Calculate intersection UVs by projecting onto plane tangents
    //    Supersample that UV coordinate from the unprojected atlas
    {
        SCOPE_CYCLE_COUNTER(STAT_SPSampleSpherical);
        // Dump out how long the process took
        const FDateTime SamplingStartTime = FDateTime::UtcNow();
        UE_LOG(LogStereoPanorama, Log, TEXT("Sampling atlas..."));

        for (int32 y = 0; y < SphericalAtlasHeight; y++)
        {
            for (int32 x = 0; x < SphericalAtlasWidth; x++)
            {
                FLinearColor samplePixelAccum = FLinearColor(0, 0, 0, 0);

                //TODO: ikrimae: Seems that bilinear filtering sans supersampling is good enough. Supersampling sans bilerp seems best.
                //               After more tests, come back to optimize by folding supersampling in and remove this outer sampling loop.
                const auto& ssPattern = g_ssPatterns[SSMethod];

                for (int32 SampleCount = 0; SampleCount < ssPattern.numSamples; SampleCount++)
                {
                    const float sampleU = ((float)x + ssPattern.ssOffsets[SampleCount].X) / SphericalAtlasWidth;
                    const float sampleV = ((float)y + ssPattern.ssOffsets[SampleCount].Y) / SphericalAtlasHeight;

                    const float sampleTheta = sampleU * 360.0f;
                    const float samplePhi = sampleV * 180.0f;

                    const FVector sampleDir = FVector(
                        FMath::Sin(FMath::DegreesToRadians(samplePhi)) * FMath::Cos(FMath::DegreesToRadians(sampleTheta)),
                        FMath::Sin(FMath::DegreesToRadians(samplePhi)) * FMath::Sin(FMath::DegreesToRadians(sampleTheta)),
                        FMath::Cos(FMath::DegreesToRadians(samplePhi)));


                    //TODO: ikrimae: ugh, ugly.
                    const int32 sliceXIndex = FMath::TruncToInt(FRotator::ClampAxis(sampleTheta + hAngIncrement / 2.0f) / hAngIncrement);
                    int32 sliceYIndex = 0;

                    //Slice Selection = slice with max{sampleDir dot  sliceNormal }
                    {
                        float largestCosAngle = 0;
                        for (int VerticalStep = 0; VerticalStep < NumberOfVerticalSteps; VerticalStep++)
                        {
                            const FVector2D sliceCenterThetaPhi = FVector2D(
                                hAngIncrement * sliceXIndex,
                                vAngIncrement * VerticalStep);

                            //TODO: ikrimae: There has got to be a faster way. Rethink reparametrization later
                            const FVector sliceDir = FVector(
                                FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                                FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                                FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)));

                            const float cosAngle = sampleDir | sliceDir;

                            if (cosAngle > largestCosAngle)
                            {
                                largestCosAngle = cosAngle;
                                sliceYIndex = VerticalStep;
                            }
                        }
                    }


                    const FVector2D sliceCenterThetaPhi = FVector2D(
                        hAngIncrement * sliceXIndex,
                        vAngIncrement * sliceYIndex);

                    //TODO: ikrimae: Reparameterize with an inverse mapping (e.g. project from slice pixels onto final u,v coordinates.
                    //               Should make code simpler and faster b/c reduces to handful of sin/cos calcs per slice. 
                    //               Supersampling will be more difficult though.

                    const FVector sliceDir = FVector(
                        FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                        FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                        FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)));

                    const FPlane slicePlane = FPlane(sliceDir, -sliceDir);

                    //Tangents from partial derivatives of sphere equation
                    const FVector slicePlanePhiTangent = FVector(
                        FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                        FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                        -FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y))).GetSafeNormal();

                    //Should be reconstructed to get around discontinuity of theta tangent at nodal points
                    const FVector slicePlaneThetaTangent = (sliceDir ^ slicePlanePhiTangent).GetSafeNormal();
                    //const FVector slicePlaneThetaTangent = FVector(
                    //    -FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                    //    FMath::Sin(FMath::DegreesToRadians(sliceCenterThetaPhi.Y)) * FMath::Cos(FMath::DegreesToRadians(sliceCenterThetaPhi.X)),
                    //    0).SafeNormal();

                    check(!slicePlaneThetaTangent.IsZero() && !slicePlanePhiTangent.IsZero());

                    const double t = (double)-slicePlane.W / (sampleDir | sliceDir);
                    const FVector sliceIntersection = FVector(t * sampleDir.X, t * sampleDir.Y, t * sampleDir.Z);

                    //Calculate scalar projection of sliceIntersection onto tangent vectors. a dot b / |b| = a dot b when tangent vectors are normalized
                    //Then reparameterize to U,V of the sliceplane based on slice plane dimensions
                    const float sliceU = (sliceIntersection | slicePlaneThetaTangent) / slicePlaneDim.X;
                    const float sliceV = (sliceIntersection | slicePlanePhiTangent) / slicePlaneDim.Y;

                    check(sliceU >= -(0.5f + KINDA_SMALL_NUMBER) &&
                        sliceU <= (0.5f + KINDA_SMALL_NUMBER));

                    check(sliceV >= -(0.5f + KINDA_SMALL_NUMBER) &&
                        sliceV <= (0.5f + KINDA_SMALL_NUMBER));

                    //TODO: ikrimae: Supersample/bilinear filter
                    const int32 slicePixelX = FMath::TruncToInt(dbgMatchCaptureSliceFovToAtlasSliceFov ? sliceU * StripWidth : sliceU * CaptureWidth);
                    const int32 slicePixelY = FMath::TruncToInt(dbgMatchCaptureSliceFovToAtlasSliceFov ? sliceV * StripHeight : sliceV * CaptureHeight);

                    FLinearColor slicePixelSample;

                    if (bEnableBilerp)
                    {
                        //TODO: ikrimae: Clean up later; too tired now
                        const int32 sliceCenterPixelX = (sliceXIndex + 0.5f) * StripWidth;
                        const int32 sliceCenterPixelY = (sliceYIndex + 0.5f) * StripHeight;

                        const FIntPoint atlasSampleTL(sliceCenterPixelX + FMath::Clamp(slicePixelX    , -StripWidth/2, StripWidth/2), sliceCenterPixelY + FMath::Clamp(slicePixelY    , -StripHeight/2, StripHeight/2));
                        const FIntPoint atlasSampleTR(sliceCenterPixelX + FMath::Clamp(slicePixelX + 1, -StripWidth/2, StripWidth/2), sliceCenterPixelY + FMath::Clamp(slicePixelY    , -StripHeight/2, StripHeight/2));
                        const FIntPoint atlasSampleBL(sliceCenterPixelX + FMath::Clamp(slicePixelX    , -StripWidth/2, StripWidth/2), sliceCenterPixelY + FMath::Clamp(slicePixelY + 1, -StripHeight/2, StripHeight/2));
                        const FIntPoint atlasSampleBR(sliceCenterPixelX + FMath::Clamp(slicePixelX + 1, -StripWidth/2, StripWidth/2), sliceCenterPixelY + FMath::Clamp(slicePixelY + 1, -StripHeight/2, StripHeight/2));

                        const FColor pixelColorTL = SurfaceData[atlasSampleTL.Y * UnprojectedAtlasWidth + atlasSampleTL.X];
                        const FColor pixelColorTR = SurfaceData[atlasSampleTR.Y * UnprojectedAtlasWidth + atlasSampleTR.X];
                        const FColor pixelColorBL = SurfaceData[atlasSampleBL.Y * UnprojectedAtlasWidth + atlasSampleBL.X];
                        const FColor pixelColorBR = SurfaceData[atlasSampleBR.Y * UnprojectedAtlasWidth + atlasSampleBR.X];

                        const float fracX = FMath::Frac(dbgMatchCaptureSliceFovToAtlasSliceFov ? sliceU * StripWidth : sliceU * CaptureWidth);
                        const float fracY = FMath::Frac(dbgMatchCaptureSliceFovToAtlasSliceFov ? sliceV * StripHeight : sliceV * CaptureHeight);

                        //Reinterpret as linear (a.k.a dont apply srgb inversion)
                        slicePixelSample = FMath::BiLerp(
                            pixelColorTL.ReinterpretAsLinear(), pixelColorTR.ReinterpretAsLinear(),
                            pixelColorBL.ReinterpretAsLinear(), pixelColorBR.ReinterpretAsLinear(),
                            fracX, fracY);
                    }
                    else
                    {
                        const int32 sliceCenterPixelX = (sliceXIndex + 0.5f) * StripWidth;
                        const int32 sliceCenterPixelY = (sliceYIndex + 0.5f) * StripHeight;

                        const int32 atlasSampleX = sliceCenterPixelX + slicePixelX;
                        const int32 atlasSampleY = sliceCenterPixelY + slicePixelY;


                        slicePixelSample = SurfaceData[atlasSampleY * UnprojectedAtlasWidth + atlasSampleX].ReinterpretAsLinear();
                    }

                    samplePixelAccum += slicePixelSample;

                    ////Output color map of projections
                    //const FColor debugEquiColors[12] = {
                    //    FColor(205, 180, 76),
                    //    FColor(190, 88, 202),
                    //    FColor(127, 185, 194),
                    //    FColor(90, 54, 47),
                    //    FColor(197, 88, 53),
                    //    FColor(197, 75, 124),
                    //    FColor(130, 208, 72),
                    //    FColor(136, 211, 153),
                    //    FColor(126, 130, 207),
                    //    FColor(83, 107, 59),
                    //    FColor(200, 160, 157),
                    //    FColor(80, 66, 106)
                    //};

                    //samplePixelAccum = ssPattern.numSamples * debugEquiColors[sliceYIndex * 4 + sliceXIndex];
                }

                SphericalAtlas[y * SphericalAtlasWidth + x] = (samplePixelAccum / ssPattern.numSamples).Quantize();

                // Force alpha value
                if (bForceAlpha)
                {
                    SphericalAtlas[y * SphericalAtlasWidth + x].A = 255;
                }
            }
        }

        //Blit the first column into the last column to make the stereo image seamless at theta=360
        for (int32 y = 0; y < SphericalAtlasHeight; y++)
        {
            SphericalAtlas[y * SphericalAtlasWidth + (SphericalAtlasWidth - 1)] = SphericalAtlas[y * SphericalAtlasWidth + 0];
        }

        const FTimespan SamplingDuration = FDateTime::UtcNow() - SamplingStartTime;
        UE_LOG(LogStereoPanorama, Log, TEXT("...done! Duration: %g seconds"), SamplingDuration.GetTotalSeconds());
    }
	
	// Generate name
	FString FrameString = FString::Printf( TEXT( "%s_%05d.png" ), *Folder, CurrentFrameCount );
    FString AtlasName =  OutputDir / Timestamp / FrameString;
    
	UE_LOG( LogStereoPanorama, Log, TEXT( "Writing atlas: %s" ), *AtlasName );

	// Write out PNG
    //TODO: ikrimae: Use threads to write out the images for performance
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
    ImageWrapper->SetRaw(SphericalAtlas.GetData(), SphericalAtlas.GetAllocatedSize(), SphericalAtlasWidth, SphericalAtlasHeight, ERGBFormat::BGRA, 8);
	const TArray<uint8>& PNGData = ImageWrapper->GetCompressed(100);
	FFileHelper::SaveArrayToFile( PNGData, *AtlasName );

    if (FStereoPanoramaManager::GenerateDebugImages->GetInt() != 0)
    {
        FString FrameStringUnprojected = FString::Printf(TEXT("%s_%05d_Unprojected.png"), *Folder, CurrentFrameCount);
        FString AtlasNameUnprojected = OutputDir / Timestamp / FrameStringUnprojected;

        ImageWrapper->SetRaw(SurfaceData.GetData(), SurfaceData.GetAllocatedSize(), UnprojectedAtlasWidth, UnprojectedAtlasHeight, ERGBFormat::BGRA, 8);
        const TArray<uint8>& PNGDataUnprojected = ImageWrapper->GetCompressed(100);
        FFileHelper::SaveArrayToFile(PNGData, *AtlasNameUnprojected);
    }
	ImageWrapper.Reset();

	UE_LOG( LogStereoPanorama, Log, TEXT( " ... done!" ), *AtlasName );

    return SphericalAtlas;
}

void USceneCapturer::CaptureComponent( int32 CurrentHorizontalStep, int32 CurrentVerticalStep, FString Folder, USceneCaptureComponent2D* CaptureComponent, TArray<FColor>& Atlas )
{
	TArray<FColor> SurfaceData;

	{
		SCOPE_CYCLE_COUNTER( STAT_SPReadStrip );
		FTextureRenderTargetResource* RenderTarget = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();

		//TODO: ikrimae: Might need to validate that this divides evenly. Might not matter
		int32 CenterX = CaptureWidth / 2;
		int32 CenterY = CaptureHeight / 2;

		SurfaceData.AddUninitialized( StripWidth * StripHeight );

		// Read pixels
		FIntRect Area( CenterX - ( StripWidth / 2 ), CenterY - ( StripHeight / 2 ), CenterX + ( StripWidth / 2 ), CenterY + ( StripHeight / 2) );
        auto readSurfaceDataFlags = FReadSurfaceDataFlags();
        readSurfaceDataFlags.SetLinearToGamma(false);
		RenderTarget->ReadPixelsPtr( SurfaceData.GetData(), readSurfaceDataFlags, Area );
	}

	// Copy off strip to atlas texture
	CopyToUnprojAtlas( CurrentHorizontalStep, CurrentVerticalStep, Atlas, SurfaceData );

	if( FStereoPanoramaManager::GenerateDebugImages->GetInt() != 0 )
	{
		SCOPE_CYCLE_COUNTER( STAT_SPSavePNG );

		// Generate name
		FString TickString = FString::Printf( TEXT( "_%05d_%04d_%04d" ), CurrentFrameCount, CurrentHorizontalStep, CurrentVerticalStep );
		FString CaptureName = OutputDir / Timestamp / Folder / TickString + TEXT( ".png" );
		UE_LOG( LogStereoPanorama, Log, TEXT( "Writing snapshot: %s" ), *CaptureName );

		// Write out PNG
        if (FStereoPanoramaManager::GenerateDebugImages->GetInt() == 2)
        {
            //Read Whole Capture Buffer
		    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );

            TArray<FColor> SurfaceDataWhole;
            SurfaceDataWhole.AddUninitialized(CaptureWidth * CaptureHeight);
            // Read pixels
            FTextureRenderTargetResource* RenderTarget = CaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();
            RenderTarget->ReadPixelsPtr(SurfaceDataWhole.GetData(), FReadSurfaceDataFlags());

            // Force alpha value
            if (bForceAlpha)
            {
                for (FColor& Color : SurfaceDataWhole)
                {
                    Color.A = 255;
                }
            }

            ImageWrapper->SetRaw(SurfaceDataWhole.GetData(), SurfaceDataWhole.GetAllocatedSize(), CaptureWidth, CaptureHeight, ERGBFormat::BGRA, 8);
            const TArray<uint8>& PNGData = ImageWrapper->GetCompressed(100);

            FFileHelper::SaveArrayToFile(PNGData, *CaptureName);
            ImageWrapper.Reset();
        }
        else
        {
            if (bForceAlpha)
            {
                for (FColor& Color : SurfaceData)
                {
                    Color.A = 255;
                }
            }

            TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
            ImageWrapper->SetRaw(SurfaceData.GetData(), SurfaceData.GetAllocatedSize(), StripWidth, StripHeight, ERGBFormat::BGRA, 8);
		    const TArray<uint8>& PNGData = ImageWrapper->GetCompressed(100);

		    FFileHelper::SaveArrayToFile( PNGData, *CaptureName );
		    ImageWrapper.Reset();
	    }
    }
}

//TODO: ikrimae: Come back and actually work out the timings. Trickery b/c SceneCaptureCubes Tick at the end of the frame so we're effectively queuing up the next
//               step (pause, unpause, setposition) for the next frame. FlushRenderingCommands() added haphazardly to test but didn't want to remove them so close to delivery. 
//               Think through when we actually need to flush and document.
void USceneCapturer::Tick( float DeltaTime )
{
	if( !bIsTicking )
	{
		return;
	}

    if ( CurrentFrameCount < StartFrame )
    {
        //Skip until we're at the frame we want to render
        CurrentFrameCount++;
        CaptureStep = ECaptureStep::Pause;
    }
	else if( CurrentStep < TotalSteps )
	{
        if (CaptureStep == ECaptureStep::Unpause)
        {
            FlushRenderingCommands();
            CaptureGameMode->ClearPause();
            //GPauseRenderingRealtimeClock = false;
            CaptureStep = ECaptureStep::Pause;
            FlushRenderingCommands();
        }
        else if (CaptureStep == ECaptureStep::Pause)
        {
            FlushRenderingCommands();
            CaptureGameMode->SetPause(CapturePlayerController);
            //GPauseRenderingRealtimeClock = true;
            CaptureStep = ECaptureStep::SetStartPosition;
            FlushRenderingCommands();
        }
        else if (CaptureStep == ECaptureStep::SetStartPosition)
        {
            //SetStartPosition();
            ENQUEUE_UNIQUE_RENDER_COMMAND(
                SceneCapturer_HeartbeatTickTickables,
            {
                TickRenderingTickables();
            });

            FlushRenderingCommands();
            
            FRotator Rotation;
            CapturePlayerController->GetPlayerViewPoint(StartLocation, Rotation);
            
            Rotation.Roll = 0.0f;
            Rotation.Yaw = (bOverrideInitialYaw) ? ForcedInitialYaw : Rotation.Yaw;
            Rotation.Pitch = 90.0f;
            StartRotation = Rotation;
            CaptureStep = ECaptureStep::SetPosition;
            FlushRenderingCommands();
        }
        else if (CaptureStep == ECaptureStep::SetPosition)
        {
            FlushRenderingCommands();
            for (int32 CaptureIndex = 0; CaptureIndex < FStereoPanoramaManager::ConcurrentCaptures->GetInt(); CaptureIndex++)
            {
                int32 CurrentHorizontalStep;
                int32 CurrentVerticalStep;
                if (GetComponentSteps(CurrentStep + CaptureIndex, CurrentHorizontalStep, CurrentVerticalStep))
                {
                    SetPositionAndRotation(CurrentHorizontalStep, CurrentVerticalStep, CaptureIndex);
                }
            }

            CaptureStep = ECaptureStep::Read;
            FlushRenderingCommands();
        }
        else if (CaptureStep == ECaptureStep::Read)
        {
            FlushRenderingCommands();
            for (int32 CaptureIndex = 0; CaptureIndex < FStereoPanoramaManager::ConcurrentCaptures->GetInt(); CaptureIndex++)
            {
                int32 CurrentHorizontalStep;
                int32 CurrentVerticalStep;
                if (GetComponentSteps(CurrentStep, CurrentHorizontalStep, CurrentVerticalStep))
                {
                    CaptureComponent(CurrentHorizontalStep, CurrentVerticalStep, TEXT("Left"), LeftEyeCaptureComponents[CaptureIndex], UnprojectedLeftEyeAtlas);
                    CaptureComponent(CurrentHorizontalStep, CurrentVerticalStep, TEXT("Right"), RightEyeCaptureComponents[CaptureIndex], UnprojectedRightEyeAtlas);

                    CurrentStep++;
                }
            }

            CaptureStep = ECaptureStep::SetPosition;
            FlushRenderingCommands();
        }
        else
        {
            //ECaptureStep::Reset:
		}
	}
	else
	{
		TArray<FColor> SphericalLeftEyeAtlas  = SaveAtlas( TEXT( "Left" ), UnprojectedLeftEyeAtlas );
        TArray<FColor> SphericalRightEyeAtlas = SaveAtlas(TEXT("Right"), UnprojectedRightEyeAtlas);
		
		// Dump out how long the process took
		FDateTime EndTime = FDateTime::UtcNow();
		FTimespan Duration = EndTime - StartTime;
		UE_LOG( LogStereoPanorama, Log, TEXT( "Duration: %g seconds for frame %d" ), Duration.GetTotalSeconds(), CurrentFrameCount );
		StartTime = EndTime;

        //NOTE: ikrimae: Since we can't synchronously finish a stereocapture, we have to notify the caller with a function pointer
        //Not sure this is the cleanest way but good enough for now
        StereoCaptureDoneDelegate.ExecuteIfBound(SphericalLeftEyeAtlas, SphericalRightEyeAtlas);

		// Construct log of saved atlases in csv format
		FrameDescriptors += FString::Printf( TEXT( "%d, %g, %g" LINE_TERMINATOR ), CurrentFrameCount, FApp::GetCurrentTime() - FApp::GetLastTime(), Duration.GetTotalSeconds() );

		CurrentFrameCount++;
		if( CurrentFrameCount <= EndFrame )
		{
            CurrentStep = 0;
			CaptureStep = ECaptureStep::Unpause;
		}
		else
		{
			CaptureGameMode->ClearPause();
            //GPauseRenderingRealtimeClock = false;

			FTimespan OverallDuration = FDateTime::UtcNow() - OverallStartTime;

            FrameDescriptors += FString::Printf(TEXT("Duration: %g minutes for frame range [%d,%d] "), OverallDuration.GetTotalMinutes(), StartFrame, EndFrame);;
			UE_LOG( LogStereoPanorama, Log, TEXT("Duration: %g minutes for frame range [%d,%d] "), OverallDuration.GetTotalMinutes(), StartFrame, EndFrame );

			FString FrameDescriptorName = OutputDir / Timestamp / TEXT( "Frames.txt" );
			FFileHelper::SaveStringToFile( FrameDescriptors, *FrameDescriptorName, FFileHelper::EEncodingOptions::ForceUTF8 );

			bIsTicking = false;
			FStereoPanoramaModule::Get()->Cleanup();
		}
	}
}
