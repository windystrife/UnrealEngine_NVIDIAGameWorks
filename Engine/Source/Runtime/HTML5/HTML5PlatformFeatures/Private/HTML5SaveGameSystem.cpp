// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5SaveGameSystem.h"
#include "GameDelegates.h"
#include "HTML5JavaScriptFx.h"

DEFINE_LOG_CATEGORY_STATIC(LogHTML5SaveGame, Log, All);

//
// Implementation members
//

FHTML5SaveGameSystem::FHTML5SaveGameSystem()
{
	Initialize();
}

FHTML5SaveGameSystem::~FHTML5SaveGameSystem()
{
	Shutdown();
}

void FHTML5SaveGameSystem::Initialize()
{
	EM_ASM(console.log("FHTML5SaveGameSystem::Initialize"));
// move this to HTML5PlatformFile.cpp
//	EM_ASM(
//		FS.mkdir('/persistent');
//		FS.mount(IDBFS, {}, '/persistent');
//		FS.syncfs(true, function (err) {
//			// handle callback
//		});
//	);
}

void FHTML5SaveGameSystem::Shutdown()
{
	EM_ASM(console.log("FHTML5SaveGameSystem::Shutdown"));
// move this to HTML5PlatformFile.cpp
//		EM_ASM(
//			FS.syncfs(function (err) {
//				// handle callback
//			});
//		);
}

ISaveGameSystem::ESaveExistsResult FHTML5SaveGameSystem::DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex)
{
	return UE_DoesSaveGameExist(GetSaveGamePath(Name, UserIndex)) ? ESaveExistsResult::OK : ESaveExistsResult::DoesNotExist;
}

bool FHTML5SaveGameSystem::SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data)
{
	return UE_SaveGame(GetSaveGamePath(Name,UserIndex),(char*)Data.GetData(),Data.Num());
}

bool FHTML5SaveGameSystem::LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data)
{
	char*	OutData;
	int		Size;
	bool Result = UE_LoadGame(GetSaveGamePath(Name,UserIndex),&OutData,&Size);
	if (!Result)
		return false;
	Data.Append((uint8*)OutData,Size);
	::free (OutData);
	return true;
}

bool FHTML5SaveGameSystem::DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex)
{
	return UE_DeleteSavedGame(GetSaveGamePath(Name,UserIndex));
}

const char* FHTML5SaveGameSystem::GetSaveGamePath(const TCHAR* Name, const int32 UserIndex)
{
	FString path = FString::Printf(TEXT("%s""SaveGames/%s%d.sav"), *FPaths::ProjectSavedDir(), Name, UserIndex);
	return TCHAR_TO_ANSI(*path);
}
