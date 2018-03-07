// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "UserDefinedStruct.generated.h"

UENUM()
enum EUserDefinedStructureStatus
{
	/** Struct is in an unknown state. */
	UDSS_UpToDate,
	/** Struct has been modified but not recompiled. */
	UDSS_Dirty,
	/** Struct tried but failed to be compiled. */
	UDSS_Error,
	/** Struct is a duplicate, the original one was changed. */
	UDSS_Duplicate,

	UDSS_MAX,
};

UCLASS()
class ENGINE_API UUserDefinedStruct : public UScriptStruct
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TEnumAsByte<enum EUserDefinedStructureStatus> Status;

	// the original struct, when current struct isn't a temporary duplicate, the field should be null
	UPROPERTY(Transient)
	TWeakObjectPtr<UUserDefinedStruct> PrimaryStruct;

	UPROPERTY()
	FString ErrorMessage;

	UPROPERTY()
	UObject* EditorData;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY()
	FGuid Guid;

#if WITH_EDITOR
	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostLoad() override;
	// End of UObject interface.

	// UScriptStruct interface.
	virtual void InitializeDefaultValue(uint8* StructData) const override;
	// End of UScriptStruct interface.

	bool DiffersFromDefaultValue(uint8* StructData) const;

	void ValidateGuid();
#endif	// WITH_EDITOR

	virtual void SerializeTaggedProperties(FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, uint8* Defaults, const UObject* BreakRecursionIfFullyLoad=NULL) const override;
	virtual FString PropertyNameToDisplayName(FName InName) const override;

	// UScriptStruct interface.
	virtual uint32 GetStructTypeHash(const void* Src) const override;
	virtual void RecursivelyPreload() override;
	virtual FGuid GetCustomGuid() const override;
	virtual FString GetStructCPPName() const override;
	// End of  UScriptStruct interface.

	static uint32 GetUserDefinedStructTypeHash(const void* Src, const UScriptStruct* Type);

#if WITH_EDITOR
public:
	// UStruct
	virtual UProperty* CustomFindProperty(const FName Name) const override;
#endif	// WITH_EDITOR
};
