// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapture.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Engine/World.h"
#include "Slate/SceneViewport.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ActiveMovieSceneCaptures.h"
#include "JsonObjectConverter.h"
#include "Misc/RemoteConfigIni.h"
#include "MovieSceneCaptureModule.h"

#define LOCTEXT_NAMESPACE "MovieSceneCapture"

struct FUniqueMovieSceneCaptureHandle : FMovieSceneCaptureHandle
{
	FUniqueMovieSceneCaptureHandle()
	{
		/// Start IDs at index 1 since 0 is deemed invalid
		static uint32 Unique = 1;
		ID = Unique++;
	}
};

FMovieSceneCaptureSettings::FMovieSceneCaptureSettings()
	: Resolution(1280, 720)
{
	OutputDirectory.Path = FPaths::VideoCaptureDir();
	FPaths::MakePlatformFilename( OutputDirectory.Path );

	bUseRelativeFrameNumbers = false;
	HandleFrames = 0;
	GameModeOverride = nullptr;
	OutputFormat = TEXT("{world}");
	FrameRate = 24;
	ZeroPadFrameNumbers = 4;
	bEnableTextureStreaming = false;
	bCinematicEngineScalability = true;
	bCinematicMode = true;
	bAllowMovement = false;
	bAllowTurning = false;
	bShowPlayer = false;
	bShowHUD = false;
}

UMovieSceneCapture::UMovieSceneCapture(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse( FCommandLine::Get(), Tokens, Switches );
	for (auto& Switch : Switches)
	{
		InheritedCommandLineArguments.AppendChar('-');
		InheritedCommandLineArguments.Append(Switch);
		InheritedCommandLineArguments.AppendChar(' ');
	}
	
	AdditionalCommandLineArguments += TEXT("-NOSCREENMESSAGES");

	Handle = FUniqueMovieSceneCaptureHandle();

	FrameCount = 0;
	bCapturing = false;
	FrameNumberOffset = 0;
	CaptureType = TEXT("Video");
}

void UMovieSceneCapture::PostInitProperties()
{
	InitializeSettings();

	Super::PostInitProperties();
}

void UMovieSceneCapture::InitializeSettings()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (ProtocolSettings)
	{
		ProtocolSettings->OnReleaseConfig(Settings);
	}
	
	ProtocolSettings = IMovieSceneCaptureModule::Get().GetProtocolRegistry().FactorySettingsType(CaptureType, this);
	if (ProtocolSettings)
	{
		ProtocolSettings->LoadConfig();
		ProtocolSettings->OnLoadConfig(Settings);
	}
}

void UMovieSceneCapture::Initialize(TSharedPtr<FSceneViewport> InSceneViewport, int32 PIEInstance)
{
	ensure(!bCapturing);

	// Apply command-line overrides
	{
		FString OutputPathOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieFolder=" ), OutputPathOverride ) )
		{
			Settings.OutputDirectory.Path = OutputPathOverride;
		}

		FString OutputNameOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieName=" ), OutputNameOverride ) )
		{
			Settings.OutputFormat = OutputNameOverride;
		}

		bool bOverrideOverwriteExisting;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieOverwriteExisting=" ), bOverrideOverwriteExisting ) )
		{
			Settings.bOverwriteExisting = bOverrideOverwriteExisting;
		}

		bool bOverrideRelativeFrameNumbers;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieRelativeFrames=" ), bOverrideRelativeFrameNumbers ) )
		{
			Settings.bUseRelativeFrameNumbers = bOverrideRelativeFrameNumbers;
		}
				
		int32 HandleFramesOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-HandleFrames=" ), HandleFramesOverride ) )
		{
			Settings.HandleFrames = HandleFramesOverride;
		}

		bool bOverrideCinematicEngineScalabilityMode;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieEngineScalabilityMode=" ), bOverrideCinematicEngineScalabilityMode ) )
		{
			Settings.bCinematicEngineScalability = bOverrideCinematicEngineScalabilityMode;
		}

		bool bOverrideCinematicMode;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieCinematicMode=" ), bOverrideCinematicMode ) )
		{
			Settings.bCinematicMode = bOverrideCinematicMode;
		}

		FString FormatOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieFormat=" ), FormatOverride ) )
		{
			CaptureType = *FormatOverride;
			InitializeSettings();
		}

		int32 FrameRateOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieFrameRate=" ), FrameRateOverride ) )
		{
			Settings.FrameRate = FrameRateOverride;
		}
	}

	bFinalizeWhenReady = false;

	InitSettings = FCaptureProtocolInitSettings::FromSlateViewport(InSceneViewport.ToSharedRef(), ProtocolSettings);

	CachedMetrics = FCachedMetrics();
	CachedMetrics.Width = InitSettings->DesiredSize.X;
	CachedMetrics.Height = InitSettings->DesiredSize.Y;

	FormatMappings.Reserve(10);
	FormatMappings.Add(TEXT("fps"), FString::Printf(TEXT("%d"), Settings.FrameRate));
	FormatMappings.Add(TEXT("width"), FString::Printf(TEXT("%d"), CachedMetrics.Width));
	FormatMappings.Add(TEXT("height"), FString::Printf(TEXT("%d"), CachedMetrics.Height));
	FormatMappings.Add(TEXT("world"), InSceneViewport->GetClient()->GetWorld()->GetName());

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FActiveMovieSceneCaptures::Get().Add(this);
	}
}

void UMovieSceneCapture::StartWarmup()
{
	if (Settings.bCinematicEngineScalability)
	{
		CachedQualityLevels = Scalability::GetQualityLevels();
		Scalability::FQualityLevels QualityLevels = CachedQualityLevels;
		QualityLevels.SetFromSingleQualityLevelRelativeToMax(0);
		Scalability::SetQualityLevels(QualityLevels);
	}

	check( !bCapturing );
	if( !CaptureStrategy.IsValid() )
	{
		CaptureStrategy = MakeShareable( new FRealTimeCaptureStrategy( Settings.FrameRate ) );
	}
	CaptureStrategy->OnWarmup();
}

void UMovieSceneCapture::StartCapture()
{
	bFinalizeWhenReady = false;
	bCapturing = true;

	if (!CaptureStrategy.IsValid())
	{
		CaptureStrategy = MakeShareable(new FRealTimeCaptureStrategy(Settings.FrameRate));
	}

	CaptureStrategy->OnStart();

	CaptureProtocol = IMovieSceneCaptureModule::Get().GetProtocolRegistry().Factory(CaptureType);
	if (ensure(CaptureProtocol.IsValid()))
	{
		CaptureProtocol->Initialize(InitSettings.GetValue(), *this);
	}
}

void UMovieSceneCapture::CaptureThisFrame(float DeltaSeconds)
{
	if (!bCapturing || !CaptureStrategy.IsValid() || !CaptureProtocol.IsValid() || bFinalizeWhenReady)
	{
		return;
	}

	CachedMetrics.ElapsedSeconds += DeltaSeconds;
	if (CaptureStrategy->ShouldPresent(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame))
	{
		uint32 NumDroppedFrames = CaptureStrategy->GetDroppedFrames(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame);
		CachedMetrics.Frame += NumDroppedFrames;

		CaptureStrategy->OnPresent(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame);

		const FFrameMetrics ThisFrameMetrics(
			CachedMetrics.ElapsedSeconds,
			DeltaSeconds,
			CachedMetrics.Frame,
			NumDroppedFrames
			);
		CaptureProtocol->CaptureFrame(ThisFrameMetrics, *this);

		++CachedMetrics.Frame;

		if (!bFinalizeWhenReady && (FrameCount != 0 && CachedMetrics.Frame >= FrameCount))
		{
			FinalizeWhenReady();
		}
	}
}

void UMovieSceneCapture::FinalizeWhenReady()
{
	bFinalizeWhenReady = true;
}

void UMovieSceneCapture::Finalize()
{
	if (Settings.bCinematicEngineScalability)
	{
		Scalability::SetQualityLevels(CachedQualityLevels);
	}

	FActiveMovieSceneCaptures::Get().Remove(this);

	if (bCapturing)
	{
		bCapturing = false;

		if (CaptureStrategy.IsValid())
		{
			CaptureStrategy->OnStop();
			CaptureStrategy = nullptr;
		}

		if (CaptureProtocol.IsValid())
		{
			CaptureProtocol->Finalize();
			CaptureProtocol = nullptr;
		}

		OnCaptureFinishedDelegate.Broadcast();
	}
}

FString UMovieSceneCapture::ResolveFileFormat(const FString& Format, const FFrameMetrics& FrameMetrics) const
{
	TMap<FString, FStringFormatArg> AllArgs = FormatMappings;

	AllArgs.Add(TEXT("frame"), FString::Printf(TEXT("%0*d"), Settings.ZeroPadFrameNumbers, Settings.bUseRelativeFrameNumbers ? FrameMetrics.FrameNumber : FrameMetrics.FrameNumber + FrameNumberOffset));

	AddFormatMappings(AllArgs, FrameMetrics);

	if (CaptureProtocol.IsValid())
	{
		CaptureProtocol->AddFormatMappings(AllArgs);
	}
	return FString::Format(*Format, AllArgs);
}

void UMovieSceneCapture::EnsureFileWritable(const FString& File) const
{
	FString Directory = FPaths::GetPath(File);

	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		IFileManager::Get().MakeDirectory(*Directory);
	}

	if (Settings.bOverwriteExisting)
	{
		// Try and delete it first
		while (IFileManager::Get().FileSize(*File) != -1 && !FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*File))
		{
			// popup a message box
			FText MessageText = FText::Format(LOCTEXT("UnableToRemoveFile_Format", "The destination file '{0}' could not be deleted because it's in use by another application.\n\nPlease close this application before continuing."), FText::FromString(File));
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *MessageText.ToString(), *LOCTEXT("UnableToRemoveFile", "Unable to remove file").ToString());
		}
	}
}

FString UMovieSceneCapture::GenerateFilename(const FFrameMetrics& FrameMetrics, const TCHAR* Extension) const
{
	const FString BaseFilename = ResolveFileFormat(Settings.OutputDirectory.Path, FrameMetrics) / ResolveFileFormat(Settings.OutputFormat, FrameMetrics);

	FString ThisTry = BaseFilename + Extension;

	if (CaptureProtocol->CanWriteToFile(*ThisTry, Settings.bOverwriteExisting))
	{
		return ThisTry;
	}

	int32 DuplicateIndex = 2;
	for (;;)
	{
		ThisTry = BaseFilename + FString::Printf(TEXT("_(%d)"), DuplicateIndex) + Extension;

		// If the file doesn't exist, we can use that, else, increment the index and try again
		if (CaptureProtocol->CanWriteToFile(*ThisTry, Settings.bOverwriteExisting))
		{
			return ThisTry;
		}

		++DuplicateIndex;
	}

	return ThisTry;
}

void UMovieSceneCapture::LoadFromConfig()
{
	LoadConfig();
	ProtocolSettings->LoadConfig();

	FString JsonString;
	FString Section = GetClass()->GetPathName() + TEXT("_Json");

	if (GConfig->GetString( *Section, TEXT("Data"), JsonString, GEditorSettingsIni))
	{
		FString UnescapedString = FRemoteConfig::ReplaceIniSpecialCharWithChar(JsonString).ReplaceEscapedCharWithChar();

		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(UnescapedString);
		if (FJsonSerializer::Deserialize(JsonReader, RootObject) && RootObject.IsValid())
		{
			DeserializeAdditionalJson(*RootObject);
		}
	}
}

void UMovieSceneCapture::SaveToConfig()
{
	TSharedRef<FJsonObject> Json = MakeShareable(new FJsonObject);
	SerializeAdditionalJson(*Json);

	FString JsonString;
	TSharedRef<TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(&JsonString, 0);
	if (FJsonSerializer::Serialize( Json, JsonWriter ))
	{
		FString Section = GetClass()->GetPathName() + TEXT("_Json");

		FString EscapedJson = FRemoteConfig::ReplaceIniCharWithSpecialChar(JsonString).ReplaceCharWithEscapedChar();

		GConfig->SetString( *Section, TEXT("Data"), *EscapedJson, GEditorSettingsIni);
		GConfig->Flush(false, GEditorSettingsIni);
	}

	SaveConfig();
	ProtocolSettings->SaveConfig();
}

void UMovieSceneCapture::SerializeJson(FJsonObject& Object)
{
	if (ProtocolSettings)
	{
		Object.SetField(TEXT("ProtocolType"), MakeShareable(new FJsonValueString(ProtocolSettings->GetClass()->GetPathName())));
		TSharedRef<FJsonObject> ProtocolDataObject = MakeShareable(new FJsonObject);
		if (FJsonObjectConverter::UStructToJsonObject(ProtocolSettings->GetClass(), ProtocolSettings, ProtocolDataObject, 0, 0))
		{
			Object.SetField(TEXT("ProtocolData"), MakeShareable(new FJsonValueObject(ProtocolDataObject)));
		}
	}

	SerializeAdditionalJson(Object);
}

void UMovieSceneCapture::DeserializeJson(const FJsonObject& Object)
{
	TSharedPtr<FJsonValue> ProtocolTypeField = Object.TryGetField(TEXT("ProtocolType"));
	if (ProtocolTypeField.IsValid())
	{
		UClass* ProtocolTypeClass = FindObject<UClass>(nullptr, *ProtocolTypeField->AsString());
		if (ProtocolTypeClass)
		{
			ProtocolSettings = NewObject<UMovieSceneCaptureProtocolSettings>(this, ProtocolTypeClass);
			if (ProtocolSettings)
			{
				TSharedPtr<FJsonValue> ProtocolDataField = Object.TryGetField(TEXT("ProtocolData"));
				if (ProtocolDataField.IsValid())
				{
					FJsonObjectConverter::JsonAttributesToUStruct(ProtocolDataField->AsObject()->Values, ProtocolTypeClass, ProtocolSettings, 0, 0);
				}
			}
		}
	}

	DeserializeAdditionalJson(Object);
}

#if WITH_EDITOR
void UMovieSceneCapture::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, CaptureType))
	{
		InitializeSettings();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FFixedTimeStepCaptureStrategy::FFixedTimeStepCaptureStrategy(uint32 InTargetFPS)
	: TargetFPS(InTargetFPS)
{
}

void FFixedTimeStepCaptureStrategy::OnWarmup()
{
	FApp::SetFixedDeltaTime(1.0 / TargetFPS);
	FApp::SetUseFixedTimeStep(true);
}

void FFixedTimeStepCaptureStrategy::OnStart()
{
	FApp::SetFixedDeltaTime(1.0 / TargetFPS);
	FApp::SetUseFixedTimeStep(true);
}

void FFixedTimeStepCaptureStrategy::OnStop()
{
	FApp::SetUseFixedTimeStep(false);
}

void FFixedTimeStepCaptureStrategy::OnPresent(double CurrentTimeSeconds, uint32 FrameIndex)
{
}

bool FFixedTimeStepCaptureStrategy::ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	return true;
}

int32 FFixedTimeStepCaptureStrategy::GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	return 0;
}

FRealTimeCaptureStrategy::FRealTimeCaptureStrategy(uint32 InTargetFPS)
	: NextPresentTimeS(0), FrameLength(1.0 / InTargetFPS)
{
}

void FRealTimeCaptureStrategy::OnWarmup()
{
}

void FRealTimeCaptureStrategy::OnStart()
{
}

void FRealTimeCaptureStrategy::OnStop()
{
}

void FRealTimeCaptureStrategy::OnPresent(double CurrentTimeSeconds, uint32 FrameIndex)
{
}

bool FRealTimeCaptureStrategy::ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	return CurrentTimeSeconds >= FrameIndex * FrameLength;
}

int32 FRealTimeCaptureStrategy::GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	uint32 ThisFrame = FMath::FloorToInt(CurrentTimeSeconds / FrameLength);
	if (ThisFrame > FrameIndex)
	{
		return ThisFrame - FrameIndex;
	}
	return 0;
}

#undef LOCTEXT_NAMESPACE
