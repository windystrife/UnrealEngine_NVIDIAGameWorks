// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdGraphSchema_K2.h"
#include "BlueprintCompilationManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/Interface.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/MemberReference.h"
#include "Components/ActorComponent.h"
#include "Misc/Attribute.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LevelScriptActor.h"
#include "Components/ChildActorComponent.h"
#include "Engine/Selection.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditorSettings.h"
#include "K2Node.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Event.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_Literal.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_Select.h"
#include "K2Node_SpawnActor.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_Switch.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorStyleSettings.h"
#include "Editor.h"

#include "Kismet/BlueprintMapLibrary.h"
#include "Kismet/BlueprintSetLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "ComponentAssetBroker.h"
#include "BlueprintEditorSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "KismetCompiler.h"
#include "Misc/DefaultValueHelper.h"
#include "ObjectEditorUtils.h"
#include "ComponentTypeRegistry.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintComponentNodeSpawner.h"
#include "AssetRegistryModule.h"
#include "Misc/HotReloadInterface.h"

#include "K2Node_CastByteToEnum.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "K2Node_ConvertAsset.h"
#include "Framework/Commands/GenericCommands.h"


//////////////////////////////////////////////////////////////////////////
// FBlueprintMetadata

const FName FBlueprintMetadata::MD_AllowableBlueprintVariableType(TEXT("BlueprintType"));
const FName FBlueprintMetadata::MD_NotAllowableBlueprintVariableType(TEXT("NotBlueprintType"));

const FName FBlueprintMetadata::MD_BlueprintSpawnableComponent(TEXT("BlueprintSpawnableComponent"));
const FName FBlueprintMetadata::MD_IsBlueprintBase(TEXT("IsBlueprintBase"));
const FName FBlueprintMetadata::MD_RestrictedToClasses(TEXT("RestrictedToClasses"));
const FName FBlueprintMetadata::MD_ChildCanTick(TEXT("ChildCanTick"));
const FName FBlueprintMetadata::MD_ChildCannotTick(TEXT("ChildCannotTick"));
const FName FBlueprintMetadata::MD_IgnoreCategoryKeywordsInSubclasses(TEXT("IgnoreCategoryKeywordsInSubclasses"));


const FName FBlueprintMetadata::MD_Protected(TEXT("BlueprintProtected"));
const FName FBlueprintMetadata::MD_Latent(TEXT("Latent"));
const FName FBlueprintMetadata::MD_UnsafeForConstructionScripts(TEXT("UnsafeDuringActorConstruction"));
const FName FBlueprintMetadata::MD_FunctionCategory(TEXT("Category"));
const FName FBlueprintMetadata::MD_DeprecatedFunction(TEXT("DeprecatedFunction"));
const FName FBlueprintMetadata::MD_DeprecationMessage(TEXT("DeprecationMessage"));
const FName FBlueprintMetadata::MD_CompactNodeTitle(TEXT("CompactNodeTitle"));
const FName FBlueprintMetadata::MD_DisplayName(TEXT("DisplayName"));
const FName FBlueprintMetadata::MD_InternalUseParam(TEXT("InternalUseParam"));

const FName FBlueprintMetadata::MD_PropertyGetFunction(TEXT("BlueprintGetter"));
const FName FBlueprintMetadata::MD_PropertySetFunction(TEXT("BlueprintSetter"));

const FName FBlueprintMetadata::MD_ExposeOnSpawn(TEXT("ExposeOnSpawn"));
const FName FBlueprintMetadata::MD_HideSelfPin(TEXT("HideSelfPin"));
const FName FBlueprintMetadata::MD_DefaultToSelf(TEXT("DefaultToSelf"));
const FName FBlueprintMetadata::MD_WorldContext(TEXT("WorldContext"));
const FName FBlueprintMetadata::MD_CallableWithoutWorldContext(TEXT("CallableWithoutWorldContext"));
const FName FBlueprintMetadata::MD_DevelopmentOnly(TEXT("DevelopmentOnly"));
const FName FBlueprintMetadata::MD_AutoCreateRefTerm(TEXT("AutoCreateRefTerm"));

const FName FBlueprintMetadata::MD_ShowWorldContextPin(TEXT("ShowWorldContextPin"));
const FName FBlueprintMetadata::MD_Private(TEXT("BlueprintPrivate"));

const FName FBlueprintMetadata::MD_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
const FName FBlueprintMetadata::MD_NeedsLatentFixup(TEXT("NeedsLatentFixup"));

const FName FBlueprintMetadata::MD_LatentCallbackTarget(TEXT("LatentCallbackTarget"));
const FName FBlueprintMetadata::MD_AllowPrivateAccess(TEXT("AllowPrivateAccess"));

const FName FBlueprintMetadata::MD_ExposeFunctionCategories(TEXT("ExposeFunctionCategories"));

const FName FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint(TEXT("CannotImplementInterfaceInBlueprint"));
const FName FBlueprintMetadata::MD_ProhibitedInterfaces(TEXT("ProhibitedInterfaces"));

const FName FBlueprintMetadata::MD_FunctionKeywords(TEXT("Keywords"));

const FName FBlueprintMetadata::MD_ExpandEnumAsExecs(TEXT("ExpandEnumAsExecs"));

const FName FBlueprintMetadata::MD_CommutativeAssociativeBinaryOperator(TEXT("CommutativeAssociativeBinaryOperator"));
const FName FBlueprintMetadata::MD_MaterialParameterCollectionFunction(TEXT("MaterialParameterCollectionFunction"));

const FName FBlueprintMetadata::MD_Tooltip(TEXT("Tooltip"));

const FName FBlueprintMetadata::MD_CallInEditor(TEXT("CallInEditor"));

const FName FBlueprintMetadata::MD_DataTablePin(TEXT("DataTablePin"));

const FName FBlueprintMetadata::MD_NativeMakeFunction(TEXT("HasNativeMake"));
const FName FBlueprintMetadata::MD_NativeBreakFunction(TEXT("HasNativeBreak"));

const FName FBlueprintMetadata::MD_DynamicOutputType(TEXT("DeterminesOutputType"));
const FName FBlueprintMetadata::MD_DynamicOutputParam(TEXT("DynamicOutputParam"));

const FName FBlueprintMetadata::MD_ArrayParam(TEXT("ArrayParm"));
const FName FBlueprintMetadata::MD_ArrayDependentParam(TEXT("ArrayTypeDependentParams"));

const FName FBlueprintMetadata::MD_SetParam(TEXT("SetParam"));

// Each of these is a | separated list of param names:
const FName FBlueprintMetadata::MD_MapParam(TEXT("MapParam"));
const FName FBlueprintMetadata::MD_MapKeyParam(TEXT("MapKeyParam"));
const FName FBlueprintMetadata::MD_MapValueParam(TEXT("MapValueParam"));

const FName FBlueprintMetadata::MD_Bitmask(TEXT("Bitmask"));
const FName FBlueprintMetadata::MD_BitmaskEnum(TEXT("BitmaskEnum"));
const FName FBlueprintMetadata::MD_Bitflags(TEXT("Bitflags"));
const FName FBlueprintMetadata::MD_UseEnumValuesAsMaskValuesInEditor(TEXT("UseEnumValuesAsMaskValuesInEditor"));

//////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "KismetSchema"

UEdGraphSchema_K2::FPinTypeTreeInfo::FPinTypeTreeInfo(const FText& InFriendlyName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FText& InTooltip, bool bInReadOnly/*=false*/, FTypesDatabase* TypesDatabase /*=nullptr*/)
	: PossibleObjectReferenceTypes(0)
{
	Init(InFriendlyName, CategoryName, Schema, InTooltip, bInReadOnly, TypesDatabase);
}

struct FUnloadedAssetData
{
	FSoftObjectPath SoftObjectPath;
	FText AssetFriendlyName;
	FText Tooltip;
	uint8 PossibleObjectReferenceTypes;

	FUnloadedAssetData()
		: PossibleObjectReferenceTypes(0)
	{}

	FUnloadedAssetData(const FAssetData& InAsset, uint8 InPossibleObjectReferenceTypes = 0)
		: SoftObjectPath(InAsset.ToSoftObjectPath())
		, AssetFriendlyName(FText::FromString(FName::NameToDisplayString(InAsset.AssetName.ToString(), false)))
		, PossibleObjectReferenceTypes(InPossibleObjectReferenceTypes)
	{
		InAsset.GetTagValue("Tooltip", Tooltip);
		if (Tooltip.IsEmpty())
		{
			Tooltip = FText::FromString(InAsset.ObjectPath.ToString());
		}
	}
};

struct FLoadedAssetData
{
	FText Tooltip;
	UObject* Object;
	uint8 PossibleObjectReferenceTypes;

	FLoadedAssetData() 
		: Object(nullptr)
		, PossibleObjectReferenceTypes(0) {}

	FLoadedAssetData(UObject* InObject, uint8 InPossibleObjectReferenceTypes = 0)
		: Object(InObject) 
		, PossibleObjectReferenceTypes(InPossibleObjectReferenceTypes)
	{
		UStruct* Struct = Cast<UStruct>(Object);
		Tooltip = Struct ? Struct->GetToolTipText() : FText::GetEmpty();
	}
};

struct FTypesDatabase
{
	typedef TSharedPtr<TArray<FLoadedAssetData>> FLoadedTypesList;
	TMap<FString, FLoadedTypesList> LoadedTypesMap;

	typedef TSharedPtr<TArray<FUnloadedAssetData>> FUnLoadedTypesList;
	TMap<FString, FUnLoadedTypesList> UnLoadedTypesMap;
};

/** Helper class to gather variable types */
class FGatherTypesHelper
{
private:
	typedef TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> FPinTypeTreeInfoPtr;
	struct FCompareChildren
	{
		FORCEINLINE bool operator()(const FPinTypeTreeInfoPtr A, const FPinTypeTreeInfoPtr B) const
		{
			return (A->GetDescription().ToString() < B->GetDescription().ToString());
		}
	};

public:
	static void FillLoadedTypesDatabase(FTypesDatabase& TypesDatabase, bool bIndexTypesOnly)
	{
		// Loaded types
		TypesDatabase.LoadedTypesMap.Reset();

		//(Type == UEdGraphSchema_K2::PC_Enum)
		{
			FTypesDatabase::FLoadedTypesList LoadedTypesList = MakeShareable(new TArray<FLoadedAssetData>());
			// Generate a list of all potential enums which have "BlueprintType=true" in their metadata
			for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
			{
				UEnum* CurrentEnum = *EnumIt;
				if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentEnum))
				{
					LoadedTypesList->Add(FLoadedAssetData(CurrentEnum));
				}
			}
			TypesDatabase.LoadedTypesMap.Add(UEdGraphSchema_K2::PC_Enum, LoadedTypesList);
		}

		if (!bIndexTypesOnly)
		{
			//(Type == UEdGraphSchema_K2::PC_Struct)
			{
				FTypesDatabase::FLoadedTypesList LoadedTypesList = MakeShareable(new TArray<FLoadedAssetData>());
				// Find script structs marked with "BlueprintType=true" in their metadata, and add to the list
				for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
				{
					UScriptStruct* ScriptStruct = *StructIt;
					if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(ScriptStruct))
					{
						LoadedTypesList->Add(FLoadedAssetData(ScriptStruct));
					}
				}
				TypesDatabase.LoadedTypesMap.Add(UEdGraphSchema_K2::PC_Struct, LoadedTypesList);
			}

			//(Type == UEdGraphSchema_K2::PC_Class || Type == UEdGraphSchema_K2::PC_SoftClass) UEdGraphSchema_K2::PC_Interface)
			//(Type == UEdGraphSchema_K2::PC_Object || Type == UEdGraphSchema_K2::PC_SoftObject)
			{
				FTypesDatabase::FLoadedTypesList InterfaceLoadedTypesList = MakeShareable(new TArray<FLoadedAssetData>());
				FTypesDatabase::FLoadedTypesList AllObjectLoadedTypesList = MakeShareable(new TArray<FLoadedAssetData>());

				// Generate a list of all potential objects which have "BlueprintType=true" in their metadata
				for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
				{
					UClass* CurrentClass = *ClassIt;
					const bool bIsInterface = CurrentClass->IsChildOf(UInterface::StaticClass());
					const bool bIsBlueprintType = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentClass);
					const bool bIsDeprecated = CurrentClass->HasAnyClassFlags(CLASS_Deprecated);
					if (bIsBlueprintType && !bIsDeprecated)
					{
						if (bIsInterface)
						{
							InterfaceLoadedTypesList->Add(FLoadedAssetData(CurrentClass));
						}
						else
						{
							AllObjectLoadedTypesList->Add(FLoadedAssetData(CurrentClass, static_cast<uint8>(EObjectReferenceType::AllTypes)));
						}
					}
				}
				TypesDatabase.LoadedTypesMap.Add(UEdGraphSchema_K2::AllObjectTypes, AllObjectLoadedTypesList);
				TypesDatabase.LoadedTypesMap.Add(UEdGraphSchema_K2::PC_Interface, InterfaceLoadedTypesList);
			}
		}
	}

	static void FillUnLoadedTypesDatabase(FTypesDatabase& TypesDatabase, bool bIndexTypesOnly)
	{
		// Loaded types
		TypesDatabase.UnLoadedTypesMap.Reset();

		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		{
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByClass(UUserDefinedEnum::StaticClass()->GetFName(), AssetData);

			FTypesDatabase::FUnLoadedTypesList UnLoadedTypesList = MakeShareable(new TArray<FUnloadedAssetData>());
			for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
			{
				const FAssetData& Asset = AssetData[AssetIndex];
				if (Asset.IsValid() && !Asset.IsAssetLoaded())
				{
					UnLoadedTypesList->Add(FUnloadedAssetData(Asset));
				}
			}

			TypesDatabase.UnLoadedTypesMap.Add(UEdGraphSchema_K2::PC_Enum, UnLoadedTypesList);
		}

		if (!bIndexTypesOnly)
		{
			{
				TArray<FAssetData> AssetData;
				AssetRegistryModule.Get().GetAssetsByClass(UUserDefinedStruct::StaticClass()->GetFName(), AssetData);

				FTypesDatabase::FUnLoadedTypesList UnLoadedTypesList = MakeShareable(new TArray<FUnloadedAssetData>());
				for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
				{
					const FAssetData& Asset = AssetData[AssetIndex];
					if (Asset.IsValid() && !Asset.IsAssetLoaded())
					{
						UnLoadedTypesList->Add(FUnloadedAssetData(Asset));
					}
				}

				TypesDatabase.UnLoadedTypesMap.Add(UEdGraphSchema_K2::PC_Struct, UnLoadedTypesList);
			}

			//else if (Schema->PC_Object == CategoryName || Schema->PC_Class == CategoryName || Schema->PC_Interface == CategoryName || Schema->PC_SoftObject == CategoryName || Schema->PC_SoftClass == CategoryName)
			{
				TArray<FAssetData> AssetData;
				AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetData);

				const FString BPInterfaceTypeAllowed(TEXT("BPTYPE_Interface"));
				const FString BPNormalTypeAllowed(TEXT("BPTYPE_Normal"));

				FTypesDatabase::FUnLoadedTypesList UnLoadedInterfacesList = MakeShareable(new TArray<FUnloadedAssetData>());
				FTypesDatabase::FUnLoadedTypesList UnLoadedClassesList = MakeShareable(new TArray<FUnloadedAssetData>());

				for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
				{
					const FAssetData& Asset = AssetData[AssetIndex];

					if (Asset.IsValid() && !Asset.IsAssetLoaded())
					{
						const FString BlueprintTypeStr = Asset.GetTagValueRef<FString>("BlueprintType");
						const bool bNormalBP = BlueprintTypeStr == BPNormalTypeAllowed;
						const bool bInterfaceBP = BlueprintTypeStr == BPInterfaceTypeAllowed;

						if (bNormalBP || bInterfaceBP)
						{
							const uint32 ClassFlags = Asset.GetTagValueRef<uint32>("ClassFlags");
							if (!(ClassFlags & CLASS_Deprecated))
							{
								if (bNormalBP)
								{
									UnLoadedClassesList->Add(FUnloadedAssetData(Asset, static_cast<uint8>(EObjectReferenceType::AllTypes)));
								}
								else if (bInterfaceBP)
								{
									UnLoadedInterfacesList->Add(FUnloadedAssetData(Asset));
								}
							}
						}
					}
				}
				TypesDatabase.UnLoadedTypesMap.Add(UEdGraphSchema_K2::PC_Interface, UnLoadedInterfacesList);
				TypesDatabase.UnLoadedTypesMap.Add(UEdGraphSchema_K2::AllObjectTypes, UnLoadedClassesList);
			}
		}
	}

	/**
	 * Gathers all valid sub-types (loaded and unloaded) of a passed category and sorts them alphabetically
	 * @param FriendlyName		Friendly name to be used for the tooltip if there is no available data
	 * @param CategoryName		Category (type) to find sub-types of
	 * @param TypesDatabase		Types database
	 * @param OutChildren		All the gathered children
	 */
	static void Gather(const FText& FriendlyName, const FString& CategoryName, FTypesDatabase& TypesDatabase, TArray<FPinTypeTreeInfoPtr>& OutChildren)
	{
		FEdGraphPinType LoadedPinSubtype;
		LoadedPinSubtype.PinCategory = (CategoryName == UEdGraphSchema_K2::PC_Enum ? UEdGraphSchema_K2::PC_Byte : CategoryName);
		LoadedPinSubtype.PinSubCategory = TEXT("");
		LoadedPinSubtype.PinSubCategoryObject = NULL;

		FTypesDatabase::FLoadedTypesList* LoadedSubTypesPtr = TypesDatabase.LoadedTypesMap.Find(CategoryName);
		if (LoadedSubTypesPtr && LoadedSubTypesPtr->IsValid())
		{
			for (FLoadedAssetData& LoadedAssetData : *LoadedSubTypesPtr->Get())
			{
				OutChildren.Add(MakeShareable(new UEdGraphSchema_K2::FPinTypeTreeInfo(LoadedPinSubtype.PinCategory
					, LoadedAssetData.Object
					, LoadedAssetData.Tooltip.IsEmpty() ? FriendlyName : LoadedAssetData.Tooltip
					, false
					, LoadedAssetData.PossibleObjectReferenceTypes)));
			}
		}

		FTypesDatabase::FUnLoadedTypesList* UnLoadedSubTypesPtr = TypesDatabase.UnLoadedTypesMap.Find(CategoryName);
		if (UnLoadedSubTypesPtr && UnLoadedSubTypesPtr->IsValid())
		{
			for (FUnloadedAssetData& It : *UnLoadedSubTypesPtr->Get())
			{
				FPinTypeTreeInfoPtr TypeTreeInfo = MakeShareable(new UEdGraphSchema_K2::FPinTypeTreeInfo(It.AssetFriendlyName
					, CategoryName
					, It.SoftObjectPath
					, It.Tooltip
					, false
					, It.PossibleObjectReferenceTypes));
				OutChildren.Add(TypeTreeInfo);
			}
		}

		OutChildren.Sort(FCompareChildren());
	}

	/** Loads an asset based on the AssetReference through the asset registry */
	static UObject* LoadAsset(const FSoftObjectPath& AssetReference)
	{
		if (AssetReference.IsValid())
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetReference.ToString());
			return AssetData.GetAsset();
		}
		return nullptr;
	}
};

const FEdGraphPinType& UEdGraphSchema_K2::FPinTypeTreeInfo::GetPinType(bool bForceLoadedSubCategoryObject)
{
	if (bForceLoadedSubCategoryObject)
	{
		// Only attempt to load the sub category object if we need to
		if ( SubCategoryObjectAssetReference.IsValid() && (!PinType.PinSubCategoryObject.IsValid() || FSoftObjectPath(PinType.PinSubCategoryObject.Get()) != SubCategoryObjectAssetReference) )
		{
			UObject* LoadedObject = FGatherTypesHelper::LoadAsset(SubCategoryObjectAssetReference);

			if(UBlueprint* BlueprintObject = Cast<UBlueprint>(LoadedObject))
			{
				PinType.PinSubCategoryObject = *BlueprintObject->GeneratedClass;
			}
			else
			{
				PinType.PinSubCategoryObject = LoadedObject;
			}
		}
	}
	else
	{
		if (SubCategoryObjectAssetReference.IsValid())
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*SubCategoryObjectAssetReference.ToString());

			if(!AssetData.IsAssetLoaded())
			{
				UObject* LoadedObject = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());

				// If the unloaded asset is a Blueprint, we need to pull the generated class and assign that
				if(UBlueprint* BlueprintObject = Cast<UBlueprint>(LoadedObject))
				{
					PinType.PinSubCategoryObject = *BlueprintObject->GeneratedClass;
				}
				else
				{
					PinType.PinSubCategoryObject = LoadedObject;
				}
			}
			else
			{
				PinType.PinSubCategoryObject = AssetData.GetAsset();
			}
		}
	}
	return PinType;
}

void UEdGraphSchema_K2::FPinTypeTreeInfo::Init(const FText& InFriendlyName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FText& InTooltip, bool bInReadOnly, FTypesDatabase* TypesDatabase)
{
	check( !CategoryName.IsEmpty() );
	check( Schema );

	FriendlyName = InFriendlyName;
	Tooltip = InTooltip;
	PinType.PinCategory = (CategoryName == PC_Enum? PC_Byte : CategoryName);
	PinType.PinSubCategory = TEXT("");
	PinType.PinSubCategoryObject = NULL;

	bReadOnly = bInReadOnly;

	CachedDescription = GenerateDescription();

	if (Schema->DoesTypeHaveSubtypes(CategoryName))
	{
		if (TypesDatabase)
		{
			FGatherTypesHelper::Gather(InFriendlyName, CategoryName, *TypesDatabase, Children);
		}
	}
}

UEdGraphSchema_K2::FPinTypeTreeInfo::FPinTypeTreeInfo(const FString& CategoryName, UObject* SubCategoryObject, const FText& InTooltip, bool bInReadOnly/*=false*/, uint8 InPossibleObjectReferenceTypes)
	: PossibleObjectReferenceTypes(InPossibleObjectReferenceTypes)
{
	check( !CategoryName.IsEmpty() );
	check( SubCategoryObject );

	Tooltip = InTooltip;
	PinType.PinCategory = CategoryName;
	PinType.PinSubCategoryObject = SubCategoryObject;

	bReadOnly = bInReadOnly;
	CachedDescription = GenerateDescription();
}

UEdGraphSchema_K2::FPinTypeTreeInfo::FPinTypeTreeInfo(const FText& InFriendlyName, const FString& CategoryName, const FSoftObjectPath& SubCategoryObject, const FText& InTooltip, bool bInReadOnly, uint8 InPossibleObjectReferenceTypes)
	: PossibleObjectReferenceTypes(InPossibleObjectReferenceTypes)
{
	FriendlyName = InFriendlyName;

	check(!CategoryName.IsEmpty());
	check(SubCategoryObject.IsValid());

	Tooltip = InTooltip;
	PinType.PinCategory = CategoryName;

	SubCategoryObjectAssetReference = SubCategoryObject;
	PinType.PinSubCategoryObject = SubCategoryObjectAssetReference.ResolveObject();

	bReadOnly = bInReadOnly;
	CachedDescription = GenerateDescription();
}

FText UEdGraphSchema_K2::FPinTypeTreeInfo::GenerateDescription()
{
	if (!FriendlyName.IsEmpty())
	{
		return FriendlyName;
	}
	else if (PinType.PinSubCategoryObject.IsValid())
	{
		FText DisplayName;
		if (UField* SubCategoryField = Cast<UField>(PinType.PinSubCategoryObject.Get()))
		{
			DisplayName = SubCategoryField->GetDisplayNameText();
		}
		else
		{
			DisplayName = FText::FromString(FName::NameToDisplayString(PinType.PinSubCategoryObject->GetName(), PinType.PinCategory == PC_Boolean));
		}

		return DisplayName;
	}
	else
	{
		return LOCTEXT("PinDescriptionError", "Error!");
	}
}

FText UEdGraphSchema_K2::FPinTypeTreeInfo::GetDescription() const
{
	return CachedDescription;
}

const FString UEdGraphSchema_K2::PC_Exec(TEXT("exec"));
const FString UEdGraphSchema_K2::PC_Boolean(TEXT("bool"));
const FString UEdGraphSchema_K2::PC_Byte(TEXT("byte"));
const FString UEdGraphSchema_K2::PC_Class(TEXT("class"));
const FString UEdGraphSchema_K2::PC_Int(TEXT("int"));
const FString UEdGraphSchema_K2::PC_Float(TEXT("float"));
const FString UEdGraphSchema_K2::PC_Name(TEXT("name"));
const FString UEdGraphSchema_K2::PC_Delegate(TEXT("delegate"));
const FString UEdGraphSchema_K2::PC_MCDelegate(TEXT("mcdelegate"));
const FString UEdGraphSchema_K2::PC_Object(TEXT("object"));
const FString UEdGraphSchema_K2::PC_Interface(TEXT("interface"));
const FString UEdGraphSchema_K2::PC_String(TEXT("string"));
const FString UEdGraphSchema_K2::PC_Text(TEXT("text"));
const FString UEdGraphSchema_K2::PC_Struct(TEXT("struct"));
const FString UEdGraphSchema_K2::PC_Wildcard(TEXT("wildcard"));
const FString UEdGraphSchema_K2::PC_Enum(TEXT("enum"));
const FString UEdGraphSchema_K2::PC_SoftObject(TEXT("softobject"));
const FString UEdGraphSchema_K2::PC_SoftClass(TEXT("softclass"));
const FString UEdGraphSchema_K2::PSC_Self(TEXT("self"));
const FString UEdGraphSchema_K2::PSC_Index(TEXT("index"));
const FString UEdGraphSchema_K2::PSC_Bitmask(TEXT("bitmask"));
const FString UEdGraphSchema_K2::PN_Execute(TEXT("execute"));
const FString UEdGraphSchema_K2::PN_Then(TEXT("then"));
const FString UEdGraphSchema_K2::PN_Completed(TEXT("Completed"));
const FString UEdGraphSchema_K2::PN_DelegateEntry(TEXT("delegate"));
const FString UEdGraphSchema_K2::PN_EntryPoint(TEXT("EntryPoint"));
const FString UEdGraphSchema_K2::PN_Self(TEXT("self"));
const FString UEdGraphSchema_K2::PN_Else(TEXT("else"));
const FString UEdGraphSchema_K2::PN_Loop(TEXT("loop"));
const FString UEdGraphSchema_K2::PN_After(TEXT("after"));
const FString UEdGraphSchema_K2::PN_ReturnValue(TEXT("ReturnValue"));
const FString UEdGraphSchema_K2::PN_ObjectToCast(TEXT("Object"));
const FString UEdGraphSchema_K2::PN_Condition(TEXT("Condition"));
const FString UEdGraphSchema_K2::PN_Start(TEXT("Start"));
const FString UEdGraphSchema_K2::PN_Stop(TEXT("Stop"));
const FString UEdGraphSchema_K2::PN_Index(TEXT("Index"));
const FString UEdGraphSchema_K2::PN_Item(TEXT("Item"));
const FString UEdGraphSchema_K2::PN_CastSucceeded(TEXT("then"));
const FString UEdGraphSchema_K2::PN_CastFailed(TEXT("CastFailed"));
const FString UEdGraphSchema_K2::PN_CastedValuePrefix(TEXT("As"));
const FString UEdGraphSchema_K2::PN_MatineeFinished(TEXT("Finished"));

const FName UEdGraphSchema_K2::FN_UserConstructionScript(TEXT("UserConstructionScript"));
const FName UEdGraphSchema_K2::FN_ExecuteUbergraphBase(TEXT("ExecuteUbergraph"));
const FName UEdGraphSchema_K2::GN_EventGraph(TEXT("EventGraph"));
const FName UEdGraphSchema_K2::GN_AnimGraph(TEXT("AnimGraph"));
const FText UEdGraphSchema_K2::VR_DefaultCategory(LOCTEXT("Default", "Default"));

const int32 UEdGraphSchema_K2::AG_LevelReference = 100;

const UScriptStruct* UEdGraphSchema_K2::VectorStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::RotatorStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::TransformStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::LinearColorStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::ColorStruct = nullptr;

bool UEdGraphSchema_K2::bGeneratingDocumentation = false;
int32 UEdGraphSchema_K2::CurrentCacheRefreshID = 0;

const FString UEdGraphSchema_K2::AllObjectTypes(TEXT("ObjectTypes"));

UEdGraphSchema_K2::UEdGraphSchema_K2(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize cached static references to well-known struct types
	if (VectorStruct == nullptr)
	{
		VectorStruct = TBaseStructure<FVector>::Get();
		RotatorStruct = TBaseStructure<FRotator>::Get();
		TransformStruct = TBaseStructure<FTransform>::Get();
		LinearColorStruct = TBaseStructure<FLinearColor>::Get();
		ColorStruct = TBaseStructure<FColor>::Get();
	}
}

bool UEdGraphSchema_K2::DoesFunctionHaveOutParameters( const UFunction* Function ) const
{
	if ( Function != NULL )
	{
		for ( TFieldIterator<UProperty> PropertyIt(Function); PropertyIt; ++PropertyIt )
		{
			if ( PropertyIt->PropertyFlags & CPF_OutParm )
			{
				return true;
			}
		}
	}

	return false;
}

bool UEdGraphSchema_K2::CanFunctionBeUsedInGraph(const UClass* InClass, const UFunction* InFunction, const UEdGraph* InDestGraph, uint32 InAllowedFunctionTypes, bool bInCalledForEach, FText* OutReason) const
{
	if (CanUserKismetCallFunction(InFunction))
	{
		bool bLatentFuncsAllowed = true;
		bool bIsConstructionScript = false;

		if(InDestGraph != nullptr)
		{
			bLatentFuncsAllowed = (GetGraphType(InDestGraph) == GT_Ubergraph || (GetGraphType(InDestGraph) == GT_Macro));
			bIsConstructionScript = IsConstructionScript(InDestGraph);
		}

		const bool bIsPureFunc = (InFunction->HasAnyFunctionFlags(FUNC_BlueprintPure) != false);
		if (bIsPureFunc)
		{
			const bool bAllowPureFuncs = (InAllowedFunctionTypes & FT_Pure) != 0;
			if (!bAllowPureFuncs)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("PureFunctionsNotAllowed", "Pure functions are not allowed.");
				}

				return false;
			}
		}
		else
		{
			const bool bAllowImperativeFuncs = (InAllowedFunctionTypes & FT_Imperative) != 0;
			if (!bAllowImperativeFuncs)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("ImpureFunctionsNotAllowed", "Impure functions are not allowed.");
				}

				return false;
			}
		}

		const bool bIsConstFunc = (InFunction->HasAnyFunctionFlags(FUNC_Const) != false);
		const bool bAllowConstFuncs = (InAllowedFunctionTypes & FT_Const) != 0;
		if (bIsConstFunc && !bAllowConstFuncs)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("ConstFunctionsNotAllowed", "Const functions are not allowed.");
			}

			return false;
		}

		const bool bIsLatent = InFunction->HasMetaData(FBlueprintMetadata::MD_Latent);
		if (bIsLatent && !bLatentFuncsAllowed)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("LatentFunctionsNotAllowed", "Latent functions cannot be used here.");
			}

			return false;
		}

		const bool bIsProtected = InFunction->GetBoolMetaData(FBlueprintMetadata::MD_Protected);
		const bool bFuncBelongsToSubClass = InClass->IsChildOf(InFunction->GetOuterUClass());
		if (bIsProtected)
		{
			const bool bAllowProtectedFuncs = (InAllowedFunctionTypes & FT_Protected) != 0;
			if (!bAllowProtectedFuncs)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("ProtectedFunctionsNotAllowed", "Protected functions are not allowed.");
				}

				return false;
			}

			if (!bFuncBelongsToSubClass)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("ProtectedFunctionInaccessible", "Function is protected and inaccessible.");
				}

				return false;
			}
		}

		const bool bIsPrivate = InFunction->GetBoolMetaData(FBlueprintMetadata::MD_Private);
		const bool bFuncBelongsToClass = bFuncBelongsToSubClass && (InFunction->GetOuterUClass() == InClass);
		if (bIsPrivate && !bFuncBelongsToClass)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("PrivateFunctionInaccessible", "Function is private and inaccessible.");
			}

			return false;
		}

		const bool bIsUnsafeForConstruction = InFunction->GetBoolMetaData(FBlueprintMetadata::MD_UnsafeForConstructionScripts);	
		if (bIsUnsafeForConstruction && bIsConstructionScript)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("FunctionUnsafeForConstructionScript", "Function cannot be used in a Construction Script.");
			}

			return false;
		}

		const bool bRequiresWorldContext = InFunction->HasMetaData(FBlueprintMetadata::MD_WorldContext);
		if (bRequiresWorldContext)
		{
			if (InDestGraph && !InFunction->HasMetaData(FBlueprintMetadata::MD_CallableWithoutWorldContext))
			{
				const FString& ContextParam = InFunction->GetMetaData(FBlueprintMetadata::MD_WorldContext);
				if (InFunction->FindPropertyByName(FName(*ContextParam)) != nullptr)
				{
					UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(InDestGraph);
					const bool bIsFunctLib = BP && (EBlueprintType::BPTYPE_FunctionLibrary == BP->BlueprintType);
					UClass* ParentClass = BP ? BP->ParentClass : NULL;
					const bool bIncompatibleParent = ParentClass && (!FBlueprintEditorUtils::ImplentsGetWorld(BP) && !ParentClass->HasMetaDataHierarchical(FBlueprintMetadata::MD_ShowWorldContextPin));
					if (!bIsFunctLib && bIncompatibleParent)
					{
						if (OutReason != nullptr)
						{
							*OutReason = LOCTEXT("FunctionRequiresWorldContext", "Function requires a world context.");
						}

						return false;
					}
				}	
			}
		}

		const bool bFunctionStatic = InFunction->HasAllFunctionFlags(FUNC_Static);
		const bool bHasReturnParams = (InFunction->GetReturnProperty() != NULL);
		const bool bHasArrayPointerParms = InFunction->HasMetaData(FBlueprintMetadata::MD_ArrayParam);

		const bool bAllowForEachCall = !bFunctionStatic && !bIsLatent && !bIsPureFunc && !bIsConstFunc && !bHasReturnParams && !bHasArrayPointerParms;
		if (bInCalledForEach && !bAllowForEachCall)
		{
			if(OutReason != nullptr)
			{
				if(bFunctionStatic)
				{
					*OutReason = LOCTEXT("StaticFunctionsNotAllowedInForEachContext", "Static functions cannot be used within a ForEach context.");
				}
				else if(bIsLatent)
				{
					*OutReason = LOCTEXT("LatentFunctionsNotAllowedInForEachContext", "Latent functions cannot be used within a ForEach context.");
				}
				else if(bIsPureFunc)
				{
					*OutReason = LOCTEXT("PureFunctionsNotAllowedInForEachContext", "Pure functions cannot be used within a ForEach context.");
				}
				else if(bIsConstFunc)
				{
					*OutReason = LOCTEXT("ConstFunctionsNotAllowedInForEachContext", "Const functions cannot be used within a ForEach context.");
				}
				else if(bHasReturnParams)
				{
					*OutReason = LOCTEXT("FunctionsWithReturnValueNotAllowedInForEachContext", "Functions that return a value cannot be used within a ForEach context.");
				}
				else if(bHasArrayPointerParms)
				{
					*OutReason = LOCTEXT("FunctionsWithArrayParmsNotAllowedInForEachContext", "Functions with array parameters cannot be used within a ForEach context.");
				}
				else
				{
					*OutReason = LOCTEXT("FunctionNotAllowedInForEachContext", "Function cannot be used within a ForEach context.");
				}
			}

			return false;
		}

		return true;
	}

	if(OutReason != nullptr)
	{
		*OutReason = LOCTEXT("FunctionInvalid", "Invalid function.");
	}

	return false;
}

UFunction* UEdGraphSchema_K2::GetCallableParentFunction(UFunction* Function) const
{
	if( Function && Cast<UClass>(Function->GetOuter()) )
	{
		const FName FunctionName = Function->GetFName();

		// Search up the parent scopes
		UClass* ParentClass = CastChecked<UClass>(Function->GetOuter())->GetSuperClass();
		UFunction* ClassFunction = ParentClass->FindFunctionByName(FunctionName);

		return ClassFunction;
	}

	return NULL;
}

bool UEdGraphSchema_K2::CanUserKismetCallFunction(const UFunction* Function)
{
	return Function && 
		(Function->HasAllFunctionFlags(FUNC_BlueprintCallable) && !Function->HasAllFunctionFlags(FUNC_Delegate) && !Function->GetBoolMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly) && !Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction));
}

bool UEdGraphSchema_K2::CanKismetOverrideFunction(const UFunction* Function)
{
	return  Function && 
		(Function->HasAllFunctionFlags(FUNC_BlueprintEvent) && !Function->HasAllFunctionFlags(FUNC_Delegate) && !Function->GetBoolMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly) && !Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction));
}

bool UEdGraphSchema_K2::HasFunctionAnyOutputParameter(const UFunction* InFunction)
{
	check(InFunction);
	for (TFieldIterator<UProperty> PropIt(InFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParam = *PropIt;
		if (FuncParam->HasAnyPropertyFlags(CPF_ReturnParm) || (FuncParam->HasAnyPropertyFlags(CPF_OutParm) && !FuncParam->HasAnyPropertyFlags(CPF_ReferenceParm) && !FuncParam->HasAnyPropertyFlags(CPF_ConstParm)))
		{
			return true;
		}
	}

	return false;
}

bool UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(const UFunction* InFunction)
{
	// First check we are override-able, non-static and non-const
	if (!InFunction || !CanKismetOverrideFunction(InFunction) || InFunction->HasAnyFunctionFlags(FUNC_Static|FUNC_Const))
	{
		return false;
	}

	// Then look to see if we have any output, return, or reference params
	return !HasFunctionAnyOutputParameter(InFunction);
}

bool UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(const UFunction* InFunction)
{	
	if (!InFunction || 
		!CanUserKismetCallFunction(InFunction) ||
		InFunction->HasMetaData(FBlueprintMetadata::MD_Latent) ||
		InFunction->HasAllFunctionFlags(FUNC_BlueprintPure))
	{
		return false;
	}

	return true;
}

FText UEdGraphSchema_K2::GetFriendlySignatureName(const UFunction* Function)
{
	return UK2Node_CallFunction::GetUserFacingFunctionName( Function );
}

void UEdGraphSchema_K2::GetAutoEmitTermParameters(const UFunction* Function, TArray<FString>& AutoEmitParameterNames) const
{
	AutoEmitParameterNames.Empty();

	if( Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) )
	{
		const FString& MetaData = Function->GetMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm);
		MetaData.ParseIntoArray(AutoEmitParameterNames, TEXT(","), true);

		for (int32 NameIndex = 0; NameIndex < AutoEmitParameterNames.Num();)
		{
			FString& ParameterName = AutoEmitParameterNames[NameIndex];
			ParameterName.TrimStartAndEndInline();
			if (ParameterName.IsEmpty())
			{
				AutoEmitParameterNames.RemoveAtSwap(NameIndex);
			}
			else
			{
				++NameIndex;
			}
		}
	}
}

bool UEdGraphSchema_K2::FunctionHasParamOfType(const UFunction* InFunction, UEdGraph const* InGraph, const FEdGraphPinType& DesiredPinType, bool bWantOutput) const
{
	TSet<FString> HiddenPins;
	FBlueprintEditorUtils::GetHiddenPinsForFunction(InGraph, InFunction, HiddenPins);

	// Iterate over all params of function
	for (TFieldIterator<UProperty> PropIt(InFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParam = *PropIt;

		// Ensure that this isn't a hidden parameter
		if (!HiddenPins.Contains(FuncParam->GetName()))
		{
			// See if this is the direction we want (input or output)
			const bool bIsFunctionInput = !FuncParam->HasAnyPropertyFlags(CPF_OutParm) || FuncParam->HasAnyPropertyFlags(CPF_ReferenceParm);
			if (bIsFunctionInput != bWantOutput)
			{
				// See if this pin has compatible types
				FEdGraphPinType ParamPinType;
				bool bConverted = ConvertPropertyToPinType(FuncParam, ParamPinType);
				if (bConverted)
				{
					UClass* Context = nullptr;
					UBlueprint* Blueprint = Cast<UBlueprint>(InGraph->GetOuter());
					if (Blueprint)
					{
						Context = Blueprint->GeneratedClass;
					}

					if (bIsFunctionInput && ArePinTypesCompatible(DesiredPinType, ParamPinType, Context))
					{
						return true;
					}
					else if (!bIsFunctionInput && ArePinTypesCompatible(ParamPinType, DesiredPinType, Context))
					{
						return true;
					}
				}
			}
		}
	}

	// Boo, no pin of this type
	return false;
}

void UEdGraphSchema_K2::AddExtraFunctionFlags(const UEdGraph* CurrentGraph, int32 ExtraFlags) const
{
	for (UEdGraphNode* Node : CurrentGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			EntryNode->AddExtraFlags(ExtraFlags);
		}
	}
}

void UEdGraphSchema_K2::MarkFunctionEntryAsEditable(const UEdGraph* CurrentGraph, bool bNewEditable) const
{
	for (UEdGraphNode* Node : CurrentGraph->Nodes)
	{
		if (UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node))
		{
			EditableNode->bIsEditable = bNewEditable;
		}
	}
}

bool UEdGraphSchema_K2::IsActorValidForLevelScriptRefs(const AActor* TestActor, const UBlueprint* Blueprint) const
{
	check(Blueprint);

	return TestActor
		&& FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& (TestActor->GetLevel() == FBlueprintEditorUtils::GetLevelFromBlueprint(Blueprint))
		&& FKismetEditorUtilities::IsActorValidForLevelScript(TestActor);
}

void UEdGraphSchema_K2::ReplaceSelectedNode(UEdGraphNode* SourceNode, AActor* TargetActor)
{
	check(SourceNode);

	if (TargetActor != NULL)
	{
		UK2Node_Literal* LiteralNode = (UK2Node_Literal*)(SourceNode);
		if (LiteralNode)
		{
			const FScopedTransaction Transaction( LOCTEXT("ReplaceSelectedNodeUndoTransaction", "Replace Selected Node") );

			LiteralNode->Modify();
			LiteralNode->SetObjectRef( TargetActor );
			LiteralNode->ReconstructNode();
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CastChecked<UEdGraph>(SourceNode->GetOuter()));
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
}

void UEdGraphSchema_K2::AddSelectedReplaceableNodes( UBlueprint* Blueprint, const UEdGraphNode* InGraphNode, FMenuBuilder* MenuBuilder ) const
{
	//Only allow replace object reference functionality for literal nodes
	const UK2Node_Literal* LiteralNode = Cast<UK2Node_Literal>(InGraphNode);
	if (LiteralNode)
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			// We only care about actors that are referenced in the world for literals, and also in the same level as this blueprint
			AActor* Actor = Cast<AActor>(*Iter);
			if( LiteralNode->GetObjectRef() != Actor && IsActorValidForLevelScriptRefs(Actor, Blueprint) )
			{
				FText Description = FText::Format( LOCTEXT("ChangeToActorName", "Change to <{0}>"), FText::FromString( Actor->GetActorLabel() ) );
				FText ToolTip = LOCTEXT("ReplaceNodeReferenceToolTip", "Replace node reference");
				MenuBuilder->AddMenuEntry( Description, ToolTip, FSlateIcon(), FUIAction(
					FExecuteAction::CreateUObject((UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::ReplaceSelectedNode, const_cast< UEdGraphNode* >(InGraphNode), Actor) ) );
			}
		}
	}
}



bool UEdGraphSchema_K2::CanUserKismetAccessVariable(const UProperty* Property, const UClass* InClass, EDelegateFilterMode FilterMode)
{
	const bool bIsDelegate = Property->IsA(UMulticastDelegateProperty::StaticClass());
	const bool bIsAccessible = Property->HasAllPropertyFlags(CPF_BlueprintVisible);
	const bool bIsAssignableOrCallable = Property->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
	
	const bool bPassesDelegateFilter = (bIsAccessible && !bIsDelegate && (FilterMode != MustBeDelegate)) || 
		(bIsAssignableOrCallable && bIsDelegate && (FilterMode != CannotBeDelegate));

	const bool bHidden = FObjectEditorUtils::IsVariableCategoryHiddenFromClass(Property, InClass);

	return !Property->HasAnyPropertyFlags(CPF_Parm) && bPassesDelegateFilter && !bHidden;
}

bool UEdGraphSchema_K2::ClassHasBlueprintAccessibleMembers(const UClass* InClass) const
{
	// @TODO Don't show other blueprints yet...
	UBlueprint* ClassBlueprint = UBlueprint::GetBlueprintFromClass(InClass);
	if (!InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && (ClassBlueprint == NULL))
	{
		// Find functions
		for (TFieldIterator<UFunction> FunctionIt(InClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			const bool bIsBlueprintProtected = Function->GetBoolMetaData(FBlueprintMetadata::MD_Protected);
			const bool bHidden = FObjectEditorUtils::IsFunctionHiddenFromClass(Function, InClass);
			if (UEdGraphSchema_K2::CanUserKismetCallFunction(Function) && !bIsBlueprintProtected && !bHidden)
			{
				return true;
			}
		}

		// Find vars
		for (TFieldIterator<UProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			if (CanUserKismetAccessVariable(Property, InClass, CannotBeDelegate))
			{
				return true;
			}
		}
	}

	return false;
}

bool UEdGraphSchema_K2::IsAllowableBlueprintVariableType(const UEnum* InEnum)
{
	return InEnum && (InEnum->GetBoolMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType) || InEnum->IsA<UUserDefinedEnum>());
}

bool UEdGraphSchema_K2::IsAllowableBlueprintVariableType(const UClass* InClass)
{
	if (InClass)
	{
		// No Skeleton classes or reinstancing classes (they would inherit the BlueprintType metadata)
		if (FKismetEditorUtilities::IsClassABlueprintSkeleton(InClass)
			|| InClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			return false;
		}

		// No Blueprint Macro Libraries
		if (FKismetEditorUtilities::IsClassABlueprintMacroLibrary(InClass))
		{
			return false;
		}

		// UObject is an exception, and is always a blueprint-able type
		if(InClass == UObject::StaticClass())
		{
			return true;
		}
		
		// cannot have level script variables
		if (InClass->IsChildOf(ALevelScriptActor::StaticClass()))
		{
			return false;
		}

		const UClass* ParentClass = InClass;
		while(ParentClass)
		{
			// Climb up the class hierarchy and look for "BlueprintType" and "NotBlueprintType" to see if this class is allowed.
			if(ParentClass->GetBoolMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType)
				|| ParentClass->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
			{
				return true;
			}
			else if(ParentClass->GetBoolMetaData(FBlueprintMetadata::MD_NotAllowableBlueprintVariableType))
			{
				return false;
			}
			ParentClass = ParentClass->GetSuperClass();
		}
	}
	
	return false;
}

bool UEdGraphSchema_K2::IsAllowableBlueprintVariableType(const UScriptStruct* InStruct, const bool bForInternalUse)
{
	if (const UUserDefinedStruct* UDStruct = Cast<const UUserDefinedStruct>(InStruct))
	{
		if (EUserDefinedStructureStatus::UDSS_UpToDate != UDStruct->Status.GetValue())
		{
			return false;
		}
	}
	return (InStruct && InStruct->GetBoolMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType) && (bForInternalUse || !InStruct->GetBoolMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly)));
}

bool UEdGraphSchema_K2::DoesGraphSupportImpureFunctions(const UEdGraph* InGraph) const
{
	const EGraphType GraphType = GetGraphType(InGraph);
	const bool bAllowImpureFuncs = GraphType != GT_Animation; //@TODO: It's really more nuanced than this (e.g., in a function someone wants to write as pure)

	return bAllowImpureFuncs;
}

bool UEdGraphSchema_K2::IsPropertyExposedOnSpawn(const UProperty* Property)
{
	Property = FBlueprintEditorUtils::GetMostUpToDateProperty(Property);
	if (Property)
	{
		const bool bMeta = Property->HasMetaData(FBlueprintMetadata::MD_ExposeOnSpawn);
		const bool bFlag = Property->HasAllPropertyFlags(CPF_ExposeOnSpawn);
		if (bMeta != bFlag)
		{
			UE_LOG(LogBlueprint, Warning
				, TEXT("ExposeOnSpawn ambiguity. Property '%s', MetaData '%s', Flag '%s'")
				, *Property->GetFullName()
				, bMeta ? *GTrue.ToString() : *GFalse.ToString()
				, bFlag ? *GTrue.ToString() : *GFalse.ToString());
		}
		return bMeta || bFlag;
	}
	return false;
}

// if node is a get/set variable and the variable it refers to does not exist
static bool IsUsingNonExistantVariable(const UEdGraphNode* InGraphNode, UBlueprint* OwnerBlueprint)
{
	bool bNonExistantVariable = false;
	const bool bBreakOrMakeStruct = 
		InGraphNode->IsA(UK2Node_BreakStruct::StaticClass()) || 
		InGraphNode->IsA(UK2Node_MakeStruct::StaticClass());
	if (!bBreakOrMakeStruct)
	{
		if (const UK2Node_Variable* Variable = Cast<const UK2Node_Variable>(InGraphNode))
		{
			if (Variable->VariableReference.IsSelfContext())
			{
				TSet<FName> CurrentVars;
				FBlueprintEditorUtils::GetClassVariableList(OwnerBlueprint, CurrentVars);
				if ( false == CurrentVars.Contains(Variable->GetVarName()) )
				{
					bNonExistantVariable = true;
				}
			}
			else if(Variable->VariableReference.IsLocalScope())
			{
				// If there is no member scope, or we can't find the local variable in the member scope, then it's non-existant
				UStruct* MemberScope = Variable->VariableReference.GetMemberScope(Variable->GetBlueprintClassFromNode());
				if (MemberScope == nullptr || !FBlueprintEditorUtils::FindLocalVariable(OwnerBlueprint, MemberScope, Variable->GetVarName()))
				{
					bNonExistantVariable = true;
				}
			}
		}
	}
	return bNonExistantVariable;
}

bool UEdGraphSchema_K2::PinHasSplittableStructType(const UEdGraphPin* InGraphPin) const
{
	const FEdGraphPinType& PinType = InGraphPin->PinType;
	bool bCanSplit = (!PinType.IsContainer() && PinType.PinCategory == PC_Struct);

	if (bCanSplit)
	{
		if (UScriptStruct* StructType = Cast<UScriptStruct>(InGraphPin->PinType.PinSubCategoryObject.Get()))
		{
			if (InGraphPin->Direction == EGPD_Input)
			{
				bCanSplit = UK2Node_MakeStruct::CanBeSplit(StructType);
				if (!bCanSplit)
				{
					const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeMake"));
					UFunction* Function = FindObject<UFunction>(NULL, *MetaData, true);
					bCanSplit = (Function != NULL);
				}
			}
			else
			{
				bCanSplit = UK2Node_BreakStruct::CanBeSplit(StructType);
				if (!bCanSplit)
				{
					const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeBreak"));
					UFunction* Function = FindObject<UFunction>(NULL, *MetaData, true);
					bCanSplit = (Function != NULL);
				}
			}
		}
		else
		{
			// If the struct type of a split struct pin no longer exists this can happen
			bCanSplit = false;
		}
	}

	return bCanSplit;
}

bool UEdGraphSchema_K2::PinDefaultValueIsEditable(const UEdGraphPin& InGraphPin) const
{
	// Array types are not currently assignable without a 'make array' node:
	if( InGraphPin.PinType.IsContainer() )
	{
		return false;
	}

	// User defined structures (from code or from data) cannot accept default values:
	if( InGraphPin.PinType.PinCategory == PC_Struct )
	{
		// Only the built in struct types are editable as 'default' values on a pin.
		// See FNodeFactory::CreatePinWidget for justification of the above statement!
		UObject const& SubCategoryObject = *InGraphPin.PinType.PinSubCategoryObject;
		return &SubCategoryObject == VectorStruct 
			|| &SubCategoryObject == RotatorStruct
			|| &SubCategoryObject == TransformStruct
			|| &SubCategoryObject == LinearColorStruct
			|| &SubCategoryObject == ColorStruct
			|| &SubCategoryObject == FCollisionProfileName::StaticStruct();
	}

	return true;
}

void UEdGraphSchema_K2::SelectAllNodesInDirection(TEnumAsByte<enum EEdGraphPinDirection> InDirection, UEdGraph* Graph, UEdGraphPin* InGraphPin)
{
	/** Traverses the node graph out from the specified pin, logging each node that it visits along the way. */
	struct FDirectionalNodeVisitor
	{
		FDirectionalNodeVisitor(UEdGraphPin* StartingPin, EEdGraphPinDirection TargetDirection)
			: Direction(TargetDirection)
		{
			TraversePin(StartingPin);
		}

		/** If the pin is the right direction, visits each of its attached nodes */
		void TraversePin(UEdGraphPin* Pin)
		{
			if (Pin->Direction == Direction)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					VisitNode(LinkedPin->GetOwningNode());
				}
			}
		}

		/** If the node has already been visited, does nothing. Otherwise it traverses each of its pins. */
		void VisitNode(UEdGraphNode* Node)
		{
			bool bAlreadyVisited = false;
			VisitedNodes.Add(Node, &bAlreadyVisited);

			if (!bAlreadyVisited)
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					TraversePin(Pin);
				}
			}
		}

		EEdGraphPinDirection Direction;
		TSet<UEdGraphNode*>  VisitedNodes;
	};

	FDirectionalNodeVisitor NodeVisitor(InGraphPin, InDirection);
	for (UEdGraphNode* Node : NodeVisitor.VisitedNodes)
	{
		FKismetEditorUtilities::AddToSelection(Graph, Node);
	}
}

void UEdGraphSchema_K2::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	check(CurrentGraph);
	UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);

	if (InGraphPin != NULL)
	{
		MenuBuilder->BeginSection("EdGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			if (!bIsDebugging)
			{
				// Break pin links
				if (InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakPinLinks );
				}
	
				// Add the change pin type action, if this is a select node
				if (InGraphNode->IsA(UK2Node_Select::StaticClass()))
				{
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ChangePinType);
				}
	
				// add sub menu for break link to
				if (InGraphPin->LinkedTo.Num() > 0)
				{
					MenuBuilder->AddMenuEntry(
						InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input ? LOCTEXT("SelectAllInputNodes", "Select All Input Nodes") : LOCTEXT("SelectAllOutputNodes", "Select All Output Nodes"),
						InGraphPin->Direction == EEdGraphPinDirection::EGPD_Input ? LOCTEXT("SelectAllInputNodesTooltip", "Adds all input Nodes linked to this Pin to selection") : LOCTEXT("SelectAllOutputNodesTooltip", "Adds all output Nodes linked to this Pin to selection"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateUObject((UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::SelectAllNodesInDirection, InGraphPin->Direction, const_cast<UEdGraph*>(CurrentGraph), const_cast<UEdGraphPin*>(InGraphPin))));

					if(InGraphPin->LinkedTo.Num() > 1)
					{
						MenuBuilder->AddSubMenu(
							LOCTEXT("BreakLinkTo", "Break Link To..."),
							LOCTEXT("BreakSpecificLinks", "Break a specific link..."),
							FNewMenuDelegate::CreateUObject( (UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));

						MenuBuilder->AddSubMenu(
							LOCTEXT("JumpToConnection", "Jump to Connection..."),
							LOCTEXT("JumpToSpecificConnection", "Jump to specific connection..."),
							FNewMenuDelegate::CreateUObject( (UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::GetJumpToConnectionSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));

						MenuBuilder->AddSubMenu(
							LOCTEXT("StraightenConnection", "Straighten Connection To..."),
							LOCTEXT("StraightenConnection_Tip", "Straighten a specific connection"),
							FNewMenuDelegate::CreateUObject( this, &UEdGraphSchema_K2::GetStraightenConnectionToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
					}
					else
					{
						((UEdGraphSchema_K2*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
						((UEdGraphSchema_K2*const)this)->GetJumpToConnectionSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
						
						UEdGraphPin* Pin = InGraphPin->LinkedTo[0];
						FText PinName = Pin->GetDisplayName();
						FText NodeName = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);

						MenuBuilder->AddMenuEntry(
							FGraphEditorCommands::Get().StraightenConnections,
							NAME_None,
							FText::Format(LOCTEXT("StraightenDescription_SinglePin", "Straighten Connection to {0} ({1})"), NodeName, PinName),
							FText::Format(LOCTEXT("StraightenDescription_SinglePin_Node_Tip", "Straighten the connection between this pin, and {0} ({1})"), NodeName, PinName),
							FSlateIcon(NAME_None, NAME_None, NAME_None)
						);
					}
				}
	
				// Conditionally add the var promotion pin if this is an output pin and it's not an exec pin
				if (InGraphPin->PinType.PinCategory != PC_Exec)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteToVariable );

					if (FBlueprintEditorUtils::DoesSupportLocalVariables(CurrentGraph))
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteToLocalVariable );
					}
				}
	
				if (InGraphPin->PinType.PinCategory == PC_Struct && InGraphNode->CanSplitPin(InGraphPin))
				{
					// If the pin cannot be split, create an error tooltip to use
					FText Tooltip;
					if (PinHasSplittableStructType(InGraphPin))
					{
						Tooltip = FGraphEditorCommands::Get().SplitStructPin->GetDescription();
					}
					else
					{
						Tooltip = LOCTEXT("SplitStructPin_Error", "Cannot split the struct pin, it may be missing Blueprint exposed properties!");
					}
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().SplitStructPin, NAME_None, FGraphEditorCommands::Get().SplitStructPin->GetLabel(), Tooltip );
				}

				if (InGraphPin->ParentPin != NULL)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().RecombineStructPin );
				}
	
				// Conditionally add the execution path pin removal if this is an execution branching node
				if( InGraphPin->Direction == EGPD_Output && InGraphPin->GetOwningNode())
				{
					if (CastChecked<UK2Node>(InGraphPin->GetOwningNode())->CanEverRemoveExecutionPin())
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().RemoveExecutionPin );
					}
				}

				if (UK2Node_SetFieldsInStruct::ShowCustomPinActions(InGraphPin, true))
				{
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveThisStructVarPin);
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveOtherStructVarPins);
				}

				if (InGraphPin->PinType.PinCategory != PC_Exec && InGraphPin->Direction == EGPD_Input && InGraphPin->LinkedTo.Num() == 0 && !ShouldHidePinDefaultValue(const_cast<UEdGraphPin*>(InGraphPin)))
				{
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ResetPinToDefaultValue);
				}
			}
		}
		MenuBuilder->EndSection();

		// Add the watch pin / unwatch pin menu items
		MenuBuilder->BeginSection("EdGraphSchemaWatches", LOCTEXT("WatchesHeader", "Watches"));
		{
			if (!IsMetaPin(*InGraphPin))
			{
				const UEdGraphPin* WatchedPin = ((InGraphPin->Direction == EGPD_Input) && (InGraphPin->LinkedTo.Num() > 0)) ? InGraphPin->LinkedTo[0] : InGraphPin;
				if (FKismetDebugUtilities::IsPinBeingWatched(OwnerBlueprint, WatchedPin))
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().StopWatchingPin );
				}
				else
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().StartWatchingPin );
				}
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode != NULL)
	{
		if (IsUsingNonExistantVariable(InGraphNode, OwnerBlueprint))
		{
			MenuBuilder->BeginSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
			{
				GetNonExistentVariableMenu(InGraphNode,OwnerBlueprint, MenuBuilder);
			}
			MenuBuilder->EndSection();
		}
		else
		{
			MenuBuilder->BeginSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
			{
				if (!bIsDebugging)
				{
					// Replaceable node display option
					AddSelectedReplaceableNodes( OwnerBlueprint, InGraphNode, MenuBuilder );

					// Node contextual actions
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Delete );
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Cut );
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Copy );
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Duplicate );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ReconstructNodes );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakNodeLinks );

					// Conditionally add the action to add an execution pin, if this is an execution node
					if( InGraphNode->IsA(UK2Node_ExecutionSequence::StaticClass()) || InGraphNode->IsA(UK2Node_Switch::StaticClass()) )
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().AddExecutionPin );
					}

					// Conditionally add the action to create a super function call node, if this is an event or function entry
					if( InGraphNode->IsA(UK2Node_Event::StaticClass()) || InGraphNode->IsA(UK2Node_FunctionEntry::StaticClass()) )
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().AddParentNode );
					}

					// Conditionally add the actions to add or remove an option pin, if this is a select node
					if (InGraphNode->IsA(UK2Node_Select::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AddOptionPin);
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveOptionPin);
					}

					// Don't show the "Assign selected Actor" option if more than one actor is selected
					if (InGraphNode->IsA(UK2Node_ActorBoundEvent::StaticClass()) && GEditor->GetSelectedActorCount() == 1)
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AssignReferencedActor);
					}
				}

				// If the node has an associated definition (for some loose sense of the word), allow going to it (same action as double-clicking on a node)
				if (InGraphNode->CanJumpToDefinition())
				{
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().GoToDefinition);
				}

				// show search for references for everyone
				MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().FindReferences);

				if (!bIsDebugging)
				{
					if (InGraphNode->IsA(UK2Node_Variable::StaticClass()))
					{
						GetReplaceVariableMenu(InGraphNode, OwnerBlueprint, MenuBuilder, true);
					}

					if (InGraphNode->IsA(UK2Node_SetFieldsInStruct::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RestoreAllStructVarPins);
					}

					MenuBuilder->AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("Rename", "Rename"), LOCTEXT("Rename_Tooltip", "Renames selected function or variable in blueprint.") );
				}

				// Select referenced actors in the level
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().SelectReferenceInLevel );
			}
			MenuBuilder->EndSection(); //EdGraphSchemaNodeActions

			if (!bIsDebugging)
			{
				// Collapse/expand nodes
				MenuBuilder->BeginSection("EdGraphSchemaOrganization", LOCTEXT("OrganizationHeader", "Organization"));
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().CollapseNodes );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().CollapseSelectionToFunction );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().CollapseSelectionToMacro );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ExpandNodes );

					if(InGraphNode->IsA(UK2Node_Composite::StaticClass()))
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteSelectionToFunction );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteSelectionToMacro );
					}

					MenuBuilder->AddSubMenu(LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewMenuDelegate::CreateLambda([](FMenuBuilder& InMenuBuilder){
						
						InMenuBuilder.BeginSection("EdGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().AlignNodesTop );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().AlignNodesMiddle );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().AlignNodesBottom );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().AlignNodesLeft );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().AlignNodesCenter );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().AlignNodesRight );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().StraightenConnections );
						InMenuBuilder.EndSection();

						InMenuBuilder.BeginSection("EdGraphSchemaDistribution", LOCTEXT("DistributionHeader", "Distribution"));
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().DistributeNodesHorizontally );
						InMenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().DistributeNodesVertically );
						InMenuBuilder.EndSection();
						
					}));
				}
				
				MenuBuilder->EndSection();
			}

			if (const UK2Node* K2Node = Cast<const UK2Node>(InGraphNode))
			{
				if (!K2Node->IsNodePure())
				{
					if (!bIsDebugging && GetDefault<UBlueprintEditorSettings>()->bAllowExplicitImpureNodeDisabling)
					{
						// Don't expose the enabled state for disabled nodes that were not explicitly disabled by the user
						if (!K2Node->IsAutomaticallyPlacedGhostNode())
						{
							// Add compile options
							MenuBuilder->BeginSection("EdGraphSchemaCompileOptions", LOCTEXT("CompileOptionsHeader", "Compile Options"));
							{
								MenuBuilder->AddMenuEntry(
									FGraphEditorCommands::Get().DisableNodes,
									NAME_None,
									LOCTEXT("DisableCompile", "Disable (Do Not Compile)"),
									LOCTEXT("DisableCompileToolTip", "Selected node(s) will not be compiled."));

								TSharedPtr<const FUICommandList> MenuCommandList = MenuBuilder->GetTopCommandList();
								if(ensure(MenuCommandList.IsValid()))
								{
									const FUIAction* SubMenuUIAction = MenuCommandList->GetActionForCommand(FGraphEditorCommands::Get().EnableNodes);
									if(ensure(SubMenuUIAction))
									{
										MenuBuilder->AddSubMenu(
											LOCTEXT("EnableCompileSubMenu", "Enable Compile"),
											LOCTEXT("EnableCompileSubMenuToolTip", "Options to enable selected node(s) for compile."),
											FNewMenuDelegate::CreateLambda([MenuCommandList](FMenuBuilder& SubMenuBuilder)
											{
												SubMenuBuilder.PushCommandList(MenuCommandList.ToSharedRef());

												SubMenuBuilder.AddMenuEntry(
													FGraphEditorCommands::Get().EnableNodes_Always,
													NAME_None,
													LOCTEXT("EnableCompileAlways", "Always"),
													LOCTEXT("EnableCompileAlwaysToolTip", "Always compile selected node(s)."));
												SubMenuBuilder.AddMenuEntry(
													FGraphEditorCommands::Get().EnableNodes_DevelopmentOnly,
													NAME_None,
													LOCTEXT("EnableCompileDevelopmentOnly", "Development Only"),
													LOCTEXT("EnableCompileDevelopmentOnlyToolTip", "Compile selected node(s) for development only."));

												SubMenuBuilder.PopCommandList();
											}),
											*SubMenuUIAction,
											NAME_None, FGraphEditorCommands::Get().EnableNodes->GetUserInterfaceType());
									}
								}
							}
							MenuBuilder->EndSection();
						}
					}
					
					// Add breakpoint actions
					MenuBuilder->BeginSection("EdGraphSchemaBreakpoints", LOCTEXT("BreakpointsHeader", "Breakpoints"));
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ToggleBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().AddBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().RemoveBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().EnableBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().DisableBreakpoint );
					}
					MenuBuilder->EndSection();
				}
			}

			MenuBuilder->BeginSection("EdGraphSchemaDocumentation", LOCTEXT("DocumentationHeader", "Documentation"));
			{
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().GoToDocumentation );
			}
			MenuBuilder->EndSection();
		}
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}


void UEdGraphSchema_K2::OnCreateNonExistentVariable( UK2Node_Variable* Variable,  UBlueprint* OwnerBlueprint)
{
	if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		const FScopedTransaction Transaction( LOCTEXT("CreateMissingVariable", "Create Missing Variable") );

		if (FBlueprintEditorUtils::AddMemberVariable(OwnerBlueprint,Variable->GetVarName(), Pin->PinType))
		{
			FGuid Guid = FBlueprintEditorUtils::FindMemberVariableGuidByName(OwnerBlueprint, Variable->GetVarName());
			Variable->VariableReference.SetSelfMember( Variable->GetVarName(), Guid );
		}
	}	
}

void UEdGraphSchema_K2::OnCreateNonExistentLocalVariable( UK2Node_Variable* Variable,  UBlueprint* OwnerBlueprint)
{
	if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		const FScopedTransaction Transaction( LOCTEXT("CreateMissingLocalVariable", "Create Missing Local Variable") );

		FName VarName = Variable->GetVarName();
		if (FBlueprintEditorUtils::AddLocalVariable(OwnerBlueprint, Variable->GetGraph(), VarName, Pin->PinType))
		{
			FGuid LocalVarGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(OwnerBlueprint, Variable->GetGraph(), VarName);
			if (LocalVarGuid.IsValid())
			{
				// Loop through every variable in the graph, check if the variable references are the same, and update them
				FMemberReference OldReference = Variable->VariableReference;
				TArray<UK2Node_Variable*> VariableNodeList;
				Variable->GetGraph()->GetNodesOfClass(VariableNodeList);
				for( UK2Node_Variable* VariableNode : VariableNodeList)
				{
					if (VariableNode->VariableReference.IsSameReference(OldReference))
					{
						VariableNode->VariableReference.SetLocalMember(VarName, FBlueprintEditorUtils::GetTopLevelGraph(Variable->GetGraph())->GetName(), LocalVarGuid);
						VariableNode->ReconstructNode();
					}
				}
			}
		}
	}	
}

void UEdGraphSchema_K2::OnReplaceVariableForVariableNode( UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint, FString VariableName, bool bIsSelfMember)
{
	if(UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_ReplaceVariable", "Replace Variable") );
		Variable->Modify();
		Pin->Modify();

		if (bIsSelfMember)
		{
			FName VarName = FName(*VariableName);
			FGuid Guid = FBlueprintEditorUtils::FindMemberVariableGuidByName(OwnerBlueprint, VarName);
			Variable->VariableReference.SetSelfMember( VarName, Guid );
		}
		else
		{
			UEdGraph* FunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(Variable->GetGraph());
			Variable->VariableReference.SetLocalMember( FName(*VariableName), FunctionGraph->GetName(), FBlueprintEditorUtils::FindLocalVariableGuidByName(OwnerBlueprint, FunctionGraph, *VariableName));
		}
		Pin->PinName = VariableName;
		Variable->ReconstructNode();

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(OwnerBlueprint);
	}
}

void UEdGraphSchema_K2::GetReplaceVariableMenu(FMenuBuilder& MenuBuilder, UK2Node_Variable* Variable,  UBlueprint* OwnerBlueprint, bool bReplaceExistingVariable/*=false*/)
{
	if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		FName ExistingVariableName = bReplaceExistingVariable? Variable->GetVarName() : NAME_None;

		FText ReplaceVariableWithTooltipFormat;
		if(!bReplaceExistingVariable)
		{
			ReplaceVariableWithTooltipFormat = LOCTEXT("ReplaceNonExistantVarToolTip", "Variable '{OldVariable}' does not exist, replace with matching variable '{AlternateVariable}'?");
		}
		else
		{
			ReplaceVariableWithTooltipFormat = LOCTEXT("ReplaceExistantVarToolTip", "Replace Variable '{OldVariable}' with matching variable '{AlternateVariable}'?");
		}

		TArray<FName> Variables;
		FBlueprintEditorUtils::GetNewVariablesOfType(OwnerBlueprint, Pin->PinType, Variables);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("Variables", "Variables"));
		for (TArray<FName>::TIterator VarIt(Variables); VarIt; ++VarIt)
		{
			if(*VarIt != ExistingVariableName)
			{
				const FText AlternativeVar = FText::FromName( *VarIt );

				FFormatNamedArguments TooltipArgs;
				TooltipArgs.Add(TEXT("OldVariable"), Variable->GetVarNameText());
				TooltipArgs.Add(TEXT("AlternateVariable"), AlternativeVar);
				const FText Desc = FText::Format(ReplaceVariableWithTooltipFormat, TooltipArgs);

				MenuBuilder.AddMenuEntry( AlternativeVar, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic(&UEdGraphSchema_K2::OnReplaceVariableForVariableNode, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint, (*VarIt).ToString(), /*bIsSelfMember=*/true ) ) );
			}
		}
		MenuBuilder.EndSection();

		FText ReplaceLocalVariableWithTooltipFormat;
		if(!bReplaceExistingVariable)
		{
			ReplaceLocalVariableWithTooltipFormat = LOCTEXT("ReplaceNonExistantLocalVarToolTip", "Variable '{OldVariable}' does not exist, replace with matching local variable '{AlternateVariable}'?");
		}
		else
		{
			ReplaceLocalVariableWithTooltipFormat = LOCTEXT("ReplaceExistantLocalVarToolTip", "Replace Variable '{OldVariable}' with matching local variable '{AlternateVariable}'?");
		}

		TArray<FName> LocalVariables;
		FBlueprintEditorUtils::GetLocalVariablesOfType(Variable->GetGraph(), Pin->PinType, LocalVariables);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("LocalVariables", "LocalVariables"));
		for (TArray<FName>::TIterator VarIt(LocalVariables); VarIt; ++VarIt)
		{
			if(*VarIt != ExistingVariableName)
			{
				const FText AlternativeVar = FText::FromName( *VarIt );

				FFormatNamedArguments TooltipArgs;
				TooltipArgs.Add(TEXT("OldVariable"), Variable->GetVarNameText());
				TooltipArgs.Add(TEXT("AlternateVariable"), AlternativeVar);
				const FText Desc = FText::Format( ReplaceLocalVariableWithTooltipFormat, TooltipArgs );

				MenuBuilder.AddMenuEntry( AlternativeVar, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic(&UEdGraphSchema_K2::OnReplaceVariableForVariableNode, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint, (*VarIt).ToString(), /*bIsSelfMember=*/false ) ) );
			}
		}
		MenuBuilder.EndSection();
	}
}

void UEdGraphSchema_K2::GetNonExistentVariableMenu( const UEdGraphNode* InGraphNode, UBlueprint* OwnerBlueprint, FMenuBuilder* MenuBuilder ) const
{

	if (const UK2Node_Variable* Variable = Cast<const UK2Node_Variable>(InGraphNode))
	{
		// Creating missing variables should never occur in a Macro Library or Interface, they do not support variables
		if(OwnerBlueprint->BlueprintType != BPTYPE_MacroLibrary && OwnerBlueprint->BlueprintType != BPTYPE_Interface )
		{		
			// Creating missing member variables should never occur in a Function Library, they do not support variables
			if(OwnerBlueprint->BlueprintType != BPTYPE_FunctionLibrary)
			{
				// create missing variable
				const FText Label = FText::Format( LOCTEXT("CreateNonExistentVar", "Create variable '{0}'"), Variable->GetVarNameText());
				const FText Desc = FText::Format( LOCTEXT("CreateNonExistentVarToolTip", "Variable '{0}' does not exist, create it?"), Variable->GetVarNameText());
				MenuBuilder->AddMenuEntry( Label, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic( &UEdGraphSchema_K2::OnCreateNonExistentVariable, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint) ) );
			}

			// Only allow creating missing local variables if in a function graph
			if(InGraphNode->GetGraph()->GetSchema()->GetGraphType(InGraphNode->GetGraph()) == GT_Function)
			{
				const FText Label = FText::Format( LOCTEXT("CreateNonExistentLocalVar", "Create local variable '{0}'"), Variable->GetVarNameText());
				const FText Desc = FText::Format( LOCTEXT("CreateNonExistentLocalVarToolTip", "Local variable '{0}' does not exist, create it?"), Variable->GetVarNameText());
				MenuBuilder->AddMenuEntry( Label, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic( &UEdGraphSchema_K2::OnCreateNonExistentLocalVariable, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint) ) );
			}
		}

		// delete this node
		{			
			const FText Desc = FText::Format( LOCTEXT("DeleteNonExistentVarToolTip", "Referenced variable '{0}' does not exist, delete this node?"), Variable->GetVarNameText());
			MenuBuilder->AddMenuEntry( FGenericCommands::Get().Delete, NAME_None, FGenericCommands::Get().Delete->GetLabel(), Desc);
		}

		GetReplaceVariableMenu(InGraphNode, OwnerBlueprint, MenuBuilder);
	}
}

void UEdGraphSchema_K2::GetReplaceVariableMenu(const UEdGraphNode* InGraphNode, UBlueprint* InOwnerBlueprint, FMenuBuilder* InMenuBuilder, bool bInReplaceExistingVariable/* = false*/) const
{
	if (const UK2Node_Variable* Variable = Cast<const UK2Node_Variable>(InGraphNode))
	{
		// replace with matching variables
		if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
		{
			FName ExistingVariableName = bInReplaceExistingVariable? Variable->GetVarName() : NAME_None;

			TArray<FName> Variables;
			FBlueprintEditorUtils::GetNewVariablesOfType(InOwnerBlueprint, Pin->PinType, Variables);
			Variables.RemoveSwap(ExistingVariableName);

			TArray<FName> LocalVariables;
			FBlueprintEditorUtils::GetLocalVariablesOfType(Variable->GetGraph(), Pin->PinType, LocalVariables);
			LocalVariables.RemoveSwap(ExistingVariableName);

			if (Variables.Num() > 0 || LocalVariables.Num() > 0)
			{
				FText ReplaceVariableWithTooltip;
				if(bInReplaceExistingVariable)
				{
					ReplaceVariableWithTooltip = LOCTEXT("ReplaceVariableWithToolTip", "Replace Variable '{0}' with another variable?");
				}
				else
				{
					ReplaceVariableWithTooltip = LOCTEXT("ReplaceMissingVariableWithToolTip", "Variable '{0}' does not exist, replace with another variable?");
				}

				InMenuBuilder->AddSubMenu(
					FText::Format( LOCTEXT("ReplaceVariableWith", "Replace variable '{0}' with..."), Variable->GetVarNameText()),
					FText::Format( ReplaceVariableWithTooltip, Variable->GetVarNameText()),
					FNewMenuDelegate::CreateStatic( &UEdGraphSchema_K2::GetReplaceVariableMenu,
					const_cast<UK2Node_Variable*>(Variable),InOwnerBlueprint, bInReplaceExistingVariable));
			}
		}
	}
}

void UEdGraphSchema_K2::GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for(TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FText Title = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);
		FString TitleString = Title.ToString();
		if ( Pin->PinName != TEXT("") )
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->GetDisplayName().ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), Pin->GetDisplayName() );
			Title = FText::Format( LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd( TitleString );

		FText Description;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeTitle"), Title );
		Args.Add( TEXT("NumberOfNodes"), Count );

		if ( Count == 0 )
		{
			Description = FText::Format( LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject(this, &UEdGraphSchema_K2::BreakSinglePinLink, const_cast< UEdGraphPin* >(InGraphPin), *Links)));
	}
}

void UEdGraphSchema_K2::GetJumpToConnectionSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for(const UEdGraphPin* PinLink : InGraphPin->LinkedTo )
	{
		FText Title = PinLink->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);
		FString TitleString = Title.ToString();
		if ( PinLink->PinName != TEXT("") )
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *PinLink->GetDisplayName().ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), PinLink->GetDisplayName() );
			Title = FText::Format( LOCTEXT("JumpToDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd( TitleString );

		FText Description;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeTitle"), Title );
		Args.Add( TEXT("NumberOfNodes"), Count );

		if ( Count == 0 )
		{
			Description = FText::Format( LOCTEXT("JumpDesc", "Jump to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("JumpDescMulti", "Jump to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
		FExecuteAction::CreateStatic(&FKismetEditorUtilities::BringKismetToFocusAttentionOnPin, PinLink)));
	}
}

// todo: this is a long way off ideal, but we can't pass context from our menu items onto the graph panel implementation
// It'd be better to be able to pass context through to menu/ui commands
namespace { UEdGraphPin* StraightenDestinationPin = nullptr; }
UEdGraphPin* UEdGraphSchema_K2::GetAndResetStraightenDestinationPin()
{
	UEdGraphPin* Temp = StraightenDestinationPin;
	StraightenDestinationPin = nullptr;
	return Temp;
}

void UEdGraphSchema_K2::GetStraightenConnectionToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin ) const
{
	TSharedPtr<const FUICommandList> MenuCommandList = MenuBuilder.GetTopCommandList();
	if(!ensure(MenuCommandList.IsValid()))
	{
		return;
	}

	// Make sure we have a unique name for every entry in the list
	TMap<FString, uint32> LinkTitleCount;

	TMap<UEdGraphNode*, TArray<UEdGraphPin*>> NodeToPins;

	for (UEdGraphPin* Pin : InGraphPin->LinkedTo)
	{
		UEdGraphNode* Node = Pin->GetOwningNode();
		if (Node)
		{
			NodeToPins.FindOrAdd(Node).Add(Pin);
		}
	}

	MenuBuilder.AddMenuEntry( FGraphEditorCommands::Get().StraightenConnections,
		NAME_None, LOCTEXT("StraightenAllConnections", "All Connected Pins"),
		TAttribute<FText>(), FSlateIcon(NAME_None, NAME_None, NAME_None) );

	for (auto& Pair : NodeToPins)
	{
		FText NodeName = Pair.Key->GetNodeTitle(ENodeTitleType::ListView);
		for (UEdGraphPin* Pin : Pair.Value)
		{
			FText PinName = Pin->GetDisplayName();
			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("StraightenDescription_Node", "{0} ({1})"), NodeName, Pin->GetDisplayName()),
				FText::Format(LOCTEXT("StraightenDescription_Node_Tip", "Straighten the connection between this pin, and {0} ({1})"), NodeName, Pin->GetDisplayName()),
				FSlateIcon(),
				FExecuteAction::CreateLambda([=]{
				if (const FUIAction* UIAction = MenuCommandList->GetActionForCommand(FGraphEditorCommands::Get().StraightenConnections))
				{
					StraightenDestinationPin = Pin;
					UIAction->ExecuteAction.Execute();
				}
			}));
		}
	}
}

const FPinConnectionResponse UEdGraphSchema_K2::DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	// Now check to see if there are already connections and this is an 'exclusive' connection
	const bool bBreakExistingDueToExecOutput = IsExecPin(*OutputPin) && (OutputPin->LinkedTo.Num() > 0);
	const bool bBreakExistingDueToDataInput = !IsExecPin(*InputPin) && (InputPin->LinkedTo.Num() > 0);

	bool bMultipleSelfException = false;
	const UK2Node* OwningNode = Cast<UK2Node>(InputPin->GetOwningNode());
	if (bBreakExistingDueToDataInput && 
		IsSelfPin(*InputPin) && 
		OwningNode &&
		OwningNode->AllowMultipleSelfs(false) &&
		!InputPin->PinType.IsContainer() &&
		!OutputPin->PinType.IsContainer() )
	{
		//check if the node wont be expanded as foreach call, if there is a link to an array
		bool bAnyContainerInput = false;
		for(int InputLinkIndex = 0; InputLinkIndex < InputPin->LinkedTo.Num(); InputLinkIndex++)
		{
			if(const UEdGraphPin* Pin = InputPin->LinkedTo[InputLinkIndex])
			{
				if(Pin->PinType.IsContainer())
				{
					bAnyContainerInput = true;
					break;
				}
			}
		}
		bMultipleSelfException = !bAnyContainerInput;
	}

	if (bBreakExistingDueToExecOutput)
	{
		const ECanCreateConnectionResponse ReplyBreakOutputs = (PinA == OutputPin) ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakOutputs, TEXT("Replace existing output connections"));
	}
	else if (bBreakExistingDueToDataInput && !bMultipleSelfException)
	{
		const ECanCreateConnectionResponse ReplyBreakInputs = (PinA == InputPin) ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakInputs, TEXT("Replace existing input connections"));
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}
}

static FText GetPinIncompatibilityReason(const UEdGraphPin* PinA, const UEdGraphPin* PinB, bool* bIsFatalOut = nullptr)
{
	const FEdGraphPinType& PinAType = PinA->PinType;
	const FEdGraphPinType& PinBType = PinB->PinType;

	FFormatNamedArguments MessageArgs;
	MessageArgs.Add(TEXT("PinAName"), PinA->GetDisplayName());
	MessageArgs.Add(TEXT("PinBName"), PinB->GetDisplayName());
	MessageArgs.Add(TEXT("PinAType"), UEdGraphSchema_K2::TypeToText(PinAType));
	MessageArgs.Add(TEXT("PinBType"), UEdGraphSchema_K2::TypeToText(PinBType));

	const UEdGraphPin* InputPin = (PinA->Direction == EGPD_Input) ? PinA : PinB;
	const FEdGraphPinType& InputType  = InputPin->PinType;
	const UEdGraphPin* OutputPin = (InputPin == PinA) ? PinB : PinA;
	const FEdGraphPinType& OutputType = OutputPin->PinType;

	FText MessageFormat = LOCTEXT("DefaultPinIncompatibilityMessage", "{PinAType} is not compatible with {PinBType}.");

	if (bIsFatalOut != nullptr)
	{
		// the incompatible pins should generate an error by default
		*bIsFatalOut = true;
	}

	if (OutputType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (InputType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			MessageFormat = LOCTEXT("StructsIncompatible", "Only exactly matching structures are considered compatible.");

			const UStruct* OutStruct = Cast<const UStruct>(OutputType.PinSubCategoryObject.Get());
			const UStruct* InStruct  = Cast<const UStruct>(InputType.PinSubCategoryObject.Get());
			if ((OutStruct != nullptr) && (InStruct != nullptr) && OutStruct->IsChildOf(InStruct))
			{
				MessageFormat = LOCTEXT("ChildStructIncompatible", "Only exactly matching structures are considered compatible. Derived structures are disallowed.");
			}
		}
	}
	else if (OutputType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		if ((InputType.PinCategory == UEdGraphSchema_K2::PC_Object) || 
			(InputType.PinCategory == UEdGraphSchema_K2::PC_Interface))
		{
			MessageArgs.Add(TEXT("OutputName"), OutputPin->GetDisplayName());
			MessageArgs.Add(TEXT("InputName"),  InputPin->GetDisplayName());
			MessageFormat = LOCTEXT("ClassObjectIncompatible", "'{PinAName}' and '{PinBName}' are incompatible ('{OutputName}' is an object type, and '{InputName}' is a reference to an object instance).");

			if ((InputType.PinCategory == UEdGraphSchema_K2::PC_Object) && (bIsFatalOut != nullptr))
			{
				// under the hood class is an object, so it's not fatal
				*bIsFatalOut = false;
			}
		}
	}
	else if ((OutputType.PinCategory == UEdGraphSchema_K2::PC_Object) )//|| (OutputType.PinCategory == UEdGraphSchema_K2::PC_Interface))
	{
		if (InputType.PinCategory == UEdGraphSchema_K2::PC_Class)
		{
			MessageArgs.Add(TEXT("OutputName"), OutputPin->GetDisplayName());
			MessageArgs.Add(TEXT("InputName"),  InputPin->GetDisplayName());
			MessageArgs.Add(TEXT("InputType"),  UEdGraphSchema_K2::TypeToText(InputType));

			MessageFormat = LOCTEXT("CannotGetClass", "'{PinAName}' and '{PinBName}' are not inherently compatible ('{InputName}' is an object type, and '{OutputName}' is a reference to an object instance).\nWe cannot use {OutputName}'s class because it is not a child of {InputType}.");
		}
		else if (InputType.PinCategory == UEdGraphSchema_K2::PC_Object)
		{
			if (bIsFatalOut != nullptr)
			{
				*bIsFatalOut = true;
			}
		}
	}

	return FText::Format(MessageFormat, MessageArgs);
}

const FPinConnectionResponse UEdGraphSchema_K2::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	const UK2Node* OwningNodeA = Cast<UK2Node>(PinA->GetOwningNodeUnchecked());
	const UK2Node* OwningNodeB = Cast<UK2Node>(PinB->GetOwningNodeUnchecked());

	if (!OwningNodeA || !OwningNodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Invalid nodes"));
	}

	// Make sure the pins are not on the same node
	if (OwningNodeA == OwningNodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are on the same node"));
	}

	if (PinA->bOrphanedPin || PinB->bOrphanedPin)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Cannot make new connections to orphaned pin"));
	}

	FString NodeResponseMessage;
	// node can disallow the connection
	{
		if(OwningNodeA && OwningNodeA->IsConnectionDisallowed(PinA, PinB, NodeResponseMessage))
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NodeResponseMessage);
		}
		if(OwningNodeB && OwningNodeB->IsConnectionDisallowed(PinB, PinA, NodeResponseMessage))
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NodeResponseMessage);
		}
	}

	// Compare the directions
	const UEdGraphPin* InputPin = NULL;
	const UEdGraphPin* OutputPin = NULL;

	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Directions are not compatible"));
	}

	bool bIgnoreArray = false;
	if(const UK2Node* OwningNode = Cast<UK2Node>(InputPin->GetOwningNode()))
	{
		const bool bAllowMultipleSelfs = OwningNode->AllowMultipleSelfs(true); // it applies also to ForEachCall
		const bool bNotAContainer = !InputPin->PinType.IsContainer();
		const bool bSelfPin = IsSelfPin(*InputPin);
		bIgnoreArray = bAllowMultipleSelfs && bNotAContainer && bSelfPin;
	}

	// Find the calling context in case one of the pins is of type object and has a value of Self
	UClass* CallingContext = NULL;
	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(PinA->GetOwningNodeUnchecked());
	if (Blueprint)
	{
		CallingContext = (Blueprint->GeneratedClass != NULL) ? Blueprint->GeneratedClass : Blueprint->ParentClass;
	}

	// Compare the types
	const bool bTypesMatch = ArePinsCompatible(OutputPin, InputPin, CallingContext, bIgnoreArray);

	if (bTypesMatch)
	{
		FPinConnectionResponse ConnectionResponse = DetermineConnectionResponseOfCompatibleTypedPins(PinA, PinB, InputPin, OutputPin);
		if (ConnectionResponse.Message.IsEmpty())
		{
			ConnectionResponse.Message = FText::FromString(NodeResponseMessage);
		}
		else if (!NodeResponseMessage.IsEmpty())
		{
			ConnectionResponse.Message = FText::Format(LOCTEXT("MultiMsgConnectionResponse", "{0} - {1}"), ConnectionResponse.Message, FText::FromString(NodeResponseMessage));
		}
		return ConnectionResponse;
	}
	else
	{
		// Autocasting
		FName DummyName;
		UClass* DummyClass;
		UK2Node* DummyNode;

		const bool bCanAutocast = SearchForAutocastFunction(OutputPin, InputPin, /*out*/ DummyName, DummyClass);
		const bool bCanAutoConvert = FindSpecializedConversionNode(OutputPin, InputPin, false, /* out */ DummyNode);

		if (bCanAutocast || bCanAutoConvert)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, FString::Printf(TEXT("Convert %s to %s"), *TypeToText(OutputPin->PinType).ToString(), *TypeToText(InputPin->PinType).ToString()));
		}
		else
		{
			bool bIsFatal = true;
			FText IncompatibilityReasonText = GetPinIncompatibilityReason(PinA, PinB, &bIsFatal);

			FPinConnectionResponse ConnectionResponse(CONNECT_RESPONSE_DISALLOW, IncompatibilityReasonText.ToString());
			if (bIsFatal)
			{
				ConnectionResponse.SetFatal();
			}
			return ConnectionResponse;
		}
	}
}

bool UEdGraphSchema_K2::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(PinA->GetOwningNode());

	bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	if (bModified && !PinA->IsPendingKill())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	return bModified;
}

struct FAutocastFunctionMap : private FNoncopyable
{
private:
	static FAutocastFunctionMap* AutocastFunctionMap;

	TMap<FString, TWeakObjectPtr<UFunction>> InnerMap;
	FDelegateHandle OnHotReloadDelegateHandle;
	FDelegateHandle OnModulesChangedDelegateHandle;

	static FString GenerateTypeData(const FEdGraphPinType& PinType)
	{
		UObject* Obj = PinType.PinSubCategoryObject.Get();
		FString PinSubCategory = PinType.PinSubCategory;
		if (PinSubCategory.StartsWith(UEdGraphSchema_K2::PSC_Bitmask))
		{
			// Exclude the bitmask subcategory string from integral types so that autocast will work.
			PinSubCategory = TEXT("");
		}
		return FString::Printf(TEXT("%s;%s;%s"), *PinType.PinCategory, *PinSubCategory, Obj ? *Obj->GetPathName() : TEXT(""));
	}

	static FString GenerateCastData(const FEdGraphPinType& InputPinType, const FEdGraphPinType& OutputPinType)
	{
		return FString::Printf(TEXT("%s;%s"), *GenerateTypeData(InputPinType), *GenerateTypeData(OutputPinType));
	}

	static bool IsInputParam(uint64 PropertyFlags)
	{
		const uint64 ConstOutParamFlag = CPF_OutParm | CPF_ConstParm;
		const uint64 IsConstOut = PropertyFlags & ConstOutParamFlag;
		return (CPF_Parm == (PropertyFlags & (CPF_Parm | CPF_ReturnParm)))
			&& ((0 == IsConstOut) || (ConstOutParamFlag == IsConstOut));
	}

	static const UProperty* GetFirstInputProperty(const UFunction* Function)
	{
		for (const UProperty* Property : TFieldRange<const UProperty>(Function))
		{
			if (Property && IsInputParam(Property->PropertyFlags))
			{
				return Property;
			}
		}
		return nullptr;
	}

	void InsertFunction(UFunction* Function, const UEdGraphSchema_K2* Schema)
	{
		FEdGraphPinType InputPinType;
		Schema->ConvertPropertyToPinType(GetFirstInputProperty(Function), InputPinType);

		FEdGraphPinType OutputPinType;
		Schema->ConvertPropertyToPinType(Function->GetReturnProperty(), OutputPinType);

		InnerMap.Add(GenerateCastData(InputPinType, OutputPinType), Function);
	}
public:

	static bool IsAutocastFunction(const UFunction* Function)
	{
		const FName BlueprintAutocast(TEXT("BlueprintAutocast"));
		return Function
			&& Function->HasMetaData(BlueprintAutocast)
			&& Function->HasAllFunctionFlags(FUNC_Static | FUNC_Native | FUNC_Public | FUNC_BlueprintPure)
			&& Function->GetReturnProperty()
			&& GetFirstInputProperty(Function);
	}

	void Refresh()
	{
#ifdef SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME
		static_assert(false, "Macro redefinition.");
#endif
#define SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME 0
#if SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME
		const double StartTime = FPlatformTime::Seconds();
#endif //SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME

		InnerMap.Empty();
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		TArray<UClass*> Libraries;
		GetDerivedClasses(UBlueprintFunctionLibrary::StaticClass(), Libraries);
		for (UClass* Library : Libraries)
		{
			if (Library && (CLASS_Native == (Library->ClassFlags & (CLASS_Native | CLASS_Deprecated | CLASS_NewerVersionExists))))
			{
				for (UFunction* Function : TFieldRange<UFunction>(Library, EFieldIteratorFlags::ExcludeSuper, EFieldIteratorFlags::ExcludeDeprecated))
				{
					if (IsAutocastFunction(Function))
					{
						InsertFunction(Function, Schema);
					}
				}
			}
		}

#if SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME
		const double EndTime = FPlatformTime::Seconds();
		UE_LOG(LogBlueprint, Warning, TEXT("FAutocastFunctionMap::Refresh took %fs"), EndTime - StartTime);
#endif //SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME
#undef SCHEMA_K2_AUTOCASTFUNCTIONMAP_LOG_TIME
	}

	UFunction* Find(const FEdGraphPinType& InputPinType, const FEdGraphPinType& OutputPinType) const
	{
		const TWeakObjectPtr<UFunction>* FuncPtr = InnerMap.Find(GenerateCastData(InputPinType, OutputPinType));
		return FuncPtr ? FuncPtr->Get() : nullptr;
	}

	static FAutocastFunctionMap& Get()
	{
		if (AutocastFunctionMap == nullptr)
		{
			AutocastFunctionMap = new FAutocastFunctionMap();
		}
		return *AutocastFunctionMap;
	}

	static void OnProjectHotReloaded(bool bWasTriggeredAutomatically)
	{
		if (AutocastFunctionMap)
		{
			AutocastFunctionMap->Refresh();
		}
	}

	static void OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange)
	{
		if (AutocastFunctionMap)
		{
			AutocastFunctionMap->Refresh();
		}
	}

	FAutocastFunctionMap()
	{
		Refresh();

		IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
		OnHotReloadDelegateHandle = HotReloadSupport.OnHotReload().AddStatic(&FAutocastFunctionMap::OnProjectHotReloaded);

		OnModulesChangedDelegateHandle = FModuleManager::Get().OnModulesChanged().AddStatic(&OnModulesChanged);
	}

	~FAutocastFunctionMap()
	{
		if (IHotReloadInterface* HotReloadSupport = FModuleManager::GetModulePtr<IHotReloadInterface>("HotReload"))
		{
			HotReloadSupport->OnHotReload().Remove(OnHotReloadDelegateHandle);
		}

		FModuleManager::Get().OnModulesChanged().Remove(OnModulesChangedDelegateHandle); 
	}
};

FAutocastFunctionMap* FAutocastFunctionMap::AutocastFunctionMap = nullptr;

bool UEdGraphSchema_K2::SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, /*out*/ FName& TargetFunction, /*out*/ UClass*& FunctionOwner) const
{
	// NOTE: Under no circumstances should anyone *ever* add a questionable cast to this function.
	// If it could be at all confusing why a function is provided, to even a novice user, err on the side of do not cast!!!
	// This includes things like string->int (does it do length, atoi, or what?) that would be autocasts in a traditional scripting language

	TargetFunction = NAME_None;
	FunctionOwner = nullptr;

	if (OutputPin->PinType.ContainerType != InputPin->PinType.ContainerType)
	{
		if (OutputPin->PinType.IsSet() && InputPin->PinType.IsArray())
		{
			UFunction* Function = UBlueprintSetLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UBlueprintSetLibrary, Set_ToArray));
			TargetFunction = Function->GetFName();
			FunctionOwner = Function->GetOwnerClass();
			return true;
		}
		return false;
	}

	// SPECIAL CASES, not supported by FAutocastFunctionMap
	if ((OutputPin->PinType.PinCategory == PC_Interface) && (InputPin->PinType.PinCategory == PC_Object))
	{
		UClass const* InputClass = Cast<UClass const>(InputPin->PinType.PinSubCategoryObject.Get());

		bool const bInputIsUObject = (InputClass && (InputClass == UObject::StaticClass()));
		if (bInputIsUObject)
		{
			UFunction* Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetSystemLibrary, Conv_InterfaceToObject));
			TargetFunction = Function->GetFName();
			FunctionOwner = Function->GetOwnerClass();
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Object)
	{
		UClass const* OutputClass = Cast<UClass const>(OutputPin->PinType.PinSubCategoryObject.Get());
		if (InputPin->PinType.PinCategory == PC_Class)
		{
			UClass const* InputClass = Cast<UClass const>(InputPin->PinType.PinSubCategoryObject.Get());
			if ((OutputClass != nullptr) &&
				(InputClass != nullptr) &&
				OutputClass->IsChildOf(InputClass))
			{
				UFunction* Function = UGameplayStatics::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UGameplayStatics, GetObjectClass));
				TargetFunction = Function->GetFName();
				FunctionOwner = Function->GetOwnerClass();
			}
		}
		else if (InputPin->PinType.PinCategory == PC_String)
		{
			UFunction* Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetSystemLibrary, GetDisplayName));
			TargetFunction = Function->GetFName();
			FunctionOwner = Function->GetOwnerClass();
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Struct)
	{
		const UScriptStruct* OutputStructType = Cast<const UScriptStruct>(OutputPin->PinType.PinSubCategoryObject.Get());
		if (OutputStructType == TBaseStructure<FRotator>::Get())
		{
			const UScriptStruct* InputStructType = Cast<const UScriptStruct>(InputPin->PinType.PinSubCategoryObject.Get());
			if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == TBaseStructure<FTransform>::Get()))
			{
				UFunction* Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetMathLibrary, MakeTransform));
				TargetFunction = Function->GetFName();
				FunctionOwner = Function->GetOwnerClass();
			}
		}
	}

	if (TargetFunction == NAME_None)
	{
		const FAutocastFunctionMap& AutocastFunctionMap = FAutocastFunctionMap::Get();
		if (UFunction* Func = AutocastFunctionMap.Find(OutputPin->PinType, InputPin->PinType))
		{
			TargetFunction = Func->GetFName();
			FunctionOwner = Func->GetOwnerClass();
		}
	}

	return TargetFunction != NAME_None;
}

bool UEdGraphSchema_K2::FindSpecializedConversionNode(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, bool bCreateNode, /*out*/ UK2Node*& TargetNode) const
{
	bool bCanConvert = false;
	TargetNode = nullptr;

	// Conversion for scalar -> array
	if( (!OutputPin->PinType.IsContainer() && InputPin->PinType.IsArray()) && ArePinTypesCompatible(OutputPin->PinType, InputPin->PinType, nullptr, true))
	{
		bCanConvert = true;
		if(bCreateNode)
		{
			TargetNode = NewObject<UK2Node_MakeArray>();
		}
	}
	// If connecting an object to a 'call function' self pin, and not currently compatible, see if there is a property we can call a function on
	else if (InputPin->GetOwningNode()->IsA(UK2Node_CallFunction::StaticClass()) && IsSelfPin(*InputPin) && 
		((OutputPin->PinType.PinCategory == PC_Object) || (OutputPin->PinType.PinCategory == PC_Interface)))
	{
		UK2Node_CallFunction* CallFunctionNode = (UK2Node_CallFunction*)(InputPin->GetOwningNode());
		UClass* OutputPinClass = Cast<UClass>(OutputPin->PinType.PinSubCategoryObject.Get());

		UClass* FunctionClass = CallFunctionNode->FunctionReference.GetMemberParentClass(CallFunctionNode->GetBlueprintClassFromNode());
		if(FunctionClass != NULL && OutputPinClass != NULL)
		{
			// Iterate over object properties..
			for (TFieldIterator<UObjectProperty> PropIt(OutputPinClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				UObjectProperty* ObjProp = *PropIt;
				// .. if we have a blueprint visible var, and is of the type which contains this function..
				if(ObjProp->HasAllPropertyFlags(CPF_BlueprintVisible) && ObjProp->PropertyClass->IsChildOf(FunctionClass))
				{
					// say we can convert
					bCanConvert = true;
					// Create 'get variable' node
					if(bCreateNode)
					{
						UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>();
						GetNode->VariableReference.SetFromField<UProperty>(ObjProp, false);
						TargetNode = GetNode;
					}
				}
			}

		}	
	}

	if(!bCanConvert)
	{
		// CHECK ENUM TO NAME CAST
		const bool bInoputMatch = InputPin && !InputPin->PinType.IsContainer() && ((PC_Name == InputPin->PinType.PinCategory) || (PC_String == InputPin->PinType.PinCategory));
		const bool bOutputMatch = OutputPin && !OutputPin->PinType.IsContainer() && (PC_Byte == OutputPin->PinType.PinCategory) && (NULL != Cast<UEnum>(OutputPin->PinType.PinSubCategoryObject.Get()));
		if(bOutputMatch && bInoputMatch)
		{
			bCanConvert = true;
			if(bCreateNode)
			{
				check(NULL == TargetNode);
				if(PC_Name == InputPin->PinType.PinCategory)
				{
					TargetNode = NewObject<UK2Node_GetEnumeratorName>();
				}
				else if(PC_String == InputPin->PinType.PinCategory)
				{
					TargetNode = NewObject<UK2Node_GetEnumeratorNameAsString>();
				}
			}
		}
	}

	if (!bCanConvert && InputPin && OutputPin)
	{
		FEdGraphPinType const& InputType  = InputPin->PinType;
		FEdGraphPinType const& OutputType = OutputPin->PinType;

		// CHECK BYTE TO ENUM CAST
		UEnum* Enum = Cast<UEnum>(InputType.PinSubCategoryObject.Get());
		const bool bInputIsEnum = !InputType.IsContainer() && (PC_Byte == InputType.PinCategory) && Enum;
		const bool bOutputIsByte = !OutputType.IsContainer() && (PC_Byte == OutputType.PinCategory);
		if (bInputIsEnum && bOutputIsByte)
		{
			bCanConvert = true;
			if(bCreateNode)
			{
				UK2Node_CastByteToEnum* CastByteToEnum = NewObject<UK2Node_CastByteToEnum>();
				CastByteToEnum->Enum = Enum;
				CastByteToEnum->bSafe = true;
				TargetNode = CastByteToEnum;
			}
		}
		else
		{
			UClass* InputClass  = Cast<UClass>(InputType.PinSubCategoryObject.Get());
			UClass* OutputClass = Cast<UClass>(OutputType.PinSubCategoryObject.Get());

			if ((OutputType.PinCategory == PC_Interface) && (InputType.PinCategory == PC_Object))
			{
				bCanConvert = (InputClass && OutputClass) && (InputClass->ImplementsInterface(OutputClass) || OutputClass->IsChildOf(InputClass));
			}
			else if (OutputType.PinCategory == PC_Object)
			{
				UBlueprintEditorSettings const* BlueprintSettings = GetDefault<UBlueprintEditorSettings>();
				if ((InputType.PinCategory == PC_Object) && BlueprintSettings->bAutoCastObjectConnections)
				{
					bCanConvert = (InputClass && OutputClass) && InputClass->IsChildOf(OutputClass);
				}
			}

			if (bCanConvert && bCreateNode)
			{
				UK2Node_DynamicCast* DynCastNode = NewObject<UK2Node_DynamicCast>();
				DynCastNode->TargetType = InputClass;
				DynCastNode->SetPurity(true);
				TargetNode = DynCastNode;
			}

			if (!bCanConvert && InputClass && OutputClass && OutputClass->IsChildOf(InputClass))
			{
				const bool bConvertAsset = (OutputType.PinCategory == PC_SoftObject) && (InputType.PinCategory == PC_Object);
				const bool bConvertAssetClass = (OutputType.PinCategory == PC_SoftClass) && (InputType.PinCategory == PC_Class);
				const bool bConvertToAsset = (OutputType.PinCategory == PC_Object) && (InputType.PinCategory == PC_SoftObject);
				const bool bConvertToAssetClass = (OutputType.PinCategory == PC_Class) && (InputType.PinCategory == PC_SoftClass);

				if (bConvertAsset || bConvertAssetClass || bConvertToAsset || bConvertToAssetClass)
				{
					bCanConvert = true;
					if (bCreateNode)
					{
						UK2Node_ConvertAsset* ConvertAssetNode = NewObject<UK2Node_ConvertAsset>();
						TargetNode = ConvertAssetNode;
					}
				}
			}
		}
	}

	return bCanConvert;
}

void UEdGraphSchema_K2::AutowireConversionNode(UEdGraphPin* InputPin, UEdGraphPin* OutputPin, UEdGraphNode* ConversionNode) const
{
	bool bAllowInputConnections = true;
	bool bAllowOutputConnections = true;

	for (int32 PinIndex = 0; PinIndex < ConversionNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* TestPin = ConversionNode->Pins[PinIndex];

		UClass* Context = nullptr;
		UK2Node* K2Node = Cast<UK2Node>(OutputPin->GetOwningNode());
		if (K2Node != nullptr)
		{
			UBlueprint* Blueprint = K2Node->GetBlueprint();
			if (Blueprint)
			{
				Context = Blueprint->GeneratedClass;
			}
		}

		if ((TestPin->Direction == EGPD_Input) && (ArePinTypesCompatible(OutputPin->PinType, TestPin->PinType, Context)))
		{
			if(bAllowOutputConnections && TryCreateConnection(TestPin, OutputPin))
			{
				// Successful connection, do not allow more output connections
				bAllowOutputConnections = false;
			}
		}
		else if ((TestPin->Direction == EGPD_Output) && (ArePinTypesCompatible(TestPin->PinType, InputPin->PinType, Context)))
		{
			if(bAllowInputConnections && TryCreateConnection(TestPin, InputPin))
			{
				// Successful connection, do not allow more input connections
				bAllowInputConnections = false;
			}
		}
	}
}

bool UEdGraphSchema_K2::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	// Determine which pin is an input and which pin is an output
	UEdGraphPin* InputPin = NULL;
	UEdGraphPin* OutputPin = NULL;
	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return false;
	}

	FName TargetFunctionName;
	UClass* ClassContainingConversionFunction = nullptr;
	TSubclassOf<UK2Node> ConversionNodeClass;

	UK2Node* TemplateConversionNode = NULL;

	if (SearchForAutocastFunction(OutputPin, InputPin, /*out*/ TargetFunctionName, /*out*/ClassContainingConversionFunction))
	{
		// Create a new call function node for the casting operator
		UK2Node_CallFunction* TemplateNode = NewObject<UK2Node_CallFunction>();
		TemplateNode->FunctionReference.SetExternalMember(TargetFunctionName, ClassContainingConversionFunction);
		//TemplateNode->bIsBeadFunction = true;

		TemplateConversionNode = TemplateNode;
	}
	else
	{
		FindSpecializedConversionNode(OutputPin, InputPin, true, /*out*/ TemplateConversionNode);
	}

	if (TemplateConversionNode != NULL)
	{
		// Determine where to position the new node (assuming it isn't going to get beaded)
		FVector2D AverageLocation = CalculateAveragePositionBetweenNodes(InputPin, OutputPin);

		UK2Node* ConversionNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node>(InputPin->GetOwningNode()->GetGraph(), TemplateConversionNode, AverageLocation);

		// Connect the cast node up to the output/input pins
		AutowireConversionNode(InputPin, OutputPin, ConversionNode);

		return true;
	}

	return false;
}

FString UEdGraphSchema_K2::IsPinDefaultValid(const UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText) const
{
	check(Pin);

	FFormatNamedArguments MessageArgs;
	MessageArgs.Add(TEXT("PinName"), Pin->GetDisplayName());

	const UBlueprint* OwningBP = FBlueprintEditorUtils::FindBlueprintForNode(Pin->GetOwningNodeUnchecked());
	if (!OwningBP)
	{
		FText MsgFormat = LOCTEXT("NoBlueprintFoundForPin", "No Blueprint was found for the pin '{PinName}'.");
		return FText::Format(MsgFormat, MessageArgs).ToString();
	}

	const bool bIsArray = Pin->PinType.IsArray();
	const bool bIsSet = Pin->PinType.IsSet();
	const bool bIsMap = Pin->PinType.IsMap();
	const bool bIsReference = Pin->PinType.bIsReference;
	const bool bIsAutoCreateRefTerm = IsAutoCreateRefTerm(Pin);

	if (OwningBP->BlueprintType != BPTYPE_Interface)
	{
		if( !bIsAutoCreateRefTerm )
		{
			// No harm in leaving a function result node input (aka functin output) unconnected - the property will be initialized correctly
			// as empty:
			bool bIsFunctionOutput = false;
			if(UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Pin->GetOwningNode()))
			{
				if(ensure(Pin->Direction == EEdGraphPinDirection::EGPD_Input))
				{
					bIsFunctionOutput = true;
				}
			}

			if(!bIsFunctionOutput)
			{
				if( bIsArray )
				{
					FText MsgFormat = LOCTEXT("BadArrayDefaultVal", "Array inputs (like '{PinName}') must have an input wired into them (try connecting a MakeArray node).");
					return FText::Format(MsgFormat, MessageArgs).ToString();
				}
				else if( bIsSet )
				{
					FText MsgFormat = LOCTEXT("BadSetDefaultVal", "Set inputs (like '{PinName}') must have an input wired into them (try connecting a MakeSet node).");
					return FText::Format(MsgFormat, MessageArgs).ToString();
				}
				else if ( bIsMap )
				{
					FText MsgFormat = LOCTEXT("BadMapDefaultVal", "Map inputs (like '{PinName}') must have an input wired into them (try connecting a MakeMap node).");
					return FText::Format(MsgFormat, MessageArgs).ToString();
				}
				else if( bIsReference )
				{
					FText MsgFormat = LOCTEXT("BadRefDefaultVal", "'{PinName}' must have an input wired into it (\"by ref\" params expect a valid input to operate on).");
					return FText::Format(MsgFormat, MessageArgs).ToString();
				}
			}
		}
	}

	FString ReturnMsg;
	DefaultValueSimpleValidation(Pin->PinType, Pin->PinName, NewDefaultValue, NewDefaultObject, InNewDefaultText, &ReturnMsg);
	return ReturnMsg;
}

bool UEdGraphSchema_K2::DoesSupportPinWatching() const
{
	return true;
}

bool UEdGraphSchema_K2::IsPinBeingWatched(UEdGraphPin const* Pin) const
{
	// Note: If you crash here; it is likely that you forgot to call Blueprint->OnBlueprintChanged.Broadcast(Blueprint) to invalidate the cached UI state
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Pin ? Pin->GetOwningNodeUnchecked() : nullptr);
	return (Blueprint ? FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Pin) : false);
}

void UEdGraphSchema_K2::ClearPinWatch(UEdGraphPin const* Pin) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Pin->GetOwningNode());
	FKismetDebugUtilities::RemovePinWatch(Blueprint, Pin);
}

FLinearColor UEdGraphSchema_K2::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	const FString& TypeString = PinType.PinCategory;
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();

	if (TypeString == PC_Exec)
	{
		return Settings->ExecutionPinTypeColor;
	}
	else if (TypeString == PC_Object)
	{
		return Settings->ObjectPinTypeColor;
	}
	else if (TypeString == PC_Interface)
	{
		return Settings->InterfacePinTypeColor;
	}
	else if (TypeString == PC_Float)
	{
		return Settings->FloatPinTypeColor;
	}
	else if (TypeString == PC_Boolean)
	{
		return Settings->BooleanPinTypeColor;
	}
	else if (TypeString == PC_Byte)
	{
		return Settings->BytePinTypeColor;
	}
	else if (TypeString == PC_Int)
	{
		return Settings->IntPinTypeColor;
	}
	else if (TypeString == PC_Struct)
	{
		if (PinType.PinSubCategoryObject == VectorStruct)
		{
			// vector
			return Settings->VectorPinTypeColor;
		}
		else if (PinType.PinSubCategoryObject == RotatorStruct)
		{
			// rotator
			return Settings->RotatorPinTypeColor;
		}
		else if (PinType.PinSubCategoryObject == TransformStruct)
		{
			// transform
			return Settings->TransformPinTypeColor;
		}
		else
		{
			return Settings->StructPinTypeColor;
		}
	}
	else if (TypeString == PC_String)
	{
		return Settings->StringPinTypeColor;
	}
	else if (TypeString == PC_Text)
	{
		return Settings->TextPinTypeColor;
	}
	else if (TypeString == PC_Wildcard)
	{
		if (PinType.PinSubCategory == PSC_Index)
		{
			return Settings->IndexPinTypeColor;
		}
		else
		{
			return Settings->WildcardPinTypeColor;
		}
	}
	else if (TypeString == PC_Name)
	{
		return Settings->NamePinTypeColor;
	}
	else if (TypeString == PC_SoftObject)
	{
		return Settings->SoftObjectPinTypeColor;
	}
	else if (TypeString == PC_SoftClass)
	{
		return Settings->SoftClassPinTypeColor;
	}
	else if (TypeString == PC_Delegate)
	{
		return Settings->DelegatePinTypeColor;
	}
	else if (TypeString == PC_Class)
	{
		return Settings->ClassPinTypeColor;
	}

	// Type does not have a defined color!
	return Settings->DefaultPinTypeColor;
}

FLinearColor UEdGraphSchema_K2::GetSecondaryPinTypeColor(const FEdGraphPinType& PinType) const
{
	if (PinType.IsMap())
	{
		FEdGraphPinType FakePrimary = PinType;
		FakePrimary.PinCategory = FakePrimary.PinValueType.TerminalCategory;
		FakePrimary.PinSubCategory = FakePrimary.PinValueType.TerminalSubCategory;
		FakePrimary.PinSubCategoryObject = FakePrimary.PinValueType.TerminalSubCategoryObject;

		return GetPinTypeColor(FakePrimary);
	}
	else
	{
		const FString& TypeString = PinType.PinCategory;
		const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
		return Settings->WildcardPinTypeColor;
	}
}

FText UEdGraphSchema_K2::GetPinDisplayName(const UEdGraphPin* Pin) const 
{
	FText DisplayName = FText::GetEmpty();

	if (Pin != NULL)
	{
		UEdGraphNode* Node = Pin->GetOwningNode();
		if (Node->ShouldOverridePinNames())
		{
			DisplayName = Node->GetPinNameOverride(*Pin);
		}
		else
		{
			DisplayName = Super::GetPinDisplayName(Pin);
	
			// bit of a hack to hide 'execute' and 'then' pin names
			if ((Pin->PinType.PinCategory == PC_Exec) && 
				((DisplayName.ToString() == PN_Execute) || (DisplayName.ToString() == PN_Then)))
			{
				DisplayName = FText::GetEmpty();
			}
		}

		if( GEditor && GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
		{
			DisplayName = FText::FromString(FName::NameToDisplayString(DisplayName.ToString(), Pin->PinType.PinCategory == PC_Boolean));
		}
	}
	return DisplayName;
}

void UEdGraphSchema_K2::ConstructBasicPinTooltip(const UEdGraphPin& Pin, const FText& PinDescription, FString& TooltipOut) const
{
	if (Pin.bWasTrashed)
	{
		return;
	}

	if (bGeneratingDocumentation)
	{
		TooltipOut = PinDescription.ToString();
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinType"), TypeToText(Pin.PinType));

		if (UEdGraphNode* PinNode = Pin.GetOwningNode())
		{
			UEdGraphSchema_K2 const* const K2Schema = Cast<const UEdGraphSchema_K2>(PinNode->GetSchema());
			if (ensure(K2Schema != NULL)) // ensure that this node belongs to this schema
			{
				Args.Add(TEXT("DisplayName"), GetPinDisplayName(&Pin));
				Args.Add(TEXT("LineFeed1"), FText::FromString(TEXT("\n")));
			}
		}
		else
		{
				Args.Add(TEXT("DisplayName"), FText::GetEmpty());
				Args.Add(TEXT("LineFeed1"), FText::GetEmpty());
		}


		if (!PinDescription.IsEmpty())
		{
			Args.Add(TEXT("Description"), PinDescription);
			Args.Add(TEXT("LineFeed2"), FText::FromString(TEXT("\n\n")));
		}
		else
		{
			Args.Add(TEXT("Description"), FText::GetEmpty());
			Args.Add(TEXT("LineFeed2"), FText::GetEmpty());
		}
	
		TooltipOut = FText::Format(LOCTEXT("PinTooltip", "{DisplayName}{LineFeed1}{PinType}{LineFeed2}{Description}"), Args).ToString(); 
	}
}

EGraphType UEdGraphSchema_K2::GetGraphType(const UEdGraph* TestEdGraph) const
{
	if (TestEdGraph)
	{
		//@TODO: Should there be a GT_Subgraph type?	
		UEdGraph* GraphToTest = const_cast<UEdGraph*>(TestEdGraph);

		for (UObject* TestOuter = GraphToTest; TestOuter; TestOuter = TestOuter->GetOuter())
		{
			// reached up to the blueprint for the graph
			if (UBlueprint* Blueprint = Cast<UBlueprint>(TestOuter))
			{
				if (Blueprint->BlueprintType == BPTYPE_MacroLibrary ||
					Blueprint->MacroGraphs.Contains(GraphToTest))
				{
					return GT_Macro;
				}
				else if (Blueprint->UbergraphPages.Contains(GraphToTest))
				{
					return GT_Ubergraph;
				}
				else if (Blueprint->FunctionGraphs.Contains(GraphToTest))
				{
					return GT_Function; 
				}
			}
			else
			{
				GraphToTest = Cast<UEdGraph>(TestOuter);
			}
		}
	}
	
	return Super::GetGraphType(TestEdGraph);
}

bool UEdGraphSchema_K2::IsTitleBarPin(const UEdGraphPin& Pin) const
{
	return IsExecPin(Pin);
}

void UEdGraphSchema_K2::CreateMacroGraphTerminators(UEdGraph& Graph, UClass* Class) const
{
	const FName GraphName = Graph.GetFName();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(&Graph);
	
	// Create the entry/exit tunnels
	{
		FGraphNodeCreator<UK2Node_Tunnel> EntryNodeCreator(Graph);
		UK2Node_Tunnel* EntryNode = EntryNodeCreator.CreateNode();
		EntryNode->bCanHaveOutputs = true;
		EntryNodeCreator.Finalize();
		SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);
	}

	{
		FGraphNodeCreator<UK2Node_Tunnel> ExitNodeCreator(Graph);
		UK2Node_Tunnel* ExitNode = ExitNodeCreator.CreateNode();
		ExitNode->bCanHaveInputs = true;
		ExitNode->NodePosX = 240;
		ExitNodeCreator.Finalize();
		SetNodeMetaData(ExitNode, FNodeMetadata::DefaultGraphNode);
	}
}

void UEdGraphSchema_K2::LinkDataPinFromOutputToInput(UEdGraphNode* InOutputNode, UEdGraphNode* InInputNode) const
{
	for (UEdGraphPin* OutputPin : InOutputNode->Pins)
	{
		if ((OutputPin->Direction == EGPD_Output) && (!IsExecPin(*OutputPin)))
		{
			UEdGraphPin* const InputPin = InInputNode->FindPinChecked(OutputPin->PinName);
			OutputPin->MakeLinkTo(InputPin);
		}
	}
}

void UEdGraphSchema_K2::CreateFunctionGraphTerminators(UEdGraph& Graph, UClass* Class) const
{
	const FName GraphName = Graph.GetFName();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(&Graph);
	check(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

	// Create a function entry node
	FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(Graph);
	UK2Node_FunctionEntry* EntryNode = FunctionEntryCreator.CreateNode();
	EntryNode->SignatureClass = Class;
	EntryNode->SignatureName = GraphName;
	FunctionEntryCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	// See if we need to implement a return node
	UFunction* InterfaceToImplement = FindField<UFunction>(Class, GraphName);
	if (InterfaceToImplement)
	{
		// Add modifier flags from the declaration
		EntryNode->AddExtraFlags(InterfaceToImplement->FunctionFlags & (FUNC_Const | FUNC_Static | FUNC_BlueprintPure));

		UK2Node* NextNode = EntryNode;
		UEdGraphPin* NextExec = FindExecutionPin(*EntryNode, EGPD_Output);
		bool bHasParentNode = false;
		// Create node for call parent function
		if (((Class->GetClassFlags() & CLASS_Interface) == 0)  &&
			(InterfaceToImplement->FunctionFlags & FUNC_BlueprintCallable))
		{
			FGraphNodeCreator<UK2Node_CallParentFunction> FunctionParentCreator(Graph);
			UK2Node_CallParentFunction* ParentNode = FunctionParentCreator.CreateNode();
			ParentNode->SetFromFunction(InterfaceToImplement);
			ParentNode->NodePosX = EntryNode->NodePosX + EntryNode->NodeWidth + 256;
			ParentNode->NodePosY = EntryNode->NodePosY;
			FunctionParentCreator.Finalize();

			UEdGraphPin* ParentNodeExec = FindExecutionPin(*ParentNode, EGPD_Input); 

			// If the parent node has an execution pin, then we should as well (we're overriding them, after all)
			// but perhaps this assumption is not valid in the case where a function becomes pure after being
			// initially declared impure - for that reason I'm checking for validity on both ParentNodeExec and NextExec
			if (ParentNodeExec && NextExec)
			{
				NextExec->MakeLinkTo(ParentNodeExec);
				NextExec = FindExecutionPin(*ParentNode, EGPD_Output);
			}

			NextNode = ParentNode;
			bHasParentNode = true;
		}

		// See if any function params are marked as out
		bool bHasOutParam =  false;
		for( TFieldIterator<UProperty> It(InterfaceToImplement); It && (It->PropertyFlags & CPF_Parm); ++It )
		{
			if( It->PropertyFlags & CPF_OutParm )
			{
				bHasOutParam = true;
				break;
			}
		}

		if( bHasOutParam )
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(Graph);
			UK2Node_FunctionResult* ReturnNode = NodeCreator.CreateNode();
			ReturnNode->SignatureClass = Class;
			ReturnNode->SignatureName = GraphName;
			ReturnNode->NodePosX = NextNode->NodePosX + NextNode->NodeWidth + 256;
			ReturnNode->NodePosY = EntryNode->NodePosY;
			NodeCreator.Finalize();
			SetNodeMetaData(ReturnNode, FNodeMetadata::DefaultGraphNode);

			// Auto-connect the pins for entry and exit, so that by default the signature is properly generated
			UEdGraphPin* ResultNodeExec = FindExecutionPin(*ReturnNode, EGPD_Input);
			if (ResultNodeExec && NextExec)
			{
				NextExec->MakeLinkTo(ResultNodeExec);
			}

			if (bHasParentNode)
			{
				LinkDataPinFromOutputToInput(NextNode, ReturnNode);
			}
		}
	}
}

void UEdGraphSchema_K2::CreateFunctionGraphTerminators(UEdGraph& Graph, UFunction* FunctionSignature) const
{
	const FName GraphName = Graph.GetFName();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(&Graph);
	check(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

	// Create a function entry node
	FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(Graph);
	UK2Node_FunctionEntry* EntryNode = FunctionEntryCreator.CreateNode();
	EntryNode->SignatureClass = NULL;
	EntryNode->SignatureName = GraphName;
	FunctionEntryCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	// We don't have a signature class to base this on permanently, because it's not an override function.
	// so we need to define the pins as user defined so that they are serialized.

	EntryNode->CreateUserDefinedPinsForFunctionEntryExit(FunctionSignature, /*bIsFunctionEntry=*/ true);

	// See if any function params are marked as out
	bool bHasOutParam =  false;
	for ( TFieldIterator<UProperty> It(FunctionSignature); It && ( It->PropertyFlags & CPF_Parm ); ++It )
	{
		if ( It->PropertyFlags & CPF_OutParm )
		{
			bHasOutParam = true;
			break;
		}
	}

	if ( bHasOutParam )
	{
		FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(Graph);
		UK2Node_FunctionResult* ReturnNode = NodeCreator.CreateNode();
		ReturnNode->SignatureClass = NULL;
		ReturnNode->SignatureName = GraphName;
		ReturnNode->NodePosX = EntryNode->NodePosX + EntryNode->NodeWidth + 256;
		ReturnNode->NodePosY = EntryNode->NodePosY;
		NodeCreator.Finalize();
		SetNodeMetaData(ReturnNode, FNodeMetadata::DefaultGraphNode);

		ReturnNode->CreateUserDefinedPinsForFunctionEntryExit(FunctionSignature, /*bIsFunctionEntry=*/ false);

		// Auto-connect the pins for entry and exit, so that by default the signature is properly generated
		UEdGraphPin* EntryNodeExec = FindExecutionPin(*EntryNode, EGPD_Output);
		UEdGraphPin* ResultNodeExec = FindExecutionPin(*ReturnNode, EGPD_Input);
		EntryNodeExec->MakeLinkTo(ResultNodeExec);
	}
}

bool UEdGraphSchema_K2::GetPropertyCategoryInfo(const UProperty* TestProperty, FString& OutCategory, FString& OutSubCategory, UObject*& OutSubCategoryObject, bool& bOutIsWeakPointer)
{
	if (const UInterfaceProperty* InterfaceProperty = Cast<const UInterfaceProperty>(TestProperty))
	{
		OutCategory = PC_Interface;
		OutSubCategoryObject = InterfaceProperty->InterfaceClass;
	}
	else if (const UClassProperty* ClassProperty = Cast<const UClassProperty>(TestProperty))
	{
		OutCategory = PC_Class;
		OutSubCategoryObject = ClassProperty->MetaClass;
	}
	else if (const USoftClassProperty* SoftClassProperty = Cast<const USoftClassProperty>(TestProperty))
	{
		OutCategory = PC_SoftClass;
		OutSubCategoryObject = SoftClassProperty->MetaClass;
	}
	else if (const USoftObjectProperty* SoftObjectProperty = Cast<const USoftObjectProperty>(TestProperty))
	{
		OutCategory = PC_SoftObject;
		OutSubCategoryObject = SoftObjectProperty->PropertyClass;
	}
	else if (const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(TestProperty))
	{
		OutCategory = PC_Object;
		OutSubCategoryObject = ObjectProperty->PropertyClass;
		bOutIsWeakPointer = TestProperty->IsA(UWeakObjectProperty::StaticClass());
	}
	else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(TestProperty))
	{
		OutCategory = PC_Struct;
		OutSubCategoryObject = StructProperty->Struct;
	}
	else if (TestProperty->IsA<UFloatProperty>())
	{
		OutCategory = PC_Float;
	}
	else if (TestProperty->IsA<UIntProperty>())
	{
		OutCategory = PC_Int;

		if (TestProperty->HasMetaData(FBlueprintMetadata::MD_Bitmask))
		{
			OutSubCategory = PSC_Bitmask;
		}
	}
	else if (const UByteProperty* ByteProperty = Cast<const UByteProperty>(TestProperty))
	{
		OutCategory = PC_Byte;

		if (TestProperty->HasMetaData(FBlueprintMetadata::MD_Bitmask))
		{
			OutSubCategory = PSC_Bitmask;
		}
		else
		{
			OutSubCategoryObject = ByteProperty->Enum;
		}
	}
	else if (const UEnumProperty* EnumProperty = Cast<const UEnumProperty>(TestProperty))
	{
		// K2 only supports byte enums right now - any violations should have been caught by UHT or the editor
		if (!EnumProperty->GetUnderlyingProperty()->IsA<UByteProperty>())
		{
			OutCategory = TEXT("unsupported_enum_type");
			return false;
		}

		OutCategory = PC_Byte;

		if (TestProperty->HasMetaData(FBlueprintMetadata::MD_Bitmask))
		{
			OutSubCategory = PSC_Bitmask;
		}
		else
		{
			OutSubCategoryObject = EnumProperty->GetEnum();
		}
	}
	else if (TestProperty->IsA<UNameProperty>())
	{
		OutCategory = PC_Name;
	}
	else if (TestProperty->IsA<UBoolProperty>())
	{
		OutCategory = PC_Boolean;
	}
	else if (TestProperty->IsA<UStrProperty>())
	{
		OutCategory = PC_String;
	}
	else if (TestProperty->IsA<UTextProperty>())
	{
		OutCategory = PC_Text;
	}
	else
	{
		OutCategory = TEXT("bad_type");
		return false;
	}

	return true;
}

bool UEdGraphSchema_K2::ConvertPropertyToPinType(const UProperty* Property, /*out*/ FEdGraphPinType& TypeOut) const
{
	if (Property == NULL)
	{
		TypeOut.PinCategory = TEXT("bad_type");
		return false;
	}

	TypeOut.PinSubCategory = TEXT("");
	
	// Handle whether or not this is an array property
	const UMapProperty* MapProperty = Cast<const UMapProperty>(Property);
	const USetProperty* SetProperty = Cast<const USetProperty>(Property);
	const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property);
	const UProperty* TestProperty = Property;
	if (MapProperty)
	{
		TestProperty = MapProperty->KeyProp;

		// set up value property:
		UObject* SubCategoryObject = nullptr;
		bool bIsWeakPtr = false;
		bool bResult = GetPropertyCategoryInfo(MapProperty->ValueProp, TypeOut.PinValueType.TerminalCategory, TypeOut.PinValueType.TerminalSubCategory, SubCategoryObject, bIsWeakPtr);
		TypeOut.PinValueType.TerminalSubCategoryObject = SubCategoryObject;

		if (bIsWeakPtr)
		{
			return false;
		}

		if (!bResult)
		{
			return false;
		}
	}
	else if (SetProperty)
	{
		TestProperty = SetProperty->ElementProp;
	}
	else if (ArrayProperty)
	{
		TestProperty = ArrayProperty->Inner;
	}
	TypeOut.ContainerType = FEdGraphPinType::ToPinContainerType(ArrayProperty != nullptr, SetProperty != nullptr, MapProperty != nullptr);
	TypeOut.bIsReference = Property->HasAllPropertyFlags(CPF_OutParm|CPF_ReferenceParm);
	TypeOut.bIsConst     = Property->HasAllPropertyFlags(CPF_ConstParm);

	// Check to see if this is the wildcard property for the target container type
	if(IsWildcardProperty(Property))
	{
		TypeOut.PinCategory = PC_Wildcard;
		if(MapProperty)
		{
			TypeOut.PinValueType.TerminalCategory = PC_Wildcard;
		}
	}
	else if (const UMulticastDelegateProperty* MulticastDelegateProperty = Cast<const UMulticastDelegateProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_MCDelegate;
		FMemberReference::FillSimpleMemberReference<UFunction>(MulticastDelegateProperty->SignatureFunction, TypeOut.PinSubCategoryMemberReference);
	}
	else if (const UDelegateProperty* DelegateProperty = Cast<const UDelegateProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Delegate;
		FMemberReference::FillSimpleMemberReference<UFunction>(DelegateProperty->SignatureFunction, TypeOut.PinSubCategoryMemberReference);
	}
	else
	{
		UObject* SubCategoryObject = nullptr;
		bool bIsWeakPointer = false;
		bool bResult = GetPropertyCategoryInfo(TestProperty, TypeOut.PinCategory, TypeOut.PinSubCategory, SubCategoryObject, bIsWeakPointer);
		TypeOut.bIsWeakPointer = bIsWeakPointer;
		TypeOut.PinSubCategoryObject = SubCategoryObject;
		if (!bResult)
		{
			return false;
		}
	}

	if (TypeOut.PinSubCategory == PSC_Bitmask)
	{
		const FString& BitmaskEnumName = TestProperty->GetMetaData(TEXT("BitmaskEnum"));
		if(!BitmaskEnumName.IsEmpty())
		{
			// @TODO: Potentially replace this with a serialized UEnum reference on the UProperty (e.g. UByteProperty::Enum)
			TypeOut.PinSubCategoryObject = FindObject<UEnum>(ANY_PACKAGE, *BitmaskEnumName);
		}
	}

	return true;
}

bool UEdGraphSchema_K2::IsWildcardProperty(const UProperty* Property)
{
	UFunction* Function = Cast<UFunction>(Property->GetOuter());

	return Function && ( UK2Node_CallArrayFunction::IsWildcardProperty(Function, Property)
		|| UK2Node_CallFunction::IsStructureWildcardProperty(Function, Property->GetName())
		|| UK2Node_CallFunction::IsWildcardProperty(Function, Property)
		|| FEdGraphUtilities::IsArrayDependentParam(Function, Property->GetName()) );
}

FText UEdGraphSchema_K2::TypeToText(UProperty* const Property)
{
	if (UStructProperty* Struct = Cast<UStructProperty>(Property))
	{
		if (Struct->Struct)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Struct;
			PinType.PinSubCategoryObject = Struct->Struct;
			return TypeToText(PinType);
		}
	}
	else if (UClassProperty* Class = Cast<UClassProperty>(Property))
	{
		if (Class->MetaClass)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Class;
			PinType.PinSubCategoryObject = Class->MetaClass;
			return TypeToText(PinType);
		}
	}
	else if (UInterfaceProperty* Interface = Cast<UInterfaceProperty>(Property))
	{
		if (Interface->InterfaceClass != nullptr)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Interface;
			PinType.PinSubCategoryObject = Interface->InterfaceClass;
			return TypeToText(PinType);
		}
	}
	else if (UObjectPropertyBase* Obj = Cast<UObjectPropertyBase>(Property))
	{
		if( Obj->PropertyClass )
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Object;
			PinType.PinSubCategoryObject = Obj->PropertyClass;
			PinType.bIsWeakPointer = Property->IsA(UWeakObjectProperty::StaticClass());
			return TypeToText(PinType);
		}

		return FText::GetEmpty();
	}
	else if (UArrayProperty* Array = Cast<UArrayProperty>(Property))
	{
		if (Array->Inner)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ArrayType"), TypeToText(Array->Inner));
			return FText::Format(LOCTEXT("ArrayPropertyText", "Array of {ArrayType}"), Args); 
		}
	}
	else if (USetProperty* Set = Cast<USetProperty>(Property))
	{
		if (Set->ElementProp)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("SetType"), TypeToText(Set->ElementProp));
			return FText::Format(LOCTEXT("SetPropertyText", "Set of {SetType}"), Args);
		}
	}
	else if (UMapProperty* Map = Cast<UMapProperty>(Property))
	{
		if (Map->KeyProp && Map->ValueProp)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("MapKeyType"), TypeToText(Map->KeyProp));
			Args.Add(TEXT("MapValueType"), TypeToText(Map->ValueProp));
			return FText::Format(LOCTEXT("MapPropertyText", "Map of {MapKeyType} to {MapValueType}"), Args);
		}
	}
	
	return FText::FromString(Property->GetClass()->GetName());
}

FText UEdGraphSchema_K2::GetCategoryText(const FString& Category, const bool bForMenu)
{
	if (Category.IsEmpty())
	{
		return FText::GetEmpty();
	}

	static TMap<FString, FText> CategoryDescriptions;
	if (CategoryDescriptions.Num() == 0)
	{
		CategoryDescriptions.Add(PC_Exec, LOCTEXT("Exec", "Exec"));
		CategoryDescriptions.Add(PC_Boolean, LOCTEXT("BoolCategory","Boolean"));
		CategoryDescriptions.Add(PC_Byte, LOCTEXT("ByteCategory","Byte"));
		CategoryDescriptions.Add(PC_Class, LOCTEXT("ClassCategory","Class Reference"));
		CategoryDescriptions.Add(PC_Int, LOCTEXT("IntCategory","Integer"));
		CategoryDescriptions.Add(PC_Float, LOCTEXT("FloatCategory","Float"));
		CategoryDescriptions.Add(PC_Name, LOCTEXT("NameCategory","Name"));
		CategoryDescriptions.Add(PC_Delegate, LOCTEXT("DelegateCategory","Delegate"));
		CategoryDescriptions.Add(PC_MCDelegate, LOCTEXT("MulticastDelegateCategory","Multicast Delegate"));
		CategoryDescriptions.Add(PC_Object, LOCTEXT("ObjectCategory","Object Reference"));
		CategoryDescriptions.Add(PC_Interface, LOCTEXT("InterfaceCategory","Interface"));
		CategoryDescriptions.Add(PC_String, LOCTEXT("StringCategory","String"));
		CategoryDescriptions.Add(PC_Text, LOCTEXT("TextCategory","Text"));
		CategoryDescriptions.Add(PC_Struct, LOCTEXT("StructCategory","Structure"));
		CategoryDescriptions.Add(PC_Wildcard, LOCTEXT("WildcardCategory","Wildcard"));
		CategoryDescriptions.Add(PC_Enum, LOCTEXT("EnumCategory","Enum"));
		CategoryDescriptions.Add(PC_SoftObject, LOCTEXT("SoftObjectReferenceCategory", "Soft Object Reference"));
		CategoryDescriptions.Add(PC_SoftClass, LOCTEXT("SoftClassReferenceCategory", "Soft Class Reference"));
		CategoryDescriptions.Add(AllObjectTypes, LOCTEXT("AllObjectTypes", "Object Types"));
	}

	if (FText const* TypeDesc = CategoryDescriptions.Find(Category))
	{
		return *TypeDesc;
	}
	else
	{
		return FText::FromString(Category);
	}
}

FText UEdGraphSchema_K2::TerminalTypeToText(const FString& Category, const FString& SubCategory, UObject* SubCategoryObject, bool bIsWeakPtr)
{
	FText PropertyText;

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (SubCategory != Schema->PSC_Bitmask && SubCategoryObject != NULL)
	{
		if (Category == Schema->PC_Byte)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("EnumName"), FText::FromString(SubCategoryObject->GetName()));
			PropertyText = FText::Format(LOCTEXT("EnumAsText", "{EnumName} Enum"), Args);
		}
		else
		{
			FString SubCategoryObjName = SubCategoryObject->GetName();
			if (UField* SubCategoryField = Cast<UField>(SubCategoryObject))
			{
				SubCategoryObjName = SubCategoryField->GetDisplayNameText().ToString();
			}

			if (!bIsWeakPtr)
			{
				UClass* PSCOAsClass = Cast<UClass>(SubCategoryObject);
				const bool bIsInterface = PSCOAsClass && PSCOAsClass->HasAnyClassFlags(CLASS_Interface);

				FFormatNamedArguments Args;
				// Don't display the category for "well-known" struct types
				if (Category == UEdGraphSchema_K2::PC_Struct && (SubCategoryObject == UEdGraphSchema_K2::VectorStruct || SubCategoryObject == UEdGraphSchema_K2::RotatorStruct || SubCategoryObject == UEdGraphSchema_K2::TransformStruct))
				{
					Args.Add(TEXT("Category"), FText::GetEmpty());
				}
				else
				{
					Args.Add(TEXT("Category"), (!bIsInterface ? UEdGraphSchema_K2::GetCategoryText(Category) : UEdGraphSchema_K2::GetCategoryText(PC_Interface)));
				}

				Args.Add(TEXT("ObjectName"), FText::FromString(FName::NameToDisplayString(SubCategoryObjName, /*bIsBool =*/false)));
				PropertyText = FText::Format(LOCTEXT("ObjectAsText", "{ObjectName} {Category}"), Args);
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("Category"), FText::FromString(Category));
				Args.Add(TEXT("ObjectName"), FText::FromString(SubCategoryObjName));
				PropertyText = FText::Format(LOCTEXT("WeakPtrAsText", "{ObjectName} Weak {Category}"), Args);
			}
		}
	}
	else if (SubCategory != TEXT(""))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Category"), UEdGraphSchema_K2::GetCategoryText(Category));
		Args.Add(TEXT("ObjectName"), FText::FromString(FName::NameToDisplayString(SubCategory, false)));
		PropertyText = FText::Format(LOCTEXT("ObjectAsText", "{ObjectName} {Category}"), Args);
	}
	else
	{
		PropertyText = UEdGraphSchema_K2::GetCategoryText(Category);
	}

	return PropertyText;
}

FText UEdGraphSchema_K2::TypeToText(const FEdGraphPinType& Type)
{
	FText PropertyText = TerminalTypeToText(Type.PinCategory, Type.PinSubCategory, Type.PinSubCategoryObject.Get(), Type.bIsWeakPointer);

	if (Type.IsMap())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("KeyTitle"), PropertyText);
		FText ValueText = TerminalTypeToText(Type.PinValueType.TerminalCategory, Type.PinValueType.TerminalSubCategory, Type.PinValueType.TerminalSubCategoryObject.Get(), Type.PinValueType.bTerminalIsWeakPointer);
		Args.Add(TEXT("ValueTitle"), ValueText);
		PropertyText = FText::Format(LOCTEXT("MapAsText", "Map of {KeyTitle}s to {ValueTitle}s"), Args);
	}
	else if (Type.IsSet())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PropertyTitle"), PropertyText);
		PropertyText = FText::Format(LOCTEXT("SetAsText", "Set of {PropertyTitle}s"), Args);
	}
	else if (Type.IsArray())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PropertyTitle"), PropertyText);
		PropertyText = FText::Format(LOCTEXT("ArrayAsText", "Array of {PropertyTitle}s"), Args);
	}
	else if (Type.bIsReference)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PropertyTitle"), PropertyText);
		PropertyText = FText::Format(LOCTEXT("PropertyByRef", "{PropertyTitle} (by ref)"), Args);
	}

	return PropertyText;
}

void UEdGraphSchema_K2::GetVariableTypeTree(TArray< TSharedPtr<FPinTypeTreeInfo> >& TypeTree, ETypeTreeFilter TypeTreeFilter) const
{
	bool bAllowExec = (TypeTreeFilter & ETypeTreeFilter::AllowExec) == ETypeTreeFilter::AllowExec;
	bool bAllowWildCard = (TypeTreeFilter & ETypeTreeFilter::AllowWildcard) == ETypeTreeFilter::AllowWildcard;
	bool bIndexTypesOnly = (TypeTreeFilter & ETypeTreeFilter::IndexTypesOnly) == ETypeTreeFilter::IndexTypesOnly;
	bool bRootTypesOnly = (TypeTreeFilter & ETypeTreeFilter::RootTypesOnly) == ETypeTreeFilter::RootTypesOnly;

#ifdef SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
	static_assert(false, "Macro redefinition.");
#endif
#define SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME 0

#if SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
	const double StartTime = FPlatformTime::Seconds();
#endif //SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME

	FTypesDatabase TypesDatabase;
	FTypesDatabase* TypesDatabasePtr = nullptr;
	if (!bRootTypesOnly)
	{
		TypesDatabasePtr = &TypesDatabase;
		FGatherTypesHelper::FillLoadedTypesDatabase(TypesDatabase, bIndexTypesOnly);
	}

#if SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
	const double DatabaseLoadedTime = FPlatformTime::Seconds();
#endif //SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME

	if (!bRootTypesOnly)
	{
		FGatherTypesHelper::FillUnLoadedTypesDatabase(TypesDatabase, bIndexTypesOnly);
	}

#if SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
	const double DatabaseUnLoadedTime = FPlatformTime::Seconds();
#endif //SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME

	// Clear the list
	TypeTree.Empty();

	if( bAllowExec )
	{
		TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Exec, true), PC_Exec, this, LOCTEXT("ExecType", "Execution pin")) ) );
	}

	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Boolean, true), PC_Boolean, this, LOCTEXT("BooleanType", "True or false value")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Byte, true), PC_Byte, this, LOCTEXT("ByteType", "8 bit number")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Int, true), PC_Int, this, LOCTEXT("IntegerType", "Integer number")) ) );
	if (!bIndexTypesOnly)
	{
		TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Float, true), PC_Float, this, LOCTEXT("FloatType", "Floating point number"))));
		TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Name, true), PC_Name, this, LOCTEXT("NameType", "A text name"))));
		TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_String, true), PC_String, this, LOCTEXT("StringType", "A text string"))));
		TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Text, true), PC_Text, this, LOCTEXT("TextType", "A localizable text string"))));

		// Add in special first-class struct types
		if (!bRootTypesOnly)
		{
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(PC_Struct, TBaseStructure<FVector>::Get(), LOCTEXT("VectorType", "A 3D vector"))));
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(PC_Struct, TBaseStructure<FRotator>::Get(), LOCTEXT("RotatorType", "A 3D rotation"))));
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(PC_Struct, TBaseStructure<FTransform>::Get(), LOCTEXT("TransformType", "A 3D transformation, including translation, rotation and 3D scale."))));
		}
	}
	// Add wildcard type
	if (bAllowWildCard)
	{
		TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Wildcard, true), PC_Wildcard, this, LOCTEXT("WildcardType", "Wildcard type (unspecified)")) ) );
	}

	// Add the types that have subtrees
	if (!bIndexTypesOnly)
	{
		TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Struct, true), PC_Struct, this, LOCTEXT("StructType", "Struct (value) types"), true, TypesDatabasePtr)));
		TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Interface, true), PC_Interface, this, LOCTEXT("InterfaceType", "Interface types"), true, TypesDatabasePtr)));

		if (!bRootTypesOnly)
		{
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(AllObjectTypes, true), AllObjectTypes, this, LOCTEXT("ObjectType", "Object types"), true, TypesDatabasePtr)));
		}
		else
		{
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Object, true), PC_Object, this, LOCTEXT("ObjectTypeHardReference", "Hard reference to an Object"), true, TypesDatabasePtr)));
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_Class, true), PC_Class, this, LOCTEXT("ClassType", "Hard reference to a Class"), true, TypesDatabasePtr)));
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_SoftObject, true), PC_SoftObject, this, LOCTEXT("SoftObjectType", "Soft reference to an Object"), true, TypesDatabasePtr)));
			TypeTree.Add(MakeShareable(new FPinTypeTreeInfo(GetCategoryText(PC_SoftClass, true), PC_SoftClass, this, LOCTEXT("SoftClassType", "Soft reference to a Class"), true, TypesDatabasePtr)));
		}
	}
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Enum, true), PC_Enum, this, LOCTEXT("EnumType", "Enumeration types."), true, TypesDatabasePtr) ) );

#if SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
	const double EndTime = FPlatformTime::Seconds();
	UE_LOG(LogBlueprint, Log, TEXT("UEdGraphSchema_K2::GetVariableTypeTree times - LoadedTypesDatabase: %f UnLoadedTypesDatabase: %f FPinTypeTreeInfo: %f"), DatabaseLoadedTime - StartTime, DatabaseUnLoadedTime - DatabaseLoadedTime, EndTime - DatabaseUnLoadedTime);
#endif //SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
#undef SCHEMA_K2_GETVARIABLETYPETREE_LOG_TIME
}

bool UEdGraphSchema_K2::DoesTypeHaveSubtypes(const FString& Category) const
{
	return (Category == PC_Struct) || (Category == PC_Object) || (Category == PC_SoftObject) || (Category == PC_SoftClass) || (Category == PC_Interface) || (Category == PC_Class) || (Category == PC_Enum) || (Category == AllObjectTypes);
}

struct FWildcardArrayPinHelper
{
	static bool CheckArrayCompatibility(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, bool bIgnoreArray)
	{
		if (bIgnoreArray)
		{
			return true;
		}

		const UK2Node* OwningNode = InputPin ? Cast<UK2Node>(InputPin->GetOwningNode()) : nullptr;
		const bool bInputWildcardPinAcceptsArray = !OwningNode || OwningNode->DoesInputWildcardPinAcceptArray(InputPin);
		if (bInputWildcardPinAcceptsArray)
		{
			return true;
		}

		const bool bCheckInputPin = (InputPin->PinType.PinCategory == GetDefault<UEdGraphSchema_K2>()->PC_Wildcard) && !InputPin->PinType.IsArray();
		const bool bArrayOutputPin = OutputPin && OutputPin->PinType.IsArray();
		return !(bCheckInputPin && bArrayOutputPin);
	}
};

bool UEdGraphSchema_K2::ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const
{
	if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Output))
	{
		return FWildcardArrayPinHelper::CheckArrayCompatibility(PinB, PinA, bIgnoreArray)
			&& ArePinTypesCompatible(PinB->PinType, PinA->PinType, CallingContext, bIgnoreArray);
	}
	else if ((PinB->Direction == EGPD_Input) && (PinA->Direction == EGPD_Output))
	{
		return FWildcardArrayPinHelper::CheckArrayCompatibility(PinA, PinB, bIgnoreArray)
			&& ArePinTypesCompatible(PinA->PinType, PinB->PinType, CallingContext, bIgnoreArray);
	}
	else
	{
	return false;
}
}

namespace
{
	static UClass* GetOriginalClassToFixCompatibilit(const UClass* InClass)
	{
		const UBlueprint* BP = InClass ? Cast<const UBlueprint>(InClass->ClassGeneratedBy) : nullptr;
		return BP ? BP->OriginalClass : nullptr;
	}

	// During compilation, pins are moved around for node expansion and the Blueprints may still inherit from REINST_ classes
	// which causes problems for IsChildOf. Because we do not want to modify IsChildOf we must use a separate function
	// that can check to see if classes have an AuthoritativeClass that IsChildOf a Target class.
	static bool IsAuthoritativeChildOf(const UStruct* InSourceStruct, const UStruct* InTargetStruct)
	{
		bool bResult = false;
		bool bIsNonNativeClass = false;
		if (const UClass* TargetAsClass = Cast<const UClass>(InTargetStruct))
		{
			InTargetStruct = TargetAsClass->GetAuthoritativeClass();
		}
		if (UClass* SourceAsClass = const_cast<UClass*>(Cast<UClass>(InSourceStruct)))
		{
			if (SourceAsClass->ClassGeneratedBy)
			{
				// We have a non-native (Blueprint) class which means it can exist in a semi-compiled state and inherit from a REINST_ class.
				bIsNonNativeClass = true;
				while (SourceAsClass)
				{
					if (SourceAsClass->GetAuthoritativeClass() == InTargetStruct)
					{
						bResult = true;
						break;
					}
					SourceAsClass = SourceAsClass->GetSuperClass();
				}
			}
		}

		// We have a native (C++) class, do a normal IsChildOf check
		if (!bIsNonNativeClass)
		{
			bResult = InSourceStruct->IsChildOf(InTargetStruct);
		}

		return bResult;
	}

	static bool ExtendedIsChildOf(const UClass* Child, const UClass* Parent)
	{
		if (Child->IsChildOf(Parent))
		{
			return true;
		}

		const UClass* OriginalChild = GetOriginalClassToFixCompatibilit(Child);
		if (OriginalChild && OriginalChild->IsChildOf(Parent))
		{
			return true;
		}

		const UClass* OriginalParent = GetOriginalClassToFixCompatibilit(Parent);
		if (OriginalParent && Child->IsChildOf(OriginalParent))
		{
			return true;
		}

		return false;
	}

	static bool ExtendedImplementsInterface(const UClass* Class, const UClass* Interface)
	{
		if (Class->ImplementsInterface(Interface))
		{
			return true;
		}

		const UClass* OriginalClass = GetOriginalClassToFixCompatibilit(Class);
		if (OriginalClass && OriginalClass->ImplementsInterface(Interface))
		{
			return true;
		}

		const UClass* OriginalInterface = GetOriginalClassToFixCompatibilit(Interface);
		if (OriginalInterface && Class->ImplementsInterface(OriginalInterface))
		{
			return true;
		}

		return false;
	}
};


bool UEdGraphSchema_K2::DefaultValueSimpleValidation(const FEdGraphPinType& PinType, const FString& PinName, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText, FString* OutMsg /*= NULL*/) const
{
#ifdef DVSV_RETURN_MSG
	static_assert(false, "Macro redefinition.");
#endif
#define DVSV_RETURN_MSG(Str) if(NULL != OutMsg) { *OutMsg = Str; } return false;

	const FString& PinCategory = PinType.PinCategory;
	const FString& PinSubCategory = PinType.PinSubCategory;
	const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();

	if (PinType.IsContainer())
	{
		// containers are validated separately
	}
	//@TODO: FCString::Atoi, FCString::Atof, and appStringToBool will 'accept' any input, but we should probably catch and warn
	// about invalid input (non numeric for int/byte/float, and non 0/1 or yes/no/true/false for bool)

	else if (PinCategory == PC_Boolean)
	{
		// All input is acceptable to some degree
	}
	else if (PinCategory == PC_Byte)
	{
		const UEnum* EnumPtr = Cast<const UEnum>(PinSubCategoryObject);
		if (EnumPtr)
		{
			if ( NewDefaultValue == TEXT("(INVALID)") || EnumPtr->GetIndexByNameString(NewDefaultValue) == INDEX_NONE)
			{
				DVSV_RETURN_MSG(FString::Printf(TEXT("'%s' is not a valid enumerant of '<%s>'"), *NewDefaultValue, *(EnumPtr->GetName())));
			}
		}
		else if (!NewDefaultValue.IsEmpty())
		{
			int32 Value;
			if (!FDefaultValueHelper::ParseInt(NewDefaultValue, Value))
			{
				DVSV_RETURN_MSG(TEXT("Expected a valid unsigned number for a byte property"));
			}
			if ((Value < 0) || (Value > 255))
			{
				DVSV_RETURN_MSG(TEXT("Expected a value between 0 and 255 for a byte property"));
			}
		}
	}
	else if ((PinCategory == PC_Class))
	{
		// Should have an object set but no string
		if (!NewDefaultValue.IsEmpty())
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("String NewDefaultValue '%s' specified on class pin '%s'"), *NewDefaultValue, *(PinName)));
		}

		if (NewDefaultObject == NULL)
		{
			// Valid self-reference or empty reference
		}
		else
		{
			// Otherwise, we expect to be able to resolve the type at least
			const UClass* DefaultClassType = Cast<const UClass>(NewDefaultObject);
			if (DefaultClassType == NULL)
			{
				DVSV_RETURN_MSG(FString::Printf(TEXT("Literal on pin %s is not a class."), *(PinName)));
			}
			else
			{
				// @TODO support PinSubCategory == 'self'
				const UClass* PinClassType = Cast<const UClass>(PinSubCategoryObject);
				if (PinClassType == NULL)
				{
					DVSV_RETURN_MSG(FString::Printf(TEXT("Failed to find class for pin %s"), *(PinName)));
				}
				else
				{
					// Have both types, make sure the specified type is a valid subtype
					if (!IsAuthoritativeChildOf(DefaultClassType, PinClassType))
					{
						DVSV_RETURN_MSG(FString::Printf(TEXT("%s isn't a valid subclass of %s (specified on pin %s)"), *NewDefaultObject->GetPathName(), *PinClassType->GetName(), *(PinName)));
					}
				}
			}
		}
	}
	else if (PinCategory == PC_Float)
	{
		if (!NewDefaultValue.IsEmpty())
		{
			if (!FDefaultValueHelper::IsStringValidFloat(NewDefaultValue))
			{
				DVSV_RETURN_MSG(TEXT("Expected a valid number for an float property"));
			}
		}
	}
	else if (PinCategory == PC_Int)
	{
		if (!NewDefaultValue.IsEmpty())
		{
			if (!FDefaultValueHelper::IsStringValidInteger(NewDefaultValue))
			{
				DVSV_RETURN_MSG(TEXT("Expected a valid number for an integer property"));
			}
		}
	}
	else if (PinCategory == PC_Name)
	{
		// Anything is allowed
	}
	else if ((PinCategory == PC_Object) || (PinCategory == PC_Interface))
	{
		if (PinSubCategoryObject == NULL && (PinSubCategory != PSC_Self))
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("PinSubCategoryObject on pin '%s' is NULL and PinSubCategory is '%s' not 'self'"), *(PinName), *PinSubCategory));
		}

		if (PinSubCategoryObject != NULL && PinSubCategory != TEXT(""))
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("PinSubCategoryObject on pin '%s' is non-NULL but PinSubCategory is '%s', should be empty"), *(PinName), *PinSubCategory));
		}

		// Should have an object set but no string - 'self' is not a valid NewDefaultValue for PC_Object pins
		if (!NewDefaultValue.IsEmpty())
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("String NewDefaultValue '%s' specified on object pin '%s'"), *NewDefaultValue, *(PinName)));
		}

		// Check that the object that is set is of the correct class
		const UClass* ObjectClass = Cast<const UClass>(PinSubCategoryObject);
		if(ObjectClass)
		{
			ObjectClass = ObjectClass->GetAuthoritativeClass();
		}
		if (NewDefaultObject != NULL && ObjectClass != NULL && !NewDefaultObject->GetClass()->GetAuthoritativeClass()->IsChildOf(ObjectClass))
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("%s isn't a %s (specified on pin %s)"), *NewDefaultObject->GetPathName(), *ObjectClass->GetName(), *(PinName)));
		}
	}
	else if ((PinCategory == PC_SoftObject) || (PinCategory == PC_SoftClass))
	{
		// Should not have an object set, should be converted to string before getting here
		if (NewDefaultObject)
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("NewDefaultObject '%s' specified on object pin '%s'"), *NewDefaultObject->GetPathName(), *(PinName)));
		}

		if (!NewDefaultValue.IsEmpty())
		{
			FText PathReason;

			if (!FPackageName::IsValidObjectPath(NewDefaultValue, &PathReason))
			{
				DVSV_RETURN_MSG(FString::Printf(TEXT("Soft Reference '%s' is invalid format for object pin '%s':"), *NewDefaultValue, *(PinName), *PathReason.ToString()));
			}

			// Class and IsAsset validation is not foolproof for soft references, skip
		}
	}
	else if (PinCategory == PC_String)
	{
		// All strings are valid
	}
	else if (PinCategory == PC_Text)
	{
		// Neither of these should ever be true 
		if (InNewDefaultText.IsTransient())
		{
			DVSV_RETURN_MSG(TEXT("Invalid text literal, text is transient!"));
		}
	}
	else if (PinCategory == PC_Struct)
	{
		if (PinSubCategory != TEXT(""))
		{
			DVSV_RETURN_MSG(FString::Printf(TEXT("Invalid PinSubCategory value '%s' (it should be empty)"), *PinSubCategory));
		}

		// Only FRotator and FVector properties are currently allowed to have a valid default value
		const UScriptStruct* StructType = Cast<const UScriptStruct>(PinSubCategoryObject);
		if (StructType == NULL)
		{
			//@TODO: MessageLog.Error(*FString::Printf(TEXT("Failed to find struct named %s (passed thru @@)"), *PinSubCategory), SourceObject);
			DVSV_RETURN_MSG(FString::Printf(TEXT("No struct specified for pin '%s'"), *(PinName)));
		}
		else if (!NewDefaultValue.IsEmpty())
		{
			if (StructType == VectorStruct)
			{
				if (!FDefaultValueHelper::IsStringValidVector(NewDefaultValue))
				{
					DVSV_RETURN_MSG(TEXT("Invalid value for an FVector"));
				}
			}
			else if (StructType == RotatorStruct)
			{
				FRotator Rot;
				if (!FDefaultValueHelper::IsStringValidRotator(NewDefaultValue))
				{
					DVSV_RETURN_MSG(TEXT("Invalid value for an FRotator"));
				}
			}
			else if (StructType == TransformStruct)
			{
				FTransform Transform;
				if (!Transform.InitFromString(NewDefaultValue))
				{
					DVSV_RETURN_MSG(TEXT("Invalid value for an FTransform"));
				}
			}
			else if (StructType == LinearColorStruct)
			{
				FLinearColor Color;
				// Color form: "(R=%f,G=%f,B=%f,A=%f)"
				if (!Color.InitFromString(NewDefaultValue))
				{
					DVSV_RETURN_MSG(TEXT("Invalid value for an FLinearColor"));
				}
			}
			else
			{
				// Structs must pass validation at this point, because we need a UStructProperty to run ImportText
				// They'll be verified in FKCHandler_CallFunction::CreateFunctionCallStatement()
			}
		}
	}
	else if (PinCategory == TEXT("CommentType"))
	{
		// Anything is allowed
	}
	else
	{
		//@TODO: MessageLog.Error(*FString::Printf(TEXT("Unsupported type %s on @@"), *UEdGraphSchema_K2::TypeToText(Type).ToString()), SourceObject);
		DVSV_RETURN_MSG(FString::Printf(TEXT("Unsupported type %s on pin %s"), *UEdGraphSchema_K2::TypeToText(PinType).ToString(), *(PinName)));
	}

#undef DVSV_RETURN_MSG

	return true;
}


bool UEdGraphSchema_K2::ArePinTypesCompatible(const FEdGraphPinType& Output, const FEdGraphPinType& Input, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const
{
	if( !bIgnoreArray && ( Output.ContainerType != Input.ContainerType ) && (Input.PinCategory != PC_Wildcard || Input.IsContainer()) )
	{
		return false;
	}
	else if (Output.PinCategory == Input.PinCategory)
	{
		if ((Output.PinSubCategory == Input.PinSubCategory) 
			&& (Output.PinSubCategoryObject == Input.PinSubCategoryObject)
			&& (Output.PinSubCategoryMemberReference == Input.PinSubCategoryMemberReference))
		{
			if(Input.IsMap())
			{
				return 
					Input.PinValueType.TerminalCategory == PC_Wildcard ||
					Output.PinValueType.TerminalCategory == PC_Wildcard ||
					Input.PinValueType == Output.PinValueType;
			}
			return true;
		}
		else if (Output.PinCategory == PC_Interface)
		{
			UClass const* OutputClass = Cast<UClass const>(Output.PinSubCategoryObject.Get());
			UClass const* InputClass = Cast<UClass const>(Input.PinSubCategoryObject.Get());
			if (!OutputClass || !InputClass 
				|| !OutputClass->IsChildOf(UInterface::StaticClass())
				|| !InputClass->IsChildOf(UInterface::StaticClass()))
			{
				UE_LOG(LogBlueprint, Error,
					TEXT("UEdGraphSchema_K2::ArePinTypesCompatible invalid interface types - OutputClass: %s, InputClass: %s, CallingContext: %s"),
					*GetPathNameSafe(OutputClass), *GetPathNameSafe(InputClass), *GetPathNameSafe(CallingContext));
				return false;
			}

			return ExtendedIsChildOf(OutputClass->GetAuthoritativeClass(), InputClass->GetAuthoritativeClass());
		}
		else if (((Output.PinCategory == PC_SoftObject) && (Input.PinCategory == PC_SoftObject))
			|| ((Output.PinCategory == PC_SoftClass) && (Input.PinCategory == PC_SoftClass)))
		{
			const UClass* OutputObject = (Output.PinSubCategory == PSC_Self) ? CallingContext : Cast<const UClass>(Output.PinSubCategoryObject.Get());
			const UClass* InputObject = (Input.PinSubCategory == PSC_Self) ? CallingContext : Cast<const UClass>(Input.PinSubCategoryObject.Get());
			if (OutputObject && InputObject)
			{
				return ExtendedIsChildOf(OutputObject ,InputObject);
			}
		}
		else if ((Output.PinCategory == PC_Object) || (Output.PinCategory == PC_Struct) || (Output.PinCategory == PC_Class))
		{
			// Subcategory mismatch, but the two could be castable
			// Only allow a match if the input is a superclass of the output
			UStruct const* OutputObject = (Output.PinSubCategory == PSC_Self) ? CallingContext : Cast<UStruct>(Output.PinSubCategoryObject.Get());
			UStruct const* InputObject  = (Input.PinSubCategory == PSC_Self)  ? CallingContext : Cast<UStruct>(Input.PinSubCategoryObject.Get());

			if (OutputObject && InputObject)
			{
				if (Output.PinCategory == PC_Struct)
				{
					return OutputObject->IsChildOf(InputObject) && FStructUtils::TheSameLayout(OutputObject, InputObject);
				}

				// Special Case:  Cannot mix interface and non-interface calls, because the pointer size is different under the hood
				const bool bInputIsInterface  = InputObject->IsChildOf(UInterface::StaticClass());
				const bool bOutputIsInterface = OutputObject->IsChildOf(UInterface::StaticClass());

				UClass const* OutputClass = Cast<const UClass>(OutputObject);
				UClass const* InputClass = Cast<const UClass>(InputObject);

				if (bInputIsInterface != bOutputIsInterface) 
				{
					if (bInputIsInterface && (OutputClass != NULL))
					{
						return ExtendedImplementsInterface(OutputClass, InputClass);
					}
					else if (bOutputIsInterface && (InputClass != NULL))
					{
						return ExtendedImplementsInterface(InputClass, OutputClass);
					}
				}				

				return (IsAuthoritativeChildOf(OutputObject, InputObject) || (OutputClass && InputClass && ExtendedIsChildOf(OutputClass, InputClass)))
					&& (bInputIsInterface == bOutputIsInterface);
			}
		}
		else if ((Output.PinCategory == PC_Byte) && (Output.PinSubCategory == Input.PinSubCategory))
		{
			// NOTE: This allows enums to be converted to bytes.  Long-term we don't want to allow that, but we need it
			// for now until we have == for enums in order to be able to compare them.
			if (Input.PinSubCategoryObject == NULL)
			{
				return true;
			}
		}
		else if (PC_Byte == Output.PinCategory || PC_Int == Output.PinCategory)
		{
			// Bitmask integral types are compatible with non-bitmask integral types (of the same word size).
			return Output.PinSubCategory.StartsWith(PSC_Bitmask) || Input.PinSubCategory.StartsWith(PSC_Bitmask);
		}
		else if (PC_Delegate == Output.PinCategory || PC_MCDelegate == Output.PinCategory)
		{
			auto CanUseFunction = [](const UFunction* Func) -> bool
			{
				return Func && (Func->HasAllFlags(RF_LoadCompleted) || !Func->HasAnyFlags(RF_NeedLoad | RF_WasLoaded));
			};

			const UFunction* OutFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Output.PinSubCategoryMemberReference);
			if (!CanUseFunction(OutFunction))
			{
				OutFunction = NULL;
			}
			if (!OutFunction && Output.PinSubCategoryMemberReference.GetMemberParentClass())
			{
				const UClass* ParentClass = Output.PinSubCategoryMemberReference.GetMemberParentClass();
				const UBlueprint* BPOwner = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);
				if (BPOwner && BPOwner->SkeletonGeneratedClass && (BPOwner->SkeletonGeneratedClass != ParentClass))
				{
					OutFunction = BPOwner->SkeletonGeneratedClass->FindFunctionByName(Output.PinSubCategoryMemberReference.MemberName);
				}
			}
			const UFunction* InFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Input.PinSubCategoryMemberReference);
			if (!CanUseFunction(InFunction))
			{
				InFunction = NULL;
			}
			if (!InFunction && Input.PinSubCategoryMemberReference.GetMemberParentClass())
			{
				const UClass* ParentClass = Input.PinSubCategoryMemberReference.GetMemberParentClass();
				const UBlueprint* BPOwner = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);
				if (BPOwner && BPOwner->SkeletonGeneratedClass && (BPOwner->SkeletonGeneratedClass != ParentClass))
				{
					InFunction = BPOwner->SkeletonGeneratedClass->FindFunctionByName(Input.PinSubCategoryMemberReference.MemberName);
				}
			}
			return !OutFunction || !InFunction || OutFunction->IsSignatureCompatibleWith(InFunction);
		}
	}
	else if (Output.PinCategory == PC_Wildcard || Input.PinCategory == PC_Wildcard)
	{
		// If this is an Index Wildcard we have to check compatibility for indexing types
		if (Output.PinSubCategory == PSC_Index)
		{
			return IsIndexWildcardCompatible(Input);
		}
		else if (Input.PinSubCategory == PSC_Index)
		{
			return IsIndexWildcardCompatible(Output);
		}

		return true;
	}
	else if ((Output.PinCategory == PC_Object) && (Input.PinCategory == PC_Interface))
	{
		UClass const* OutputClass    = Cast<UClass const>(Output.PinSubCategoryObject.Get());
		UClass const* InterfaceClass = Cast<UClass const>(Input.PinSubCategoryObject.Get());

		if ((OutputClass == nullptr) && (Output.PinSubCategory == PSC_Self))
		{
			OutputClass = CallingContext;
		}

		return OutputClass && (ExtendedImplementsInterface(OutputClass, InterfaceClass) || ExtendedIsChildOf(OutputClass, InterfaceClass));
	}

	return false;
}

void UEdGraphSchema_K2::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(&TargetNode);

	Super::BreakNodeLinks(TargetNode);
	
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links") );

	// cache this here, as BreakPinLinks can trigger a node reconstruction invalidating the TargetPin referenceS
	UBlueprint* const Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin.GetOwningNode());



	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link") );

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin->GetOwningNode());

	Super::BreakSinglePinLink(SourcePin, TargetPin);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::ReconstructNode(UEdGraphNode& TargetNode, bool bIsBatchRequest/*=false*/) const
{
	Super::ReconstructNode(TargetNode, bIsBatchRequest);

	// If the reconstruction is being handled by something doing a batch (i.e. the blueprint autoregenerating itself), defer marking the blueprint as modified to prevent multiple recompiles
	if (!bIsBatchRequest)
	{
		const UK2Node* K2Node = Cast<UK2Node>(&TargetNode);
		if (K2Node && K2Node->NodeCausesStructuralBlueprintChange())
		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(&TargetNode);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
}

bool UEdGraphSchema_K2::CanEncapuslateNode(UEdGraphNode const& TestNode) const
{
	// Can't encapsulate entry points (may relax this restriction in the future, but it makes sense for now)
	return !TestNode.IsA(UK2Node_FunctionTerminator::StaticClass()) && 
			TestNode.GetClass() != UK2Node_Tunnel::StaticClass(); //Tunnel nodes getting sucked into collapsed graphs fails badly, want to allow derived types though(composite node/Macroinstances)
}

void UEdGraphSchema_K2::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		// Look for collapsed graph nodes that reference this graph
		TArray<UK2Node_Composite*> CompositeNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Composite>(Blueprint, /*out*/ CompositeNodes);

		TSet<UK2Node_Composite*> NodesToDelete;
		for (int32 i = 0; i < CompositeNodes.Num(); ++i)
		{
			UK2Node_Composite* CompositeNode = CompositeNodes[i];
			if (CompositeNode->BoundGraph == &GraphBeingRemoved)
			{
				NodesToDelete.Add(CompositeNode);
			}
		}

		// Delete the node that owns us
		ensure(NodesToDelete.Num() <= 1);
		for (TSet<UK2Node_Composite*>::TIterator It(NodesToDelete); It; ++It)
		{
			UK2Node_Composite* NodeToDelete = *It;

			// Prevent re-entrancy here
			NodeToDelete->BoundGraph = NULL;

			NodeToDelete->Modify();
			NodeToDelete->DestroyNode();
		}

		// Remove from the list of recently edited documents
		Blueprint->LastEditedDocuments.RemoveAll([&GraphBeingRemoved](const FEditedDocumentInfo& TestDoc) { return TestDoc.EditedObject == &GraphBeingRemoved; });
	}
}

void UEdGraphSchema_K2::GetPinDefaultValuesFromString(const FEdGraphPinType& PinType, UObject* OwningObject, const FString& NewDefaultValue, FString& UseDefaultValue, UObject*& UseDefaultObject, FText& UseDefaultText) const
{
	if ((PinType.PinCategory == PC_Object)
		|| (PinType.PinCategory == PC_Class)
		|| (PinType.PinCategory == PC_Interface))
	{
		FString ObjectPathLocal = NewDefaultValue;
		ConstructorHelpers::StripObjectClass(ObjectPathLocal);

		// If this is not a full object path it's a relative path so should be saved as a string
		if (FPackageName::IsValidObjectPath(ObjectPathLocal))
		{
			FSoftObjectPath AssetRef = ObjectPathLocal;
			UseDefaultValue.Empty();
			UseDefaultObject = AssetRef.TryLoad();
			UseDefaultText = FText::GetEmpty();
		}
		else
		{
			// "None" should be saved as empty string
			if (ObjectPathLocal == TEXT("None"))
			{
				ObjectPathLocal.Empty();
			}

			UseDefaultValue = ObjectPathLocal;
			UseDefaultObject = nullptr;
			UseDefaultText = FText::GetEmpty();
		}
	}
	else if (PinType.PinCategory == PC_Text)
	{
		FString PackageNamespace;
#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			PackageNamespace = TextNamespaceUtil::EnsurePackageNamespace(OwningObject);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
		if (!FTextStringHelper::ReadFromString(*NewDefaultValue, UseDefaultText, nullptr, *PackageNamespace))
		{
			UseDefaultText = FText::FromString(NewDefaultValue);
		}
		UseDefaultObject = nullptr;
		UseDefaultValue.Empty();
	}
	else
	{
		UseDefaultValue = NewDefaultValue;
		UseDefaultObject = nullptr;
		UseDefaultText = FText::GetEmpty();

		if (PinType.PinCategory == PC_Byte && UseDefaultValue.IsEmpty())
		{
			UEnum* EnumPtr = Cast<UEnum>(PinType.PinSubCategoryObject.Get());
			if (EnumPtr)
			{
				// Enums are stored as empty string in autogenerated defaults, but should turn into the first value in array 
				UseDefaultValue = EnumPtr->GetNameStringByIndex(0);
			}
		}
		else if ((PinType.PinCategory == PC_SoftObject) || (PinType.PinCategory == PC_SoftClass))
		{
			ConstructorHelpers::StripObjectClass(UseDefaultValue);
		}
	}
}

void UEdGraphSchema_K2::TrySetDefaultValue(UEdGraphPin& Pin, const FString& NewDefaultValue) const
{
	FString UseDefaultValue;
	UObject* UseDefaultObject = nullptr;
	FText UseDefaultText;

	GetPinDefaultValuesFromString(Pin.PinType, Pin.GetOwningNodeUnchecked(), NewDefaultValue, UseDefaultValue, UseDefaultObject, UseDefaultText);

	// Check the default value and make it an error if it's bogus
	if (IsPinDefaultValid(&Pin, UseDefaultValue, UseDefaultObject, UseDefaultText).IsEmpty())
	{
		Pin.DefaultObject = UseDefaultObject;
		Pin.DefaultValue = UseDefaultValue;
		Pin.DefaultTextValue = UseDefaultText;

		UEdGraphNode* Node = Pin.GetOwningNode();
		Node->PinDefaultValueChanged(&Pin);

		// If the default value is manually set then treat it as if the value was reset to default and remove the orphaned pin
		if (Pin.bOrphanedPin && Pin.DoesDefaultValueMatchAutogenerated())
		{
			Node->PinConnectionListChanged(&Pin);
		}

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Node);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void UEdGraphSchema_K2::TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject) const
{
	FText UseDefaultText;

	if ((Pin.PinType.PinCategory == PC_SoftObject) || (Pin.PinType.PinCategory == PC_SoftClass))
	{
		TrySetDefaultValue(Pin, NewDefaultObject ? NewDefaultObject->GetPathName() : FString());
		return;
	}

	// Check the default value and make it an error if it's bogus
	if (IsPinDefaultValid(&Pin, FString(), NewDefaultObject, UseDefaultText).IsEmpty())
	{
		Pin.DefaultObject = NewDefaultObject;
		Pin.DefaultValue.Empty();
		Pin.DefaultTextValue = UseDefaultText;
	}

	UEdGraphNode* Node = Pin.GetOwningNode();
	Node->PinDefaultValueChanged(&Pin);

	// If the default value is manually set then treat it as if the value was reset to default and remove the orphaned pin
	if (Pin.bOrphanedPin && Pin.DoesDefaultValueMatchAutogenerated())
	{
		Node->PinConnectionListChanged(&Pin);
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const
{
	// No reason to set the FText if it is not a PC_Text.
	if(InPin.PinType.PinCategory == PC_Text)
	{
		// Check the default value and make it an error if it's bogus
		if (IsPinDefaultValid(&InPin, FString(), nullptr, InNewDefaultText).IsEmpty())
		{
			InPin.DefaultObject = nullptr;
			InPin.DefaultValue.Empty();
			InPin.DefaultTextValue = InNewDefaultText;
		}

		UEdGraphNode* Node = InPin.GetOwningNode();
		Node->PinDefaultValueChanged(&InPin);

		// If the default value is manually set then treat it as if the value was reset to default and remove the orphaned pin
		if (InPin.bOrphanedPin && InPin.DoesDefaultValueMatchAutogenerated())
		{
			Node->PinConnectionListChanged(&InPin);
		}

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Node);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

bool UEdGraphSchema_K2::DoesDefaultValueMatchAutogenerated(const UEdGraphPin& InPin) const
{
	if (InPin.PinType.PinCategory == PC_Enum || InPin.PinType.PinCategory == PC_Byte)
	{
		// The autogenerated default value for an enum is left empty in case the default enum value (index 0) changes, in this case we
		// want to validate against the actual value of the 0 index entry
		if (InPin.AutogeneratedDefaultValue.IsEmpty())
		{
			const FString PinDefaultValue = InPin.GetDefaultAsString();
			if (PinDefaultValue.IsEmpty())
			{
				return true;
			}
			else if (UEnum* PinEnumType = Cast<UEnum>(InPin.PinType.PinSubCategoryObject.Get()))
			{
				return InPin.DefaultValue.Equals(PinEnumType->GetNameStringByIndex(0), ESearchCase::IgnoreCase);
			}
			else if (!InPin.bUseBackwardsCompatForEmptyAutogeneratedValue && InPin.PinType.PinCategory == PC_Byte && FCString::Atoi(*PinDefaultValue) == 0)
			{
				return true;
			}
		}
	}
	else if (!InPin.bUseBackwardsCompatForEmptyAutogeneratedValue)
	{
		if (InPin.PinType.PinCategory == PC_Float)
		{
			const float AutogeneratedFloat = FCString::Atof(*InPin.AutogeneratedDefaultValue);
			const float DefaultFloat = FCString::Atof(*InPin.DefaultValue);
			return (AutogeneratedFloat == DefaultFloat);
		}
		else if (InPin.PinType.PinCategory == PC_Struct)
		{
			if (InPin.PinType.PinSubCategoryObject == VectorStruct)
			{
				FVector AutogeneratedVector = FVector::ZeroVector;
				FVector DefaultVector = FVector::ZeroVector;
				FDefaultValueHelper::ParseVector(InPin.AutogeneratedDefaultValue, AutogeneratedVector);
				FDefaultValueHelper::ParseVector(InPin.DefaultValue, DefaultVector);
				return (AutogeneratedVector == DefaultVector);
			}
			else if (InPin.PinType.PinSubCategoryObject == RotatorStruct)
			{
				FRotator AutogeneratedRotator = FRotator::ZeroRotator;
				FRotator DefaultRotator = FRotator::ZeroRotator;
				FDefaultValueHelper::ParseRotator(InPin.AutogeneratedDefaultValue, AutogeneratedRotator);
				FDefaultValueHelper::ParseRotator(InPin.DefaultValue, DefaultRotator);
				return (AutogeneratedRotator == DefaultRotator);
			}
		}
		else if (InPin.AutogeneratedDefaultValue.IsEmpty())
		{
			const FString PinDefaultValue = InPin.GetDefaultAsString();
			if (PinDefaultValue.IsEmpty())
			{
				return true;
			}
			else if (InPin.PinType.PinCategory == PC_Boolean)
			{
				return (PinDefaultValue == TEXT("false"));
			}
			else if (InPin.PinType.PinCategory == PC_Int)
			{
				if (FCString::Atoi(*PinDefaultValue) == 0)
				{
					return true;
				}
			}
			else if (InPin.PinType.PinCategory == PC_Name)
			{
				return (PinDefaultValue == TEXT("None"));
			}
		}
	}

	return Super::DoesDefaultValueMatchAutogenerated(InPin);
}

bool UEdGraphSchema_K2::IsAutoCreateRefTerm(const UEdGraphPin* Pin) const
{
	check(Pin);

	bool bIsAutoCreateRefTerm = false;
	UEdGraphNode* OwningNode = Pin->GetOwningNode();
	UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(OwningNode);
	if (FuncNode)
	{
		UFunction* TargetFunction = FuncNode->GetTargetFunction();
		if (TargetFunction && !Pin->PinName.IsEmpty())
		{
			TArray<FString> AutoCreateParameterNames;
			GetAutoEmitTermParameters(TargetFunction, AutoCreateParameterNames);
			bIsAutoCreateRefTerm = AutoCreateParameterNames.Contains(Pin->PinName);
		}
	}

	return bIsAutoCreateRefTerm;
}

bool UEdGraphSchema_K2::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	check(Pin != NULL);

	if (Pin->bDefaultValueIsIgnored || Pin->PinType.IsContainer() || (Pin->PinName == PN_Self && Pin->LinkedTo.Num() > 0) || (Pin->PinType.PinCategory == PC_Exec) || (Pin->PinType.bIsReference && !IsAutoCreateRefTerm(Pin)))
	{
		return true;
	}

	return false;
}

bool UEdGraphSchema_K2::ShouldShowAssetPickerForPin(UEdGraphPin* Pin) const
{
	bool bShow = true;
	if (Pin->PinType.PinCategory == PC_Object)
	{
		UClass* ObjectClass = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
		if (ObjectClass)
		{
			// Don't show literal buttons for component type objects
			bShow = !ObjectClass->IsChildOf(UActorComponent::StaticClass());

			if (bShow && ObjectClass->IsChildOf(AActor::StaticClass()))
			{
				// Only show the picker for Actor classes if the class is placeable and we are in the level script
				bShow = !ObjectClass->HasAllClassFlags(CLASS_NotPlaceable)
							&& FBlueprintEditorUtils::IsLevelScriptBlueprint(FBlueprintEditorUtils::FindBlueprintForNode(Pin->GetOwningNode()));
			}

			if (bShow)
			{
				if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Pin->GetOwningNode()))
				{
					if ( UFunction* FunctionRef = CallFunctionNode->GetTargetFunction() )
					{
						const UEdGraphPin* WorldContextPin = CallFunctionNode->FindPin(FunctionRef->GetMetaData(FBlueprintMetadata::MD_WorldContext));
						bShow = ( WorldContextPin != Pin );
					}
				}
				else if (Cast<UK2Node_CreateDelegate>( Pin->GetOwningNode())) 
				{
					bShow = false;
				}
			}
		}
	}
	return bShow;
}

bool UEdGraphSchema_K2::FindFunctionParameterDefaultValue(const UFunction* Function, const UProperty* Param, FString& OutString)
{
	bool bHasAutomaticValue = false;

	const FString& MetadataDefaultValue = Function->GetMetaData(*Param->GetName());
	if (!MetadataDefaultValue.IsEmpty())
	{
		// Specified default value in the metadata
		OutString = MetadataDefaultValue;
		bHasAutomaticValue = true;
	}
	else
	{
		const FName MetadataCppDefaultValueKey(*(FString(TEXT("CPP_Default_")) + Param->GetName()));
		const FString& MetadataCppDefaultValue = Function->GetMetaData(MetadataCppDefaultValueKey);
		if (!MetadataCppDefaultValue.IsEmpty())
		{
			OutString = MetadataCppDefaultValue;
			bHasAutomaticValue = true;
		}
	}

	return bHasAutomaticValue;
}

void UEdGraphSchema_K2::SetPinAutogeneratedDefaultValue(UEdGraphPin* Pin, const FString& NewValue) const
{
	Pin->AutogeneratedDefaultValue = NewValue;

	ResetPinToAutogeneratedDefaultValue(Pin, false);
}

void UEdGraphSchema_K2::SetPinAutogeneratedDefaultValueBasedOnType(UEdGraphPin* Pin) const
{
	FString NewValue;

	// Create a useful default value based on the pin type
	if(Pin->PinType.IsContainer() )
	{
		NewValue = FString();
	}
	else if (Pin->PinType.PinCategory == PC_Int)
	{
		NewValue = TEXT("0");
	}
	else if (Pin->PinType.PinCategory == PC_Byte)
	{
		UEnum* EnumPtr = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
		if(EnumPtr)
		{
			// First element of enum can change. If the enum is { A, B, C } and the default value is A, 
			// the defult value should not change when enum will be changed into { N, A, B, C }
			NewValue = FString();
		}
		else
		{
			NewValue = TEXT("0");
		}
	}
	else if (Pin->PinType.PinCategory == PC_Float)
	{
		// This is a slightly different format than is produced by PropertyValueToString, but changing it has backward compatibility issues
		NewValue = TEXT("0.0");
	}
	else if (Pin->PinType.PinCategory == PC_Boolean)
	{
		NewValue = TEXT("false");
	}
	else if (Pin->PinType.PinCategory == PC_Name)
	{
		NewValue = TEXT("None");
	}
	else if ((Pin->PinType.PinCategory == PC_Struct) && ((Pin->PinType.PinSubCategoryObject == VectorStruct) || (Pin->PinType.PinSubCategoryObject == RotatorStruct)))
	{
		// This is a slightly different format than is produced by PropertyValueToString, but changing it has backward compatibility issues
		NewValue = TEXT("0, 0, 0");
	}

	// PropertyValueToString also has cases for LinerColor and Transform, LinearColor is identical to export text so is fine, the Transform case is specially handled in the vm

	SetPinAutogeneratedDefaultValue(Pin, NewValue);
}

void UEdGraphSchema_K2::ResetPinToAutogeneratedDefaultValue(UEdGraphPin* Pin, bool bCallModifyCallbacks) const
{
	if (Pin->bOrphanedPin)
	{
		UEdGraphNode* Node = Pin->GetOwningNode();
		Node->PinConnectionListChanged(Pin);
	}
	else
	{
		GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNodeUnchecked(), Pin->AutogeneratedDefaultValue, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);

		if (bCallModifyCallbacks)
		{
			UEdGraphNode* Node = Pin->GetOwningNode();
			Node->PinDefaultValueChanged(Pin);

			if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node))
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			}
		}
	}
}

void UEdGraphSchema_K2::SetPinDefaultValueAtConstruction(UEdGraphPin* Pin, const FString& DefaultValueString) const
{
	GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNodeUnchecked(), DefaultValueString, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);
}

void UEdGraphSchema_K2::SetPinDefaultValue(UEdGraphPin* Pin, const UFunction* Function, const UProperty* Param) const
{
	if (Function != nullptr && Param != nullptr)
	{
		FString NewValue;
		FindFunctionParameterDefaultValue(Function, Param, NewValue);
		SetPinAutogeneratedDefaultValue(Pin, NewValue);
	}
	else
	{
		SetPinAutogeneratedDefaultValueBasedOnType(Pin);
	}
}

void UEdGraphSchema_K2::SetPinDefaultValueBasedOnType(UEdGraphPin* Pin) const
{
	SetPinAutogeneratedDefaultValueBasedOnType(Pin);
}

void UEdGraphSchema_K2::ValidateExistingConnections(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Pin->GetOwningNodeUnchecked());
	const UClass* CallingContext = Blueprint
		? ((Blueprint->GeneratedClass != nullptr) ? Blueprint->GeneratedClass : Blueprint->ParentClass)
		: nullptr;

	// Break any newly invalid links
	TArray<UEdGraphPin*> BrokenLinks;
	for (int32 Index = 0; Index < Pin->LinkedTo.Num();)
	{
		UEdGraphPin* OtherPin = Pin->LinkedTo[Index];
		if (K2Schema->ArePinsCompatible(Pin, OtherPin, CallingContext))
		{
			++Index;
		}
		else
		{
			OtherPin->LinkedTo.Remove(Pin);
			Pin->LinkedTo.RemoveAtSwap(Index);

			BrokenLinks.Add(OtherPin);
		}
	}

	// Cascade the check for changed pin types
	for (TArray<UEdGraphPin*>::TIterator PinIt(BrokenLinks); PinIt; ++PinIt)
	{
		UEdGraphPin* OtherPin = *PinIt;
		OtherPin->GetOwningNode()->PinConnectionListChanged(OtherPin);
	}
}

namespace FSetVariableByNameFunctionNames
{
	static const FName SetIntName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetIntPropertyByName));
	static const FName SetByteName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetBytePropertyByName));
	static const FName SetFloatName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetFloatPropertyByName));
	static const FName SetBoolName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetBoolPropertyByName));
	static const FName SetObjectName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetObjectPropertyByName));
	static const FName SetClassName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetClassPropertyByName));
	static const FName SetInterfaceName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetInterfacePropertyByName));
	static const FName SetStringName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetStringPropertyByName));
	static const FName SetTextName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetTextPropertyByName));
	static const FName SetSoftObjectName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetSoftObjectPropertyByName));
	static const FName SetSoftClassName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetSoftClassPropertyByName));
	static const FName SetNameName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetNamePropertyByName));
	static const FName SetVectorName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetVectorPropertyByName));
	static const FName SetRotatorName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetRotatorPropertyByName));
	static const FName SetLinearColorName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetLinearColorPropertyByName));
	static const FName SetTransformName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetTransformPropertyByName));
	static const FName SetCollisionProfileName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetCollisionProfileNameProperty));
	static const FName SetStructureName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetStructurePropertyByName));
	static const FName SetArrayName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, SetArrayPropertyByName));
	static const FName SetSetName(GET_FUNCTION_NAME_CHECKED(UBlueprintSetLibrary, SetSetPropertyByName));
	static const FName SetMapName(GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, SetMapPropertyByName));
};

UFunction* UEdGraphSchema_K2::FindSetVariableByNameFunction(const FEdGraphPinType& PinType)
{
	//!!!! Keep this function synced with FExposeOnSpawnValidator::IsSupported !!!!

	struct FIsCustomStructureParamHelper
	{
		static bool Is(const UObject* Obj)
		{
			const UScriptStruct* Struct = Cast<const UScriptStruct>(Obj);
			return Struct ? Struct->GetBoolMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType) : false;
		}
	};

	UClass* SetFunctionLibraryClass = UKismetSystemLibrary::StaticClass();
	FName SetFunctionName = NAME_None;
	if (PinType.ContainerType == EPinContainerType::Array)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetArrayName;
		SetFunctionLibraryClass = UKismetArrayLibrary::StaticClass();
	}
	else if (PinType.ContainerType == EPinContainerType::Set)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetSetName;
		SetFunctionLibraryClass = UBlueprintSetLibrary::StaticClass();
	}
	else if (PinType.ContainerType == EPinContainerType::Map)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetMapName;
		SetFunctionLibraryClass = UBlueprintMapLibrary::StaticClass();
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetIntName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetByteName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetFloatName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetBoolName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetObjectName;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetClassName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetInterfaceName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetStringName;
	}
	else if ( PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetTextName;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetSoftObjectName;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetSoftClassName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetNameName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == VectorStruct)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetVectorName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == RotatorStruct)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetRotatorName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == ColorStruct)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetLinearColorName;
	}
	else if(PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == TransformStruct)
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetTransformName;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject == FCollisionProfileName::StaticStruct())
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetCollisionProfileName;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && FIsCustomStructureParamHelper::Is(PinType.PinSubCategoryObject.Get()))
	{
		SetFunctionName = FSetVariableByNameFunctionNames::SetStructureName;
	}

	UFunction* Function = nullptr;
	if (!SetFunctionName.IsNone())
	{
		Function = SetFunctionLibraryClass->FindFunctionByName(SetFunctionName);
	}

	return Function;
}

bool UEdGraphSchema_K2::CanPromotePinToVariable( const UEdGraphPin& Pin ) const
{
	const FEdGraphPinType& PinType = Pin.PinType;
	bool bCanPromote = (PinType.PinCategory != PC_Wildcard && PinType.PinCategory != PC_Exec ) ? true : false;

	const UK2Node* Node = Cast<UK2Node>(Pin.GetOwningNode());
	const UBlueprint* OwningBlueprint = Node->GetBlueprint();
	
	if (Pin.bNotConnectable)
	{
		bCanPromote = false;
	}
	else if (!OwningBlueprint || (OwningBlueprint->BlueprintType == BPTYPE_MacroLibrary) || (OwningBlueprint->BlueprintType == BPTYPE_FunctionLibrary) || IsStaticFunctionGraph(Node->GetGraph()))
	{
		// Never allow promotion in macros, because there's not a scope to define them in
		bCanPromote = false;
	}
	else
	{
		if (PinType.PinCategory == PC_Delegate)
		{
			bCanPromote = false;
		}
		else if ((PinType.PinCategory == PC_Object) || (PinType.PinCategory == PC_Interface))
		{
			if (PinType.PinSubCategoryObject != NULL)
			{
				if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
				{
					bCanPromote = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Class);
				}	
			}
		}
		else if ((PinType.PinCategory == PC_Struct) && (PinType.PinSubCategoryObject != NULL))
		{
			if (UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
			{
				bCanPromote = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct);
			}
		}
	}
	
	return bCanPromote;
}

bool UEdGraphSchema_K2::CanSplitStructPin( const UEdGraphPin& Pin ) const
{
	return Pin.GetOwningNode()->CanSplitPin(&Pin) && PinHasSplittableStructType(&Pin);
}

bool UEdGraphSchema_K2::CanRecombineStructPin( const UEdGraphPin& Pin ) const
{
	bool bCanRecombine = (Pin.ParentPin != NULL && Pin.LinkedTo.Num() == 0);
	if (bCanRecombine)
	{
		// Go through all the other subpins and ensure they also are not connected to anything
		TArray<UEdGraphPin*> PinsToExamine = Pin.ParentPin->SubPins;

		int32 PinIndex = 0;
		while (bCanRecombine && PinIndex < PinsToExamine.Num())
		{
			UEdGraphPin* SubPin = PinsToExamine[PinIndex];
			if (SubPin->LinkedTo.Num() > 0)
			{
				bCanRecombine = false;
			}
			else if (SubPin->SubPins.Num() > 0)
			{
				PinsToExamine.Append(SubPin->SubPins);
			}
			++PinIndex;
		}
	}

	return bCanRecombine;
}

void UEdGraphSchema_K2::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.DocLink = TEXT("Shared/Editors/BlueprintEditor/GraphTypes");
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() ); // Fallback is graph name

	UFunction* Function = NULL;
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&Graph);
	if (Blueprint && Blueprint->SkeletonGeneratedClass)
	{
		Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph.GetFName());
	}

	const EGraphType GraphType = GetGraphType(&Graph);
	if (GraphType == GT_Ubergraph)
	{
		DisplayInfo.DocExcerptName = TEXT("EventGraph");

		if (Graph.GetFName() == GN_EventGraph)
		{
			// localized name for the first event graph
			DisplayInfo.PlainName = LOCTEXT("GraphDisplayName_EventGraph", "EventGraph");
			DisplayInfo.Tooltip = DisplayInfo.PlainName;
		}
		else
		{
			DisplayInfo.Tooltip = FText::FromString(Graph.GetName());
		}
	}
	else if (GraphType == GT_Function)
	{
		if ( Graph.GetFName() == FN_UserConstructionScript )
		{
			DisplayInfo.PlainName = LOCTEXT("GraphDisplayName_ConstructionScript", "ConstructionScript");

			DisplayInfo.Tooltip = LOCTEXT("GraphTooltip_ConstructionScript", "Function executed when Blueprint is placed or modified.");
			DisplayInfo.DocExcerptName = TEXT("ConstructionScript");
		}
		else
		{
			// If we found a function from this graph..
			if (Function)
			{
				DisplayInfo.PlainName = FText::FromString(Function->GetName());
				DisplayInfo.Tooltip = FText::FromString(UK2Node_CallFunction::GetDefaultTooltipForFunction(Function)); // grab its tooltip
			}
			else
			{
				DisplayInfo.Tooltip = FText::FromString(Graph.GetName());
			}

			DisplayInfo.DocExcerptName = TEXT("FunctionGraph");
		}
	}
	else if (GraphType == GT_Macro)
	{
		// Show macro description if set
		FKismetUserDeclaredFunctionMetadata* MetaData = UK2Node_MacroInstance::GetAssociatedGraphMetadata(&Graph);
		DisplayInfo.Tooltip = (MetaData && !MetaData->ToolTip.IsEmpty()) ? MetaData->ToolTip : FText::FromString(Graph.GetName());

		DisplayInfo.DocExcerptName = TEXT("MacroGraph");
	}
	else if (GraphType == GT_Animation)
	{
		DisplayInfo.PlainName = LOCTEXT("GraphDisplayName_AnimGraph", "AnimGraph");

		DisplayInfo.Tooltip = LOCTEXT("GraphTooltip_AnimGraph", "Graph used to blend together different animations.");
		DisplayInfo.DocExcerptName = TEXT("AnimGraph");
	}
	else if (GraphType == GT_StateMachine)
	{
		DisplayInfo.Tooltip = FText::FromString(Graph.GetName());
		DisplayInfo.DocExcerptName = TEXT("StateMachine");
	}

	// Add pure/static/const to notes if set
	if (Function)
	{
		if(Function->HasAnyFunctionFlags(FUNC_BlueprintPure))
		{
			DisplayInfo.Notes.Add(TEXT("pure"));
		}

		// since 'static' is implied in a function library, not going to display it (to be consistent with previous behavior)
		if(Function->HasAnyFunctionFlags(FUNC_Static) && Blueprint->BlueprintType != BPTYPE_FunctionLibrary)
		{
			DisplayInfo.Notes.Add(TEXT("static"));
		}
		else if(Function->HasAnyFunctionFlags(FUNC_Const))
		{
			DisplayInfo.Notes.Add(TEXT("const"));
		}
	}

	// Mark transient graphs as obviously so
	if (Graph.HasAllFlags(RF_Transient))
	{
		DisplayInfo.PlainName = FText::FromString( FString::Printf(TEXT("$$ %s $$"), *DisplayInfo.PlainName.ToString()) );
		DisplayInfo.Notes.Add(TEXT("intermediate build product"));
	}

	if( GEditor && GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
	{
		if (GraphType == GT_Function && Function)
		{
			DisplayInfo.DisplayName = GetFriendlySignatureName(Function);
		}
		else
		{
			DisplayInfo.DisplayName = FText::FromString(FName::NameToDisplayString(DisplayInfo.PlainName.ToString(), false));
		}
	}
	else
	{
		DisplayInfo.DisplayName = DisplayInfo.PlainName;
	}
}

bool UEdGraphSchema_K2::IsSelfPin(const UEdGraphPin& Pin) const 
{
	return (Pin.PinName == PN_Self);
}

bool UEdGraphSchema_K2::IsDelegateCategory(const FString& Category) const
{
	return (Category == PC_Delegate);
}

FVector2D UEdGraphSchema_K2::CalculateAveragePositionBetweenNodes(UEdGraphPin* InputPin, UEdGraphPin* OutputPin)
{
	UEdGraphNode* InputNode = InputPin->GetOwningNode();
	UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
	const FVector2D InputCorner(InputNode->NodePosX, InputNode->NodePosY);
	const FVector2D OutputCorner(OutputNode->NodePosX, OutputNode->NodePosY);
	
	return (InputCorner + OutputCorner) * 0.5f;
}

bool UEdGraphSchema_K2::IsConstructionScript(const UEdGraph* TestEdGraph)
{
	TArray<class UK2Node_FunctionEntry*> EntryNodes;
	TestEdGraph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);

	bool bIsConstructionScript = false;
	if (EntryNodes.Num() > 0)
	{
		UK2Node_FunctionEntry const* const EntryNode = EntryNodes[0];
		bIsConstructionScript = (EntryNode->SignatureName == FN_UserConstructionScript);
	}
	return bIsConstructionScript;
}

bool UEdGraphSchema_K2::IsCompositeGraph( const UEdGraph* TestEdGraph ) const
{
	check(TestEdGraph);

	const EGraphType GraphType = GetGraphType(TestEdGraph);
	if(GraphType == GT_Function) 
	{
		//Find the Tunnel node for composite graph and see if its output is a composite node
		for (UEdGraphNode* Node : TestEdGraph->Nodes)
		{
			if (UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node))
			{
				if (UK2Node_Tunnel* OutNode = Tunnel->OutputSourceNode)
				{
					if (OutNode->IsA<UK2Node_Composite>())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool UEdGraphSchema_K2::IsConstFunctionGraph( const UEdGraph* TestEdGraph, bool* bOutIsEnforcingConstCorrectness ) const
{
	check(TestEdGraph);

	const EGraphType GraphType = GetGraphType(TestEdGraph);
	if(GraphType == GT_Function) 
	{
		// Find the entry node for the function graph and see if the 'const' flag is set
		for (UEdGraphNode* Node : TestEdGraph->Nodes)
		{
			if(UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				if(bOutIsEnforcingConstCorrectness != nullptr)
				{
					*bOutIsEnforcingConstCorrectness = EntryNode->bEnforceConstCorrectness;
				}

				return (EntryNode->GetFunctionFlags() & FUNC_Const) != 0;
			}
		}
	}

	if(bOutIsEnforcingConstCorrectness != nullptr)
	{
		*bOutIsEnforcingConstCorrectness = false;
	}

	return false;
}

bool UEdGraphSchema_K2::IsStaticFunctionGraph( const UEdGraph* TestEdGraph ) const
{
	check(TestEdGraph);

	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TestEdGraph);
	if (Blueprint && (EBlueprintType::BPTYPE_FunctionLibrary == Blueprint->BlueprintType))
	{
		return true;
	}

	const EGraphType GraphType = GetGraphType(TestEdGraph);
	if(GraphType == GT_Function) 
	{
		// Find the entry node for the function graph and see if the 'static' flag is set
		for (UEdGraphNode* Node : TestEdGraph->Nodes)
		{
			if(UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				return (EntryNode->GetFunctionFlags() & FUNC_Static) != 0;
			}
		}
	}

	return false;
}

void UEdGraphSchema_K2::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const 
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if ((Blueprint != NULL) && FBlueprintEditorUtils::IsActorBased(Blueprint))
	{
		float XOffset = 0.0f;
		for(int32 AssetIdx=0; AssetIdx < Assets.Num(); AssetIdx++)
		{
			FVector2D Position = GraphPosition + (AssetIdx * FVector2D(XOffset, 0.0f));

			UObject* Asset = Assets[AssetIdx].GetAsset();

			UClass* AssetClass = Asset->GetClass();		
			if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(Asset))
			{
				AssetClass = BlueprintAsset->GeneratedClass;
			}

			TSubclassOf<UActorComponent> DestinationComponentType;
			if (AssetClass->IsChildOf(UActorComponent::StaticClass()) && IsAllowableBlueprintVariableType(AssetClass))
			{
				// If it's an actor component subclass that is a BlueprintableComponent, we're good to go
				DestinationComponentType = AssetClass;
			}
			else
			{
				// Otherwise see if we can factory a component from the asset
				DestinationComponentType = FComponentAssetBrokerage::GetPrimaryComponentForAsset(AssetClass);
				if ((DestinationComponentType == nullptr) && AssetClass->IsChildOf(AActor::StaticClass()))
				{
					DestinationComponentType = UChildActorComponent::StaticClass();
				}
			}

			// Make sure we have an asset type that's registered with the component list
			if (DestinationComponentType != nullptr)
			{
				const FScopedTransaction Transaction(LOCTEXT("CreateAddComponentFromAsset", "Add Component From Asset"));

				FComponentTypeEntry ComponentType = { FString(), FString(), DestinationComponentType };

				IBlueprintNodeBinder::FBindingSet Bindings;
				Bindings.Add(Asset);
				UBlueprintComponentNodeSpawner::Create(ComponentType)->Invoke(Graph, Bindings, GraphPosition);
			}
		}
	}
}

void UEdGraphSchema_K2::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{
	// @TODO: Should dropping on component node change the component?
}

void UEdGraphSchema_K2::DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const
{
	// If dropping onto an 'object' pin, try and set the literal
	if ((Pin->PinType.PinCategory == PC_Object) || (Pin->PinType.PinCategory == PC_Interface))
	{
		UClass* PinClass = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
		if(PinClass != NULL)
		{
			// Find first asset of type of the pin
			UObject* Asset = FAssetData::GetFirstAssetDataOfClass(Assets, PinClass).GetAsset();
			if(Asset != NULL)
			{
				TrySetDefaultObject(*Pin, Asset);
			}
		}
	}
}

void UEdGraphSchema_K2::GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	// No comment at the moment because this doesn't do anything
	OutTooltipText = TEXT("");
	OutOkIcon = false;
}

void UEdGraphSchema_K2::GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	OutTooltipText = TEXT("");
	OutOkIcon = false;

	// If dropping onto an 'object' pin, try and set the literal
	if ((HoverPin->PinType.PinCategory == PC_Object) || (HoverPin->PinType.PinCategory == PC_Interface))
	{
		UClass* PinClass = Cast<UClass>(HoverPin->PinType.PinSubCategoryObject.Get());
		if(PinClass != NULL)
		{
			// Find first asset of type of the pin
			FAssetData AssetData = FAssetData::GetFirstAssetDataOfClass(Assets, PinClass);
			if(AssetData.IsValid())
			{
				OutOkIcon = true;
				OutTooltipText = FString::Printf(TEXT("Assign %s to this pin"), *(AssetData.AssetName.ToString()));
			}
			else
			{
				OutOkIcon = false;
				OutTooltipText = FString::Printf(TEXT("Not compatible with this pin"));
			}
		}
	}
}


void UEdGraphSchema_K2::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const
{
	OutOkIcon = false;

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(HoverGraph);
	if ((Blueprint != nullptr) && FBlueprintEditorUtils::IsActorBased(Blueprint))
	{
		OutTooltipText = LOCTEXT("UnsupportedAssetTypeForGraphDragDrop", "Cannot create a node from this type of asset").ToString();
		for (const FAssetData& AssetData : Assets)
		{
			if (UObject* Asset = AssetData.GetAsset())
			{
				UClass* AssetClass = Asset->GetClass();
				if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(Asset))
				{
					AssetClass = BlueprintAsset->GeneratedClass;
				}

				TSubclassOf<UActorComponent> DestinationComponentType;
				if (AssetClass->IsChildOf(UActorComponent::StaticClass()) && IsAllowableBlueprintVariableType(AssetClass))
				{
					// If it's an actor component subclass that is a BlueprintableComponent, we're good to go
					DestinationComponentType = AssetClass;
				}
				else
				{
					// Otherwise, see if we have a way to make a component out of the specified asset
					DestinationComponentType = FComponentAssetBrokerage::GetPrimaryComponentForAsset(AssetClass);
					if ((DestinationComponentType == nullptr) && AssetClass->IsChildOf(AActor::StaticClass()))
					{
						DestinationComponentType = UChildActorComponent::StaticClass();
					}
				}

				if (DestinationComponentType != nullptr)
				{
					OutOkIcon = true;
					OutTooltipText = TEXT("");
					return;
				}
			}
		}
	}
	else
	{
		OutTooltipText = LOCTEXT("CannotCreateComponentsInNonActorBlueprints", "Cannot create components from assets in a non-Actor blueprint").ToString();
	}
}

bool UEdGraphSchema_K2::FadeNodeWhenDraggingOffPin(const UEdGraphNode* Node, const UEdGraphPin* Pin) const
{
	if(Node && Pin && (PC_Delegate == Pin->PinType.PinCategory) && (EGPD_Input == Pin->Direction))
	{
		//When dragging off a delegate pin, we should duck the alpha of all nodes except the Custom Event nodes that are compatible with the delegate signature
		//This would help reinforce the connection between delegates and their matching events, and make it easier to see at a glance what could be matched up.
		if(const UK2Node_Event* EventNode = Cast<const UK2Node_Event>(Node))
		{
			const UEdGraphPin* DelegateOutPin = EventNode->FindPin(UK2Node_Event::DelegateOutputName);
			if ((NULL != DelegateOutPin) && 
				(ECanCreateConnectionResponse::CONNECT_RESPONSE_DISALLOW != CanCreateConnection(DelegateOutPin, Pin).Response))
			{
				return false;
			}
		}

		if(const UK2Node_CreateDelegate* CreateDelegateNode = Cast<const UK2Node_CreateDelegate>(Node))
		{
			const UEdGraphPin* DelegateOutPin = CreateDelegateNode->GetDelegateOutPin();
			if ((NULL != DelegateOutPin) && 
				(ECanCreateConnectionResponse::CONNECT_RESPONSE_DISALLOW != CanCreateConnection(DelegateOutPin, Pin).Response))
			{
				return false;
			}
		}
		
		return true;
	}
	return false;
}

struct FBackwardCompatibilityConversionHelper
{
	static bool ConvertNode(
		UK2Node* OldNode,
		const FString& BlueprintPinName,
		UK2Node* NewNode,
		const FString& ClassPinName,
		const UEdGraphSchema_K2& Schema,
		bool bOnlyWithDefaultBlueprint)
	{
		check(OldNode && NewNode);
		const UBlueprint* Blueprint = OldNode->GetBlueprint();

		UEdGraph* Graph = OldNode->GetGraph();
		if (!Graph)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No graph containing the node."),
				Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
				*OldNode->GetName(),
				*BlueprintPinName);
			return false;
		}

		UEdGraphPin* OldBlueprintPin = OldNode->FindPin(BlueprintPinName);
		if (!OldBlueprintPin)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No bp pin found '%s'"),
				Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
				*OldNode->GetName(),
				*BlueprintPinName);
			return false;
		}

		const bool bNondefaultBPConnected = (OldBlueprintPin->LinkedTo.Num() > 0);
		const bool bTryConvert = !bNondefaultBPConnected || !bOnlyWithDefaultBlueprint;
		if (bTryConvert)
		{
			// CREATE NEW NODE
			NewNode->SetFlags(RF_Transactional);
			Graph->AddNode(NewNode, false, false);
			NewNode->CreateNewGuid();
			NewNode->PostPlacedNewNode();
			NewNode->AllocateDefaultPins();
			NewNode->NodePosX = OldNode->NodePosX;
			NewNode->NodePosY = OldNode->NodePosY;

			UEdGraphPin* ClassPin = NewNode->FindPin(ClassPinName);
			if (!ClassPin)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No class pin found '%s'"),
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
					*NewNode->GetName(),
					*ClassPinName);
				return false;
			}
			UClass* TargetClass = Cast<UClass>(ClassPin->PinType.PinSubCategoryObject.Get());
			if (!TargetClass)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No class found '%s'"),
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
					*NewNode->GetName(),
					*ClassPinName);
				return false;
			}

			// REPLACE BLUEPRINT WITH CLASS
			if (!bNondefaultBPConnected)
			{
				// DEFAULT VALUE
				const UBlueprint* UsedBlueprint = Cast<UBlueprint>(OldBlueprintPin->DefaultObject);
				ensure(!OldBlueprintPin->DefaultObject || UsedBlueprint);
				ensure(!UsedBlueprint || *UsedBlueprint->GeneratedClass);
				UClass* UsedClass = UsedBlueprint ? *UsedBlueprint->GeneratedClass : NULL;
				Schema.TrySetDefaultObject(*ClassPin, UsedClass);
				if (ClassPin->DefaultObject != UsedClass)
				{
					FString ErrorStr = Schema.IsPinDefaultValid(ClassPin, FString(), UsedClass, FText());
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot set class' in blueprint: %s node: '%s' actor bp: %s, reason: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*OldNode->GetName(),
						UsedBlueprint ? *UsedBlueprint->GetName() : TEXT("Unknown"),
						ErrorStr.IsEmpty() ? TEXT("Unknown") : *ErrorStr);
					return false;
				}
			}
			else
			{
				// LINK
				UK2Node_ClassDynamicCast* CastNode = NewObject<UK2Node_ClassDynamicCast>(Graph);
				CastNode->SetFlags(RF_Transactional);
				CastNode->TargetType = TargetClass;
				Graph->AddNode(CastNode, false, false);
				CastNode->CreateNewGuid();
				CastNode->PostPlacedNewNode();
				CastNode->AllocateDefaultPins();
				const int32 OffsetOnGraph = 200;
				CastNode->NodePosX = OldNode->NodePosX - OffsetOnGraph;
				CastNode->NodePosY = OldNode->NodePosY;

				UEdGraphPin* ExecPin = OldNode->GetExecPin();
				UEdGraphPin* ExecCastPin = CastNode->GetExecPin();
				check(ExecCastPin);
				if (!ExecPin || !Schema.MovePinLinks(*ExecPin, *ExecCastPin).CanSafeConnect())
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*ExecCastPin->PinName);
					return false;
				}

				UEdGraphPin* ValidCastPin = CastNode->GetValidCastPin();
				check(ValidCastPin);
				if (!Schema.TryCreateConnection(ValidCastPin, ExecPin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*ValidCastPin->PinName);
					return false;
				}

				UEdGraphPin* InValidCastPin = CastNode->GetInvalidCastPin();
				check(InValidCastPin);
				if (!Schema.TryCreateConnection(InValidCastPin, ExecPin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*InValidCastPin->PinName);
					return false;
				}

				UEdGraphPin* CastSourcePin = CastNode->GetCastSourcePin();
				check(CastSourcePin);
				if (!Schema.MovePinLinks(*OldBlueprintPin, *CastSourcePin).CanSafeConnect())
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*CastSourcePin->PinName);
					return false;
				}

				UEdGraphPin* CastResultPin = CastNode->GetCastResultPin();
				check(CastResultPin);
				if (!Schema.TryCreateConnection(CastResultPin, ClassPin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*CastResultPin->PinName);
					return false;
				}
			}

			// MOVE OTHER PINS
			TArray<UEdGraphPin*> OldPins;
			OldPins.Add(OldBlueprintPin);
			for (UEdGraphPin* Pin : NewNode->Pins)
			{
				check(Pin);
				if (ClassPin != Pin)
				{
					UEdGraphPin* OldPin = OldNode->FindPin(Pin->PinName);
					if (OldPin)
					{
						OldPins.Add(OldPin);
						if (!Schema.MovePinLinks(*OldPin, *Pin).CanSafeConnect())
						{
							UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
								Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
								*Pin->PinName);
						}
					}
					else
					{
						UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'missing old pin' in blueprint: %s"),
							Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
							Pin ? *Pin->PinName : TEXT("Unknown"));
					}
				}
			}
			OldNode->BreakAllNodeLinks();
			for (UEdGraphPin* Pin : OldNode->Pins)
			{
				if (!OldPins.Contains(Pin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'missing new pin' in blueprint: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						Pin ? *Pin->PinName : TEXT("Unknown"));
				}
			}
			Graph->RemoveNode(OldNode);
			return true;
		}
		return false;
	}

	struct FFunctionCallParams
	{
		const FName OldFuncName;
		const FName NewFuncName;
		const FString& BlueprintPinName;
		const FString& ClassPinName;
		const UClass* FuncScope;

		FFunctionCallParams(FName InOldFunc, FName InNewFunc, const FString& InBlueprintPinName, const FString& InClassPinName, const UClass* InFuncScope)
			: OldFuncName(InOldFunc), NewFuncName(InNewFunc), BlueprintPinName(InBlueprintPinName), ClassPinName(InClassPinName), FuncScope(InFuncScope)
		{
			check(FuncScope);
		}

		FFunctionCallParams(const FBlueprintCallableFunctionRedirect& FunctionRedirect)
			: OldFuncName(*FunctionRedirect.OldFunctionName)
			, NewFuncName(*FunctionRedirect.NewFunctionName)
			, BlueprintPinName(FunctionRedirect.BlueprintParamName)
			, ClassPinName(FunctionRedirect.ClassParamName)
			, FuncScope(NULL)
		{
			FuncScope = FindObject<UClass>(ANY_PACKAGE, *FunctionRedirect.ClassName);
		}

	};

	static void ConvertFunctionCallNodes(const FFunctionCallParams& ConversionParams, TArray<UK2Node_CallFunction*>& Nodes, UEdGraph* Graph, const UEdGraphSchema_K2& Schema, bool bOnlyWithDefaultBlueprint)
	{
		if (ConversionParams.FuncScope)
		{
			const UFunction* OldFunc = ConversionParams.FuncScope->FindFunctionByName(ConversionParams.OldFuncName);
			check(OldFunc);
			const UFunction* NewFunc = ConversionParams.FuncScope->FindFunctionByName(ConversionParams.NewFuncName);
			check(NewFunc);

			for (UK2Node_CallFunction* Node : Nodes)
			{
				if (OldFunc == Node->GetTargetFunction())
				{
					UK2Node_CallFunction* NewNode = NewObject<UK2Node_CallFunction>(Graph);
					NewNode->SetFromFunction(NewFunc);
					ConvertNode(Node, ConversionParams.BlueprintPinName, NewNode,
						ConversionParams.ClassPinName, Schema, bOnlyWithDefaultBlueprint);
				}
			}
		}
	}
};

bool UEdGraphSchema_K2::ReplaceOldNodeWithNew(UK2Node* OldNode, UK2Node* NewNode, const TMap<FString, FString>& OldPinToNewPinMap) const
{
	if (!ensure(NewNode->GetGraph() == OldNode->GetGraph()))
	{
		return false;
	}
	const UBlueprint* Blueprint = OldNode->GetBlueprint();	
	const UEdGraphSchema* Schema = NewNode->GetSchema();

	NewNode->NodePosX = OldNode->NodePosX;
	NewNode->NodePosY = OldNode->NodePosY;

	bool bFailedToFindPin = false;
	TArray<UEdGraphPin*> NewPinArray;

	for (int32 PinIdx = 0; PinIdx < OldNode->Pins.Num(); ++PinIdx)
	{
		UEdGraphPin* OldPin = OldNode->Pins[PinIdx];
		UEdGraphPin* NewPin = nullptr;

		const FString* NewPinNamePtr = OldPinToNewPinMap.Find(OldPin->PinName);
		if (NewPinNamePtr && NewPinNamePtr->IsEmpty())
		{
			// if they added an remapping for this pin, but left it empty, then it's assumed that they didn't want us to port any of the connections
			NewPinArray.Add(nullptr);
			continue;
		}
		else
		{
			const FString NewPinName = NewPinNamePtr ? *NewPinNamePtr : OldPin->PinName;
			NewPin = NewNode->FindPin(*NewPinName);

			if (!NewPin && OldPin->ParentPin)
			{
				int32 ParentIndex = INDEX_NONE;
				if (OldNode->Pins.Find(OldPin->ParentPin, ParentIndex))
				{
					if (ensure(ParentIndex < PinIdx))
					{
						UEdGraphPin* OldParent = OldNode->Pins[ParentIndex];
						UEdGraphPin* NewParent = NewPinArray[ParentIndex];

						if (NewParent->SubPins.Num() == 0)
						{
							if (NewParent->PinType.PinCategory == PC_Wildcard)
							{
								NewParent->PinType = OldParent->PinType;
							}
							SplitPin(NewParent);
						}

						FString OldPinName = OldPin->PinName;
						OldPinName.RemoveFromStart(OldParent->PinName);

						for (UEdGraphPin* SubPin : NewParent->SubPins)
						{
							FString SubPinName = SubPin->PinName;
							SubPinName.RemoveFromStart(NewParent->PinName);

							if (SubPinName == OldPinName)
							{
								NewPin = SubPin;
								break;
							}
						}
					}
				}
			}
		}

		if (NewPin == nullptr)
		{
			bFailedToFindPin = true;

			UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot find pin %s in node %s' in blueprint: %s"),
				*OldPin->PinName,
				*NewNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(),
				Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));

			break;
		}
		else
		{
			NewPinArray.Add(NewPin);
		}
	}

	if (!bFailedToFindPin)
	{
		for (int32 PinIdx = 0; PinIdx < OldNode->Pins.Num(); ++PinIdx)
		{
			UEdGraphPin* OldPin = OldNode->Pins[PinIdx];
			UEdGraphPin* NewPin = NewPinArray[PinIdx];

			// could be null, meaning they didn't want to map this OldPin to anything
			if (NewPin == nullptr)
			{
				continue;
			}
			else if (!Schema->MovePinLinks(*OldPin, *NewPin).CanSafeConnect())
			{
				UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot safely move pin %s to %s' in blueprint: %s"),
					*OldPin->PinName,
					*NewPin->PinName,
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));
			}
			else
			{
				// for wildcard pins, which may have to react to being connected with
				NewNode->NotifyPinConnectionListChanged(NewPin);
			}
		}

		NewNode->NodeComment = OldNode->NodeComment;
		NewNode->bCommentBubblePinned = OldNode->bCommentBubblePinned;
		NewNode->bCommentBubbleVisible = OldNode->bCommentBubbleVisible;

		OldNode->DestroyNode();
	}
	return !bFailedToFindPin;
}

UK2Node* UEdGraphSchema_K2::ConvertDeprecatedNodeToFunctionCall(UK2Node* OldNode, UFunction* NewFunction, TMap<FString, FString>& OldPinToNewPinMap, UEdGraph* Graph) const
{
	UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(Graph);
	check(CallFunctionNode);
	CallFunctionNode->SetFlags(RF_Transactional);
	Graph->AddNode(CallFunctionNode, false, false);
	CallFunctionNode->SetFromFunction(NewFunction);
	CallFunctionNode->CreateNewGuid();
	CallFunctionNode->PostPlacedNewNode();
	CallFunctionNode->AllocateDefaultPins();

	if (!ReplaceOldNodeWithNew(OldNode, CallFunctionNode, OldPinToNewPinMap))
	{
		// Failed, destroy node
		CallFunctionNode->DestroyNode();
		return nullptr;
	}
	return CallFunctionNode;
}

void UEdGraphSchema_K2::BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const 
{
	if (Graph)
	{
		{
			static const FString BlueprintPinName(TEXT("Blueprint"));
			static const FString ClassPinName(TEXT("Class"));
			TArray<UK2Node_SpawnActor*> SpawnActorNodes;
			Graph->GetNodesOfClass(SpawnActorNodes);
			for (UK2Node_SpawnActor* SpawnActorNode : SpawnActorNodes)
			{
				FBackwardCompatibilityConversionHelper::ConvertNode(
					SpawnActorNode, BlueprintPinName, NewObject<UK2Node_SpawnActorFromClass>(Graph),
					ClassPinName, *this, bOnlySafeChanges);
			}
		}

		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
			if (Blueprint && *Blueprint->SkeletonGeneratedClass)
			{
				TArray<UK2Node_CallFunction*> Nodes;
				Graph->GetNodesOfClass(Nodes);
				for (const FBlueprintCallableFunctionRedirect& FunctionRedirect : EditoronlyBPFunctionRedirects)
				{
					FBackwardCompatibilityConversionHelper::ConvertFunctionCallNodes(
						FBackwardCompatibilityConversionHelper::FFunctionCallParams(FunctionRedirect),
						Nodes, Graph, *this, bOnlySafeChanges);
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("BackwardCompatibilityNodeConversion: Blueprint '%s' cannot be fully converted. It has no skeleton class!"),
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));
			}
		}

		// Call per-node deprecation functions
		TArray<UK2Node*> PossiblyDeprecatedNodes;
		Graph->GetNodesOfClass<UK2Node>(PossiblyDeprecatedNodes);

		for (UK2Node* Node : PossiblyDeprecatedNodes)
		{
			Node->ConvertDeprecatedNode(Graph, bOnlySafeChanges);
		}
	}
}

UEdGraphNode* UEdGraphSchema_K2::CreateSubstituteNode(UEdGraphNode* Node, const UEdGraph* Graph, FObjectInstancingGraph* InstanceGraph, TSet<FName>& InOutExtraNames) const
{
	// If this is an event node, create a unique custom event node as a substitute
	UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
	if(EventNode)
	{
		if(!Graph)
		{
			// Use the node's graph (outer) if an explicit graph was not specified
			Graph = Node->GetGraph();
		}

		// Can only place events in ubergraphs
		if (GetGraphType(Graph) != EGraphType::GT_Ubergraph)
		{
			return NULL;
		}

		// Find the Blueprint that owns the graph
		UBlueprint* Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : NULL;
		if(Blueprint && Blueprint->SkeletonGeneratedClass)
		{
			// Gather all names in use by the Blueprint class
			TSet<FName> ExistingNamesInUse = InOutExtraNames;
			FBlueprintEditorUtils::GetFunctionNameList(Blueprint, ExistingNamesInUse);
			FBlueprintEditorUtils::GetClassVariableList(Blueprint, ExistingNamesInUse);

			const ERenameFlags RenameFlags = (Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0);

			// Allow the old object name to be used in the graph
			FName ObjName = EventNode->GetFName();
			UObject* Found = FindObject<UObject>(EventNode->GetOuter(), *ObjName.ToString());
			if(Found)
			{
				Found->Rename(NULL, NULL, REN_DontCreateRedirectors | RenameFlags | ((IsAsyncLoading() || Found->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)) ? REN_ForceNoResetLoaders : RF_NoFlags));
			}

			// Create a custom event node to replace the original event node imported from text
			UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventNode->GetOuter(), ObjName, EventNode->GetFlags(), nullptr, true, InstanceGraph);

			// Ensure that it is editable
			CustomEventNode->bIsEditable = true;

			// Set grid position to match that of the target node
			CustomEventNode->NodePosX = EventNode->NodePosX;
			CustomEventNode->NodePosY = EventNode->NodePosY;

			// Build a function name that is appropriate for the event we're replacing
			FString FunctionName;
			const UK2Node_ActorBoundEvent* ActorBoundEventNode = Cast<const UK2Node_ActorBoundEvent>(EventNode);
			const UK2Node_ComponentBoundEvent* CompBoundEventNode = Cast<const UK2Node_ComponentBoundEvent>(EventNode);

			const UEdGraphNode* PreExistingNode = nullptr;
			
			if (InstanceGraph)
			{
				// Use a generic name for the new custom event
				FunctionName = TEXT("CustomEvent");
			}
			else
			{
				// Create a name for the custom event based off the original function
				if (ActorBoundEventNode)
				{
					FString TargetName = TEXT("None");
					if (ActorBoundEventNode->EventOwner)
					{
						TargetName = ActorBoundEventNode->EventOwner->GetActorLabel();
					}

					FunctionName = FString::Printf(TEXT("%s_%s"), *ActorBoundEventNode->DelegatePropertyName.ToString(), *TargetName);
					PreExistingNode = FKismetEditorUtilities::FindBoundEventForActor(ActorBoundEventNode->GetReferencedLevelActor(), ActorBoundEventNode->DelegatePropertyName);
				}
				else if (CompBoundEventNode)
				{
					FunctionName = FString::Printf(TEXT("%s_%s"), *CompBoundEventNode->DelegatePropertyName.ToString(), *CompBoundEventNode->ComponentPropertyName.ToString());
					PreExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, CompBoundEventNode->DelegatePropertyName, CompBoundEventNode->ComponentPropertyName);
				}
				else if (EventNode->CustomFunctionName != NAME_None)
				{
					FunctionName = EventNode->CustomFunctionName.ToString();
				}
				else if (EventNode->bOverrideFunction)
				{
					FunctionName = EventNode->EventReference.GetMemberName().ToString();
				}
				else
				{
					FunctionName = CustomEventNode->GetName().Replace(TEXT("K2Node_"), TEXT(""), ESearchCase::CaseSensitive);
				}
			}

			// Ensure the name does not overlap with other names
			CustomEventNode->CustomFunctionName = FName(*FunctionName, FNAME_Find);
			if (CustomEventNode->CustomFunctionName != NAME_None
				&& ExistingNamesInUse.Contains(CustomEventNode->CustomFunctionName))
			{
				int32 i = 0;
				FString TempFuncName;

				do
				{
					TempFuncName = FString::Printf(TEXT("%s_%d"), *FunctionName, ++i);
					CustomEventNode->CustomFunctionName = FName(*TempFuncName, FNAME_Find);
				} while (CustomEventNode->CustomFunctionName != NAME_None
					&& ExistingNamesInUse.Contains(CustomEventNode->CustomFunctionName));

				FunctionName = TempFuncName;
			}

			if (ActorBoundEventNode)
			{
				PreExistingNode = FKismetEditorUtilities::FindBoundEventForActor(ActorBoundEventNode->GetReferencedLevelActor(), ActorBoundEventNode->DelegatePropertyName);
			}
			else if (CompBoundEventNode)
			{
				PreExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, CompBoundEventNode->DelegatePropertyName, CompBoundEventNode->ComponentPropertyName);
			}
			else
			{
				if (Cast<UK2Node_CustomEvent>(EventNode))
				{
					PreExistingNode = FBlueprintEditorUtils::FindCustomEventNode(Blueprint, EventNode->CustomFunctionName);
				}
				else if (UFunction* EventSignature = EventNode->FindEventSignatureFunction())
				{
					// Note: EventNode::FindEventSignatureFunction will return null if it is deleted (for instance, someone declared a 
					// BlueprintImplementableEvent, and some blueprint implements it, but then the declaration is deleted). It also
					// returns null if the pasted node was sourced from another asset that's not included in the destination project.
					// This is acceptable since we've already created a substitute anyway; this is just looking to see if we actually
					// have a valid pre-existing node that was in conflict, in which case we will emit a warning to the message log.
					UClass* ClassOwner = EventSignature->GetOwnerClass();
					if (ensureMsgf(ClassOwner, TEXT("Wrong class owner of signature %s in node %s"), *GetPathNameSafe(EventSignature), *GetPathNameSafe(EventNode)))
					{
						PreExistingNode = FBlueprintEditorUtils::FindOverrideForFunction(Blueprint, ClassOwner->GetAuthoritativeClass(), EventSignature->GetFName());
					}
				}
			}

			// Should be a unique name now, go ahead and assign it
			CustomEventNode->CustomFunctionName = FName(*FunctionName);
			InOutExtraNames.Add(CustomEventNode->CustomFunctionName);

			// Copy the pins from the old node to the new one that's replacing it
			CustomEventNode->Pins = EventNode->Pins;
			CustomEventNode->UserDefinedPins = EventNode->UserDefinedPins;

			// Clear out the pins from the old node so that links aren't broken later when it's destroyed
			EventNode->Pins.Empty();
			EventNode->UserDefinedPins.Empty();

			bool bOriginalWasCustomEvent = Cast<UK2Node_CustomEvent>(Node) != nullptr;

			// Fixup pins
			for(int32 PinIndex = 0; PinIndex < CustomEventNode->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* Pin = CustomEventNode->Pins[PinIndex];
				check(Pin);

				// Reparent the pin to the new custom event node
				Pin->SetOwningNode(CustomEventNode);

				// Don't include execution or delegate output pins as user-defined pins
				if(!bOriginalWasCustomEvent && !IsExecPin(*Pin) && !IsDelegateCategory(Pin->PinType.PinCategory))
				{
					// Check to see if this pin already exists as a user-defined pin on the custom event node
					bool bFoundUserDefinedPin = false;
					for(int32 UserDefinedPinIndex = 0; UserDefinedPinIndex < CustomEventNode->UserDefinedPins.Num() && !bFoundUserDefinedPin; ++UserDefinedPinIndex)
					{
						const FUserPinInfo& UserDefinedPinInfo = *CustomEventNode->UserDefinedPins[UserDefinedPinIndex].Get();
						bFoundUserDefinedPin = Pin->PinName == UserDefinedPinInfo.PinName && Pin->PinType == UserDefinedPinInfo.PinType;
					}

					if(!bFoundUserDefinedPin)
					{
						// Add a new entry into the user-defined pin array for the custom event node
						TSharedPtr<FUserPinInfo> UserPinInfo = MakeShareable(new FUserPinInfo());
						UserPinInfo->PinName = Pin->PinName;
						UserPinInfo->PinType = Pin->PinType;
						CustomEventNode->UserDefinedPins.Add(UserPinInfo);
					}
				}
			}

			if (PreExistingNode)
			{
				if (!Blueprint->PreCompileLog.IsValid())
				{
					Blueprint->PreCompileLog = TSharedPtr<FCompilerResultsLog>(new FCompilerResultsLog(false));
					Blueprint->PreCompileLog->bSilentMode = false;
					Blueprint->PreCompileLog->bAnnotateMentionedNodes = false;
				}

				// Append a warning to the node and to the logs
				CustomEventNode->bHasCompilerMessage = true;
				CustomEventNode->ErrorType = EMessageSeverity::Warning;
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeName"), CustomEventNode->GetNodeTitle(ENodeTitleType::ListView));
				Args.Add(TEXT("OriginalNodeName"), FText::FromString(PreExistingNode->GetName()));
				CustomEventNode->ErrorMsg = FText::Format(LOCTEXT("ReverseUpgradeWarning", "Conflicted with {OriginalNodeName} and was replaced as a Custom Event!"), Args).ToString();
				Blueprint->PreCompileLog->Warning(*LOCTEXT("ReverseUpgradeWarning_Log", "Pasted node @@  conflicted with @@ and was replaced as a Custom Event!").ToString(), CustomEventNode, PreExistingNode);
			}
			// Return the new custom event node that we just created as a substitute for the original event node
			return CustomEventNode;
		}
	}

	// Use the default logic in all other cases
	return UEdGraphSchema::CreateSubstituteNode(Node, Graph, InstanceGraph, InOutExtraNames);
}

int32 UEdGraphSchema_K2::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	int32 SelectionCount = 0;
	
	if( Blueprint )
	{
		SelectionCount = FKismetEditorUtilities::GetNumberOfSelectedNodes(Blueprint);
	}
	return SelectionCount;
}

TSharedPtr<FEdGraphSchemaAction> UEdGraphSchema_K2::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FEdGraphSchemaAction_K2AddComment));
}

bool UEdGraphSchema_K2::CanDuplicateGraph(UEdGraph* InSourceGraph) const
{
	EGraphType GraphType = GetGraphType(InSourceGraph);
	return GraphType == GT_Function || GraphType == GT_Macro;
}

UEdGraph* UEdGraphSchema_K2::DuplicateGraph(UEdGraph* GraphToDuplicate) const
{
	UEdGraph* NewGraph = NULL;

	if (CanDuplicateGraph(GraphToDuplicate))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(GraphToDuplicate);

		NewGraph = FEdGraphUtilities::CloneGraph(GraphToDuplicate, Blueprint);

		if (NewGraph)
		{
			bool bIsOverrideGraph = false;
			if (Blueprint->BlueprintType == BPTYPE_Interface)
			{
				bIsOverrideGraph = true;
			}
			else if (FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(Blueprint, GraphToDuplicate->GetFName()))
			{
				bIsOverrideGraph = true;
			}
			else if (FindField<UFunction>(Blueprint->ParentClass, GraphToDuplicate->GetFName()))
			{
				bIsOverrideGraph = true;
			}

			// When duplicating an override function, we must put the graph through some extra work to properly own the data being duplicated, instead of expecting pin information will come from a parent
			if (bIsOverrideGraph)
			{
				FBlueprintEditorUtils::PromoteGraphFromInterfaceOverride(Blueprint, NewGraph);
				
				// Remove all calls to the parent function, fix any exec pin links to pass through
				TArray< UK2Node_CallParentFunction* > ParentFunctionCalls;
				NewGraph->GetNodesOfClass(ParentFunctionCalls);

				for (UK2Node_CallParentFunction* ParentFunctionCall : ParentFunctionCalls)
				{
					UEdGraphPin* ExecPin = ParentFunctionCall->GetExecPin();
					UEdGraphPin* ThenPin = ParentFunctionCall->GetThenPin();
					if (ExecPin->LinkedTo.Num() && ThenPin->LinkedTo.Num())
					{
						MovePinLinks(*ExecPin, *ThenPin->LinkedTo[0]);
					}
					NewGraph->RemoveNode(ParentFunctionCall);
				}
			}

			FName NewGraphName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, GraphToDuplicate->GetFName().GetPlainNameString());
			FEdGraphUtilities::RenameGraphCloseToName(NewGraph,NewGraphName.ToString());
			// can't have two graphs with the same guid... that'd be silly!
			NewGraph->GraphGuid = FGuid::NewGuid();

			//Rename the entry node or any further renames will not update the entry node, also fixes a duplicate node issue on compile
			for (int32 NodeIndex = 0; NodeIndex < NewGraph->Nodes.Num(); ++NodeIndex)
			{
				UEdGraphNode* Node = NewGraph->Nodes[NodeIndex];
				if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
				{
					if (EntryNode->SignatureName == GraphToDuplicate->GetFName())
					{
						EntryNode->Modify();
						EntryNode->SignatureName = NewGraph->GetFName();
						break;
					}
				}
				// Rename any custom events to be unique
				else if (Node->GetClass()->GetFName() ==  TEXT("K2Node_CustomEvent"))
				{
					UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
					CustomEvent->RenameCustomEventCloseToName();
				}
			}

			// Potentially adjust variable names for any child blueprints
			FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewGraph->GetFName());
		}
	}
	return NewGraph;
}

/**
 * Attempts to best-guess the height of the node. This is necessary because we don't know the actual
 * size of the node until the next Slate tick
 *
 * @param Node The node to guess the height of
 * @return The estimated height of the specified node
 */
float UEdGraphSchema_K2::EstimateNodeHeight( UEdGraphNode* Node )
{
	float HeightEstimate = 0.0f;

	if ( Node != NULL )
	{
		float BaseNodeHeight = 48.0f;
		bool bConsiderNodePins = false;
		float HeightPerPin = 18.0f;

		if ( Node->IsA( UK2Node_CallFunction::StaticClass() ) )
		{
			BaseNodeHeight = 80.0f;
			bConsiderNodePins = true;
			HeightPerPin = 18.0f;
		}
		else if ( Node->IsA( UK2Node_Event::StaticClass() ) )
		{
			BaseNodeHeight = 48.0f;
			bConsiderNodePins = true;
			HeightPerPin = 16.0f;
		}

		HeightEstimate = BaseNodeHeight;

		if ( bConsiderNodePins )
		{
			int32 NumInputPins = 0;
			int32 NumOutputPins = 0;

			for ( int32 PinIndex = 0; PinIndex < Node->Pins.Num(); PinIndex++ )
			{
				UEdGraphPin* CurrentPin = Node->Pins[PinIndex];
				if ( CurrentPin != NULL && !CurrentPin->bHidden )
				{
					switch ( CurrentPin->Direction )
					{
						case EGPD_Input:
							{
								NumInputPins++;
							}
							break;
						case EGPD_Output:
							{
								NumOutputPins++;
							}
							break;
					}
				}
			}

			float MaxNumPins = float(FMath::Max<int32>( NumInputPins, NumOutputPins ));

			HeightEstimate += MaxNumPins * HeightPerPin;
		}
	}

	return HeightEstimate;
}


bool UEdGraphSchema_K2::CollapseGatewayNode(UK2Node* InNode, UEdGraphNode* InEntryNode, UEdGraphNode* InResultNode, FKismetCompilerContext* CompilerContext, TSet<UEdGraphNode*>* OutExpandedNodes) const
{
	bool bSuccessful = true;

	// Handle any split pin cleanup in either the Entry or Result node first
	auto HandleSplitPins = [CompilerContext, OutExpandedNodes](UK2Node* Node)
	{
		if (Node)
		{
			for (int32 PinIdx = Node->Pins.Num() - 1; PinIdx >= 0; --PinIdx)
			{
				UEdGraphPin* const Pin = Node->Pins[PinIdx];

				// Expand any gateway pins as needed
				if (Pin->SubPins.Num() > 0)
				{
					if (UK2Node* ExpandedNode = Node->ExpandSplitPin(CompilerContext, Node->GetGraph(), Pin))
					{
						if (OutExpandedNodes)
						{
							OutExpandedNodes->Add(ExpandedNode);
						}
					}
				}
			}
		}
	};
	HandleSplitPins(Cast<UK2Node>(InEntryNode));
	HandleSplitPins(Cast<UK2Node>(InResultNode));

	// We iterate the array in reverse so we can both remove the subpins safely after we've read them and
	// so we have split nested structs we combine them back together in the right order
	for (int32 BoundaryPinIndex = InNode->Pins.Num() - 1; BoundaryPinIndex >= 0; --BoundaryPinIndex)
	{
		UEdGraphPin* const BoundaryPin = InNode->Pins[BoundaryPinIndex];

		bool bFunctionNode = InNode->IsA(UK2Node_CallFunction::StaticClass());

		// For each pin in the gateway node, find the associated pin in the entry or result node.
		UEdGraphNode* const GatewayNode = (BoundaryPin->Direction == EGPD_Input) ? InEntryNode : InResultNode;
		UEdGraphPin* GatewayPin = nullptr;
		if (GatewayNode)
		{
			// First handle struct combining if necessary
			if (BoundaryPin->SubPins.Num() > 0)
			{
				if (UK2Node* ExpandedNode = InNode->ExpandSplitPin(CompilerContext, InNode->GetGraph(), BoundaryPin))
				{
					if (OutExpandedNodes)
					{
						OutExpandedNodes->Add(ExpandedNode);
					}
				}
			}

			for (int32 PinIdx = GatewayNode->Pins.Num() - 1; PinIdx >= 0; --PinIdx)
			{
				UEdGraphPin* const Pin = GatewayNode->Pins[PinIdx];

				// Function graphs have a single exec path through them, so only one exec pin for input and another for output. In this fashion, they must not be handled by name.
				if(InNode->GetClass() == UK2Node_CallFunction::StaticClass() && Pin->PinType.PinCategory == PC_Exec && BoundaryPin->PinType.PinCategory == PC_Exec && (Pin->Direction != BoundaryPin->Direction))
				{
					GatewayPin = Pin;
					break;
				}
				else if ((Pin->PinName == BoundaryPin->PinName) && (Pin->Direction != BoundaryPin->Direction))
				{
					GatewayPin = Pin;
					break;
				}
			}
		}

		if (GatewayPin)
		{
			CombineTwoPinNetsAndRemoveOldPins(BoundaryPin, GatewayPin);
		}
		else
		{
			if (BoundaryPin->LinkedTo.Num() > 0 && BoundaryPin->ParentPin == nullptr)
			{
				UBlueprint* OwningBP = InNode->GetBlueprint();
				if( OwningBP )
				{
					// We had an input/output with a connection that wasn't twinned
					bSuccessful = false;
					OwningBP->Message_Warn( FString::Printf(*NSLOCTEXT("K2Node", "PinOnBoundryNode_Warning", "Warning: Pin '%s' on boundary node '%s' could not be found in the composite node '%s'").ToString(),
						*(BoundaryPin->PinName),
						(GatewayNode ? *(GatewayNode->GetName()) : TEXT("(null)")),
						*(GetName()))					
						);
				}
				else
				{
					UE_LOG(LogBlueprint, Warning, TEXT("%s"), *FString::Printf(*NSLOCTEXT("K2Node", "PinOnBoundryNode_Warning", "Warning: Pin '%s' on boundary node '%s' could not be found in the composite node '%s'").ToString(),
						*(BoundaryPin->PinName),
						(GatewayNode ? *(GatewayNode->GetName()) : TEXT("(null)")),
						*(GetName()))					
						);
				}
			}
			else
			{
				// Associated pin was not found but there were no links on this side either, so no harm no foul
			}
		}
	}

	return bSuccessful;
}

void UEdGraphSchema_K2::CombineTwoPinNetsAndRemoveOldPins(UEdGraphPin* InPinA, UEdGraphPin* InPinB) const
{
	check(InPinA != NULL);
	check(InPinB != NULL);
	ensure(InPinA->Direction != InPinB->Direction);

	if ((InPinA->LinkedTo.Num() == 0) && (InPinA->Direction == EGPD_Input))
	{
		// Push the literal value of A to InPinB's connections
		for (int32 IndexB = 0; IndexB < InPinB->LinkedTo.Num(); ++IndexB)
		{
			UEdGraphPin* FarB = InPinB->LinkedTo[IndexB];
			// TODO: Michael N. says this if check should be unnecessary once the underlying issue is fixed.
			// (Probably should use a check() instead once it's removed though.  See additional cases below.
			if (FarB != nullptr)
			{
				FarB->DefaultValue = InPinA->DefaultValue;
				FarB->DefaultObject = InPinA->DefaultObject;
				FarB->DefaultTextValue = InPinA->DefaultTextValue;
			}
		}
	}
	else if ((InPinB->LinkedTo.Num() == 0) && (InPinB->Direction == EGPD_Input))
	{
		// Push the literal value of B to InPinA's connections
		for (int32 IndexA = 0; IndexA < InPinA->LinkedTo.Num(); ++IndexA)
		{
			UEdGraphPin* FarA = InPinA->LinkedTo[IndexA];
			// TODO: Michael N. says this if check should be unnecessary once the underlying issue is fixed.
			// (Probably should use a check() instead once it's removed though.  See additional cases above and below.
			if (FarA != nullptr)
			{
				FarA->DefaultValue = InPinB->DefaultValue;
				FarA->DefaultObject = InPinB->DefaultObject;
				FarA->DefaultTextValue = InPinB->DefaultTextValue;
			}
		}
	}
	else
	{
		// Make direct connections between the things that connect to A or B, removing A and B from the picture
		for (int32 IndexA = 0; IndexA < InPinA->LinkedTo.Num(); ++IndexA)
		{
			UEdGraphPin* FarA = InPinA->LinkedTo[IndexA];
			// TODO: Michael N. says this if check should be unnecessary once the underlying issue is fixed.
			// (Probably should use a check() instead once it's removed though.  See additional cases above.
			if (FarA != nullptr)
			{
				for (int32 IndexB = 0; IndexB < InPinB->LinkedTo.Num(); ++IndexB)
				{
					UEdGraphPin* FarB = InPinB->LinkedTo[IndexB];

					if (FarB != nullptr)
					{
						FarA->Modify();
						FarB->Modify();
						FarA->MakeLinkTo(FarB);
					}
					
				}
			}
		}
	}

	InPinA->BreakAllPinLinks();
	InPinB->BreakAllPinLinks();
}

UK2Node* UEdGraphSchema_K2::CreateSplitPinNode(UEdGraphPin* Pin, const FCreateSplitPinNodeParams& Params) const
{
	ensure((Params.bTransient == false) || ((Params.CompilerContext == nullptr) && (Params.SourceGraph == nullptr)));

	UEdGraphNode* GraphNode = Pin->GetOwningNode();
	UEdGraph* Graph = GraphNode->GetGraph();
	UScriptStruct* StructType = CastChecked<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get(), ECastCheckedType::NullAllowed);
	if (!StructType)
	{
		if (Params.CompilerContext)
		{
			Params.CompilerContext->MessageLog.Error(TEXT("No structure in SubCategoryObject in pin @@"), Pin);
		}
		StructType = GetFallbackStruct();
	}

	UK2Node* SplitPinNode = nullptr;

	if (Pin->Direction == EGPD_Input)
	{
		if (UK2Node_MakeStruct::CanBeMade(StructType))
		{
			UK2Node_MakeStruct* MakeStructNode;

			if (Params.bTransient || Params.CompilerContext)
			{
				MakeStructNode = (Params.bTransient ? NewObject<UK2Node_MakeStruct>(Graph) : Params.CompilerContext->SpawnIntermediateNode<UK2Node_MakeStruct>(GraphNode, Params.SourceGraph));
				MakeStructNode->StructType = StructType;
				MakeStructNode->bMadeAfterOverridePinRemoval = true;
				MakeStructNode->AllocateDefaultPins();
			}
			else
			{
				FGraphNodeCreator<UK2Node_MakeStruct> MakeStructCreator(*Graph);
				MakeStructNode = MakeStructCreator.CreateNode(false);
				MakeStructNode->StructType = StructType;
				MakeStructNode->bMadeAfterOverridePinRemoval = true;
				MakeStructCreator.Finalize();
			}

			SplitPinNode = MakeStructNode;
		}
		else
		{
			const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeMake"));
			const UFunction* Function = FindObject<UFunction>(nullptr, *MetaData, true);

			UK2Node_CallFunction* CallFunctionNode;

			if (Params.bTransient || Params.CompilerContext)
			{
				CallFunctionNode = (Params.bTransient ? NewObject<UK2Node_CallFunction>(Graph) : Params.CompilerContext->SpawnIntermediateNode<UK2Node_CallFunction>(GraphNode, Params.SourceGraph));
				CallFunctionNode->SetFromFunction(Function);
				CallFunctionNode->AllocateDefaultPins();
			}
			else
			{
				FGraphNodeCreator<UK2Node_CallFunction> MakeStructCreator(*Graph);
				CallFunctionNode = MakeStructCreator.CreateNode(false);
				CallFunctionNode->SetFromFunction(Function);
				MakeStructCreator.Finalize();
			}

			SplitPinNode = CallFunctionNode;
		}
	}
	else
	{
		if (UK2Node_BreakStruct::CanBeBroken(StructType))
		{
			UK2Node_BreakStruct* BreakStructNode;

			if (Params.bTransient || Params.CompilerContext)
			{
				BreakStructNode = (Params.bTransient ? NewObject<UK2Node_BreakStruct>(Graph) : Params.CompilerContext->SpawnIntermediateNode<UK2Node_BreakStruct>(GraphNode, Params.SourceGraph));
				BreakStructNode->StructType = StructType;
				BreakStructNode->bMadeAfterOverridePinRemoval = true;
				BreakStructNode->AllocateDefaultPins();
			}
			else
			{
				FGraphNodeCreator<UK2Node_BreakStruct> MakeStructCreator(*Graph);
				BreakStructNode = MakeStructCreator.CreateNode(false);
				BreakStructNode->StructType = StructType;
				BreakStructNode->bMadeAfterOverridePinRemoval = true;
				MakeStructCreator.Finalize();
			}

			SplitPinNode = BreakStructNode;
		}
		else
		{
			const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeBreak"));
			const UFunction* Function = FindObject<UFunction>(nullptr, *MetaData, true);

			UK2Node_CallFunction* CallFunctionNode;

			if (Params.bTransient || Params.CompilerContext)
			{
				CallFunctionNode = (Params.bTransient ? NewObject<UK2Node_CallFunction>(Graph) : Params.CompilerContext->SpawnIntermediateNode<UK2Node_CallFunction>(GraphNode, Params.SourceGraph));
				CallFunctionNode->SetFromFunction(Function);
				CallFunctionNode->AllocateDefaultPins();
			}
			else
			{
				FGraphNodeCreator<UK2Node_CallFunction> MakeStructCreator(*Graph);
				CallFunctionNode = MakeStructCreator.CreateNode(false);
				CallFunctionNode->SetFromFunction(Function);
				MakeStructCreator.Finalize();
			}

			SplitPinNode = CallFunctionNode;
		}
	}

	SplitPinNode->NodePosX = GraphNode->NodePosX - SplitPinNode->NodeWidth - 10;
	SplitPinNode->NodePosY = GraphNode->NodePosY;

	return SplitPinNode;
}

void UEdGraphSchema_K2::SplitPin(UEdGraphPin* Pin, const bool bNotify) const
{
	// Under some circumstances we can get here when PinSubCategoryObject is not set, so we just can't split the pin in that case
	UScriptStruct* StructType = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
	if (StructType == nullptr)
	{
		return;
	}

	UEdGraphNode* GraphNode = Pin->GetOwningNode();
	UK2Node* K2Node = Cast<UK2Node>(GraphNode);
	UEdGraph* Graph = CastChecked<UEdGraph>(GraphNode->GetOuter());

	GraphNode->Modify();
	Pin->Modify();

	Pin->bHidden = true;

	UK2Node* ProtoExpandNode = CreateSplitPinNode(Pin, FCreateSplitPinNodeParams(/*bTransient*/true));
			
	for (UEdGraphPin* ProtoPin : ProtoExpandNode->Pins)
	{
		if (ProtoPin->Direction == Pin->Direction && !ProtoPin->bHidden)
		{
			const FString PinName = FString::Printf(TEXT("%s_%s"), *Pin->PinName, *ProtoPin->PinName);
			const FEdGraphPinType& ProtoPinType = ProtoPin->PinType;
			UEdGraphPin* SubPin = GraphNode->CreatePin(Pin->Direction, ProtoPinType.PinCategory, ProtoPinType.PinSubCategory, ProtoPinType.PinSubCategoryObject.Get(), PinName, ProtoPinType.ContainerType, false, false, INDEX_NONE, ProtoPinType.PinValueType);

			if (K2Node != nullptr && K2Node->ShouldDrawCompact() && !Pin->ParentPin)
			{
				SubPin->PinFriendlyName = ProtoPin->GetDisplayName();
			}
			else
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("PinDisplayName"), Pin->GetDisplayName());
				Arguments.Add(TEXT("ProtoPinDisplayName"), ProtoPin->GetDisplayName());
				SubPin->PinFriendlyName = FText::Format(LOCTEXT("SplitPinFriendlyNameFormat", "{PinDisplayName} {ProtoPinDisplayName}"), Arguments);
			}

			SubPin->DefaultValue = ProtoPin->DefaultValue;
			SubPin->AutogeneratedDefaultValue = ProtoPin->AutogeneratedDefaultValue;

			SubPin->ParentPin = Pin;

			// CreatePin puts the Pin in the array, but we are going to insert it later, so pop it back out
			GraphNode->Pins.Pop(/*bAllowShrinking=*/ false);

			Pin->SubPins.Add(SubPin);
		}
	}

	ProtoExpandNode->DestroyNode();

	if (Pin->Direction == EGPD_Input)
	{
		TArray<FString> OriginalDefaults;
		if (   StructType == TBaseStructure<FVector>::Get()
			|| StructType == TBaseStructure<FRotator>::Get())
		{
			Pin->DefaultValue.ParseIntoArray(OriginalDefaults, TEXT(","), false);
			for (FString& Default : OriginalDefaults)
			{
				Default = FString::SanitizeFloat(FCString::Atof(*Default));
			}
			// In some cases (particularly wildcards) the default value may not accurately reflect the normal component elements
			while (OriginalDefaults.Num() < 3)
			{
				OriginalDefaults.Add(TEXT("0.0"));
			}
			
			// Rotator OriginalDefaults are in the form of Y,Z,X but our pins are in the form of X,Y,Z
			// so we have to change the OriginalDefaults order here to match our pins
			if (StructType == TBaseStructure<FRotator>::Get())
			{
				OriginalDefaults.Swap(0, 2);
				OriginalDefaults.Swap(1, 2);
			}
		}
		else if (StructType == TBaseStructure<FVector2D>::Get())
		{
			FVector2D V2D;
			V2D.InitFromString(Pin->DefaultValue);

			OriginalDefaults.Add(FString::SanitizeFloat(V2D.X));
			OriginalDefaults.Add(FString::SanitizeFloat(V2D.Y));
		}
		else if (StructType == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor LC;
			LC.InitFromString(Pin->DefaultValue);

			OriginalDefaults.Add(FString::SanitizeFloat(LC.R));
			OriginalDefaults.Add(FString::SanitizeFloat(LC.G));
			OriginalDefaults.Add(FString::SanitizeFloat(LC.B));
			OriginalDefaults.Add(FString::SanitizeFloat(LC.A));
		}

		check(OriginalDefaults.Num() == 0 || OriginalDefaults.Num() == Pin->SubPins.Num());

		for (int32 SubPinIndex = 0; SubPinIndex < OriginalDefaults.Num(); ++SubPinIndex)
		{
			UEdGraphPin* SubPin = Pin->SubPins[SubPinIndex];
			SubPin->DefaultValue = OriginalDefaults[SubPinIndex];
		}
	}

	GraphNode->Pins.Insert(Pin->SubPins, GraphNode->Pins.Find(Pin) + 1);

	if (bNotify)
	{
		Graph->NotifyGraphChanged();

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void UEdGraphSchema_K2::RecombinePin(UEdGraphPin* Pin) const
{
	UEdGraphNode* GraphNode = Pin->GetOwningNode();
	UEdGraphPin* ParentPin = Pin->ParentPin;

	GraphNode->Modify();
	ParentPin->Modify();

	ParentPin->bHidden = false;

	UEdGraph* Graph = CastChecked<UEdGraph>(GraphNode->GetOuter());
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);

	for (int32 SubPinIndex = 0; SubPinIndex < ParentPin->SubPins.Num(); ++SubPinIndex)
	{
		UEdGraphPin* SubPin = ParentPin->SubPins[SubPinIndex];

		if (SubPin->SubPins.Num() > 0)
		{
			RecombinePin(SubPin->SubPins[0]);
		}

		GraphNode->Pins.Remove(SubPin);
		Blueprint->WatchedPins.Remove(SubPin);
	}

	if (Pin->Direction == EGPD_Input)
	{
		if (UScriptStruct* StructType = Cast<UScriptStruct>(ParentPin->PinType.PinSubCategoryObject.Get()))
		{
			if (StructType == TBaseStructure<FVector>::Get())
			{
				ParentPin->DefaultValue = ParentPin->SubPins[0]->DefaultValue + TEXT(",") 
										+ ParentPin->SubPins[1]->DefaultValue + TEXT(",")
										+ ParentPin->SubPins[2]->DefaultValue;
			}
			else if (StructType == TBaseStructure<FRotator>::Get())
			{
				// Our pins are in the form X,Y,Z but the Rotator pin type expects the form Y,Z,X
				// so we need to make sure they are added in that order here
				ParentPin->DefaultValue = ParentPin->SubPins[1]->DefaultValue + TEXT(",")
										+ ParentPin->SubPins[2]->DefaultValue + TEXT(",")
										+ ParentPin->SubPins[0]->DefaultValue;
			}
			else if (StructType == TBaseStructure<FVector2D>::Get())
			{
				FVector2D V2D;
				V2D.X = FCString::Atof(*ParentPin->SubPins[0]->DefaultValue);
				V2D.Y = FCString::Atof(*ParentPin->SubPins[1]->DefaultValue);
			
				ParentPin->DefaultValue = V2D.ToString();
			}
			else if (StructType == TBaseStructure<FLinearColor>::Get())
			{
				FLinearColor LC;
				LC.R = FCString::Atof(*ParentPin->SubPins[0]->DefaultValue);
				LC.G = FCString::Atof(*ParentPin->SubPins[1]->DefaultValue);
				LC.B = FCString::Atof(*ParentPin->SubPins[2]->DefaultValue);
				LC.A = FCString::Atof(*ParentPin->SubPins[3]->DefaultValue);

				ParentPin->DefaultValue = LC.ToString();
			}
		}
	}

	// Clear out subpins:
	TArray<UEdGraphPin*>& ParentSubPins = ParentPin->SubPins;
	while (ParentSubPins.Num())
	{
		// To ensure that MarkPendingKill does not mutate ParentSubPins, we null out the ParentPin
		// if we assume that MarkPendingKill *will* mutate ParentSubPins we could introduce an infinite
		// loop. No known case of this being possible, but it would be trivial to write bad node logic
		// that introduces this problem:
		ParentSubPins.Last()->ParentPin = nullptr; 
		ParentSubPins.Last()->MarkPendingKill();
		ParentSubPins.RemoveAt(ParentSubPins.Num()-1);
	}

	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(LOCTEXT("CreateRerouteNodeOnWire", "Create Reroute Node"));

	//@TODO: This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	// Create a new knot
	UEdGraph* ParentGraph = PinA->GetOwningNode()->GetGraph();
	if (!FBlueprintEditorUtils::IsGraphReadOnly(ParentGraph))
	{
		UK2Node_Knot* NewKnot = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_Knot>(ParentGraph, NewObject<UK2Node_Knot>(), KnotTopLeft);

		// Move the connections across (only notifying the knot, as the other two didn't really change)
		PinA->BreakLinkTo(PinB);
		PinA->MakeLinkTo((PinA->Direction == EGPD_Output) ? NewKnot->GetInputPin() : NewKnot->GetOutputPin());
		PinB->MakeLinkTo((PinB->Direction == EGPD_Output) ? NewKnot->GetInputPin() : NewKnot->GetOutputPin());
		NewKnot->PostReconstructNode();

		// Dirty the blueprint
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

void UEdGraphSchema_K2::ConfigureVarNode(UK2Node_Variable* InVarNode, FName InVariableName, UStruct* InVariableSource, UBlueprint* InTargetBlueprint)
{
	// See if this is a 'self context' (ie. blueprint class is owner (or child of owner) of dropped var class)
	if ((InVariableSource == NULL) || InTargetBlueprint->SkeletonGeneratedClass->IsChildOf(InVariableSource))
	{
		FGuid Guid = FBlueprintEditorUtils::FindMemberVariableGuidByName(InTargetBlueprint, InVariableName);
		InVarNode->VariableReference.SetSelfMember(InVariableName, Guid);
	}
	else if (InVariableSource->IsA(UClass::StaticClass()))
	{
		FGuid Guid;
		if (UBlueprint* VariableOwnerBP = Cast<UBlueprint>(Cast<UClass>(InVariableSource)->ClassGeneratedBy))
		{
			Guid = FBlueprintEditorUtils::FindMemberVariableGuidByName(VariableOwnerBP, InVariableName);
		}

		InVarNode->VariableReference.SetExternalMember(InVariableName, CastChecked<UClass>(InVariableSource), Guid);
	}
	else
	{
		FGuid LocalVarGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(InTargetBlueprint, InVariableSource, InVariableName);
		if (LocalVarGuid.IsValid())
		{
			InVarNode->VariableReference.SetLocalMember(InVariableName, InVariableSource, LocalVarGuid);
		}
	}
}

UK2Node_VariableGet* UEdGraphSchema_K2::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	UK2Node_VariableGet* NodeTemplate = NewObject<UK2Node_VariableGet>();
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	UEdGraphSchema_K2::ConfigureVarNode(NodeTemplate, VariableName, Source, Blueprint);

	return FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_VariableGet>(ParentGraph, NodeTemplate, GraphPosition);
}

UK2Node_VariableSet* UEdGraphSchema_K2::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	UK2Node_VariableSet* NodeTemplate = NewObject<UK2Node_VariableSet>();
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	UEdGraphSchema_K2::ConfigureVarNode(NodeTemplate, VariableName, Source, Blueprint);

	return FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_VariableSet>(ParentGraph, NodeTemplate, GraphPosition);
}

UEdGraphPin* UEdGraphSchema_K2::DropPinOnNode(UEdGraphNode* InTargetNode, const FString& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const
{
	UEdGraphPin* ResultPin = nullptr;
	if (UK2Node_EditablePinBase* EditablePinNode = Cast<UK2Node_EditablePinBase>(InTargetNode))
	{
		TArray<UK2Node_EditablePinBase*> EditablePinNodes;
		EditablePinNode->Modify();

		if (InSourcePinDirection == EGPD_Output && Cast<UK2Node_FunctionEntry>(InTargetNode))
		{
			if (UK2Node_FunctionResult* ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(EditablePinNode))
			{
				EditablePinNodes.Add(ResultNode);
			}
			else
			{
				// If we did not successfully find or create a result node, just fail out
				return nullptr;
			}
		}
		else if (InSourcePinDirection == EGPD_Input && Cast<UK2Node_FunctionResult>(InTargetNode))
		{
			TArray<UK2Node_FunctionEntry*> FunctionEntryNode;
			InTargetNode->GetGraph()->GetNodesOfClass(FunctionEntryNode);

			if (FunctionEntryNode.Num() == 1)
			{
				EditablePinNodes.Add(FunctionEntryNode[0]);
			}
			else
			{
				// If we did not successfully find the entry node, just fail out
				return nullptr;
			}
		}
		else
		{
			if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(EditablePinNode))
			{
				EditablePinNodes.Append(ResultNode->GetAllResultNodes());
			}
			else
			{
				EditablePinNodes.Add(EditablePinNode);
			}
		}

		FString NewPinName = InSourcePinName;
		for (UK2Node_EditablePinBase* CurrentEditablePinNode : EditablePinNodes)
		{
			CurrentEditablePinNode->Modify();
			UEdGraphPin* CreatedPin = CurrentEditablePinNode->CreateUserDefinedPin(NewPinName, InSourcePinType, (InSourcePinDirection == EGPD_Input) ? EGPD_Output : EGPD_Input);

			// The final ResultPin is from the node the user dragged and dropped to
			if (EditablePinNode == CurrentEditablePinNode)
			{
				ResultPin = CreatedPin;
			}
		}

		HandleParameterDefaultValueChanged(EditablePinNode);
	}
	return ResultPin;
}

void UEdGraphSchema_K2::HandleParameterDefaultValueChanged(UK2Node* InTargetNode) const
{
	if (UK2Node_EditablePinBase* EditablePinNode = Cast<UK2Node_EditablePinBase>(InTargetNode))
	{
		FParamsChangedHelper ParamsChangedHelper;
		ParamsChangedHelper.ModifiedBlueprints.Add(FBlueprintEditorUtils::FindBlueprintForNode(InTargetNode));
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNode(InTargetNode));

		ParamsChangedHelper.Broadcast(FBlueprintEditorUtils::FindBlueprintForNode(InTargetNode), EditablePinNode, InTargetNode->GetGraph());

		for (UEdGraph* ModifiedGraph : ParamsChangedHelper.ModifiedGraphs)
		{
			if (ModifiedGraph)
			{
				ModifiedGraph->NotifyGraphChanged();
			}
		}

		// Now update all the blueprints that got modified
		for (UBlueprint* Blueprint : ParamsChangedHelper.ModifiedBlueprints)
		{
			if (Blueprint)
			{
				Blueprint->BroadcastChanged();
			}
		}
	}
}

bool UEdGraphSchema_K2::SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const
{
	bool bIsSupported = false;
	if (UK2Node_EditablePinBase* EditablePinNode = Cast<UK2Node_EditablePinBase>(InTargetNode))
	{
		if (InSourcePinDirection == EGPD_Output && Cast<UK2Node_FunctionEntry>(InTargetNode))
		{
			// Just check with the Function Entry and see if it's legal, we'll create/use a result node if the user drops
			bIsSupported = EditablePinNode->CanCreateUserDefinedPin(InSourcePinType, InSourcePinDirection, OutErrorMessage);

			if (bIsSupported)
			{
				OutErrorMessage = LOCTEXT("AddConnectResultNode", "Add Pin to Result Node");
			}
		}
		else if (InSourcePinDirection == EGPD_Input && Cast<UK2Node_FunctionResult>(InTargetNode))
		{
			// Just check with the Function Result and see if it's legal, we'll create/use a result node if the user drops
			bIsSupported = EditablePinNode->CanCreateUserDefinedPin(InSourcePinType, InSourcePinDirection, OutErrorMessage);

			if (bIsSupported)
			{
				OutErrorMessage = LOCTEXT("AddPinEntryNode", "Add Pin to Entry Node");
			}
		}
		else
		{
			bIsSupported = EditablePinNode->CanCreateUserDefinedPin(InSourcePinType, (InSourcePinDirection == EGPD_Input)? EGPD_Output : EGPD_Input, OutErrorMessage);
			if (bIsSupported)
			{
				OutErrorMessage = LOCTEXT("AddPinToNode", "Add Pin to Node");
			}
		}
	}
	return bIsSupported;
}

bool UEdGraphSchema_K2::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return CurrentCacheRefreshID != InVisualizationCacheID;
}

int32 UEdGraphSchema_K2::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UEdGraphSchema_K2::ForceVisualizationCacheClear() const
{
	++CurrentCacheRefreshID;
}


bool UEdGraphSchema_K2::SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* NodeToDelete) const 
{
	UK2Node* Node = Cast<UK2Node>(NodeToDelete);
	if (Node == nullptr || Graph == nullptr || NodeToDelete->GetGraph() != Graph)
	{
		return false;
	}

	UBlueprint* OwnerBlueprint = Node->GetBlueprint();
	Graph->Modify();

	FBlueprintEditorUtils::RemoveNode(OwnerBlueprint, Node, /*bDontRecompile=*/ true);
	FBlueprintEditorUtils::MarkBlueprintAsModified(OwnerBlueprint);
	return true;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
