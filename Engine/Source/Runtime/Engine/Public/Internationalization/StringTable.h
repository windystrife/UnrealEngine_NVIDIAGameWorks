// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "StringTableCoreFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "StringTable.generated.h"

/** String table wrapper asset */
UCLASS()
class ENGINE_API UStringTable : public UObject
{
	GENERATED_BODY()

public:
	UStringTable();

	/** Called during Engine init to initialize the engine bridge instance */
	static void InitializeEngineBridge();

	//~ UObject interface
	virtual void FinishDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;

	/** Get the string table ID that should be used by this asset */
	FName GetStringTableId() const;

	/** Get the underlying string table owned by this asset */
	FStringTableConstRef GetStringTable() const;

	/** Get the underlying string table owned by this asset */
	FStringTableRef GetMutableStringTable() const;

private:
	/** Internal string table. Will never be null, but is a TSharedPtr rather than a TSharedRef so we can use a forward declaration */
	FStringTablePtr StringTable;

	/** Cached ID of this string table asset in the string table registry */
	FName StringTableId;
};
