// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MapPakDownloader.h"
#include "MapPakDownloaderLog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include <emscripten/emscripten.h>
#include "Misc/Guid.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"

DECLARE_DELEGATE_OneParam(FDelegateFString, FString)
DECLARE_DELEGATE_OneParam(FDelegateInt32, int32)

// avoid IHttpModule
class FEmscriptenHttpFileRequest
{
public:
	typedef int32 Handle;
private:

	FString FileName;
	FString Url;

	FDelegateFString OnLoadCallBack;
	FDelegateInt32 OnErrorCallBack;
	FDelegateInt32 OnProgressCallBack;

public:

	FEmscriptenHttpFileRequest()
	{}

	static void OnLoad(uint32 Var, void* Arg, const ANSICHAR* FileName)
	{
		FEmscriptenHttpFileRequest* Request = reinterpret_cast<FEmscriptenHttpFileRequest*>(Arg);
		Request->OnLoadCallBack.ExecuteIfBound(FString(ANSI_TO_TCHAR(FileName)));
	}

	static void OnError(uint32 Var,void* Arg, int32 ErrorCode)
	{
		FEmscriptenHttpFileRequest* Request = reinterpret_cast<FEmscriptenHttpFileRequest*>(Arg);
		Request->OnErrorCallBack.ExecuteIfBound(ErrorCode);
	}

	static void OnProgress(uint32 Var,void* Arg, int32 Progress)
	{
		FEmscriptenHttpFileRequest* Request = reinterpret_cast<FEmscriptenHttpFileRequest*>(Arg);
		Request->OnProgressCallBack.ExecuteIfBound(Progress);
	}

	void Process()
	{
		UE_LOG(LogMapPakDownloader, Warning, TEXT("Starting Download for %s"), *FileName);

		emscripten_async_wget2(
			TCHAR_TO_ANSI(*(Url + FString(TEXT("?rand=")) + FGuid::NewGuid().ToString())),
			TCHAR_TO_ANSI(*FileName),
			TCHAR_TO_ANSI(TEXT("GET")),
			TCHAR_TO_ANSI(TEXT("")),
			this,
			&FEmscriptenHttpFileRequest::OnLoad,
			&FEmscriptenHttpFileRequest::OnError,
			&FEmscriptenHttpFileRequest::OnProgress
			);
	}

	void SetFileName(const FString& InFileName) { FileName = InFileName; }
	FString GetFileName() { return FileName;  }

	void SetUrl(const FString& InUrl) { Url = InUrl; }
	void SetOnLoadCallBack(const FDelegateFString& CallBack) { OnLoadCallBack = CallBack; }
	void SetOnErrorCallBack(const FDelegateInt32& CallBack) { OnErrorCallBack = CallBack; }
	void SetOnProgressCallBack(const FDelegateInt32& CallBack) { OnProgressCallBack = CallBack; }

};

class FloatOption : public TSharedFromThis<FloatOption>
{
public:
	FloatOption() :
		Value(0.0f)
	{}
	void SetFloat(float InFloat)
	{
		Value = InFloat;
	}
	TOptional<float> GetFloat() const
	{
		return Value;
	}
private:
	float Value;
};


FMapPakDownloader::FMapPakDownloader()
	:IsTransitionLevel(false)
{}

bool FMapPakDownloader::Init()
{
	//  figure out where we are hosted
	ANSICHAR *LocationString = (ANSICHAR*)EM_ASM_INT_V({

		var hoststring = location.href.substring(0, location.href.lastIndexOf('/'));
		var buffer = Module._malloc(hoststring.length);
		Module.writeAsciiToMemory(hoststring, buffer);
		return buffer;

	});

	HostName = FString(ANSI_TO_TCHAR(LocationString));

	PakLocation = FString(FApp::GetProjectName()) / FString(TEXT("Content")) / FString(TEXT("Paks"));

	// Create directory.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectory(*PakLocation);

	ProgressContainter = MakeShareable(new FloatOption());

	// Thin progress bar. Change this widget if you want a custom loading screen.
	LoadingWidget = SNew(SBox)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.HeightOverride(FOptionalSize(4))
			.WidthOverride(FOptionalSize(300))
			[
				SNew(SProgressBar)
				.Percent(ProgressContainter->AsShared(), &FloatOption::GetFloat)
				.BorderPadding(FVector2D(0, 0))
				.BarFillType(EProgressBarFillType::LeftToRight)
			]
		];

	// Hook up PostLoad
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda([=](UWorld*)
	{
		{
			if (IsTransitionLevel)
			{
				GEngine->GameViewport->AddViewportWidgetContent(LoadingWidget->AsShared());
			}
		}
	});

	return true;
}

void FMapPakDownloader::CachePak()
{
	UE_LOG(LogMapPakDownloader, Warning, TEXT("Caching Dependencies for %s"), *MapToCache);

	FString PakName = FPackageName::GetShortName(FName(*MapToCache)) + ".pak";
	FString DeltaPakName = FPackageName::GetShortName(FName(*LastMap)) + TEXT("_") + FPackageName::GetShortName(FName(*MapToCache)) + ".pak";

	FEmscriptenHttpFileRequest* PakRequest = new FEmscriptenHttpFileRequest; // can't use shared ptrs.


	FDelegateFString OnFileDownloaded = FDelegateFString::CreateLambda([=](FString Name){

											UE_LOG(LogMapPakDownloader, Warning, TEXT("%s download complete!"), *PakRequest->GetFileName());
											UE_LOG(LogMapPakDownloader, Warning, TEXT("Mounting..."), *Name);
											FCoreDelegates::OnMountPak.Execute(Name, 0, nullptr);
											UE_LOG(LogMapPakDownloader, Warning, TEXT("%s Mounted!"), *Name);

											// Get hold of the world.
											TObjectIterator<UWorld> It;
											UWorld* ItWorld = *It;
											if (IsTransitionLevel)
												GEngine->GameViewport->RemoveViewportWidgetContent(LoadingWidget->AsShared());

											UE_LOG(LogMapPakDownloader, Warning, TEXT("Travel to %s"), *MapToCache);
											// Make engine Travel to the cached Map.
											GEngine->SetClientTravel(ItWorld, *MapToCache, TRAVEL_Absolute);

											IsTransitionLevel = false;
											ProgressContainter->SetFloat(0.0f);
											// delete the HTTP request.
											delete PakRequest;
										});

	FDelegateInt32 OnFileDownloadProgress = FDelegateInt32::CreateLambda([=](int32 Progress){
												ProgressContainter->SetFloat((float)Progress/100.0f);
												UE_LOG(LogMapPakDownloader, Warning, TEXT(" %s %d%% downloaded"), *PakRequest->GetFileName(), Progress);
											});

	PakRequest->SetFileName(PakLocation / DeltaPakName);
	PakRequest->SetUrl(HostName / PakLocation / DeltaPakName);
	PakRequest->SetOnLoadCallBack(OnFileDownloaded);
	PakRequest->SetOnProgressCallBack(OnFileDownloadProgress);
	PakRequest->SetOnErrorCallBack(FDelegateInt32::CreateLambda([=](int32){

											UE_LOG(LogMapPakDownloader, Warning, TEXT("Could not download %s"), *PakRequest->GetFileName());
											// lets try again with regular map pak
											PakRequest->SetFileName(PakLocation/PakName);
											PakRequest->SetUrl(HostName/PakLocation/PakName);
											PakRequest->SetOnErrorCallBack(
												FDelegateInt32::CreateLambda([=](int32){
												// we can't even find regular maps. fatal.
												UE_LOG(LogMapPakDownloader, Fatal, TEXT("Could not find any Map Paks, exiting"), *PakRequest->GetFileName());
												}
											));
											PakRequest->Process();

									}));

	PakRequest->Process();
}

void FMapPakDownloader::Cache(FString& Map, FString& InLastMap, void* InDynData)
{
#if 0 // TODO: currently disabled -- this will be converted to use CHUNK settings -- in part 2 of level streaming support for HTML5
	bool UseMapDownloader = false;
	if (GConfig)
	{
		GConfig->GetBool(TEXT("/Script/HTML5PlatformEditor.HTML5TargetSettings"), TEXT("UseAsyncLevelLoading"), UseMapDownloader, GEngineIni);
	}

	if (UseMapDownloader)
	{
		MapToCache = Map;
		LastMap = InLastMap;

		FString OutLongPackageName;
		FString OutFileName;

		if (!FPackageName::SearchForPackageOnDisk(Map, &OutLongPackageName, &OutFileName))
		{
			UE_LOG(LogMapPakDownloader, Warning, TEXT("Caching.... %s"), *Map);
			CachePak();
			Map = TEXT("/Engine/Maps/Entry");
			IsTransitionLevel = true;
		}

	}
#endif
}

