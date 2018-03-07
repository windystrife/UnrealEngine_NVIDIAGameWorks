// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendUtils.h"
#include "Misc/App.h"
#include "UObject/StructOnScope.h"
#include "UObject/Interface.h"
#include "UObject/MetaData.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/SoftObjectPath.h"
#include "Components/ActorComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectHash.h"
#include "KismetCompiler.h"
#include "Misc/DefaultValueHelper.h"
#include "BlueprintCompilerCppBackend.h"
#include "Animation/AnimBlueprint.h"

extern ENGINE_API FString GetPathPostfix(const UObject* ForObject);

FString FEmitterLocalContext::GenerateUniqueLocalName()
{
	const FString UniqueNameBase = TEXT("__Local__");
	const FString UniqueName = FString::Printf(TEXT("%s%d"), *UniqueNameBase, LocalNameIndexMax);
	++LocalNameIndexMax;
	return UniqueName;
}

FString FEmitterLocalContext::FindGloballyMappedObject(const UObject* Object, const UClass* ExpectedClass, bool bLoadIfNotFound, bool bTryUsedAssetsList)
{
	if (const UBlueprint* BP = Cast<const UBlueprint>(Object))
	{
		// BP should never be wanted. BPGC should be loaded instead.
		if (!ExpectedClass || UClass::StaticClass()->IsChildOf(ExpectedClass))
		{
			Object = BP->GeneratedClass;
		}
	}

	UClass* ActualClass = Cast<UClass>(Dependencies.GetActualStruct());
	UClass* OriginalActualClass = Dependencies.FindOriginalClass(ActualClass);
	UClass* OuterClass = Object ? Cast<UClass>(Object->GetOuter()) : nullptr;	// SCS component templates will have an Outer that equates to their owning BPGC; since they're not currently DSOs, we have to special-case them.
	
	// The UsedAssets list is only applicable to UClass derivatives.
	bTryUsedAssetsList &= (ActualClass != nullptr);

	auto ClassString = [&]() -> FString
	{
		const UClass* ObjectClassToUse = ExpectedClass ? ExpectedClass : GetFirstNativeOrConvertedClass(Object->GetClass());
		ObjectClassToUse = (UUserDefinedEnum::StaticClass() == ObjectClassToUse) ? UEnum::StaticClass() : ObjectClassToUse;
		ObjectClassToUse = (UUserDefinedStruct::StaticClass() == ObjectClassToUse) ? UScriptStruct::StaticClass() : ObjectClassToUse;
		ObjectClassToUse = (!ExpectedClass && ObjectClassToUse && ObjectClassToUse->IsChildOf<UBlueprintGeneratedClass>()) ? UClass::StaticClass() : ObjectClassToUse;
		return FEmitHelper::GetCppName(ObjectClassToUse);
	};
	
	if (ActualClass && Object 
		&& (Object->IsIn(ActualClass) 
		|| Object->IsIn(ActualClass->GetDefaultObject(false))
		|| (OuterClass && ActualClass->IsChildOf(OuterClass)) ))
	{
		if (const FString* NamePtr = (CurrentCodeType == EGeneratedCodeType::SubobjectsOfClass) ? ClassSubobjectsMap.Find(Object) : nullptr)
		{
			return *NamePtr;
		}

		if (const FString* NamePtr = (CurrentCodeType == EGeneratedCodeType::CommonConstructor) ? CommonSubobjectsMap.Find(Object) : nullptr)
		{
			return *NamePtr;
		}

		int32 ObjectsCreatedPerClassIdx = MiscConvertedSubobjects.IndexOfByKey(Object);
		if ((INDEX_NONE == ObjectsCreatedPerClassIdx) && (CurrentCodeType != EGeneratedCodeType::SubobjectsOfClass))
		{
			ObjectsCreatedPerClassIdx = TemplateFromSubobjectsOfClass.IndexOfByKey(Object);
		}
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->%s[%d])")
				, *ClassString(), *FEmitHelper::GetCppName(ActualClass)
				, GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, MiscConvertedSubobjects)
				, ObjectsCreatedPerClassIdx);
		}

		ObjectsCreatedPerClassIdx = DynamicBindingObjects.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->%s[%d])")
				, *ClassString(), *FEmitHelper::GetCppName(ActualClass)
				, GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, DynamicBindingObjects)
				, ObjectsCreatedPerClassIdx);
		}

		ObjectsCreatedPerClassIdx = ComponentTemplates.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->%s[%d])")
				, *ClassString(), *FEmitHelper::GetCppName(ActualClass)
				, GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ComponentTemplates)
				, ObjectsCreatedPerClassIdx);
		}

		ObjectsCreatedPerClassIdx = Timelines.IndexOfByKey(Object);
		if (INDEX_NONE != ObjectsCreatedPerClassIdx)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->%s[%d])")
				, *ClassString(), *FEmitHelper::GetCppName(ActualClass)
				, GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, Timelines)
				, ObjectsCreatedPerClassIdx);
		}

		if ((CurrentCodeType == EGeneratedCodeType::SubobjectsOfClass) || (CurrentCodeType == EGeneratedCodeType::CommonConstructor))
		{
			if ((Object == ActualClass->GetDefaultObject(false)) || (Object == OriginalActualClass->GetDefaultObject(false)))
			{
				return TEXT("this");
			}
		}
	}

	auto CastCustomClass = [&](const FString& InResult)->FString
	{
		if (ExpectedClass && !UClass::StaticClass()->IsChildOf(ExpectedClass))
		{
			return FString::Printf(TEXT("Cast<%s>(%s)"), *ClassString(), *InResult);
		}
		return InResult;
	};

	const TCHAR* DynamicClassParam = TEXT("InDynamicClass");
	if (ActualClass && ((Object == ActualClass) || (Object == OriginalActualClass)))
	{
		return CastCustomClass(((CurrentCodeType == EGeneratedCodeType::SubobjectsOfClass) ? DynamicClassParam : TEXT("GetClass()")));
	}

	{
		auto Field = Cast<UField>(Object);
		auto FieldOwnerStruct = Field ? Field->GetOwnerStruct() : nullptr;
		if (FieldOwnerStruct && (Field != FieldOwnerStruct))
		{
			ensure(Field == FindField<UField>(FieldOwnerStruct, Field->GetFName()));
			const FString MappedOwner = FindGloballyMappedObject(FieldOwnerStruct, UStruct::StaticClass(), bLoadIfNotFound, bTryUsedAssetsList);
			if (!MappedOwner.IsEmpty() && ensure(MappedOwner != TEXT("nullptr")))
			{
				if ((Field->GetClass() == UStructProperty::StaticClass()) && (MappedOwner == DynamicClassParam))
				{
					// Non-template version to reduce size
					return FString::Printf(TEXT("%s->FindStructPropertyChecked(TEXT(\"%s\"))")
						, *MappedOwner, *Field->GetName());
				}

				return FString::Printf(TEXT("FindFieldChecked<%s>(%s, TEXT(\"%s\"))")
					, *FEmitHelper::GetCppName(Field->GetClass())
					, *MappedOwner
					, *Field->GetName());
			}
		}
	}

	if (auto ObjClass = Cast<UClass>(Object))
	{
		auto BPGC = Cast<UBlueprintGeneratedClass>(ObjClass);
		if (ObjClass->HasAnyClassFlags(CLASS_Native) || (BPGC && Dependencies.WillClassBeConverted(BPGC)))
		{
			return CastCustomClass(FString::Printf(TEXT("%s::StaticClass()"), *FEmitHelper::GetCppName(ObjClass, true)));
		}
	}

	// TODO Handle native structires, and special cases..

	if (auto ScriptStruct = Cast<UScriptStruct>(Object))
	{
		if (ScriptStruct->StructFlags & STRUCT_NoExport)
		{
			return FStructAccessHelper::EmitStructAccessCode(ScriptStruct);
		}
		else
		{
			// Check if  
			// TODO: check if supported 
			return FString::Printf(TEXT("%s::StaticStruct()"), *FEmitHelper::GetCppName(ScriptStruct));
		}
	}

	if (const UUserDefinedEnum* UDE = Cast<UUserDefinedEnum>(Object))
	{
		const int32 EnumIndex = EnumsInCurrentClass.IndexOfByKey(UDE);
		if (INDEX_NONE != EnumIndex)
		{
			return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->%s[%d])")
				, *ClassString()
				, *FEmitHelper::GetCppName(ActualClass)
				, GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields)
				, EnumIndex);
		}
	}

	// TODO: handle subobjects
	ensure(!bLoadIfNotFound || Object);
	if (Object && (bLoadIfNotFound || bTryUsedAssetsList))
	{
		if (bTryUsedAssetsList)
		{
			int32 AssetIndex = UsedObjectInCurrentClass.IndexOfByKey(Object);
			if (INDEX_NONE == AssetIndex && Dependencies.Assets.Contains(Object))
			{
				AssetIndex = UsedObjectInCurrentClass.Add(Object);
			}

			if (INDEX_NONE != AssetIndex)
			{
				return FString::Printf(TEXT("CastChecked<%s>(CastChecked<UDynamicClass>(%s::StaticClass())->%s[%d], ECastCheckedType::NullAllowed)")
					, *ClassString()
					, *FEmitHelper::GetCppName(ActualClass)
					, GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, UsedAssets)
					, AssetIndex);
			}
		}

		if (bLoadIfNotFound)
		{
			return FString::Printf(TEXT("LoadObject<%s>(nullptr, TEXT(\"%s\"))")
				, *ClassString()
				, *(Object->GetPathName().ReplaceCharWithEscapedChar()));
		}
	}

	return FString{};
}

FString FEmitterLocalContext::ExportTextItem(const UProperty* Property, const void* PropertyValue) const
{
	const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
		| EPropertyExportCPPFlags::CPPF_NoConst
		| EPropertyExportCPPFlags::CPPF_NoRef
		| EPropertyExportCPPFlags::CPPF_NoStaticArray
		| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
	if (auto ArrayProperty = Cast<const UArrayProperty>(Property))
	{
		const FString ConstPrefix = Property->HasMetaData(TEXT("NativeConstTemplateArg")) ? TEXT("const ") : TEXT("");
		const FString TypeText = ExportCppDeclaration(ArrayProperty, EExportedDeclaration::Parameter, LocalExportCPPFlags, EPropertyNameInDeclaration::Skip, FString(), ConstPrefix);
		return FString::Printf(TEXT("%s()"), *TypeText);
	}
	FString ValueStr;
	Property->ExportTextItem(ValueStr, PropertyValue, PropertyValue, nullptr, EPropertyPortFlags::PPF_ExportCpp);
	if (Property->IsA<UIntProperty>())
	{
		int32 Value = *(int32*)PropertyValue;
		if (Value == (-2147483647 - 1)) // END OF RANGE
		{
			ValueStr = TEXT("(-2147483647 - 1)");
		}
	}
	if (Property->IsA<USoftObjectProperty>())
	{
		const FString TypeText = ExportCppDeclaration(Property, EExportedDeclaration::Parameter, LocalExportCPPFlags, EPropertyNameInDeclaration::Skip);
		return FString::Printf(TEXT("%s(%s)"), *TypeText, *ValueStr);
	}
	return ValueStr;
}

FString FEmitterLocalContext::ExportCppDeclaration(const UProperty* Property, EExportedDeclaration::Type DeclarationType, uint32 InExportCPPFlags, FEmitterLocalContext::EPropertyNameInDeclaration ParameterName, const FString& NamePostfix, const FString& TypePrefix) const
{
	FString ActualCppType;
	FString* ActualCppTypePtr = nullptr;
	FString ActualExtendedType;
	FString* ActualExtendedTypePtr = nullptr;
	uint32 ExportCPPFlags = InExportCPPFlags;

	auto GetActualNameCPP = [&](const UObjectPropertyBase* ObjectPropertyBase, UClass* InActualClass)
	{
		auto BPGC = Cast<UBlueprintGeneratedClass>(InActualClass);
		if (BPGC || !TypePrefix.IsEmpty())
		{
			const bool bIsParameter = (DeclarationType == EExportedDeclaration::Parameter) || (DeclarationType == EExportedDeclaration::MacroParameter);
			const uint32 LocalExportCPPFlags = ExportCPPFlags | (bIsParameter ? CPPF_ArgumentOrReturnValue : 0);
			UClass* NativeType = GetFirstNativeOrConvertedClass(InActualClass);
			check(NativeType);
			ActualCppType = TypePrefix + ObjectPropertyBase->GetCPPTypeCustom(&ActualExtendedType, LocalExportCPPFlags, FEmitHelper::GetCppName(NativeType));
			ActualCppTypePtr = &ActualCppType;
			if (!ActualExtendedType.IsEmpty())
			{
				ActualExtendedTypePtr = &ActualExtendedType;
			}
		}
	};

	const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property);
	if (ArrayProperty)
	{
		Property = ArrayProperty->Inner;
		ExportCPPFlags = (ExportCPPFlags & ~CPPF_ArgumentOrReturnValue);
	}

	if (const UClassProperty* ClassProperty = Cast<const UClassProperty>(Property))
	{
		GetActualNameCPP(ClassProperty, ClassProperty->MetaClass);
	}
	else if (auto SoftClassProperty = Cast<const USoftClassProperty>(Property))
	{
		GetActualNameCPP(SoftClassProperty, SoftClassProperty->MetaClass);
	}
	else if (auto ObjectProperty = Cast<const UObjectPropertyBase>(Property))
	{
		GetActualNameCPP(ObjectProperty, ObjectProperty->PropertyClass);
	}
	else if (auto StructProperty = Cast<const UStructProperty>(Property))
	{
		ActualCppType = FEmitHelper::GetCppName(StructProperty->Struct, false);
		ActualCppTypePtr = &ActualCppType;
	}
	else if (auto SCDelegateProperty = Cast<const UDelegateProperty>(Property))
	{
		const FString* SCDelegateTypeName = MCDelegateSignatureToSCDelegateType.Find(SCDelegateProperty->SignatureFunction);
		if (SCDelegateTypeName)
		{
			ActualCppType = *SCDelegateTypeName;
			ActualCppTypePtr = &ActualCppType;
		}
	}

	// TODO: TypePrefix for other properties

	if (ArrayProperty)
	{
		Property = ArrayProperty;
		if (ActualCppTypePtr)
		{
			const FString LocalActualCppType = ActualCppType;
			ActualCppType.Empty();
			const FString LocalActualExtendedType = ActualExtendedType;
			ActualExtendedType.Empty();
			ActualExtendedTypePtr = nullptr;

			const bool bIsParameter = (DeclarationType == EExportedDeclaration::Parameter) || (DeclarationType == EExportedDeclaration::MacroParameter);
			const uint32 LocalExportCPPFlags = InExportCPPFlags | (bIsParameter ? CPPF_ArgumentOrReturnValue : 0);

			ActualCppType = ArrayProperty->GetCPPTypeCustom(&ActualExtendedType, LocalExportCPPFlags, LocalActualCppType, LocalActualExtendedType);
			if (!ActualExtendedType.IsEmpty())
			{
				ActualExtendedTypePtr = &ActualExtendedType;
			}
		}
	}

	FStringOutputDevice Out;
	const bool bSkipParameterName = (ParameterName == EPropertyNameInDeclaration::Skip);
	const FString ActualNativeName = bSkipParameterName ? FString() : (FEmitHelper::GetCppName(Property, false, ParameterName == EPropertyNameInDeclaration::ForceConverted) + NamePostfix);
	Property->ExportCppDeclaration(Out, DeclarationType, nullptr, ExportCPPFlags, bSkipParameterName, ActualCppTypePtr, ActualExtendedTypePtr, &ActualNativeName);
	return FString(Out);

}

void FEmitterLocalContext::MarkUnconvertedClassAsNecessary(UField* InField)
{
	UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(InField);
	UBlueprint* BP = (BPGC && !Dependencies.WillClassBeConverted(BPGC)) ? Cast<UBlueprint>(BPGC->ClassGeneratedBy) : nullptr;
	if (ensure(BP))
	{
		IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		BackEndModule.OnIncludingUnconvertedBP().ExecuteIfBound(BP, NativizationOptions);
		UsedUnconvertedWrapper.Add(InField);
	}
}


FString FEmitHelper::GetCppName(const UField* Field, bool bUInterface, bool bForceParameterNameModification)
{
	check(Field);
	auto AsClass = Cast<UClass>(Field);
	auto AsScriptStruct = Cast<UScriptStruct>(Field);
	if (AsClass || AsScriptStruct)
	{
		if (AsClass && AsClass->HasAnyClassFlags(CLASS_Interface))
		{
			ensure(AsClass->IsChildOf<UInterface>());
			return FString::Printf(TEXT("%s%s")
				, bUInterface ? TEXT("U") : TEXT("I")
				, *AsClass->GetName());
		}
		auto AsStruct = CastChecked<UStruct>(Field);
		if (AsStruct->IsNative())
		{
			return FString::Printf(TEXT("%s%s"), AsStruct->GetPrefixCPP(), *AsStruct->GetName());
		}
		else
		{
			return ::UnicodeToCPPIdentifier(*AsStruct->GetName(), false, AsStruct->GetPrefixCPP()) + GetPathPostfix(AsStruct);
		}
	}
	else if (auto AsProperty = Cast<UProperty>(Field))
	{
		const UStruct* Owner = AsProperty->GetOwnerStruct();
		const bool bModifyName = ensure(Owner)
			&& (Cast<UBlueprintGeneratedClass>(Owner) || !Owner->IsNative() || bForceParameterNameModification);
		if (bModifyName)
		{
			FString VarPrefix;

			const bool bIsUberGraphVariable = Owner->IsA<UBlueprintGeneratedClass>() && AsProperty->HasAllPropertyFlags(CPF_Transient | CPF_DuplicateTransient);
			const bool bIsParameter = AsProperty->HasAnyPropertyFlags(CPF_Parm);
			const bool bFunctionLocalVariable = Owner->IsA<UFunction>();
			if (bIsUberGraphVariable)
			{
				int32 InheritenceLevel = GetInheritenceLevel(Owner);
				VarPrefix = FString::Printf(TEXT("b%dl__"), InheritenceLevel);
			}
			else if (bIsParameter)
			{
				VarPrefix = TEXT("bpp__");
			}
			else if (bFunctionLocalVariable)
			{
				VarPrefix = TEXT("bpfv__");
			}
			else
			{
				VarPrefix = TEXT("bpv__");
			}
			return ::UnicodeToCPPIdentifier(AsProperty->GetName(), AsProperty->HasAnyPropertyFlags(CPF_Deprecated), *VarPrefix);
		}
		return AsProperty->GetNameCPP();
	}

	if (Field->IsA<UUserDefinedEnum>())
	{
		return ::UnicodeToCPPIdentifier(Field->GetName(), false, TEXT("E__"));
	}

	if (!Field->IsNative())
	{
		return ::UnicodeToCPPIdentifier(Field->GetName(), false, TEXT("bpf__"));
	}
	return Field->GetName();
}

int32 FEmitHelper::GetInheritenceLevel(const UStruct* Struct)
{
	const UStruct* StructIt = Struct ? Struct->GetSuperStruct() : nullptr;
	int32 InheritenceLevel = 0;
	while ((StructIt != nullptr) && !StructIt->IsNative())
	{
		++InheritenceLevel;
		StructIt = StructIt->GetSuperStruct();
	}
	return InheritenceLevel;
}

bool FEmitHelper::PropertyForConstCast(const UProperty* Property)
{
	return Property && (Property->HasAnyPropertyFlags(CPF_ConstParm) 
			|| (Property->PassCPPArgsByRef() && !Property->HasAnyPropertyFlags(CPF_OutParm))); // See implementation in UProperty::ExportCppDeclaration
}

void FEmitHelper::ArrayToString(const TArray<FString>& Array, FString& OutString, const TCHAR* Separator)
{
	if (Array.Num())
	{
		OutString += Array[0];
	}
	for (int32 i = 1; i < Array.Num(); ++i)
	{
		OutString += Separator;
		OutString += Array[i];
	}
}

bool FEmitHelper::HasAllFlags(uint64 Flags, uint64 FlagsToCheck)
{
	return FlagsToCheck == (Flags & FlagsToCheck);
}

FString FEmitHelper::HandleRepNotifyFunc(const UProperty* Property)
{
	check(Property);
	if (HasAllFlags(Property->PropertyFlags, CPF_Net | CPF_RepNotify))
	{
		if (Property->RepNotifyFunc != NAME_None)
		{
			return FString::Printf(TEXT("ReplicatedUsing=\"%s\""), *Property->RepNotifyFunc.ToString());
		}
		else
		{
			UE_LOG(LogK2Compiler, Warning, TEXT("Invalid RepNotifyFunc in %s"), *GetPathNameSafe(Property));
		}
	}

	if (HasAllFlags(Property->PropertyFlags, CPF_Net))
	{
		return TEXT("Replicated");
	}
	return FString();
}

bool FEmitHelper::IsMetaDataValid(const FName Name, const FString& Value)
{
	static const FName UIMin(TEXT("UIMin"));
	static const FName UIMax(TEXT("UIMax"));
	static const FName ClampMin(TEXT("ClampMin"));
	static const FName ClampMax(TEXT("ClampMax"));
	if ((Name == UIMin)
		|| (Name == UIMax)
		|| (Name == ClampMin)
		|| (Name == ClampMax))
	{
		// those MD require no warning
		return Value.IsNumeric();
	}
	return true;
}

bool FEmitHelper::MetaDataCanBeNative(const FName MetaDataName, const UField* Field)
{
	if (MetaDataName == TEXT("ModuleRelativePath"))
	{
		return false;
	}
	if (MetaDataName == TEXT("MakeStructureDefaultValue")) // can be too long
	{
		return false;
	}
	if (const UFunction* Function = Cast<const UFunction>(Field))
	{
		const UProperty* Param = Function->FindPropertyByName(MetaDataName);
		if (Param && Param->HasAnyPropertyFlags(CPF_Parm))
		{
			return false;
		}
	}
	return true;
}

FString FEmitHelper::HandleMetaData(const UField* Field, bool AddCategory, const TArray<FString>* AdditinalMetaData)
{
	FString MetaDataStr;

	UPackage* Package = Field ? Field->GetOutermost() : nullptr;
	const UMetaData* MetaData = Package ? Package->GetMetaData() : nullptr;
	const TMap<FName, FString>* ValuesMap = MetaData ? MetaData->ObjectMetaDataMap.Find(Field) : nullptr;
	TArray<FString> MetaDataStrings;
	if (ValuesMap && ValuesMap->Num())
	{
		for (auto& Pair : *ValuesMap)
		{
			FName CurrentKey = Pair.Key;
			FName NewKey = UMetaData::GetRemappedKeyName(CurrentKey);

			if (NewKey != NAME_None)
			{
				CurrentKey = NewKey;
			}

			if (!MetaDataCanBeNative(CurrentKey, Field) || !IsMetaDataValid(CurrentKey, Pair.Value))
			{
				continue;
			}
			if (!Pair.Value.IsEmpty())
			{
				FString Value = Pair.Value.Replace(TEXT("\n"), TEXT("")).ReplaceCharWithEscapedChar();
				MetaDataStrings.Emplace(FString::Printf(TEXT("%s=\"%s\""), *CurrentKey.ToString(), *Value));
			}
			else
			{
				MetaDataStrings.Emplace(CurrentKey.ToString());
			}
		}
	}
	if (AddCategory && (!ValuesMap || !ValuesMap->Find(TEXT("Category"))))
	{
		MetaDataStrings.Emplace(TEXT("Category"));
	}
	if (AdditinalMetaData)
	{
		MetaDataStrings.Append(*AdditinalMetaData);
	}
	if (Field)
	{
		MetaDataStrings.Emplace(FString::Printf(TEXT("OverrideNativeName=\"%s\""), *Field->GetName().ReplaceCharWithEscapedChar()));
	}
	MetaDataStrings.Remove(FString());
	if (MetaDataStrings.Num())
	{
		MetaDataStr += TEXT("meta=(");
		ArrayToString(MetaDataStrings, MetaDataStr, TEXT(", "));
		MetaDataStr += TEXT(")");
	}
	return MetaDataStr;
}

#ifdef HANDLE_CPF_TAG
	static_assert(false, "Macro HANDLE_CPF_TAG redefinition.");
#endif
#define HANDLE_CPF_TAG(TagName, CheckedFlags) if (HasAllFlags(Flags, (CheckedFlags))) { Tags.Emplace(TagName); }

TArray<FString> FEmitHelper::ProperyFlagsToTags(uint64 Flags, bool bIsClassProperty)
{
	TArray<FString> Tags;

	// EDIT FLAGS
	if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst | CPF_DisableEditOnInstance)))
	{
		Tags.Emplace(TEXT("VisibleDefaultsOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst | CPF_DisableEditOnTemplate)))
	{
		Tags.Emplace(TEXT("VisibleInstanceOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst)))
	{
		Tags.Emplace(TEXT("VisibleAnywhere"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_DisableEditOnInstance)))
	{
		Tags.Emplace(TEXT("EditDefaultsOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit | CPF_DisableEditOnTemplate)))
	{
		Tags.Emplace(TEXT("EditInstanceOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_Edit)))
	{
		Tags.Emplace(TEXT("EditAnywhere"));
	}

	// BLUEPRINT EDIT
	if (HasAllFlags(Flags, (CPF_BlueprintVisible | CPF_BlueprintReadOnly)))
	{
		Tags.Emplace(TEXT("BlueprintReadOnly"));
	}
	else if (HasAllFlags(Flags, (CPF_BlueprintVisible)))
	{
		Tags.Emplace(TEXT("BlueprintReadWrite"));
	}

	// CONFIG
	if (HasAllFlags(Flags, (CPF_GlobalConfig | CPF_Config)))
	{
		Tags.Emplace(TEXT("GlobalConfig"));
	}
	else if (HasAllFlags(Flags, (CPF_Config)))
	{
		Tags.Emplace(TEXT("Config"));
	}

	// OTHER
	HANDLE_CPF_TAG(TEXT("Transient"), CPF_Transient)
	HANDLE_CPF_TAG(TEXT("DuplicateTransient"), CPF_DuplicateTransient)
	HANDLE_CPF_TAG(TEXT("TextExportTransient"), CPF_TextExportTransient)
	HANDLE_CPF_TAG(TEXT("NonPIEDuplicateTransient"), CPF_NonPIEDuplicateTransient)
	HANDLE_CPF_TAG(TEXT("Export"), CPF_ExportObject)
	HANDLE_CPF_TAG(TEXT("NoClear"), CPF_NoClear)
	HANDLE_CPF_TAG(TEXT("EditFixedSize"), CPF_EditFixedSize)
	if (!bIsClassProperty)
	{
		HANDLE_CPF_TAG(TEXT("NotReplicated"), CPF_RepSkip)
	}

	HANDLE_CPF_TAG(TEXT("Interp"), CPF_Edit | CPF_BlueprintVisible | CPF_Interp)
	HANDLE_CPF_TAG(TEXT("NonTransactional"), CPF_NonTransactional)
	HANDLE_CPF_TAG(TEXT("BlueprintAssignable"), CPF_BlueprintAssignable)
	HANDLE_CPF_TAG(TEXT("BlueprintCallable"), CPF_BlueprintCallable)
	HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), CPF_BlueprintAuthorityOnly)
	HANDLE_CPF_TAG(TEXT("AssetRegistrySearchable"), CPF_AssetRegistrySearchable)
	HANDLE_CPF_TAG(TEXT("SimpleDisplay"), CPF_SimpleDisplay)
	HANDLE_CPF_TAG(TEXT("AdvancedDisplay"), CPF_AdvancedDisplay)
	HANDLE_CPF_TAG(TEXT("SaveGame"), CPF_SaveGame)

	//TODO:
	//HANDLE_CPF_TAG(TEXT("Instanced"), CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference)

	return Tags;
}

TArray<FString> FEmitHelper::FunctionFlagsToTags(uint64 Flags)
{
	TArray<FString> Tags;

	//Pointless: BlueprintNativeEvent, BlueprintImplementableEvent
	//Pointless: CustomThunk
	//Pointless: ServiceRequest, ServiceResponse - only useful for native UFunctions, they're for serializing to json
	//Pointless: SealedEvent

	HANDLE_CPF_TAG(TEXT("Exec"), FUNC_Exec)
	HANDLE_CPF_TAG(TEXT("Server"), FUNC_Net | FUNC_NetServer)
	HANDLE_CPF_TAG(TEXT("Client"), FUNC_Net | FUNC_NetClient)
	HANDLE_CPF_TAG(TEXT("NetMulticast"), FUNC_Net | FUNC_NetMulticast)
	HANDLE_CPF_TAG(TEXT("Reliable"), FUNC_NetReliable)
	HANDLE_CPF_TAG(TEXT("BlueprintCallable"), FUNC_BlueprintCallable)
	HANDLE_CPF_TAG(TEXT("BlueprintPure"), FUNC_BlueprintCallable | FUNC_BlueprintPure)
	HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), FUNC_BlueprintAuthorityOnly)
	HANDLE_CPF_TAG(TEXT("BlueprintCosmetic"), FUNC_BlueprintCosmetic)
	HANDLE_CPF_TAG(TEXT("WithValidation"), FUNC_NetValidate)

	if (HasAllFlags(Flags, FUNC_Net) && !HasAllFlags(Flags, FUNC_NetReliable))
	{
		Tags.Emplace(TEXT("Unreliable"));
	}

	return Tags;
}

#undef HANDLE_CPF_TAG

bool FEmitHelper::IsBlueprintNativeEvent(uint64 FunctionFlags)
{
	return HasAllFlags(FunctionFlags, FUNC_Event | FUNC_BlueprintEvent | FUNC_Native);
}

bool FEmitHelper::IsBlueprintImplementableEvent(uint64 FunctionFlags)
{
	return HasAllFlags(FunctionFlags, FUNC_Event | FUNC_BlueprintEvent) && !HasAllFlags(FunctionFlags, FUNC_Native);
}

FString FEmitHelper::GenerateReplaceConvertedMD(UObject* Obj)
{
	FString Result;
	if (Obj)
	{
		Result = TEXT("ReplaceConverted=\"");

		// 1. Current object
		Result += Obj->GetPathName();

		// 2. Loaded Redirectors
		{
			struct FLocalHelper
			{
				static UObject* FindFinalObject(UObjectRedirector* Redirector)
				{
					auto DestObj = Redirector ? Redirector->DestinationObject : nullptr;
					auto InnerRedir = Cast<UObjectRedirector>(DestObj);
					return InnerRedir ? FindFinalObject(InnerRedir) : DestObj;
				}
			};

			TArray<UObject*> AllObjects;
			GetObjectsOfClass(UObjectRedirector::StaticClass(), AllObjects);
			for (auto LocalObj : AllObjects)
			{
				auto Redirector = CastChecked<UObjectRedirector>(LocalObj);
				if (Obj == FLocalHelper::FindFinalObject(Redirector))
				{
					Result += TEXT(",");
					Result += Redirector->GetPathName();
				}
			}
		}

		// 3. Unloaded Redirectors
		// TODO: It would be better to load all redirectors before compiling. Than checking here AssetRegistry.

		Result += TEXT("\"");

		// 4. Add overridden name:
		Result += TEXT(", OverrideNativeName=\"");
		Result += Obj->GetName();
		Result += TEXT("\"");
		
		if(const UEnum* Enum = Cast<const UEnum>(Obj))
		{
			Result += FString::Printf(TEXT(", EnumDisplayNameFn=\"%s__GetUserFriendlyName\""), *FEmitHelper::GetCppName(Enum));
		}
	}

	return Result;
}

FString FEmitHelper::GetBaseFilename(const UObject* AssetObj, const FCompilerNativizationOptions& NativizationOptions)
{
	FString AssetName = FPackageName::GetLongPackageAssetName(AssetObj->GetOutermost()->GetPathName());
	// We have to sanitize the package path because UHT is going to generate header guards (preprocessor symbols)
	// based on the filename. I'm also not interested in exploring the depth of unicode filename support in UHT,
	// UBT, and our various c++ toolchains, so this logic is pretty aggressive:
	FString Postfix = TEXT("__pf");
	for (auto& Char : AssetName)
	{
		if (!IsValidCPPIdentifierChar(Char))
		{
			// deterministically map char to a valid ascii character, we have 63 characters available (aA-zZ, 0-9, and _)
			// so the optimal encoding would be base 63:
			Postfix.Append(ToValidCPPIdentifierChars(Char));
			Char = TCHAR('x');
		}
	}
	Postfix += GetPathPostfix(AssetObj);
	return AssetName + Postfix;
}

FString FEmitHelper::GetPCHFilename()
{
	FString PCHFilename;
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();

	auto& PchFilenameQuery = BackEndModule.OnPCHFilenameQuery();
	if (PchFilenameQuery.IsBound())
	{
		PCHFilename = PchFilenameQuery.Execute();
	}

	return PCHFilename;
}

FString FEmitHelper::GetGameMainHeaderFilename()
{
	return FString::Printf(TEXT("%s.h"), FApp::GetProjectName());
}

FString FEmitHelper::EmitUFuntion(UFunction* Function, const TArray<FString>& AdditionalTags, const TArray<FString>& AdditinalMetaData)
{
	TArray<FString> Tags = FEmitHelper::FunctionFlagsToTags(Function->FunctionFlags);

	Tags.Append(AdditionalTags);
	const bool bMustHaveCategory = (Function->FunctionFlags & (FUNC_BlueprintCallable | FUNC_BlueprintPure)) != 0;
	Tags.Emplace(FEmitHelper::HandleMetaData(Function, bMustHaveCategory, &AdditinalMetaData));
	Tags.Remove(FString());

	FString AllTags;
	FEmitHelper::ArrayToString(Tags, AllTags, TEXT(", "));

	return FString::Printf(TEXT("UFUNCTION(%s)"), *AllTags);
}

int32 FEmitHelper::ParseDelegateDetails(FEmitterLocalContext& EmitterContext, UFunction* Signature, FString& OutParametersMacro, FString& OutParamNumberStr)
{
	int32 ParameterNum = 0;
	FStringOutputDevice Parameters;
	for (TFieldIterator<UProperty> PropIt(Signature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		Parameters += ", ";
		Parameters += EmitterContext.ExportCppDeclaration(*PropIt
			, EExportedDeclaration::MacroParameter
			, EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend
			, FEmitterLocalContext::EPropertyNameInDeclaration::ForceConverted);
		++ParameterNum;
	}

	FString ParamNumberStr;
	switch (ParameterNum)
	{
	case 0: ParamNumberStr = TEXT("");				break;
	case 1: ParamNumberStr = TEXT("_OneParam");		break;
	case 2: ParamNumberStr = TEXT("_TwoParams");	break;
	case 3: ParamNumberStr = TEXT("_ThreeParams");	break;
	case 4: ParamNumberStr = TEXT("_FourParams");	break;
	case 5: ParamNumberStr = TEXT("_FiveParams");	break;
	case 6: ParamNumberStr = TEXT("_SixParams");	break;
	case 7: ParamNumberStr = TEXT("_SevenParams");	break;
	case 8: ParamNumberStr = TEXT("_EightParams");	break;
	case 9: ParamNumberStr = TEXT("_NineParams");   break;
	default: ParamNumberStr = TEXT("_TooMany");		break;
	}

	OutParametersMacro = Parameters;
	OutParamNumberStr = ParamNumberStr;
	return ParameterNum;
}

void FEmitHelper::EmitSinglecastDelegateDeclarations_Inner(FEmitterLocalContext& EmitterContext, UFunction* Signature, const FString& TypeName)
{
	check(Signature);
	FString ParamNumberStr, Parameters;
	ParseDelegateDetails(EmitterContext, Signature, Parameters, ParamNumberStr);
	EmitterContext.Header.AddLine(FString::Printf(TEXT("UDELEGATE(%s)"), *FEmitHelper::HandleMetaData(Signature, false)));
	EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_DELEGATE%s(%s%s);"), *ParamNumberStr, *TypeName, *Parameters));
}

void FEmitHelper::EmitSinglecastDelegateDeclarations(FEmitterLocalContext& EmitterContext, const TArray<UDelegateProperty*>& Delegates)
{
	for (auto It : Delegates)
	{
		check(It);
		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
		const FString TypeName = EmitterContext.ExportCppDeclaration(It, EExportedDeclaration::Parameter, LocalExportCPPFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);
		EmitSinglecastDelegateDeclarations_Inner(EmitterContext, It->SignatureFunction, TypeName);
	}
}

void FEmitHelper::EmitMulticastDelegateDeclarations(FEmitterLocalContext& EmitterContext)
{
	for (TFieldIterator<UMulticastDelegateProperty> It(EmitterContext.GetCurrentlyGeneratedClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		auto Signature = It->SignatureFunction;
		check(Signature);

		FString ParamNumberStr, Parameters;
		ParseDelegateDetails(EmitterContext, Signature, Parameters, ParamNumberStr);

		const uint32 LocalExportCPPFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
			| EPropertyExportCPPFlags::CPPF_NoConst
			| EPropertyExportCPPFlags::CPPF_NoRef
			| EPropertyExportCPPFlags::CPPF_NoStaticArray
			| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
		EmitterContext.Header.AddLine(FString::Printf(TEXT("UDELEGATE(%s)"), *FEmitHelper::HandleMetaData(Signature, false)));
		const FString TypeName = EmitterContext.ExportCppDeclaration(*It, EExportedDeclaration::Parameter, LocalExportCPPFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);
		EmitterContext.Header.AddLine(FString::Printf(TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE%s(%s%s);"), *ParamNumberStr, *TypeName, *Parameters));
	}
}

void FEmitHelper::EmitLifetimeReplicatedPropsImpl(FEmitterLocalContext& EmitterContext)
{
	auto SourceClass = EmitterContext.GetCurrentlyGeneratedClass();
	auto CppClassName = FEmitHelper::GetCppName(SourceClass);
	bool bFunctionInitilzed = false;
	for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if ((It->PropertyFlags & CPF_Net) != 0)
		{
			if (!bFunctionInitilzed)
			{
				EmitterContext.AddLine(FString::Printf(TEXT("void %s::%s(TArray< FLifetimeProperty > & OutLifetimeProps) const"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UActorComponent, GetLifetimeReplicatedProps)));
				EmitterContext.AddLine(TEXT("{"));
				EmitterContext.IncreaseIndent();
				EmitterContext.AddLine(FString::Printf(TEXT("Super::%s(OutLifetimeProps);"), GET_FUNCTION_NAME_STRING_CHECKED(UActorComponent, GetLifetimeReplicatedProps)));
				bFunctionInitilzed = true;
			}
			EmitterContext.AddLine(FString::Printf(TEXT("DOREPLIFETIME_DIFFNAMES(%s, %s, FName(TEXT(\"%s\")));"), *CppClassName, *FEmitHelper::GetCppName(*It), *(It->GetName())));
		}
	}
	if (bFunctionInitilzed)
	{
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));
	}
}

FString FEmitHelper::FloatToString(float Value)
{
	if (FMath::IsNaN(Value))
	{
		UE_LOG(LogK2Compiler, Warning, TEXT("A NotANNumber value cannot be nativized. It is changed into 0.0f."));
		return TEXT("/*The original value was NaN!*/ 0.0f");
	}
	/*
	if (TNumericLimits<float>::Lowest() == Value)
	{
		return TEXT("TNumericLimits<float>::Lowest()");
	}
	if (TNumericLimits<float>::Min() == Value)
	{
		return TEXT("TNumericLimits<float>::Min()");
	}
	if (TNumericLimits<float>::Max() == Value)
	{
		return TEXT("TNumericLimits<float>::Max()");
	}
	*/
	return FString::Printf(TEXT("%f"), Value);
}

FString FEmitHelper::LiteralTerm(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& Type, const FString& CustomValue, UObject* LiteralObject, const FText* OptionalTextLiteral)
{
	auto Schema = GetDefault<UEdGraphSchema_K2>();

	if (UEdGraphSchema_K2::PC_String == Type.PinCategory)
	{
		return FString::Printf(TEXT("FString(%s)"), *UStrProperty::ExportCppHardcodedText(CustomValue, EmitterContext.DefaultTarget->Indent));
	}
	else if (UEdGraphSchema_K2::PC_Text == Type.PinCategory)
	{
		ensure(OptionalTextLiteral);
		if (OptionalTextLiteral)
		{
			return UTextProperty::GenerateCppCodeForTextValue(*OptionalTextLiteral, FString());
		}
		return FString::Printf(TEXT("FText::FromString(%s)"), *UStrProperty::ExportCppHardcodedText(CustomValue, EmitterContext.DefaultTarget->Indent));
	}
	else if (UEdGraphSchema_K2::PC_Float == Type.PinCategory)
	{
		float Value = CustomValue.IsEmpty() ? 0.0f : FCString::Atof(*CustomValue);
		return FString::Printf(TEXT("%s"), *FloatToString(Value));
	}
	else if (UEdGraphSchema_K2::PC_Int == Type.PinCategory)
	{
		int32 Value = CustomValue.IsEmpty() ? 0 : FCString::Atoi(*CustomValue);
		return FString::Printf(TEXT("%d"), Value);
	}
	else if ((UEdGraphSchema_K2::PC_Byte == Type.PinCategory) || (UEdGraphSchema_K2::PC_Enum == Type.PinCategory))
	{
		auto TypeEnum = Cast<UEnum>(Type.PinSubCategoryObject.Get());
		if (TypeEnum)
		{
			// @note: We have to default to the zeroth entry because there may not be a symbol associated with the 
			// last element (UHT generates a MAX entry for UEnums, even if the user did not declare them, but UHT 
			// does not generate a symbol for said entry.
			if (CustomValue.Contains(TEXT("::")))
			{
				return CustomValue;
			}
			return FString::Printf(TEXT("%s::%s"), *FEmitHelper::GetCppName(TypeEnum)
				, CustomValue.IsEmpty() ? *TypeEnum->GetNameStringByIndex(0) : *CustomValue);
		}
		else
		{
			uint8 Value = CustomValue.IsEmpty() ? 0 : FCString::Atoi(*CustomValue);
			return FString::Printf(TEXT("%u"), Value);
		}
	}
	else if (UEdGraphSchema_K2::PC_Boolean == Type.PinCategory)
	{
		const bool bValue = CustomValue.ToBool();
		return bValue ? TEXT("true") : TEXT("false");
	}
	else if (UEdGraphSchema_K2::PC_Name == Type.PinCategory)
	{
		return CustomValue.IsEmpty() 
			? TEXT("FName()") 
			: FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *(FName(*CustomValue).ToString().ReplaceCharWithEscapedChar()));
	}
	else if (UEdGraphSchema_K2::PC_Struct == Type.PinCategory)
	{
		UScriptStruct* StructType = Cast<UScriptStruct>(Type.PinSubCategoryObject.Get());
		ensure(StructType);

		if (StructType == TBaseStructure<FVector>::Get())
		{
			FVector Vect = FVector::ZeroVector;
			FDefaultValueHelper::ParseVector(CustomValue, /*out*/ Vect);
			return FString::Printf(TEXT("FVector(%s,%s,%s)"), *FloatToString(Vect.X), *FloatToString(Vect.Y), *FloatToString(Vect.Z));
		}
		else if (StructType == TBaseStructure<FRotator>::Get())
		{
			FRotator Rot = FRotator::ZeroRotator;
			FDefaultValueHelper::ParseRotator(CustomValue, /*out*/ Rot);
			return FString::Printf(TEXT("FRotator(%s,%s,%s)"), *FloatToString(Rot.Pitch), *FloatToString(Rot.Yaw), *FloatToString(Rot.Roll));
		}
		else if (StructType == TBaseStructure<FTransform>::Get())
		{
			FTransform Trans = FTransform::Identity;
			Trans.InitFromString(CustomValue);
			const FQuat Rot = Trans.GetRotation();
			const FVector Translation = Trans.GetTranslation();
			const FVector Scale = Trans.GetScale3D();
			return FString::Printf(TEXT("FTransform( FQuat(%s,%s,%s,%s), FVector(%s,%s,%s), FVector(%s,%s,%s) )"),
				*FloatToString(Rot.X), *FloatToString(Rot.Y), *FloatToString(Rot.Z), *FloatToString(Rot.W),
				*FloatToString(Translation.X), *FloatToString(Translation.Y), *FloatToString(Translation.Z), 
				*FloatToString(Scale.X), *FloatToString(Scale.Y), *FloatToString(Scale.Z));
		}
		else if (StructType == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor LinearColor;
			LinearColor.InitFromString(CustomValue);
			return FString::Printf(TEXT("FLinearColor(%s,%s,%s,%s)"), *FloatToString(LinearColor.R), *FloatToString(LinearColor.G), *FloatToString(LinearColor.B), *FloatToString(LinearColor.A));
		}
		else if (StructType == TBaseStructure<FColor>::Get())
		{
			FColor Color;
			Color.InitFromString(CustomValue);
			return FString::Printf(TEXT("FColor(%d,%d,%d,%d)"), Color.R, Color.G, Color.B, Color.A);
		}
		else if (StructType == TBaseStructure<FVector2D>::Get())
		{
			FVector2D Vect = FVector2D::ZeroVector;
			Vect.InitFromString(CustomValue);
			return FString::Printf(TEXT("FVector2D(%s,%s)"), *FloatToString(Vect.X), *FloatToString(Vect.Y));
		}
		else if(StructType)
		{
			//@todo:  This needs to be more robust, since import text isn't really proper for struct construction.
			const bool bEmptyCustomValue = CustomValue.IsEmpty() || (CustomValue == TEXT("()"));
			const FString StructName = *FEmitHelper::GetCppName(StructType);
			const FString LocalStructNativeName = EmitterContext.GenerateUniqueLocalName();
			if (bEmptyCustomValue)
			{
				UUserDefinedStruct* AsUDS = Cast<UUserDefinedStruct>(StructType);
				// The local variable is created to fix: "fatal error C1001: An internal error has occurred in the compiler."
				EmitterContext.AddLine(FString::Printf(TEXT("auto %s = %s%s;")
					, *LocalStructNativeName
					, *StructName
					, AsUDS ? TEXT("::GetDefaultValue()") : FEmitHelper::EmptyDefaultConstructor(StructType)));
				if (AsUDS)
				{
					EmitterContext.StructsWithDefaultValuesUsed.Add(AsUDS);
				}
			}
			else
			{
				FStructOnScope StructOnScope(StructType);
				StructType->InitializeDefaultValue(StructOnScope.GetStructMemory()); // after cl#3098294 only delta (of structure data) against the default value is stored in string. So we need to explicitly provide default values before serialization.

				class FImportTextErrorContext : public FStringOutputDevice
				{
				public:
					int32 NumErrors;

					FImportTextErrorContext() : FStringOutputDevice(), NumErrors(0) {}

					virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
					{
						if (ELogVerbosity::Type::Error == Verbosity)
						{
							NumErrors++;
						}
						FStringOutputDevice::Serialize(V, Verbosity, Category);
					}
				};

				FImportTextErrorContext ImportError;
				const TCHAR* EndOfParsedBuff = StructType->ImportText(*CustomValue, StructOnScope.GetStructMemory(), nullptr, PPF_None, &ImportError, TEXT("FEmitHelper::LiteralTerm"));
				if (!EndOfParsedBuff || ImportError.NumErrors)
				{
					UE_LOG(LogK2Compiler, Error, TEXT("FEmitHelper::LiteralTerm cannot parse struct \"%s\" error: %s class: %s"), *CustomValue, *ImportError, *GetPathNameSafe(EmitterContext.GetCurrentlyGeneratedClass()));
				}

				FString CustomConstructor;
				if (FEmitDefaultValueHelper::SpecialStructureConstructor(StructType, StructOnScope.GetStructMemory(), &CustomConstructor))
				{
					return CustomConstructor;
				}

				{
					const FString StructMemoryVar = EmitterContext.GenerateUniqueLocalName();
					EmitterContext.AddLine(FString::Printf(TEXT("uint8* %s = (uint8*)FMemory_Alloca(%s::StaticStruct()->GetStructureSize());"), *StructMemoryVar, *StructName));
					EmitterContext.AddLine(FString::Printf(TEXT("%s::StaticStruct()->InitializeStruct(%s);"), *StructName, *StructMemoryVar));
					EmitterContext.AddLine(FString::Printf(TEXT("%s& %s = *reinterpret_cast<%s*>(%s);"), *StructName, *LocalStructNativeName, *StructName, *StructMemoryVar));  // TODO: ?? should "::GetDefaultValue()" be called here?
				}

				{
					FStructOnScope DefaultStructOnScope(StructType);
					for (auto LocalProperty : TFieldRange<const UProperty>(StructType))
					{
						FEmitDefaultValueHelper::OuterGenerate(EmitterContext, LocalProperty, LocalStructNativeName, StructOnScope.GetStructMemory(), DefaultStructOnScope.GetStructMemory(), FEmitDefaultValueHelper::EPropertyAccessOperator::Dot);
					}
				}
			}
			return LocalStructNativeName;
		}
	}
	else if (Type.PinSubCategory == UEdGraphSchema_K2::PSC_Self)
	{
		return TEXT("this");
	}
	else if (UEdGraphSchema_K2::PC_Class == Type.PinCategory)
	{
		if (auto FoundClass = Cast<const UClass>(LiteralObject))
		{
			const FString MappedObject = EmitterContext.FindGloballyMappedObject(LiteralObject, UClass::StaticClass());
			if (!MappedObject.IsEmpty())
			{
				return MappedObject;
			}
			return FString::Printf(TEXT("LoadClass<UClass>(nullptr, TEXT(\"%s\"), nullptr, 0, nullptr)"), *(LiteralObject->GetPathName().ReplaceCharWithEscapedChar()));
		}
		return FString(TEXT("((UClass*)nullptr)"));
	}
	else if((UEdGraphSchema_K2::PC_SoftClass == Type.PinCategory) || (UEdGraphSchema_K2::PC_SoftObject == Type.PinCategory))
	{
		UClass* MetaClass = Cast<UClass>(Type.PinSubCategoryObject.Get());
		MetaClass = MetaClass ? MetaClass : UObject::StaticClass();
		const FString ObjTypeStr = FEmitHelper::GetCppName(EmitterContext.GetFirstNativeOrConvertedClass(MetaClass));

		if (!CustomValue.IsEmpty())
		{
			const bool bAssetSubclassOf = (UEdGraphSchema_K2::PC_SoftClass == Type.PinCategory);
			return FString::Printf(TEXT("%s<%s>(FSoftObjectPath(TEXT(\"%s\")))")
				, bAssetSubclassOf ? TEXT("TSoftClassPtr") : TEXT("TSoftObjectPtr")
				, *ObjTypeStr
				, *(CustomValue.ReplaceCharWithEscapedChar()));
		}
		return FString::Printf(TEXT("((%s*)nullptr)"), *ObjTypeStr);
	}
	else if (UEdGraphSchema_K2::PC_Object == Type.PinCategory)
	{
		UClass* FoundClass = Cast<UClass>(Type.PinSubCategoryObject.Get());
		UClass* ObjectClassToUse = FoundClass ? EmitterContext.GetFirstNativeOrConvertedClass(FoundClass) : UObject::StaticClass();
		if (LiteralObject)
		{
			const FString MappedObject = EmitterContext.FindGloballyMappedObject(LiteralObject, ObjectClassToUse, true);
			if (!MappedObject.IsEmpty())
			{
				return MappedObject;
			}
		}
		const FString ObjTypeStr = FEmitHelper::GetCppName(EmitterContext.GetFirstNativeOrConvertedClass(ObjectClassToUse));
		return FString::Printf(TEXT("((%s*)nullptr)"), *ObjTypeStr);
	}
	else if (UEdGraphSchema_K2::PC_Interface == Type.PinCategory)
	{
		if (!LiteralObject && CustomValue.IsEmpty())
		{
			return FString(TEXT("nullptr"));
		}
	}
	/*
	else if (CoerceProperty->IsA(UInterfaceProperty::StaticClass()))
	{
		if (Term->Type.PinSubCategory == PSC_Self)
		{
			return TEXT("this");
		}
		else
		{
			ensureMsgf(false, TEXT("It is not possible to express this interface property as a literal value!"));
			return Term->Name;
		}
	}
	*/
	ensureMsgf(false, TEXT("It is not possible to express this type as a literal value!"));
	return CustomValue;
}

FString FEmitHelper::PinTypeToNativeType(const FEdGraphPinType& Type)
{
	// A temp uproperty should be generated?
	auto PinTypeToNativeTypeInner = [](const FEdGraphPinType& InType) -> FString
	{
		auto Schema = GetDefault<UEdGraphSchema_K2>();
		if (UEdGraphSchema_K2::PC_String == InType.PinCategory)
		{
			return TEXT("FString");
		}
		else if (UEdGraphSchema_K2::PC_Boolean == InType.PinCategory)
		{
			return TEXT("bool");
		}
		else if ((UEdGraphSchema_K2::PC_Byte == InType.PinCategory) || (UEdGraphSchema_K2::PC_Enum == InType.PinCategory))
		{
			if (UEnum* Enum = Cast<UEnum>(InType.PinSubCategoryObject.Get()))
			{
				const bool bEnumClassForm = Enum->GetCppForm() == UEnum::ECppForm::EnumClass;
				const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass();
				ensure(!bNonNativeEnum || Enum->CppType.IsEmpty());
				const FString FullyQualifiedEnumName = (!Enum->CppType.IsEmpty()) ? Enum->CppType : FEmitHelper::GetCppName(Enum);
				// TODO: sometimes we need unwrapped type for enums without size specified. For example when native function has a raw ref param.
				return (bEnumClassForm || bNonNativeEnum) ? FullyQualifiedEnumName : FString::Printf(TEXT("TEnumAsByte<%s>"), *FullyQualifiedEnumName);
			}
			return TEXT("uint8");
		}
		else if (UEdGraphSchema_K2::PC_Int == InType.PinCategory)
		{
			return TEXT("int32");
		}
		else if (UEdGraphSchema_K2::PC_Float == InType.PinCategory)
		{
			return TEXT("float");
		}
		else if (UEdGraphSchema_K2::PC_Name == InType.PinCategory)
		{
			return TEXT("FName");
		}
		else if (UEdGraphSchema_K2::PC_Text == InType.PinCategory)
		{
			return TEXT("FText");
		}
		else if (UEdGraphSchema_K2::PC_Struct == InType.PinCategory)
		{
			if (UScriptStruct* Struct = Cast<UScriptStruct>(InType.PinSubCategoryObject.Get()))
			{
				return FEmitHelper::GetCppName(Struct);
			}
		}
		else if (UEdGraphSchema_K2::PC_Class == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSubclassOf<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftClass == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftClassPtr<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Interface == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TScriptInterface<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftObject == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *FEmitHelper::GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Object == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("%s*"), *FEmitHelper::GetCppName(Class));
			}
		}
		UE_LOG(LogK2Compiler, Error, TEXT("FEmitHelper::DefaultValue cannot generate an array type"));
		return FString{};
	};

	FString InnerTypeName = PinTypeToNativeTypeInner(Type);
	ensure(!Type.IsSet() && !Type.IsMap());
	return Type.IsArray() ? FString::Printf(TEXT("TArray<%s>"), *InnerTypeName) : InnerTypeName;
}

UFunction* FEmitHelper::GetOriginalFunction(UFunction* Function)
{
	check(Function);
	const FName FunctionName = Function->GetFName();

	UClass* Owner = Function->GetOwnerClass();
	check(Owner);
	for (auto& Inter : Owner->Interfaces)
	{
		if (UFunction* Result = Inter.Class->FindFunctionByName(FunctionName))
		{
			return GetOriginalFunction(Result);
		}
	}

	for (UClass* SearchClass = Owner->GetSuperClass(); SearchClass; SearchClass = SearchClass->GetSuperClass())
	{
		if (UFunction* Result = SearchClass->FindFunctionByName(FunctionName))
		{
			return GetOriginalFunction(Result);
		}
	}

	return Function;
}

bool FEmitHelper::ShouldHandleAsNativeEvent(UFunction* Function, bool bOnlyIfOverridden)
{
	check(Function);
	auto OriginalFunction = FEmitHelper::GetOriginalFunction(Function);
	check(OriginalFunction);
	if (!bOnlyIfOverridden || (OriginalFunction != Function))
	{
		const uint32 FlagsToCheckMask = EFunctionFlags::FUNC_Event | EFunctionFlags::FUNC_BlueprintEvent | EFunctionFlags::FUNC_Native;
		const uint32 FlagsToCheck = OriginalFunction->FunctionFlags & FlagsToCheckMask;
		return (FlagsToCheck == FlagsToCheckMask);
	}
	return false;
}

bool FEmitHelper::ShouldHandleAsImplementableEvent(UFunction* Function)
{
	check(Function);
	auto OriginalFunction = FEmitHelper::GetOriginalFunction(Function);
	check(OriginalFunction);
	if (OriginalFunction != Function)
	{
		const uint32 FlagsToCheckMask = EFunctionFlags::FUNC_Event | EFunctionFlags::FUNC_BlueprintEvent | EFunctionFlags::FUNC_Native;
		const uint32 FlagsToCheck = OriginalFunction->FunctionFlags & FlagsToCheckMask;
		return (FlagsToCheck == (EFunctionFlags::FUNC_Event | EFunctionFlags::FUNC_BlueprintEvent));
	}
	return false;
}

bool FEmitHelper::GenerateAutomaticCast(FEmitterLocalContext& EmitterContext, const FEdGraphPinType& LType, const FEdGraphPinType& RType, const UProperty* LProp, const UProperty* RProp, FString& OutCastBegin, FString& OutCastEnd, bool bForceReference)
{
	if ((RType.ContainerType != LType.ContainerType) || (LType.PinCategory != RType.PinCategory))
	{
		return false;
	}

	// BYTE to ENUM cast
	// ENUM to BYTE cast
	if (LType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		if (!RType.IsContainer())
		{
			UEnum* LTypeEnum = Cast<UEnum>(LType.PinSubCategoryObject.Get());
			UEnum* RTypeEnum = Cast<UEnum>(RType.PinSubCategoryObject.Get());
			if (!RTypeEnum && LTypeEnum)
			{
				ensure(!LTypeEnum->IsA<UUserDefinedEnum>() || LTypeEnum->CppType.IsEmpty());
				const FString EnumCppType = !LTypeEnum->CppType.IsEmpty() ? LTypeEnum->CppType : FEmitHelper::GetCppName(LTypeEnum);
				OutCastBegin = bForceReference
					? FString::Printf(TEXT("*(%s*)(&("), *EnumCppType)
					: FString::Printf(TEXT("static_cast<%s>("), *EnumCppType);
				OutCastEnd = bForceReference ? TEXT("))") : TEXT(")");
				return true;
			}
			if (!LTypeEnum && RTypeEnum)
			{
				ensure(!RTypeEnum->IsA<UUserDefinedEnum>() || RTypeEnum->CppType.IsEmpty());
				const FString EnumCppType = !RTypeEnum->CppType.IsEmpty() ? RTypeEnum->CppType : FEmitHelper::GetCppName(RTypeEnum);

				if (bForceReference)
				{
					OutCastBegin = TEXT("*static_cast<uint8*>(&(");
					OutCastEnd = TEXT("))");
				}
				else
				{
					OutCastBegin = TEXT("static_cast<uint8>(");
					OutCastEnd = TEXT(")");
				}

				return true;
			}
		}
	}
	else // UObject casts (UClass, etc.)
	{
		auto GetClassType = [&EmitterContext](const FEdGraphPinType& PinType)->UClass*
		{
			UClass* TypeClass = EmitterContext.Dependencies.FindOriginalClass(Cast<UClass>(PinType.PinSubCategoryObject.Get()));
			return  TypeClass ? EmitterContext.GetFirstNativeOrConvertedClass(TypeClass) : nullptr;
		};

		auto RequiresArrayCast = [&RType](UClass* LClass, UClass* RClass)->bool
		{
			return RType.IsArray() && LClass && RClass && (LClass->IsChildOf(RClass) || RClass->IsChildOf(LClass)) && (LClass != RClass);
		};

		const bool bIsClassTerm = LType.PinCategory == UEdGraphSchema_K2::PC_Class;
		auto GetTypeString = [bIsClassTerm](UClass* TermType, const UObjectProperty* AssociatedProperty)->FString
		{
			// favor the property's CPPType since it makes choices based off of things like CPF_UObjectWrapper (which 
			// adds things like TSubclassof<> to the decl)... however, if the property type doesn't match the term 
			// type, then ignore the property (this can happen for things like our array library, which uses wildcards 
			// and custom thunks to allow differing types)
			const bool bPropertyMatch = AssociatedProperty && ((AssociatedProperty->PropertyClass == TermType) ||
				(bIsClassTerm && CastChecked<UClassProperty>(AssociatedProperty)->MetaClass == TermType));

			if (bPropertyMatch)
			{
				// use GetCPPTypeCustom() so that it properly fills out nativized class names
				return AssociatedProperty->GetCPPTypeCustom(/*ExtendedTypeText=*/nullptr, CPPF_None, FEmitHelper::GetCppName(TermType));
			}
			else if (bIsClassTerm)
			{
				return TEXT("UClass*");
			}
			return FEmitHelper::GetCppName(TermType) + TEXT("*");
		};

		auto GetInnerTypeString = [GetTypeString](UClass* TermType, const UArrayProperty* ArrayProp)
		{
			const UObjectProperty* InnerProp = ArrayProp ? Cast<UObjectProperty>(ArrayProp->Inner) : nullptr;
			return GetTypeString(TermType, InnerProp);
		};

		auto GenerateArrayCast = [&OutCastBegin, &OutCastEnd](const FString& LTypeStr, const FString& RTypeStr)
		{
			OutCastBegin = FString::Printf(TEXT("TArrayCaster< %s >("), *RTypeStr);
			OutCastEnd   = FString::Printf(TEXT(").Get< %s >()"), *LTypeStr);
		};
		// CLASS/TSubClassOf<> to CLASS/TSubClassOf<>
		if (bIsClassTerm)
		{
			UClass* LClass = GetClassType(LType);
			UClass* RClass = GetClassType(RType);
			// it seems that we only need to cast class types when they're in arrays (TSubClassOf<>
			// has built in conversions to/from other TSubClassOfs and raw UClasses)
			if (RType.IsArray())
			{
				const UArrayProperty* LArrayProp = Cast<UArrayProperty>(LProp);
				const UClassProperty* LInnerProp = LArrayProp ? Cast<UClassProperty>(LArrayProp->Inner) : nullptr;
				const UArrayProperty* RArrayProp = Cast<UArrayProperty>(RProp);
				const UClassProperty* RInnerProp = RArrayProp ? Cast<UClassProperty>(RArrayProp->Inner) : nullptr;

				const bool bLHasWrapper = LInnerProp ? LInnerProp->HasAnyPropertyFlags(CPF_UObjectWrapper) : false;
				const bool bRHasWrapper = RInnerProp ? RInnerProp->HasAnyPropertyFlags(CPF_UObjectWrapper) : false;
				// if neither has a TSubClass<> wrapper, then they'll both be declared as a UClass*, and a cast is uneeded
				if ((bLHasWrapper != bRHasWrapper) || (bLHasWrapper && RequiresArrayCast(LClass, RClass)))
				{
					GenerateArrayCast(GetTypeString(LClass, LInnerProp), GetTypeString(RClass, RInnerProp));
					return true;
				}
			}
		}
		// OBJECT to OBJECT
		else if (LType.PinCategory == UEdGraphSchema_K2::PC_Object)
		{
			UClass* LClass = GetClassType(LType);
			UClass* RClass = GetClassType(RType);

			if (!RType.IsContainer() && LClass && RClass && (LType.bIsReference || bForceReference) && (LClass != RClass) && RClass->IsChildOf(LClass))
			{
				// when pointer is passed as reference, the type must be exactly the same
				OutCastBegin = FString::Printf(TEXT("*(%s*)(&("), *GetTypeString(LClass, Cast<UObjectProperty>(LProp)));
				OutCastEnd = TEXT("))");
				return true;
			}
			if (!RType.IsContainer() && LClass && RClass && LClass->IsChildOf(RClass) && !RClass->IsChildOf(LClass))
			{
				OutCastBegin = FString::Printf(TEXT("CastChecked<%s>("), *FEmitHelper::GetCppName(LClass));
				OutCastEnd = TEXT(", ECastCheckedType::NullAllowed)");
				return true;
			}
			else if (RequiresArrayCast(LClass, RClass))
			{
				GenerateArrayCast(GetInnerTypeString(LClass, Cast<UArrayProperty>(LProp)), GetInnerTypeString(RClass, Cast<UArrayProperty>(RProp)));
				return true;
			}
		}
	}
	return false;
}

FString FEmitHelper::ReplaceConvertedMetaData(UObject* Obj)
{
	FString Result;
	const FString ReplaceConvertedMD = FEmitHelper::GenerateReplaceConvertedMD(Obj);
	if (!ReplaceConvertedMD.IsEmpty())
	{
		TArray<FString> AdditionalMD;
		AdditionalMD.Add(ReplaceConvertedMD);
		Result += FEmitHelper::HandleMetaData(nullptr, false, &AdditionalMD);
	}
	return Result;
}

FString FEmitHelper::GenerateGetPropertyByName(FEmitterLocalContext& EmitterContext, const UProperty* Property)
{
	check(Property);

	FString* AlreadyCreatedProperty = EmitterContext.PropertiesForInaccessibleStructs.Find(Property);
	if (AlreadyCreatedProperty)
	{
		return *AlreadyCreatedProperty;
	}

	const FString PropertyPtrName = EmitterContext.GenerateUniqueLocalName();

	static const FBoolConfigValueHelper UseStaticVariables(TEXT("BlueprintNativizationSettings"), TEXT("bUseStaticVariablesInClasses"));
	const bool bUseStaticVariables = UseStaticVariables;
	if (bUseStaticVariables)
	{
		const FString PropertyWeakPtrName = EmitterContext.GenerateUniqueLocalName();
		EmitterContext.AddLine(FString::Printf(TEXT("static TWeakObjectPtr<UProperty> %s{};"), *PropertyWeakPtrName));

		EmitterContext.AddLine(FString::Printf(TEXT("const UProperty* %s = %s.Get();"), *PropertyPtrName, *PropertyWeakPtrName));
		EmitterContext.AddLine(FString::Printf(TEXT("if (nullptr == %s)"), *PropertyPtrName));
		EmitterContext.AddLine(TEXT("{"));
		EmitterContext.IncreaseIndent();

		const FString PropertyOwnerStruct = EmitterContext.FindGloballyMappedObject(Property->GetOwnerStruct(), UStruct::StaticClass());
		EmitterContext.AddLine(FString::Printf(TEXT("%s = (%s)->%s(FName(TEXT(\"%s\")));")
			, *PropertyPtrName
			, *PropertyOwnerStruct
			, GET_FUNCTION_NAME_STRING_CHECKED(UStruct, FindPropertyByName)
			, *Property->GetName()));
		EmitterContext.AddLine(FString::Printf(TEXT("check(%s);"), *PropertyPtrName));
		EmitterContext.AddLine(FString::Printf(TEXT("%s = %s;"), *PropertyWeakPtrName, *PropertyPtrName));
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));
	}
	else 
	{
		const FString PropertyOwnerStruct = EmitterContext.FindGloballyMappedObject(Property->GetOwnerStruct(), UStruct::StaticClass());
		EmitterContext.AddLine(FString::Printf(TEXT("const UProperty* %s = (%s)->FindPropertyByName(FName(TEXT(\"%s\")));")
			, *PropertyPtrName
			, *PropertyOwnerStruct
			, GET_FUNCTION_NAME_STRING_CHECKED(UStruct, FindPropertyByName)
			, *Property->GetName()));
		EmitterContext.AddLine(FString::Printf(TEXT("check(%s);"), *PropertyPtrName));
	}

	if (EmitterContext.CurrentCodeType != FEmitterLocalContext::EGeneratedCodeType::Regular)
	{
		EmitterContext.PropertiesForInaccessibleStructs.Add(Property, PropertyPtrName);
		if (EmitterContext.ActiveScopeBlock)
		{
			EmitterContext.ActiveScopeBlock->TrackLocalAccessorDecl(Property);
		}
	}
	return PropertyPtrName;
}


void FNativizationSummaryHelper::InaccessibleProperty(const UProperty* Property)
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
	if (NativizationSummary.IsValid())
	{
		FSoftObjectPath Key(Property);
		int32* FoundStat = NativizationSummary->InaccessiblePropertyStat.Find(Key);
		if (FoundStat)
		{
			(*FoundStat)++;
		}
		else
		{
			NativizationSummary->InaccessiblePropertyStat.Add(Key, 1);
		}
	}
}

static void FNativizationSummaryHelper__MemberUsed(const UClass* Class, const UField* Field, int32 FNativizationSummary::FAnimBlueprintDetails::*CouterPtr)
{
	if (Field && Class)
	{
		IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
		if (NativizationSummary.IsValid())
		{
			const UClass* Owner = Field->GetOwnerClass();
			UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(UBlueprint::GetBlueprintFromClass(Owner));
			const bool bUnrelatedClass = !Class->IsChildOf(Owner);
			if (AnimBP && bUnrelatedClass)
			{
				FNativizationSummary::FAnimBlueprintDetails& AnimBlueprintDetails = NativizationSummary->AnimBlueprintStat.FindOrAdd(FSoftObjectPath(AnimBP));
				(AnimBlueprintDetails.*CouterPtr)++;
			}
		}
	}
}

void FNativizationSummaryHelper::PropertyUsed(const UClass* Class, const UProperty* Property)
{
	FNativizationSummaryHelper__MemberUsed(Class, Property, &FNativizationSummary::FAnimBlueprintDetails::VariableUsage);
}

void FNativizationSummaryHelper::FunctionUsed(const UClass* Class, const UFunction* Function)
{
	FNativizationSummaryHelper__MemberUsed(Class, Function, &FNativizationSummary::FAnimBlueprintDetails::FunctionUsage);
}

void FNativizationSummaryHelper::ReducibleFunciton(const UClass* OriginalClass)
{
	if (OriginalClass)
	{
		IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
		UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(UBlueprint::GetBlueprintFromClass(OriginalClass));
		if (NativizationSummary.IsValid() && OriginalClass && AnimBP)
		{
			FNativizationSummary::FAnimBlueprintDetails& AnimBlueprintDetails = NativizationSummary->AnimBlueprintStat.FindOrAdd(FSoftObjectPath(AnimBP));
			AnimBlueprintDetails.ReducibleFunctions++;
		}
	}
}

void FNativizationSummaryHelper::RegisterRequiredModules(const FName PlatformName, const TSet<TSoftObjectPtr<UPackage>>& InModules)
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
	if (NativizationSummary.IsValid())
	{
		TSet<TSoftObjectPtr<UPackage>>& Modules = NativizationSummary->ModulesRequiredByPlatform.FindOrAdd(PlatformName);
		Modules.Append(InModules);
	}
}

void FNativizationSummaryHelper::RegisterClass(const UClass* OriginalClass)
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(UBlueprint::GetBlueprintFromClass(OriginalClass));
	if (NativizationSummary.IsValid() && OriginalClass && AnimBP)
	{
		{
			FNativizationSummary::FAnimBlueprintDetails& AnimBlueprintDetails = NativizationSummary->AnimBlueprintStat.FindOrAdd(FSoftObjectPath(AnimBP));

			AnimBlueprintDetails.Variables = AnimBP->NewVariables.Num();

			UFunction* UberGraphFunction = CastChecked<UBlueprintGeneratedClass>(OriginalClass)->UberGraphFunction;
			for (auto FunctIter : TFieldRange<UFunction>(OriginalClass, EFieldIteratorFlags::ExcludeSuper))
			{
				if (UberGraphFunction != FunctIter)
				{
					AnimBlueprintDetails.Functions++;
				}
			}
		}

		for (UClass* SuperClass = OriginalClass->GetSuperClass(); SuperClass; SuperClass = SuperClass->GetSuperClass())
		{
			UAnimBlueprint* ParentAnimBP = Cast<UAnimBlueprint>(UBlueprint::GetBlueprintFromClass(SuperClass));
			if (ParentAnimBP)
			{
				FNativizationSummary::FAnimBlueprintDetails& AnimBlueprintDetails = NativizationSummary->AnimBlueprintStat.FindOrAdd(FSoftObjectPath(ParentAnimBP));
				AnimBlueprintDetails.Children++;
			}
		}
	}
}

FString FEmitHelper::AccessInaccessibleProperty(FEmitterLocalContext& EmitterContext, const UProperty* Property, FString CustomTypeDeclaration
	, const FString& ContextStr, const FString& ContextAdressOp, int32 StaticArrayIdx, ENativizedTermUsage TermUsage, FString* CustomSetExpressionEnding)
{
	check(Property);
	ensure((TermUsage == ENativizedTermUsage::Setter) == (CustomSetExpressionEnding != nullptr));
	if (CustomSetExpressionEnding)
	{
		CustomSetExpressionEnding->Reset();
	}

	const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(Property);
	const bool bBitfield = BoolProperty && !BoolProperty->IsNativeBool();
	if (bBitfield)
	{
		if (TermUsage == ENativizedTermUsage::Getter)
		{
			FNativizationSummaryHelper::InaccessibleProperty(Property);
			const FString PropertyLocalName = GenerateGetPropertyByName(EmitterContext, Property);
			return  FString::Printf(TEXT("(((UBoolProperty*)%s)->%s(%s(%s), %d))")
				, *PropertyLocalName
				, GET_FUNCTION_NAME_STRING_CHECKED(UBoolProperty, GetPropertyValue_InContainer)
				, *ContextAdressOp
				, *ContextStr
				, StaticArrayIdx);
		}

		if (TermUsage == ENativizedTermUsage::Setter)
		{
			FNativizationSummaryHelper::InaccessibleProperty(Property);
			const FString PropertyLocalName = GenerateGetPropertyByName(EmitterContext, Property);
			if (ensure(CustomSetExpressionEnding))
			{
				*CustomSetExpressionEnding = FString::Printf(TEXT(", %d))"), StaticArrayIdx);
			}
			return FString::Printf(TEXT("(((UBoolProperty*)%s)->%s(%s(%s), ")
				, *PropertyLocalName
				, GET_FUNCTION_NAME_STRING_CHECKED(UBoolProperty, SetPropertyValue_InContainer)
				, *ContextAdressOp
				, *ContextStr);
		}
		UE_LOG(LogK2Compiler, Error, TEXT("AccessInaccessibleProperty - bitfield %s"), *GetPathNameSafe(Property));
	}

	const uint32 CppTemplateTypeFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
		| EPropertyExportCPPFlags::CPPF_NoConst | EPropertyExportCPPFlags::CPPF_NoRef | EPropertyExportCPPFlags::CPPF_NoStaticArray
		| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;
	const FString TypeDeclaration = (!CustomTypeDeclaration.IsEmpty() ? MoveTemp(CustomTypeDeclaration)
									: EmitterContext.ExportCppDeclaration(Property, EExportedDeclaration::Member, CppTemplateTypeFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip));
	
	// Private Property Offset functions are generated only for private/protected properties - see PrivatePropertiesOffsetGetters in CodeGenerator.cpp
	const bool bHasPPO = Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected); 
	if (!bHasPPO)
	{
		//TODO: if property is inaccessible due to const specifier, use const_cast

		FNativizationSummaryHelper::InaccessibleProperty(Property);
		const FString PropertyLocalName = GenerateGetPropertyByName(EmitterContext, Property);
		return FString::Printf(TEXT("(*(%s->ContainerPtrToValuePtr<%s>(%s(%s), %d)))")
			, *PropertyLocalName
			, *TypeDeclaration
			, *ContextAdressOp
			, *ContextStr
			, StaticArrayIdx);
	}

	const UStruct* PropertyOwner = Property->GetOwnerStruct();
	const FString OwnerStructName = FEmitHelper::GetCppName(PropertyOwner);
	const FString PropertyName = FEmitHelper::GetCppName(Property);
	const FString ArrayParams = (StaticArrayIdx != 0)
		? FString::Printf(TEXT(", sizeof(%s), %d"), *TypeDeclaration, StaticArrayIdx)
		: FString();
	return FString::Printf(TEXT("(*(AccessPrivateProperty<%s>(%s(%s), %s::__PPO__%s() %s)))")
		, *TypeDeclaration
		, *ContextAdressOp
		, *ContextStr
		, *OwnerStructName
		, *PropertyName
		, *ArrayParams);
}

struct FSearchableValuesdHelper_StaticData
{
	TArray<FSoftClassPath> ClassesWithStaticSearchableValues;
	TArray<FName> TagPropertyNames;
private:
	FSearchableValuesdHelper_StaticData()
	{
		{
			TArray<FString> Paths;
			GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ClassesWithStaticSearchableValues"), Paths, GEditorIni);
			for (FString& Path : Paths)
			{
				ClassesWithStaticSearchableValues.Add(FSoftClassPath(Path));
			}
		}

		{
			TArray<FString> Names;
			GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("StaticSearchableTagNames"), Names, GEditorIni);
			for (FString& Name : Names)
			{
				TagPropertyNames.Add(FName(*Name));
			}
		}
	}

public:
	static FSearchableValuesdHelper_StaticData& Get()
	{
		static FSearchableValuesdHelper_StaticData StaticInstance;
		return StaticInstance;
	}
};

bool FBackendHelperStaticSearchableValues::HasSearchableValues(UClass* InClass)
{
	for (const FSoftClassPath& ClassStrRef : FSearchableValuesdHelper_StaticData::Get().ClassesWithStaticSearchableValues)
	{
		UClass* IterClass = ClassStrRef.ResolveClass();
		if (IterClass && InClass && InClass->IsChildOf(IterClass))
		{
			return true;
		}
	}
	return false;
}

FString FBackendHelperStaticSearchableValues::GetFunctionName()
{
	return TEXT("__InitializeStaticSearchableValues");
}

FString FBackendHelperStaticSearchableValues::GenerateClassMetaData(UClass* Class)
{
	const FString MetaDataName = TEXT("InitializeStaticSearchableValues");
	const FString FunctionName = GetFunctionName();
	return FString::Printf(TEXT("%s=\"%s\""), *MetaDataName, *FunctionName);
}

void FBackendHelperStaticSearchableValues::EmitFunctionDeclaration(FEmitterLocalContext& Context)
{
	const FString FunctionName = GetFunctionName();
	Context.Header.AddLine(FString::Printf(TEXT("static void %s(TMap<FName, FName>& SearchableValues);"), *FunctionName));
}

void FBackendHelperStaticSearchableValues::EmitFunctionDefinition(FEmitterLocalContext& Context)
{
	auto BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	const FString CppClassName = FEmitHelper::GetCppName(BPGC);
	const FString FunctionName = GetFunctionName();

	Context.Body.AddLine(FString::Printf(TEXT("void %s::%s(TMap<FName, FName>& SearchableValues)"), *CppClassName, *FunctionName));
	Context.Body.AddLine(TEXT("{"));
	Context.IncreaseIndent();

	UClass* OriginalSourceClass = Context.Dependencies.FindOriginalClass(BPGC);
	if(ensure(OriginalSourceClass))
	{
		FAssetData ClassAsset(OriginalSourceClass);
		for (const FName TagPropertyName : FSearchableValuesdHelper_StaticData::Get().TagPropertyNames)
		{
			const FName FoundValue = ClassAsset.GetTagValueRef<FName>(TagPropertyName);
			if (!FoundValue.IsNone())
			{
				Context.Body.AddLine(FString::Printf(TEXT("SearchableValues.Add(FName(TEXT(\"%s\")), FName(TEXT(\"%s\")));"), *TagPropertyName.ToString(), *FoundValue.ToString()));
			}
			else
			{
				UE_LOG(LogK2Compiler, Warning, TEXT("FBackendHelperStaticSearchableValues - None value. Tag: %s Asset: %s"), *TagPropertyName.ToString(), *GetPathNameSafe(OriginalSourceClass));
			}
		}
	}

	Context.Body.DecreaseIndent();
	Context.Body.AddLine(TEXT("}"));
}

const TCHAR* FEmitHelper::EmptyDefaultConstructor(UScriptStruct* Struct)
{
	UScriptStruct::ICppStructOps* StructOps = Struct ? Struct->GetCppStructOps() : nullptr;
	const bool bUseForceInitConstructor = StructOps && StructOps->HasNoopConstructor();
	return bUseForceInitConstructor ? TEXT("(EForceInit::ForceInit)") : TEXT("{}");
}

FString FDependenciesGlobalMapHelper::EmitHeaderCode()
{
	return TEXT("#pragma once\n#include \"Blueprint/BlueprintSupport.h\"\nstruct F__NativeDependencies { \n\tstatic const FBlueprintDependencyObjectRef& Get(int16 Index);\n };");
}

FString FDependenciesGlobalMapHelper::EmitBodyCode(const FString& PCHFilename)
{
	FCodeText CodeText;
	CodeText.AddLine(FString::Printf(TEXT("#include \"%s.h\""), *PCHFilename));
	{
		FDisableUnwantedWarningOnScope DisableUnwantedWarningOnScope(CodeText);

		CodeText.AddLine("namespace");
		CodeText.AddLine("{");
		CodeText.IncreaseIndent();
		CodeText.AddLine("static const FBlueprintDependencyObjectRef NativizedCodeDependenties[] =");
		CodeText.AddLine("{");

		TArray<FNativizationSummary::FDependencyRecord> DependenciesArray;
		{
			auto& DependenciesGlobalMap = GetDependenciesGlobalMap();
			DependenciesGlobalMap.GenerateValueArray(DependenciesArray);
		}

		if (DependenciesArray.Num() > 0)
		{
			DependenciesArray.Sort(
				[](const FNativizationSummary::FDependencyRecord& A, const FNativizationSummary::FDependencyRecord& B) -> bool
			{
				return A.Index < B.Index;
			});
			int32 Index = 0;
			for (FNativizationSummary::FDependencyRecord& Record : DependenciesArray)
			{
				ensure(!Record.NativeLine.IsEmpty());
				ensure(Record.Index == Index);
				Index++;
				CodeText.AddLine(Record.NativeLine);
			}
		}
		else
		{
			CodeText.AddLine(TEXT("FBlueprintDependencyObjectRef()"));
		}

		CodeText.AddLine(TEXT("};"));
		CodeText.DecreaseIndent();
		CodeText.AddLine(TEXT("}"));

		CodeText.AddLine(TEXT("const FBlueprintDependencyObjectRef& F__NativeDependencies::Get(int16 Index)"));
		CodeText.AddLine(TEXT("{"));
		CodeText.AddLine(TEXT("static const FBlueprintDependencyObjectRef& NullObjectRef = FBlueprintDependencyObjectRef();"));
		CodeText.AddLine(TEXT("if (Index == -1) { return NullObjectRef; }"));
		CodeText.AddLine(FString::Printf(TEXT("\tcheck((Index >= 0) && (Index < %d));"), DependenciesArray.Num()));
		CodeText.AddLine(TEXT("\treturn ::NativizedCodeDependenties[Index];"));
		CodeText.AddLine(TEXT("};"));
	}
	return CodeText.Result;
}

FNativizationSummary::FDependencyRecord& FDependenciesGlobalMapHelper::FindDependencyRecord(const FSoftObjectPath& Key)
{
	auto& DependenciesGlobalMap = GetDependenciesGlobalMap();
	FNativizationSummary::FDependencyRecord& DependencyRecord = DependenciesGlobalMap.FindOrAdd(Key);
	if (DependencyRecord.Index == -1)
	{
		DependencyRecord.Index = DependenciesGlobalMap.Num() - 1;
	}
	return DependencyRecord;
}

TMap<FSoftObjectPath, FNativizationSummary::FDependencyRecord>& FDependenciesGlobalMapHelper::GetDependenciesGlobalMap()
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	TSharedPtr<FNativizationSummary> NativizationSummary = BackEndModule.NativizationSummary();
	check(NativizationSummary.IsValid());
	return NativizationSummary->DependenciesGlobalMap;
}

FDisableUnwantedWarningOnScope::FDisableUnwantedWarningOnScope(FCodeText& InCodeText)
	: CodeText(InCodeText)
{
	CodeText.AddLine(TEXT("#ifdef _MSC_VER"));
	CodeText.AddLine(TEXT("#pragma warning (push)"));
	// C4883 is a strange error (for big functions), introduced in VS2015 update 2
	CodeText.AddLine(TEXT("#pragma warning (disable : 4883)"));
	CodeText.AddLine(TEXT("#endif"));
	CodeText.AddLine(TEXT("PRAGMA_DISABLE_DEPRECATION_WARNINGS"));
}

FDisableUnwantedWarningOnScope::~FDisableUnwantedWarningOnScope()
{
	CodeText.AddLine(TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS"));
	CodeText.AddLine(TEXT("#ifdef _MSC_VER"));
	CodeText.AddLine(TEXT("#pragma warning (pop)"));
	CodeText.AddLine(TEXT("#endif"));
}

FScopeBlock::FScopeBlock(FEmitterLocalContext& InContext)
	: Context(InContext)
	, OuterScopeBlock(InContext.ActiveScopeBlock)
{
	Context.ActiveScopeBlock = this;
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();
}

FScopeBlock::~FScopeBlock()
{
	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));
	Context.ActiveScopeBlock = OuterScopeBlock;

	for (const UProperty* InaccessibleProp : LocalAccessorDecls)
	{
		Context.PropertiesForInaccessibleStructs.Remove(InaccessibleProp);
	}
}

void FScopeBlock::TrackLocalAccessorDecl(const UProperty* Property)
{
	LocalAccessorDecls.AddUnique(Property);
}

#define MAP_BASE_STRUCTURE_ACCESS(x) \
	BaseStructureAccessorsMap.Add(x, TEXT("#x"))

struct FStructAccessHelper_StaticData
{
	TMap<const UStruct*, FString> BaseStructureAccessorsMap;
	TMap<const UStruct*, bool> SupportsDirectNativeAccessMap;
	TArray<FSoftClassPath> NoExportTypesWithDirectNativeFieldAccess;

	static FStructAccessHelper_StaticData& Get()
	{
		static FStructAccessHelper_StaticData StaticInstance;
		return StaticInstance;
	}

private:
	FStructAccessHelper_StaticData()
	{
		// These are declared in Class.h; it's more efficient to access these native struct types at runtime using the specialized template functions, so we list them here.
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FRotator>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FTransform>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FLinearColor>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FColor>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FVector>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FVector2D>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FRandomStream>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FGuid>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FTransform>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FBox2D>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FFallbackStruct>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FFloatRangeBound>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FFloatRange>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FInt32RangeBound>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FInt32Range>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FFloatInterval>::Get());
		MAP_BASE_STRUCTURE_ACCESS(TBaseStructure<FInt32Interval>::Get());

		{
			// Cache the known set of noexport types that are known to be compatible with emitting native code to access fields directly.
			TArray<FString> Paths;
			GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("NoExportTypesWithDirectNativeFieldAccess"), Paths, GEditorIni);
			for (FString& Path : Paths)
			{
				NoExportTypesWithDirectNativeFieldAccess.Add(FSoftClassPath(Path));
			}
		}
	}
};

FString FStructAccessHelper::EmitStructAccessCode(const UStruct* InStruct)
{
	check(InStruct != nullptr);
	
	if (const FString* MappedAccessorCode = FStructAccessHelper_StaticData::Get().BaseStructureAccessorsMap.Find(InStruct))
	{
		return *MappedAccessorCode;
	}
	else
	{
		return FString::Printf(TEXT("FStructUtils::FindStructureInPackageChecked(TEXT(\"%s\"), TEXT(\"%s\"))"), *InStruct->GetName(), *InStruct->GetOutermost()->GetName());
	}
}

bool FStructAccessHelper::CanEmitDirectFieldAccess(const UScriptStruct* InStruct)
{
	check(InStruct != nullptr);

	// Don't allow direct field access for native, noexport types that have not been explicitly listed as compatible. In order to be listed, all
	// properties within the noexport type must match up with a member's name and accessibility in the corresponding native C++ type declaration.
	if (InStruct->IsNative() && (InStruct->StructFlags & STRUCT_NoExport))
	{
		FStructAccessHelper_StaticData& StaticStructAccessData = FStructAccessHelper_StaticData::Get();
		if (const bool* CachedResult = StaticStructAccessData.SupportsDirectNativeAccessMap.Find(InStruct))
		{
			return *CachedResult;
		}
		else
		{
			const FString PathName = InStruct->GetPathName();
			return StaticStructAccessData.SupportsDirectNativeAccessMap.Add(InStruct, StaticStructAccessData.NoExportTypesWithDirectNativeFieldAccess.Contains(PathName));
		}
	}

	// All other cases will support direct field access.
	return true;
}