// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "Developer/CollectionManager/Private/CollectionManagerLog.h"

#define LOCTEXT_NAMESPACE "CollectionManager"

class FCollectionManagerConsoleCommands
{
public:
	const FCollectionManagerModule& Module;
	
	FAutoConsoleCommand CreateCommand;
	FAutoConsoleCommand DestroyCommand;
	FAutoConsoleCommand AddCommand;
	FAutoConsoleCommand RemoveCommand;

	FCollectionManagerConsoleCommands(const FCollectionManagerModule& InModule)
		: Module(InModule)
	,	CreateCommand(
		TEXT( "CollectionManager.Create" ),
		*LOCTEXT("CommandText_Create", "Creates a collection of the specified name and type").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FCollectionManagerConsoleCommands::Create ) )
	,	DestroyCommand(
		TEXT( "CollectionManager.Destroy" ),
		*LOCTEXT("CommandText_Destroy", "Deletes a collection of the specified name and type").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FCollectionManagerConsoleCommands::Destroy ) )
	,	AddCommand(
		TEXT( "CollectionManager.Add" ),
		*LOCTEXT("CommandText_Add", "Adds the specified object path to the specified collection").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FCollectionManagerConsoleCommands::Add ) )
	,	RemoveCommand(
		TEXT( "CollectionManager.Remove" ),
		*LOCTEXT("CommandText_Remove", "Removes the specified object path from the specified collection").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw( this, &FCollectionManagerConsoleCommands::Remove ) )
	{}

	void Create(const TArray<FString>& Args)
	{
		if ( Args.Num() < 2 )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("Usage: CollectionManager.Create CollectionName CollectionType"));
			return;
		}

		FName CollectionName = FName(*Args[0]);

		FString ShareStr = Args[1];
		ECollectionShareType::Type ShareType;

		if ( ShareStr == TEXT("LOCAL") )
		{
			ShareType = ECollectionShareType::CST_Local;
		}
		else if ( ShareStr == TEXT("PRIVATE") )
		{
			ShareType = ECollectionShareType::CST_Private;
		}
		else if ( ShareStr == TEXT("SHARED") )
		{
			ShareType = ECollectionShareType::CST_Shared;
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Invalid collection share type: %s"), *ShareStr);
			return;
		}
		
		ECollectionStorageMode::Type StorageMode = ECollectionStorageMode::Static;
		if (Args.Num() >= 3)
		{
			StorageMode = ECollectionStorageMode::FromString(*Args[2]);
		}

		if ( Module.Get().CreateCollection(CollectionName, ShareType, StorageMode) )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("Collection created: %s"), *CollectionName.ToString());
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Failed to create collection: %s"), *CollectionName.ToString());
		}
	}

	void Destroy(const TArray<FString>& Args)
	{
		if ( Args.Num() < 2 )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("Usage: CollectionManager.Destroy CollectionName CollectionType"));
			return;
		}

		FName CollectionName = FName(*Args[0]);
		FString ShareStr = Args[1];
		ECollectionShareType::Type Type;

		if ( ShareStr == TEXT("LOCAL") )
		{
			Type = ECollectionShareType::CST_Local;
		}
		else if ( ShareStr == TEXT("PRIVATE") )
		{
			Type = ECollectionShareType::CST_Private;
		}
		else if ( ShareStr == TEXT("SHARED") )
		{
			Type = ECollectionShareType::CST_Shared;
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Invalid collection share type: %s"), *ShareStr);
			return;
		}
		
		if ( Module.Get().DestroyCollection(CollectionName, Type) )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("Collection destroyed: %s"), *CollectionName.ToString());
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Failed to destroyed collection: %s"), *CollectionName.ToString());
		}
	}

	void Add(const TArray<FString>& Args)
	{
		if ( Args.Num() < 3 )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("Usage: CollectionManager.Add CollectionName CollectionType ObjectPath"));
			return;
		}

		FName CollectionName = FName(*Args[0]);
		FString ShareStr = Args[1];
		FName ObjectPath = FName(*Args[2]);
		ECollectionShareType::Type Type;

		if ( ShareStr == TEXT("LOCAL") )
		{
			Type = ECollectionShareType::CST_Local;
		}
		else if ( ShareStr == TEXT("PRIVATE") )
		{
			Type = ECollectionShareType::CST_Private;
		}
		else if ( ShareStr == TEXT("SHARED") )
		{
			Type = ECollectionShareType::CST_Shared;
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Invalid collection share type: %s"), *ShareStr);
			return;
		}
		
		if ( Module.Get().AddToCollection(CollectionName, Type, ObjectPath) )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("%s added to collection %s"), *ObjectPath.ToString(), *CollectionName.ToString());
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Failed to add %s to collection %s"), *ObjectPath.ToString(), *CollectionName.ToString());
		}
	}

	void Remove(const TArray<FString>& Args)
	{
		if ( Args.Num() < 3 )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("Usage: CollectionManager.Remove CollectionName CollectionType ObjectPath"));
			return;
		}

		FName CollectionName = FName(*Args[0]);
		FString ShareStr = Args[1];
		FName ObjectPath = FName(*Args[2]);

		ECollectionShareType::Type Type;

		if ( ShareStr == TEXT("LOCAL") )
		{
			Type = ECollectionShareType::CST_Local;
		}
		else if ( ShareStr == TEXT("PRIVATE") )
		{
			Type = ECollectionShareType::CST_Private;
		}
		else if ( ShareStr == TEXT("SHARED") )
		{
			Type = ECollectionShareType::CST_Shared;
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Invalid collection share type: %s"), *ShareStr);
			return;
		}
		
		if ( Module.Get().RemoveFromCollection(CollectionName, Type, ObjectPath) )
		{
			UE_LOG(LogCollectionManager, Log, TEXT("%s removed from collection %s"), *ObjectPath.ToString(), *CollectionName.ToString());
		}
		else
		{
			UE_LOG(LogCollectionManager, Warning, TEXT("Failed to remove %s from collection %s"), *ObjectPath.ToString(), *CollectionName.ToString());
		}
	}
};


#undef LOCTEXT_NAMESPACE
