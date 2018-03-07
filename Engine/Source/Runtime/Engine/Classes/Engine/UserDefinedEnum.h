// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 *	User defined enumerations:
 *	- EnumType is always UEnum::ECppForm::Namespaced (to comfortable handle names collisions)
 *	- always have the last '_MAX' enumerator, that cannot be changed by user
 *	- Full enumerator name has form: '<enumeration path>::<short, user defined enumerator name>'
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UserDefinedEnum.generated.h"

/** 
 *	An Enumeration is a list of named values.
 */
UCLASS()
class ENGINE_API UUserDefinedEnum : public UEnum
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint32 UniqueNameIndex;

	/** Shows up in the content browser when the enum asset is hovered */
	UPROPERTY(EditAnywhere, Category=Description, meta=(MultiLine=true))
	FText EnumDescription;
#endif //WITH_EDITORONLY_DATA

	/**
	 * De-facto display names for enum entries mapped against their raw enum name (UEnum::GetNameByIndex).
	 * To sync DisplayNameMap use FEnumEditorUtils::EnsureAllDisplayNamesExist.
	 */
	UPROPERTY()
	TMap<FName, FText> DisplayNameMap;

public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.

	/**
	 * Generates full enum name give enum name.
	 * For UUserDefinedEnum full enumerator name has form: '<enumeration path>::<short, user defined enumerator name>'
	 *
	 * @param InEnumName Enum name.
	 * @return Full enum name.
	 */
	virtual FString GenerateFullEnumName(const TCHAR* InEnumName) const override;

	/**
	 *	Try to update value in enum variable after an enum's change.
	 *
	 *	@param EnumeratorIndex	old index
	 *	@return	new index
	 */
	virtual int64 ResolveEnumerator(FArchive& Ar, int64 EnumeratorValue) const override;

	/** Overridden to read DisplayNameMap*/
	virtual FText GetDisplayNameTextByIndex(int32 InIndex) const override;

	virtual bool SetEnums(TArray<TPair<FName, int64>>& InNames, ECppForm InCppForm, bool bAddMaxKeyIfMissing = true) override;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual bool Rename(const TCHAR* NewName = NULL, UObject* NewOuter = NULL, ERenameFlags Flags = REN_None) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface

	FString GenerateNewEnumeratorName();
#endif	// WITH_EDITOR
};
