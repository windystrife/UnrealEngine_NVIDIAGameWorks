// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "UObject/SoftObjectPtr.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/StructOnScope.h"
#include "EditorUndoClient.h"
#include "StructureEditorUtils.h"
#include "UserDefinedStructEditorData.generated.h"

class ITransactionObjectAnnotation;

USTRUCT()
struct FStructVariableDescription
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName VarName;

	UPROPERTY()
	FGuid VarGuid;

	UPROPERTY()
	FString FriendlyName;

	UPROPERTY()
	FString DefaultValue;

	// TYPE DATA
	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString SubCategory;

	UPROPERTY()
	TSoftObjectPtr<UObject> SubCategoryObject;

	UPROPERTY()
	FEdGraphTerminalType PinValueType;

	UPROPERTY()
	EPinContainerType ContainerType;

	// DEPRECATED(4.17)
	UPROPERTY()
	uint8 bIsArray_DEPRECATED:1;

	// DEPRECATED(4.17)
	UPROPERTY()
	uint8 bIsSet_DEPRECATED:1;

	// DEPRECATED(4.17)
	UPROPERTY()
	uint8 bIsMap_DEPRECATED:1;

	UPROPERTY(Transient)
	uint8 bInvalidMember:1;

	UPROPERTY()
	uint8 bDontEditoOnInstance:1;

	UPROPERTY()
	uint8 bEnableMultiLineText:1;

	UPROPERTY()
	uint8 bEnable3dWidget:1;

	// CurrentDefaultValue stores the actual default value, after the DefaultValue was changed, and before the struct was recompiled
	UPROPERTY()
	FString CurrentDefaultValue;

	UPROPERTY()
	FString ToolTip;

	UNREALED_API bool SetPinType(const struct FEdGraphPinType& VarType);

	UNREALED_API FEdGraphPinType ToPinType() const;

	// DEPRECATED(4.17)
	void PostSerialize(const FArchive& Ar);

	FStructVariableDescription()
		: ContainerType(EPinContainerType::None)
		, bIsArray_DEPRECATED(false)
		, bIsSet_DEPRECATED(false)
		, bIsMap_DEPRECATED(false)
		, bInvalidMember(false)
		, bDontEditoOnInstance(false)
		, bEnableMultiLineText(false)
		, bEnable3dWidget(false)
	{ }
};

template<>
struct TStructOpsTypeTraits< FStructVariableDescription > : public TStructOpsTypeTraitsBase2< FStructVariableDescription >
{
	enum 
	{
		WithPostSerialize = true,
	};
};

class FStructOnScopeMember : public FStructOnScope
{
public:
	FStructOnScopeMember() : FStructOnScope() {}

	void Recreate(const UStruct* InScriptStruct)
	{
		Destroy();
		ScriptStruct = InScriptStruct;
		Initialize();
	}
};

UCLASS()
class UNREALED_API UUserDefinedStructEditorData : public UObject, public FEditorUndoClient
{
	GENERATED_UCLASS_BODY()

private:
	// the property is used to generate an uniqe name id for member variable
	UPROPERTY(NonTransactional) 
	uint32 UniqueNameId;

public:
	UPROPERTY()
	TArray<FStructVariableDescription> VariablesDescriptions;

	UPROPERTY()
	FString ToolTip;

	// optional super struct
	UPROPERTY()
	UScriptStruct* NativeBase;

protected:
	FStructOnScopeMember DefaultStructInstance;

public:
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	// UObject interface.
	virtual TSharedPtr<ITransactionObjectAnnotation> GetTransactionAnnotation() const override;
	virtual void PostEditUndo() override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;
	virtual void PostLoadSubobjects(struct FObjectInstancingGraph* OuterInstanceGraph) override;
	// End of UObject interface.

	// FEditorUndoClient interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient interface.


	uint32 GenerateUniqueNameIdForMemberVariable();
	class UUserDefinedStruct* GetOwnerStruct() const;

	const uint8* GetDefaultInstance() const;
	void RecreateDefaultInstance(FString* OutLog = nullptr);
	void CleanDefaultInstance();

private:

	// Track the structure change that PostEditUndo undid to pass to FUserDefinedStructureCompilerUtils::CompileStruct
	FStructureEditorUtils::EStructureEditorChangeInfo CachedStructureChange;

	// Utility function for both PostEditUndo to route through
	void ConsolidatedPostEditUndo(FStructureEditorUtils::EStructureEditorChangeInfo ActiveChange);

};
