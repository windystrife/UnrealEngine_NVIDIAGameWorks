// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Serialization/PropertyLocalizationDataGathering.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectHash.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "HAL/UnrealMemory.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"

FPropertyLocalizationDataGatherer::FPropertyLocalizationDataGatherer(TArray<FGatherableTextData>& InOutGatherableTextDataArray, const UPackage* const InPackage, EPropertyLocalizationGathererResultFlags& OutResultFlags)
	: GatherableTextDataArray(InOutGatherableTextDataArray)
	, Package(InPackage)
	, ResultFlags(OutResultFlags)
	, AllObjectsInPackage()
{
	// Build up the list of objects that are within our package - we won't follow object references to things outside of our package
	{
		TArray<UObject*> AllObjectsInPackageArray;
		GetObjectsWithOuter(Package, AllObjectsInPackageArray, true, RF_Transient, EInternalObjectFlags::PendingKill);

		AllObjectsInPackage.Reserve(AllObjectsInPackageArray.Num());
		for (UObject* Object : AllObjectsInPackageArray)
		{
			AllObjectsInPackage.Add(Object);
		}
	}

	TArray<UObject*> RootObjectsInPackage;
	GetObjectsWithOuter(Package, RootObjectsInPackage, false, RF_Transient, EInternalObjectFlags::PendingKill);

	// Iterate over each root object in the package
	for (const UObject* Object : RootObjectsInPackage)
	{
		GatherLocalizationDataFromObjectWithCallbacks(Object, EPropertyLocalizationGathererTextFlags::None);
	}
}

bool FPropertyLocalizationDataGatherer::ShouldProcessObject(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags) const
{
	if (Object->HasAnyFlags(RF_Transient))
	{
		// Transient objects aren't saved, so skip them as part of the gather
		return false;
	}

	// Skip objects that we've already processed to avoid repeated work and cyclic chains
	const bool bAlreadyProcessed = ProcessedObjects.Contains(FObjectAndGatherFlags(Object, GatherTextFlags));
	return !bAlreadyProcessed;
}

void FPropertyLocalizationDataGatherer::MarkObjectProcessed(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	ProcessedObjects.Add(FObjectAndGatherFlags(Object, GatherTextFlags));
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromObjectWithCallbacks(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	// See if we have a custom handler for this type
	FLocalizationDataGatheringCallback* CustomCallback = nullptr;
	for (const UClass* Class = Object->GetClass(); Class != nullptr; Class = Class->GetSuperClass())
	{
		CustomCallback = GetTypeSpecificLocalizationDataGatheringCallbacks().Find(Class);
		if (CustomCallback)
		{
			break;
		}
	}

	if (CustomCallback)
	{
		checkf(IsObjectValidForGather(Object), TEXT("Cannot gather for objects outside of the current package! Package: '%s'. Object: '%s'."), *Package->GetFullName(), *Object->GetFullName());

		if (ShouldProcessObject(Object, GatherTextFlags))
		{
			MarkObjectProcessed(Object, GatherTextFlags);
			(*CustomCallback)(Object, *this, GatherTextFlags);
		}
	}
	else if (ShouldProcessObject(Object, GatherTextFlags))
	{
		MarkObjectProcessed(Object, GatherTextFlags);
		GatherLocalizationDataFromObject(Object, GatherTextFlags);
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromObject(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	checkf(IsObjectValidForGather(Object), TEXT("Cannot gather for objects outside of the current package! Package: '%s'. Object: '%s'."), *Package->GetFullName(), *Object->GetFullName());

	const FString Path = Object->GetPathName();

	// Gather text from our fields.
	GatherLocalizationDataFromObjectFields(Path, Object, GatherTextFlags);

	// Also gather from the script data on UStruct types.
	{
		if (!!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::ForceHasScript))
		{
			ResultFlags |= EPropertyLocalizationGathererResultFlags::HasScript;
		}

		const UStruct* Struct = Cast<UStruct>(Object);
		if (Struct)
		{
			GatherScriptBytecode(Path, Struct->Script, !!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::ForceEditorOnlyScriptData));
		}
	}

	// Gather from anything that has us as their outer, as not all objects are reachable via a property pointer.
	if (!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::SkipSubObjects))
	{
		TArray<UObject*> ChildObjects;
		GetObjectsWithOuter(Object, ChildObjects, false, RF_Transient, EInternalObjectFlags::PendingKill);

		for (const UObject* ChildObject : ChildObjects)
		{
			GatherLocalizationDataFromObjectWithCallbacks(ChildObject, GatherTextFlags);
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromObjectFields(const FString& PathToParent, const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	const UObject* ArchetypeObject = Object->GetArchetype();

	for (TFieldIterator<const UField> FieldIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); FieldIt; ++FieldIt)
	{
		// Gather text from the property data
		{
			const UProperty* PropertyField = Cast<UProperty>(*FieldIt);
			if (PropertyField)
			{
				const void* ValueAddress = PropertyField->ContainerPtrToValuePtr<void>(Object);
				const void* DefaultValueAddress = nullptr;

				if (ArchetypeObject)
				{
					const UProperty* ArchetypePropertyField = FindField<UProperty>(ArchetypeObject->GetClass(), *PropertyField->GetName());
					if (ArchetypePropertyField && ArchetypePropertyField->IsA(PropertyField->GetClass()))
					{
						DefaultValueAddress = ArchetypePropertyField->ContainerPtrToValuePtr<void>(ArchetypeObject);
					}
				}

				GatherLocalizationDataFromChildTextProperties(PathToParent, PropertyField, ValueAddress, DefaultValueAddress, GatherTextFlags | (PropertyField->HasAnyPropertyFlags(CPF_EditorOnly) ? EPropertyLocalizationGathererTextFlags::ForceEditorOnly : EPropertyLocalizationGathererTextFlags::None));
			}
		}

		// Gather text from the script bytecode
		{
			const UStruct* StructField = Cast<UStruct>(*FieldIt);
			if (StructField && IsObjectValidForGather(StructField) && ShouldProcessObject(StructField, GatherTextFlags))
			{
				MarkObjectProcessed(StructField, GatherTextFlags);
				GatherLocalizationDataFromObject(StructField, GatherTextFlags);
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromStructFields(const FString& PathToParent, const UStruct* Struct, const void* StructData, const void* DefaultStructData, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	const UStruct* ArchetypeStruct = Cast<UStruct>(Struct->GetArchetype());

	for (TFieldIterator<const UField> FieldIt(Struct, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); FieldIt; ++FieldIt)
	{
		// Gather text from the property data
		{
			const UProperty* PropertyField = Cast<UProperty>(*FieldIt);
			if (PropertyField)
			{
				const void* ValueAddress = PropertyField->ContainerPtrToValuePtr<void>(StructData);
				const void* DefaultValueAddress = nullptr;

				if (ArchetypeStruct && DefaultStructData)
				{
					const UProperty* ArchetypePropertyField = FindField<UProperty>(ArchetypeStruct, *PropertyField->GetName());
					if (ArchetypePropertyField && ArchetypePropertyField->IsA(PropertyField->GetClass()))
					{
						DefaultValueAddress = ArchetypePropertyField->ContainerPtrToValuePtr<void>(DefaultStructData);
					}
				}

				GatherLocalizationDataFromChildTextProperties(PathToParent, PropertyField, ValueAddress, DefaultValueAddress, GatherTextFlags | (PropertyField->HasAnyPropertyFlags(CPF_EditorOnly) ? EPropertyLocalizationGathererTextFlags::ForceEditorOnly : EPropertyLocalizationGathererTextFlags::None));
			}
		}

		// Gather text from the script bytecode
		{
			const UStruct* StructField = Cast<UStruct>(*FieldIt);
			if (StructField && IsObjectValidForGather(StructField) && ShouldProcessObject(StructField, GatherTextFlags))
			{
				MarkObjectProcessed(StructField, GatherTextFlags);
				GatherLocalizationDataFromObject(StructField, GatherTextFlags);
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherLocalizationDataFromChildTextProperties(const FString& PathToParent, const UProperty* const Property, const void* const ValueAddress, const void* const DefaultValueAddress, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
{
	if (Property->HasAnyPropertyFlags(CPF_Transient))
	{
		// Transient properties aren't saved, so skip them as part of the gather
		return;
	}

	const UTextProperty* const TextProperty = Cast<const UTextProperty>(Property);
	const UArrayProperty* const ArrayProperty = Cast<const UArrayProperty>(Property);
	const UMapProperty* const MapProperty = Cast<const UMapProperty>(Property);
	const USetProperty* const SetProperty = Cast<const USetProperty>(Property);
	const UStructProperty* const StructProperty = Cast<const UStructProperty>(Property);
	const UObjectPropertyBase* const ObjectProperty = Cast<const UObjectPropertyBase>(Property);

	const EPropertyLocalizationGathererTextFlags ChildPropertyGatherTextFlags = GatherTextFlags | (Property->HasAnyPropertyFlags(CPF_EditorOnly) ? EPropertyLocalizationGathererTextFlags::ForceEditorOnly : EPropertyLocalizationGathererTextFlags::None);

	// Handles both native, fixed-size arrays and plain old non-array properties.
	const bool IsFixedSizeArray = Property->ArrayDim > 1;
	for(int32 i = 0; i < Property->ArrayDim; ++i)
	{
		const FString PathToElement = FString(PathToParent.IsEmpty() ? TEXT("") : PathToParent + TEXT(".")) + (IsFixedSizeArray ? Property->GetName() + FString::Printf(TEXT("[%d]"), i) : Property->GetName());
		const void* const ElementValueAddress = reinterpret_cast<const uint8*>(ValueAddress) + Property->ElementSize * i;
		const void* const DefaultElementValueAddress = DefaultValueAddress ? (reinterpret_cast<const uint8*>(DefaultValueAddress) + Property->ElementSize * i) : nullptr;

		const bool bIsDefaultValue = DefaultElementValueAddress && Property->Identical(ElementValueAddress, DefaultElementValueAddress, PPF_None);
		if (bIsDefaultValue)
		{
			// Skip any properties that have the default value, as those will be gathered from the default instance
			continue;
		}

		// Property is a text property.
		if (TextProperty)
		{
			const FText* const TextElementValueAddress = static_cast<const FText*>(ElementValueAddress);

			UPackage* const PropertyPackage = TextProperty->GetOutermost();
			if (FTextInspector::GetFlags(*TextElementValueAddress) & ETextFlag::ConvertedProperty)
			{
				PropertyPackage->MarkPackageDirty();
			}

			GatherTextInstance(*TextElementValueAddress, PathToElement, !!(GatherTextFlags & EPropertyLocalizationGathererTextFlags::ForceEditorOnlyProperties) || TextProperty->HasAnyPropertyFlags(CPF_EditorOnly));
		}
		// Property is a DYNAMIC array property.
		else if (ArrayProperty)
		{
			// Iterate over all elements of the array.
			FScriptArrayHelper ScriptArrayHelper(ArrayProperty, ElementValueAddress);
			const int32 ElementCount = ScriptArrayHelper.Num();
			for(int32 j = 0; j < ElementCount; ++j)
			{
				GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d)"), j), ArrayProperty->Inner, ScriptArrayHelper.GetRawPtr(j), nullptr, ChildPropertyGatherTextFlags);
			}
		}
		// Property is a map property.
		else if (MapProperty)
		{
			// Iterate over all elements of the map.
			FScriptMapHelper ScriptMapHelper(MapProperty, ElementValueAddress);
			const int32 ElementCount = ScriptMapHelper.Num();
			for(int32 j = 0, ElementIndex = 0; ElementIndex < ElementCount; ++j)
			{
				if (!ScriptMapHelper.IsValidIndex(j))
				{
					continue;
				}

				const uint8* MapPairPtr = ScriptMapHelper.GetPairPtr(j);
				GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d - Key)"), ElementIndex), MapProperty->KeyProp, MapPairPtr + MapProperty->MapLayout.KeyOffset, nullptr, ChildPropertyGatherTextFlags);
				GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d - Value)"), ElementIndex), MapProperty->ValueProp, MapPairPtr + MapProperty->MapLayout.ValueOffset, nullptr, ChildPropertyGatherTextFlags);
				++ElementIndex;
			}
		}
		// Property is a set property.
		else if (SetProperty)
		{
			// Iterate over all elements of the Set.
			FScriptSetHelper ScriptSetHelper(SetProperty, ElementValueAddress);
			const int32 ElementCount = ScriptSetHelper.Num();
			for(int32 j = 0, ElementIndex = 0; ElementIndex < ElementCount; ++j)
			{
				if (!ScriptSetHelper.IsValidIndex(j))
				{
					continue;
				}

				const uint8* ElementPtr = ScriptSetHelper.GetElementPtr(j);
				GatherLocalizationDataFromChildTextProperties(PathToElement + FString::Printf(TEXT("(%d)"), ElementIndex), SetProperty->ElementProp, ElementPtr + SetProperty->SetLayout.ElementOffset, nullptr, ChildPropertyGatherTextFlags);
				++ElementIndex;
			}
		}
		// Property is a struct property.
		else if (StructProperty)
		{
			GatherLocalizationDataFromStructFields(PathToElement, StructProperty->Struct, ElementValueAddress, DefaultElementValueAddress, ChildPropertyGatherTextFlags);
		}
		// Property is an object property.
		else if (ObjectProperty && !(GatherTextFlags & EPropertyLocalizationGathererTextFlags::SkipSubObjects))
		{
			const UObject* InnerObject = ObjectProperty->GetObjectPropertyValue(ElementValueAddress);
			if (InnerObject && IsObjectValidForGather(InnerObject))
			{
				GatherLocalizationDataFromObjectWithCallbacks(InnerObject, ChildPropertyGatherTextFlags);
			}
		}
	}
}

void FPropertyLocalizationDataGatherer::GatherTextInstance(const FText& Text, const FString& Description, const bool bIsEditorOnly)
{
	auto AddGatheredText = [this, &Description](const FString& InNamespace, const FString& InKey, const FTextSourceData& InSourceData, const bool InIsEditorOnly)
	{
		FGatherableTextData* GatherableTextData = GatherableTextDataArray.FindByPredicate([&](const FGatherableTextData& Candidate)
		{
			return Candidate.NamespaceName.Equals(InNamespace, ESearchCase::CaseSensitive)
				&& Candidate.SourceData.SourceString.Equals(InSourceData.SourceString, ESearchCase::CaseSensitive)
				&& Candidate.SourceData.SourceStringMetaData == InSourceData.SourceStringMetaData;
		});
		if (!GatherableTextData)
		{
			GatherableTextData = &GatherableTextDataArray[GatherableTextDataArray.AddDefaulted()];
			GatherableTextData->NamespaceName = InNamespace;
			GatherableTextData->SourceData = InSourceData;
		}

		// We might attempt to add the same text multiple times if we process the same object with slightly different flags - only add this source site once though.
		{
			static const FLocMetadataObject DefaultMetadataObject;
			const bool bFoundSourceSiteContext = GatherableTextData->SourceSiteContexts.ContainsByPredicate([&](const FTextSourceSiteContext& InSourceSiteContext) -> bool
			{
				return InSourceSiteContext.KeyName.Equals(InKey, ESearchCase::CaseSensitive)
					&& InSourceSiteContext.SiteDescription.Equals(Description, ESearchCase::CaseSensitive)
					&& InSourceSiteContext.IsEditorOnly == InIsEditorOnly
					&& InSourceSiteContext.IsOptional == false
					&& InSourceSiteContext.InfoMetaData == DefaultMetadataObject
					&& InSourceSiteContext.KeyMetaData == DefaultMetadataObject;
			});

			if (!bFoundSourceSiteContext)
			{
				FTextSourceSiteContext& SourceSiteContext = GatherableTextData->SourceSiteContexts[GatherableTextData->SourceSiteContexts.AddDefaulted()];
				SourceSiteContext.KeyName = InKey;
				SourceSiteContext.SiteDescription = Description;
				SourceSiteContext.IsEditorOnly = InIsEditorOnly;
				SourceSiteContext.IsOptional = false;
			}
		}
	};

	FString Namespace;
	FString Key;
	const FTextDisplayStringRef DisplayString = FTextInspector::GetSharedDisplayString(Text);
	const bool bFoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(DisplayString, Namespace, Key);

	if (!bFoundNamespaceAndKey || !Text.ShouldGatherForLocalization())
	{
		return;
	}

	ResultFlags |= EPropertyLocalizationGathererResultFlags::HasText;

	FTextSourceData SourceData;
	{
		const FString* SourceString = FTextInspector::GetSourceString(Text);
		SourceData.SourceString = SourceString ? *SourceString : FString();
	}

#if USE_STABLE_LOCALIZATION_KEYS
	// Make sure the namespace is what we expect for this package
	// This would usually be done when the text is loaded, but we also gather text when saving packages so we need to make sure things are consistent
	{
		const FString PackageNamespace = TextNamespaceUtil::GetPackageNamespace(Package);
		if (!PackageNamespace.IsEmpty())
		{
			Namespace = TextNamespaceUtil::BuildFullNamespace(Namespace, PackageNamespace);
		}
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	// Always include the text without its package localization ID
	const FString CleanNamespace = TextNamespaceUtil::StripPackageNamespace(Namespace);
	AddGatheredText(CleanNamespace, Key, SourceData, bIsEditorOnly);
}

struct FGatherTextFromScriptBytecode
{
public:
	FGatherTextFromScriptBytecode(const TCHAR* InSourceDescription, const TArray<uint8>& InScript, FPropertyLocalizationDataGatherer& InPropertyLocalizationDataGatherer, const bool InTreatAsEditorOnlyData)
		: SourceDescription(InSourceDescription)
		, Script(const_cast<TArray<uint8>&>(InScript)) // We won't change the script, but we have to lie so that the code in ScriptSerialization.h will compile :(
		, PropertyLocalizationDataGatherer(InPropertyLocalizationDataGatherer)
		, bTreatAsEditorOnlyData(InTreatAsEditorOnlyData)
		, bIsParsingText(false)
	{
		const int32 ScriptSizeBytes = Script.Num();

		int32 iCode = 0;
		while (iCode < ScriptSizeBytes)
		{
			SerializeExpr(iCode, DummyArchive);
		}
	}

private:
	FLinker* GetLinker()
	{
		return nullptr;
	}

	EExprToken SerializeExpr(int32& iCode, FArchive& Ar)
	{
	#define XFERSTRING()		SerializeString(iCode, Ar)
	#define XFERUNICODESTRING() SerializeUnicodeString(iCode, Ar)
	#define XFERTEXT()			SerializeText(iCode, Ar)

	#define SERIALIZEEXPR_INC
	#define SERIALIZEEXPR_AUTO_UNDEF_XFER_MACROS
	#include "ScriptSerialization.h"
		return Expr;
	#undef SERIALIZEEXPR_INC
	#undef SERIALIZEEXPR_AUTO_UNDEF_XFER_MACROS
	}

	void SerializeString(int32& iCode, FArchive& Ar)
	{
		if (bIsParsingText)
		{
			LastParsedString.Reset();

			do
			{
				LastParsedString += (char)(Script[iCode]);

				iCode += sizeof(uint8);
			}
			while (Script[iCode-1]);
		}
		else
		{
			do
			{
				iCode += sizeof(uint8);
			}
			while (Script[iCode-1]);
		}
	}

	void SerializeUnicodeString(int32& iCode, FArchive& Ar)
	{
		if (bIsParsingText)
		{
			LastParsedString.Reset();

			do
			{
				uint16 UnicodeChar = 0;
#ifdef REQUIRES_ALIGNED_INT_ACCESS
				FMemory::Memcpy(&UnicodeChar, &Script[iCode], sizeof(uint16));
#else
				UnicodeChar = *((uint16*)(&Script[iCode]));
#endif
				LastParsedString += (TCHAR)UnicodeChar;

				iCode += sizeof(uint16);
			}
			while (Script[iCode-1] || Script[iCode-2]);
		}
		else
		{
			do
			{
				iCode += sizeof(uint16);
			}
			while (Script[iCode-1] || Script[iCode-2]);
		}
	}

	void SerializeText(int32& iCode, FArchive& Ar)
	{
		// What kind of text are we dealing with?
		const EBlueprintTextLiteralType TextLiteralType = (EBlueprintTextLiteralType)Script[iCode++];

		switch (TextLiteralType)
		{
		case EBlueprintTextLiteralType::Empty:
			// Don't need to gather empty text
			break;

		case EBlueprintTextLiteralType::LocalizedText:
			{
				bIsParsingText = true;

				SerializeExpr(iCode, Ar);
				const FString SourceString = MoveTemp(LastParsedString);

				SerializeExpr(iCode, Ar);
				const FString TextKey = MoveTemp(LastParsedString);

				SerializeExpr(iCode, Ar);
				const FString TextNamespace = MoveTemp(LastParsedString);

				bIsParsingText = false;

				const FText TextInstance = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *TextNamespace, *TextKey);

				PropertyLocalizationDataGatherer.GatherTextInstance(TextInstance, FString::Printf(TEXT("%s [Script Bytecode]"), SourceDescription), bTreatAsEditorOnlyData);
			}
			break;

		case EBlueprintTextLiteralType::InvariantText:
			// Don't need to gather invariant text, but we do need to walk over the string in the buffer
			SerializeExpr(iCode, Ar);
			break;

		case EBlueprintTextLiteralType::LiteralString:
			// Don't need to gather literal strings, but we do need to walk over the string in the buffer
			SerializeExpr(iCode, Ar);
			break;

		case EBlueprintTextLiteralType::StringTableEntry:
			// Don't need to gather string table entries, but we do need to walk over the strings in the buffer
			iCode += sizeof(ScriptPointerType); // String Table asset (if any)
			SerializeExpr(iCode, Ar);
			SerializeExpr(iCode, Ar);
			break;

		default:
			checkf(false, TEXT("Unknown EBlueprintTextLiteralType! Please update FGatherTextFromScriptBytecode::SerializeText to handle this type of text."));
			break;
		}
	}

	const TCHAR* SourceDescription;
	TArray<uint8>& Script;
	FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer;
	bool bTreatAsEditorOnlyData;

	FArchive DummyArchive;
	bool bIsParsingText;
	FString LastParsedString;
};

void FPropertyLocalizationDataGatherer::GatherScriptBytecode(const FString& PathToScript, const TArray<uint8>& ScriptData, const bool bIsEditorOnly)
{
	if (ScriptData.Num() > 0)
	{
		ResultFlags |= EPropertyLocalizationGathererResultFlags::HasScript;
	}

	FGatherTextFromScriptBytecode(*PathToScript, ScriptData, *this, bIsEditorOnly);
}

FPropertyLocalizationDataGatherer::FLocalizationDataGatheringCallbackMap& FPropertyLocalizationDataGatherer::GetTypeSpecificLocalizationDataGatheringCallbacks()
{
	static FLocalizationDataGatheringCallbackMap TypeSpecificLocalizationDataGatheringCallbacks;
	return TypeSpecificLocalizationDataGatheringCallbacks;
}
