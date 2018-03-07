// Copyright 2015 Kite & Lightning.  All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Components/SceneCaptureComponent2D.h"
#include "SceneCapturer.generated.h"

class IImageWrapperModule;

DECLARE_LOG_CATEGORY_EXTERN( LogStereoPanorama, Log, All );

DECLARE_STATS_GROUP( TEXT( "SP" ), STATGROUP_SP, STATCAT_Advanced );

DECLARE_CYCLE_STAT( TEXT( "SavePNG" ),         STAT_SPSavePNG,         STATGROUP_SP );
DECLARE_CYCLE_STAT( TEXT( "SampleSpherical" ), STAT_SPSampleSpherical, STATGROUP_SP );
DECLARE_CYCLE_STAT( TEXT( "ReadStrip" ),       STAT_SPReadStrip,       STATGROUP_SP );
DECLARE_CYCLE_STAT( TEXT( "FillAlpha" ),       STAT_SPFillAlpha,       STATGROUP_SP );


UENUM()
enum class ECaptureStep : uint8
{
	Reset,
    SetStartPosition,
	SetPosition,
	Read,
	Pause,
	Unpause
};


DECLARE_DELEGATE_TwoParams(FStereoCaptureDoneDelegate, const TArray<FColor>&, const TArray<FColor>&);


UCLASS()
class USceneCapturer 
	: public UObject
	, public FTickableGameObject
{
    GENERATED_BODY()

public:

    USceneCapturer();

    //NOTE: ikrimae: Adding this ctor hack to fix the 4.8p2 problem with hot reload macros calling empty constructors
    //               https://answers.unrealengine.com/questions/228042/48p2-compile-fails-on-class-with-non-default-const.html
    USceneCapturer(FVTableHelper& Helper);
    
public:

	//~ FTickableGameObject interface

	virtual void Tick( float DeltaTime ) override;

	virtual bool IsTickable() const override
	{ 
		return true; 
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return bIsTicking;
	}

	virtual UWorld* GetTickableGameObjectWorld() const override;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( USceneCapturer, STATGROUP_Tickables );
	}

public:

	void InitCaptureComponent( USceneCaptureComponent2D* CaptureComponent, float HFov, float VFov, EStereoscopicPass InStereoPass );

	void CaptureComponent( int32 CurrentHorizontalStep, int32 CurrentVerticalStep, FString Folder, USceneCaptureComponent2D* CaptureComponent, TArray<FColor>& Atlas );

	void CopyToUnprojAtlas( int32 CurrentHorizontalStep, int32 CurrentVerticalStep, TArray<FColor>& Atlas, TArray<FColor>& SurfaceData );

	TArray<FColor> SaveAtlas( FString Folder, const TArray<FColor>& SurfaceData );

	void SetPositionAndRotation( int32 CurrentHorizontalStep, int32 CurrentVerticalStep, int32 CaptureIndex );

	void ValidateParameters();

	void SetInitialState( int32 InStartFrame, int32 InEndFrame, FStereoCaptureDoneDelegate& InStereoCaptureDoneDelegate );

	void Reset();

public:

	IImageWrapperModule& ImageWrapperModule;

	bool bIsTicking;
	FDateTime OverallStartTime;
	FDateTime StartTime;

	FVector StartLocation;
	FRotator StartRotation;
	FString Timestamp;
    int32 StartFrame;
    int32 EndFrame;

	ECaptureStep CaptureStep;
	int32 CurrentFrameCount;

	int32 CaptureWidth;
	int32 CaptureHeight;

	int32 StripWidth;
    int32 StripHeight;

	class APlayerController* CapturePlayerController;
	class AGameModeBase* CaptureGameMode;

	TArray<USceneCaptureComponent2D*> LeftEyeCaptureComponents;
	TArray<USceneCaptureComponent2D*> RightEyeCaptureComponents;

	bool GetComponentSteps( int32 Step, int32& CurrentHorizontalStep, int32& CurrentVerticalStep )
	{
		if( Step < TotalSteps )
		{
			CurrentHorizontalStep = Step / NumberOfVerticalSteps;
			CurrentVerticalStep = Step - ( CurrentHorizontalStep * NumberOfVerticalSteps );
			return true;
		}

		return false;
	}

private:

    const float hAngIncrement;
    const float vAngIncrement;
    const float eyeSeparation;
    
    float sliceHFov;
    float sliceVFov;

    const int32 NumberOfHorizontalSteps;
    const int32 NumberOfVerticalSteps;

    int32 UnprojectedAtlasWidth;
    int32 UnprojectedAtlasHeight;

    const int32 SphericalAtlasWidth;
    const int32 SphericalAtlasHeight;

    int32 CurrentStep;
	int32 TotalSteps;

    TArray<FColor> UnprojectedLeftEyeAtlas;
    TArray<FColor> UnprojectedRightEyeAtlas;

    const bool bForceAlpha;

    const bool bEnableBilerp;
    const int32 SSMethod;
    const bool bOverrideInitialYaw;
    const float ForcedInitialYaw;
    const FString OutputDir;

    bool dbgMatchCaptureSliceFovToAtlasSliceFov;
    bool dbgDisableOffsetRotation;
	FString FrameDescriptors;

    FStereoCaptureDoneDelegate StereoCaptureDoneDelegate;
};
