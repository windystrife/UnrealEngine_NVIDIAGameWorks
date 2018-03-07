// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompilerMisc.cpp
=============================================================================*/

#include "KismetCompilerMisc.h"

#include "BlueprintCompilationManager.h"
#include "Misc/CoreMisc.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/ObjectRedirector.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectHash.h"
#include "Engine/MemberReference.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedStruct.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Timeline.h"
#include "K2Node_Variable.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompiler.h"

#include "AnimGraphNode_Base.h"
#include "K2Node_EnumLiteral.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "ObjectTools.h"
#include "BlueprintEditorSettings.h"

#define LOCTEXT_NAMESPACE "KismetCompiler"

DECLARE_CYCLE_STAT(TEXT("Choose Terminal Scope"), EKismetCompilerStats_ChooseTerminalScope, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Resolve compiled statements"), EKismetCompilerStats_ResolveCompiledStatements, STATGROUP_KismetCompiler );


//////////////////////////////////////////////////////////////////////////
// FKismetCompilerUtilities

static bool IsTypeCompatibleWithProperty(UEdGraphPin* SourcePin, const FEdGraphPinType& OwningType, const FEdGraphTerminalType& TerminalType, UProperty* TestProperty, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass)
{
	check(SourcePin != NULL);
	const EEdGraphPinDirection Direction = SourcePin->Direction; 
	const FString& PinCategory = TerminalType.TerminalCategory;
	const FString& PinSubCategory = TerminalType.TerminalSubCategory;
	const UObject* PinSubCategoryObject = TerminalType.TerminalSubCategoryObject.Get();
	
	const UFunction* OwningFunction = Cast<UFunction>(TestProperty->GetOuter());

	bool bTypeMismatch = false;
	bool bSubtypeMismatch = false;
	FString DesiredSubType(TEXT(""));

	if (PinCategory == Schema->PC_Boolean)
	{
		UBoolProperty* SpecificProperty = Cast<UBoolProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Byte)
	{
		UByteProperty* ByteProperty = Cast<UByteProperty>(TestProperty);
		UEnumProperty* EnumProperty = Cast<UEnumProperty>(TestProperty);
		bTypeMismatch = (ByteProperty == nullptr) && (EnumProperty == nullptr || !EnumProperty->GetUnderlyingProperty()->IsA<UByteProperty>());
	}
	else if ((PinCategory == Schema->PC_Class) || (PinCategory == Schema->PC_SoftClass))
	{
		const UClass* ClassType = (PinSubCategory == Schema->PSC_Self) ? SelfClass : Cast<const UClass>(PinSubCategoryObject);

		if (ClassType == NULL)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FindClassForPin_Error", "Failed to find class for pin @@").ToString()), SourcePin);
		}
		else
		{
			const UClass* MetaClass = NULL;
			if (auto ClassProperty = Cast<UClassProperty>(TestProperty))
			{
				MetaClass = ClassProperty->MetaClass;
			}
			else if (auto SoftClassProperty = Cast<USoftClassProperty>(TestProperty))
			{
				MetaClass = SoftClassProperty->MetaClass;
			}

			if (MetaClass != NULL)
			{
				DesiredSubType = MetaClass->GetName();

				const UClass* OutputClass = (Direction == EGPD_Output) ? ClassType :  MetaClass;
				const UClass* InputClass = (Direction == EGPD_Output) ? MetaClass : ClassType;

				// It matches if it's an exact match or if the output class is more derived than the input class
				bTypeMismatch = bSubtypeMismatch = !((OutputClass == InputClass) || (OutputClass->IsChildOf(InputClass)));

				if ((PinCategory == Schema->PC_SoftClass) && (!TestProperty->IsA<USoftClassProperty>()))
				{
					bTypeMismatch = true;
				}
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else if (PinCategory == Schema->PC_Float)
	{
		UFloatProperty* SpecificProperty = Cast<UFloatProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Int)
	{
		UIntProperty* SpecificProperty = Cast<UIntProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Name)
	{
		UNameProperty* SpecificProperty = Cast<UNameProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Delegate)
	{
		const UFunction* SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(OwningType.PinSubCategoryMemberReference);
		const UDelegateProperty* PropertyDelegate = Cast<const UDelegateProperty>(TestProperty);
		bTypeMismatch = !(SignatureFunction 
			&& PropertyDelegate 
			&& PropertyDelegate->SignatureFunction 
			&& PropertyDelegate->SignatureFunction->IsSignatureCompatibleWith(SignatureFunction));
	}
	else if ((PinCategory == Schema->PC_Object) || (PinCategory == Schema->PC_Interface) || (PinCategory == Schema->PC_SoftObject))
	{
		const UClass* ObjectType = (PinSubCategory == Schema->PSC_Self) ? SelfClass : Cast<const UClass>(PinSubCategoryObject);

		if (ObjectType == NULL)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FindClassForPin_Error", "Failed to find class for pin @@").ToString()), SourcePin);
		}
		else
		{
			UObjectPropertyBase* ObjProperty = Cast<UObjectPropertyBase>(TestProperty);
			if (ObjProperty != NULL && ObjProperty->PropertyClass)
			{
				DesiredSubType = ObjProperty->PropertyClass->GetName();

				const UClass* OutputClass = (Direction == EGPD_Output) ? ObjectType : ObjProperty->PropertyClass;
				const UClass* InputClass  = (Direction == EGPD_Output) ? ObjProperty->PropertyClass : ObjectType;

				// Fixup stale types to avoid unwanted mismatches during the reinstancing process
				if (OutputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					UBlueprint* GeneratedByBP = Cast<UBlueprint>(OutputClass->ClassGeneratedBy);
					if (GeneratedByBP != nullptr)
					{
						const UClass* NewOutputClass = GeneratedByBP->GeneratedClass;
						if (NewOutputClass && !NewOutputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
						{
							OutputClass = NewOutputClass;
						}
					}
				}
				if (InputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					UBlueprint* GeneratedByBP = Cast<UBlueprint>(InputClass->ClassGeneratedBy);
					if (GeneratedByBP != nullptr)
					{
						const UClass* NewInputClass = GeneratedByBP->GeneratedClass;
						if (NewInputClass && !NewInputClass->HasAnyClassFlags(CLASS_NewerVersionExists))
						{
							InputClass = NewInputClass;
						}
					}
				}

				// It matches if it's an exact match or if the output class is more derived than the input class
				bTypeMismatch = bSubtypeMismatch = !((OutputClass == InputClass) || (OutputClass->IsChildOf(InputClass)));

				if ((PinCategory == Schema->PC_SoftObject) && (!TestProperty->IsA<USoftObjectProperty>()))
				{
					bTypeMismatch = true;
				}
			}
			else if (UInterfaceProperty* IntefaceProperty = Cast<UInterfaceProperty>(TestProperty))
			{
				UClass const* InterfaceClass = IntefaceProperty->InterfaceClass;
				if (InterfaceClass == NULL)
				{
					bTypeMismatch = true;
				}
				else 
				{
					DesiredSubType = InterfaceClass->GetName();
					bTypeMismatch = ObjectType->ImplementsInterface(InterfaceClass);
				}
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else if (PinCategory == Schema->PC_String)
	{
		UStrProperty* SpecificProperty = Cast<UStrProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Text)
	{
		UTextProperty* SpecificProperty = Cast<UTextProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Struct)
	{
		const UScriptStruct* StructType = Cast<const UScriptStruct>(PinSubCategoryObject);
		if (StructType == NULL)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FindStructForPin_Error", "Failed to find struct for pin @@").ToString()), SourcePin);
		}
		else
		{
			UStructProperty* StructProperty = Cast<UStructProperty>(TestProperty);
			if (StructProperty != NULL)
			{
				DesiredSubType = StructProperty->Struct->GetName();
				bool bMatchingStructs = (StructType == StructProperty->Struct);
				if (auto UserDefinedStructFromProperty = Cast<const UUserDefinedStruct>(StructProperty->Struct))
				{
					bMatchingStructs |= (UserDefinedStructFromProperty->PrimaryStruct.Get() == StructType);
				}
				bSubtypeMismatch = bTypeMismatch = !bMatchingStructs;
			}
			else
			{
				bTypeMismatch = true;
			}

			if (OwningFunction && bTypeMismatch)
			{
				if (UK2Node_CallFunction::IsStructureWildcardProperty(OwningFunction, SourcePin->PinName))
				{
					bSubtypeMismatch = bTypeMismatch = false;
				}
			}
		}
	}
	else
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("UnsupportedTypeForPin", "Unsupported type (%s) on @@").ToString(), *UEdGraphSchema_K2::TypeToText(OwningType).ToString()), SourcePin);
	}

	return false;
}

/** Tests to see if a pin is schema compatible with a property */
bool FKismetCompilerUtilities::IsTypeCompatibleWithProperty(UEdGraphPin* SourcePin, UProperty* Property, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass)
{
	check(SourcePin != NULL);
	const FEdGraphPinType& Type = SourcePin->PinType;
	const EEdGraphPinDirection Direction = SourcePin->Direction; 

	const FString& PinCategory = Type.PinCategory;
	const FString& PinSubCategory = Type.PinSubCategory;
	const UObject* PinSubCategoryObject = Type.PinSubCategoryObject.Get();

	UProperty* TestProperty = NULL;
	const UFunction* OwningFunction = Cast<UFunction>(Property->GetOuter());
	
	int32 NumErrorsAtStart = MessageLog.NumErrors;
	bool bTypeMismatch = false;

	if( Type.IsArray() )
	{
		// For arrays, the property we want to test against is the inner property
		if( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property) )
		{
			if(OwningFunction)
			{
				// Check for the magic ArrayParm property, which always matches array types
				const FString& ArrayPointerMetaData = OwningFunction->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
				TArray<FString> ArrayPinComboNames;
				ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

				for(auto Iter = ArrayPinComboNames.CreateConstIterator(); Iter; ++Iter)
				{
					TArray<FString> ArrayPinNames;
					Iter->ParseIntoArray(ArrayPinNames, TEXT("|"), true);

					if( ArrayPinNames[0] == SourcePin->PinName )
					{
						return true;
					}
				}
			}
			
			bTypeMismatch = ::IsTypeCompatibleWithProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), ArrayProp->Inner, MessageLog, Schema, SelfClass);
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsArray_Error", "Pin @@ is specified as an array, but does not have a valid array property.").ToString(), SourcePin);
			return false;
		}
	}
	else if (Type.IsSet())
	{
		if (USetProperty* SetProperty = Cast<USetProperty>(Property))
		{
			if (OwningFunction && FEdGraphUtilities::IsSetParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}

			bTypeMismatch = ::IsTypeCompatibleWithProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), SetProperty->ElementProp, MessageLog, Schema, SelfClass);
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsSet_Error", "Pin @@ is specified as a set, but does not have a valid set property.").ToString(), SourcePin);
			return false;
		}
	}
	else if (Type.IsMap())
	{
		if (UMapProperty* MapProperty = Cast<UMapProperty>(Property))
		{
			if (OwningFunction && FEdGraphUtilities::IsMapParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}

			bTypeMismatch = ::IsTypeCompatibleWithProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), MapProperty->KeyProp, MessageLog, Schema, SelfClass);
			bTypeMismatch = bTypeMismatch && ::IsTypeCompatibleWithProperty(SourcePin, Type, Type.PinValueType, MapProperty->ValueProp, MessageLog, Schema, SelfClass);
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsSet_Error", "Pin @@ is specified as a set, but does not have a valid set property.").ToString(), SourcePin);
			return false;
		}
	}
	else
	{
		// For scalars, we just take the passed in property
		bTypeMismatch = ::IsTypeCompatibleWithProperty(SourcePin, Type, SourcePin->GetPrimaryTerminalType(), Property, MessageLog, Schema, SelfClass);
	}

	// Check for the early out...if this is a type dependent parameter in an array function
	if( OwningFunction )
	{
		if ( OwningFunction->HasMetaData(FBlueprintMetadata::MD_ArrayParam) )
		{
			// Check to see if this param is type dependent on an array parameter
			const FString& DependentParams = OwningFunction->GetMetaData(FBlueprintMetadata::MD_ArrayDependentParam);
			TArray<FString>	DependentParamNames;
			DependentParams.ParseIntoArray(DependentParamNames, TEXT(","), true);
			if (DependentParamNames.Find(SourcePin->PinName) != INDEX_NONE)
			{
				//@todo:  This assumes that the wildcard coercion has done its job...I'd feel better if there was some easier way of accessing the target array type
				return true;
			}
		}
		else if (OwningFunction->HasMetaData(FBlueprintMetadata::MD_SetParam))
		{
			// If the pin in question is part of a Set (inferred) parameter, then ignore pin matching:
			// @todo:  This assumes that the wildcard coercion has done its job...I'd feel better if 
			// there was some easier way of accessing the target set type
			if (FEdGraphUtilities::IsSetParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}
		}
		else if(OwningFunction->HasMetaData(FBlueprintMetadata::MD_MapParam))
		{
			// If the pin in question is part of a Set (inferred) parameter, then ignore pin matching:
			// @todo:  This assumes that the wildcard coercion has done its job...I'd feel better if 
			// there was some easier way of accessing the target container type
			if(FEdGraphUtilities::IsMapParam(OwningFunction, SourcePin->PinName))
			{
				return true;
			}
		}
	}

	if (bTypeMismatch)
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("TypeDoesNotMatchPropertyOfType_Error", "@@ of type %s doesn't match the property %s of type %s").ToString(),
			*UEdGraphSchema_K2::TypeToText(Type).ToString(),
			*Property->GetName(),
			*UEdGraphSchema_K2::TypeToText(Property).ToString()),
			SourcePin);
	}

	// Now check the direction if it is parameter coming in or out of a function call style node (variable nodes are excluded since they maybe local parameters)
	if (Property->HasAnyPropertyFlags(CPF_Parm) && !SourcePin->GetOwningNode()->IsA(UK2Node_Variable::StaticClass()))
	{
		// Parameters are directional
		const bool bOutParam = Property->HasAllPropertyFlags(CPF_ReturnParm) || (Property->HasAllPropertyFlags(CPF_OutParm) && !Property->HasAnyPropertyFlags(CPF_ReferenceParm));

		if ( ((SourcePin->Direction == EGPD_Input) && bOutParam) || ((SourcePin->Direction == EGPD_Output) && !bOutParam))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("DirectionMismatchParameter_Error", "The direction of @@ doesn't match the direction of parameter %s").ToString(), *Property->GetName()), SourcePin);
		}

 		if (Property->HasAnyPropertyFlags(CPF_ReferenceParm))
 		{
			TArray<FString> AutoEmittedTerms;
			Schema->GetAutoEmitTermParameters(OwningFunction, AutoEmittedTerms);
			const bool bIsAutoEmittedTerm = AutoEmittedTerms.Contains(SourcePin->PinName);

			// Make sure reference parameters are linked, except for FTransforms, which have a special node handler that adds an internal constant term
 			if (!bIsAutoEmittedTerm
				&& (SourcePin->LinkedTo.Num() == 0)
				&& (!SourcePin->PinType.PinSubCategoryObject.IsValid() || SourcePin->PinType.PinSubCategoryObject.Get()->GetName() != TEXT("Transform")) )
 			{
 				MessageLog.Error(*LOCTEXT("PassLiteral_Error", "Cannot pass a literal to @@.  Connect a variable to it instead.").ToString(), SourcePin);
 			}
 		}
	}

	return NumErrorsAtStart == MessageLog.NumErrors;
}

uint32 FKismetCompilerUtilities::ConsignToOblivionCounter = 0;

// Rename a class and it's CDO into the transient package, and clear RF_Public on both of them
void FKismetCompilerUtilities::ConsignToOblivion(UClass* OldClass, bool bForceNoResetLoaders)
{
	if (OldClass != NULL)
	{
		// Use the Kismet class reinstancer to ensure that the CDO and any existing instances of this class are cleaned up!
		auto CTOResinstancer = FBlueprintCompileReinstancer::Create(OldClass);

		UPackage* OwnerOutermost = OldClass->GetOutermost();
		if( OldClass->ClassDefaultObject )
		{
			// rename to a temp name, move into transient package
			OldClass->ClassDefaultObject->ClearFlags(RF_Public);
			OldClass->ClassDefaultObject->SetFlags(RF_Transient);
			OldClass->ClassDefaultObject->RemoveFromRoot(); // make sure no longer in root set
		}
		
		OldClass->SetMetaData(FBlueprintMetadata::MD_IsBlueprintBase, TEXT("false"));
		OldClass->ClearFlags(RF_Public);
		OldClass->SetFlags(RF_Transient);
		OldClass->ClassFlags |= CLASS_Deprecated|CLASS_NewerVersionExists;
		OldClass->RemoveFromRoot(); // make sure no longer in root set

		// Invalidate the export for all old properties, to make sure they don't get partially reloaded and corrupt the class
		for( TFieldIterator<UProperty> It(OldClass,EFieldIteratorFlags::ExcludeSuper); It; ++It )
		{
			UProperty* Current = *It;
			InvalidatePropertyExport(Current);
		}

		for( TFieldIterator<UFunction> ItFunc(OldClass,EFieldIteratorFlags::ExcludeSuper); ItFunc; ++ItFunc )
		{
			UFunction* CurrentFunc = *ItFunc;
			FLinkerLoad::InvalidateExport(CurrentFunc);

			for( TFieldIterator<UProperty> It(CurrentFunc,EFieldIteratorFlags::ExcludeSuper); It; ++It )
			{
				UProperty* Current = *It;
				InvalidatePropertyExport(Current);
			}
		}

		const FString BaseName = FString::Printf(TEXT("DEADCLASS_%s_C_%d"), *OldClass->ClassGeneratedBy->GetName(), ConsignToOblivionCounter++);
		OldClass->Rename(*BaseName, GetTransientPackage(), (REN_DontCreateRedirectors|REN_NonTransactional|(bForceNoResetLoaders ? REN_ForceNoResetLoaders : 0)));

		// Make sure MetaData doesn't have any entries to the class we just renamed out of package
		OwnerOutermost->GetMetaData()->RemoveMetaDataOutsidePackage();
	}
}

void FKismetCompilerUtilities::InvalidatePropertyExport(UProperty* PropertyToInvalidate)
{
	// Arrays need special handling to make sure the inner property is also cleared
	UArrayProperty* ArrayProp = Cast<UArrayProperty>(PropertyToInvalidate);
 	if( ArrayProp && ArrayProp->Inner )
 	{
		FLinkerLoad::InvalidateExport(ArrayProp->Inner);
 	}

	FLinkerLoad::InvalidateExport(PropertyToInvalidate);
}

void FKismetCompilerUtilities::RemoveObjectRedirectorIfPresent(UObject* Package, const FString& NewName, UObject* ObjectBeingMovedIn)
{
	// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
	if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(StaticFindObject(UObjectRedirector::StaticClass(), Package, *NewName)))
	{
		ObjectTools::DeleteRedirector(Redirector);
		Redirector = NULL;
	}
}

void FKismetCompilerUtilities::EnsureFreeNameForNewClass(UClass* ClassToConsign, FString& ClassName, UBlueprint* Blueprint)
{
	check(Blueprint);

	UObject* OwnerOutermost = Blueprint->GetOutermost();

	// Try to find a class with the name we want to use in the scope
	UClass* AnyClassWithGoodName = (UClass*)StaticFindObject(UClass::StaticClass(), OwnerOutermost, *ClassName, false);
	if (AnyClassWithGoodName == ClassToConsign)
	{
		// Ignore it if it's the class we're already consigning anyway
		AnyClassWithGoodName = NULL;
	}

	if( ClassToConsign )
	{
		ConsignToOblivion(ClassToConsign, Blueprint->bIsRegeneratingOnLoad);
	}

	// Consign the class with the name we want to use
	if( AnyClassWithGoodName )
	{
		ConsignToOblivion(AnyClassWithGoodName, Blueprint->bIsRegeneratingOnLoad);
	}
}


/** Finds a property by name, starting in the specified scope; Validates property type and returns NULL along with emitting an error if there is a mismatch. */
UProperty* FKismetCompilerUtilities::FindPropertyInScope(UStruct* Scope, UEdGraphPin* Pin, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass)
{
	UStruct* InitialScope = Scope;
	while (Scope != NULL)
	{
		for (TFieldIterator<UProperty> It(Scope, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			UProperty* Property = *It;

			if (Property->GetName() == Pin->PinName)
			{
				if (FKismetCompilerUtilities::IsTypeCompatibleWithProperty(Pin, Property, MessageLog, Schema, SelfClass))
				{
					return Property;
				}
				else
				{
					// Exit now, we found one with the right name but the type mismatched (and there was a type mismatch error)
					return NULL;
				}
			}
		}

		// Functions don't automatically check their class when using a field iterator
		UFunction* Function = Cast<UFunction>(Scope);
		Scope = (Function != NULL) ? Cast<UStruct>(Function->GetOuter()) : NULL;
	}

	// Couldn't find the name
	if (!FKismetCompilerUtilities::IsMissingMemberPotentiallyLoading(Cast<UBlueprint>(SelfClass->ClassGeneratedBy), InitialScope))
	{
		MessageLog.Error(*LOCTEXT("PropertyNotFound_Error", "The property associated with @@ could not be found").ToString(), Pin);
	}
	return NULL;
}

// Finds a property by name, starting in the specified scope, returning NULL if it's not found
UProperty* FKismetCompilerUtilities::FindNamedPropertyInScope(UStruct* Scope, FName PropertyName)
{
	while (Scope != NULL)
	{
		for (TFieldIterator<UProperty> It(Scope, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			UProperty* Property = *It;

			// If we match by name, and var is not deprecated...
			if (Property->GetFName() == PropertyName && !Property->HasAllPropertyFlags(CPF_Deprecated))
			{
				return Property;
			}
		}

		// Functions don't automatically check their class when using a field iterator
		UFunction* Function = Cast<UFunction>(Scope);
		Scope = (Function != NULL) ? Cast<UStruct>(Function->GetOuter()) : NULL;
	}

	return NULL;
}

void FKismetCompilerUtilities::CompileDefaultProperties(UClass* Class)
{
	UObject* DefaultObject = Class->GetDefaultObject(); // Force the default object to be constructed if it isn't already
	check(DefaultObject);
}

void FKismetCompilerUtilities::LinkAddedProperty(UStruct* Structure, UProperty* NewProperty)
{
	check(NewProperty->Next == NULL);
	check(Structure->Children != NewProperty);

	NewProperty->Next = Structure->Children;
	Structure->Children = NewProperty;
}

const UFunction* FKismetCompilerUtilities::FindOverriddenImplementableEvent(const FName& EventName, const UClass* Class)
{
	const uint32 RequiredFlagMask = FUNC_Event | FUNC_BlueprintEvent | FUNC_Native;
	const uint32 RequiredFlagResult = FUNC_Event | FUNC_BlueprintEvent;

	const UFunction* FoundEvent = Class ? Class->FindFunctionByName(EventName, EIncludeSuperFlag::ExcludeSuper) : NULL;

	const bool bFlagsMatch = (NULL != FoundEvent) && (RequiredFlagResult == ( FoundEvent->FunctionFlags & RequiredFlagMask ));

	return bFlagsMatch ? FoundEvent : NULL;
}

void FKismetCompilerUtilities::ValidateEnumProperties(UObject* DefaultObject, FCompilerResultsLog& MessageLog)
{
	check(DefaultObject);
	for (TFieldIterator<UProperty> It(DefaultObject->GetClass()); It; ++It)
	{
		UProperty* Property = *It;

		if(!Property->HasAnyPropertyFlags(CPF_Transient))
		{
			const UEnum* Enum = nullptr;
			const UNumericProperty* UnderlyingProp = nullptr;
			if (const UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
			{
				Enum = EnumProperty->GetEnum();
				UnderlyingProp = EnumProperty->GetUnderlyingProperty();
			}
			else if(const UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
			{
				Enum = ByteProperty->GetIntPropertyEnum();
				UnderlyingProp = ByteProperty;
			}

			if(Enum)
			{
				const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(DefaultObject));
				if(!Enum->IsValidEnumValue(EnumValue))
				{
					MessageLog.Warning(
						*FString::Printf(
							*LOCTEXT("InvalidEnumDefaultValue_Error", "Default Enum value '%s' for class '%s' is invalid in object '%s'. EnumVal: %lld. EnumAcceptableMax: %lld ").ToString(),
							*Property->GetName(),
							*DefaultObject->GetClass()->GetName(),
							*DefaultObject->GetName(),
							EnumValue,
							Enum->GetMaxEnumValue()
						)
					);
				}
			}
		}
	}
}

bool FKismetCompilerUtilities::ValidateSelfCompatibility(const UEdGraphPin* Pin, FKismetFunctionContext& Context)
{
	const UBlueprint* Blueprint = Context.Blueprint;
	const UEdGraph* SourceGraph = Context.SourceGraph;
	const UEdGraphSchema_K2* K2Schema = Context.Schema;
	const UBlueprintGeneratedClass* BPClass = Context.NewClass;

	FString ErrorMsg;
	if (Blueprint->BlueprintType != BPTYPE_FunctionLibrary && K2Schema->IsStaticFunctionGraph(SourceGraph))
	{
		ErrorMsg = FString::Printf(*LOCTEXT("PinMustHaveConnection_Static_Error", "'@@' must have a connection, because %s is a static function and will not be bound to instances of this blueprint.").ToString(), *SourceGraph->GetName());
	}
	else
	{
		FEdGraphPinType SelfType;
		SelfType.PinCategory = K2Schema->PC_Object;
		SelfType.PinSubCategory = K2Schema->PSC_Self;

		if (!K2Schema->ArePinTypesCompatible(SelfType, Pin->PinType, BPClass))
		{
			FString PinType = Pin->PinType.PinCategory;
			if ((Pin->PinType.PinCategory == K2Schema->PC_Object) ||
				(Pin->PinType.PinCategory == K2Schema->PC_Interface) ||
				(Pin->PinType.PinCategory == K2Schema->PC_Class))
			{
				if (Pin->PinType.PinSubCategoryObject.IsValid())
				{
					PinType = Pin->PinType.PinSubCategoryObject->GetName();
				}
				else
				{
					PinType = TEXT("");
				}
			}

			if (PinType.IsEmpty())
			{
				ErrorMsg = FString::Printf(*LOCTEXT("PinMustHaveConnection_NoType_Error", "This blueprint (self) is not compatible with '@@', therefore that pin must have a connection.").ToString());
			}
			else
			{
				ErrorMsg = FString::Printf(*LOCTEXT("PinMustHaveConnection_WrongClass_Error", "This blueprint (self) is not a %s, therefore '@@' must have a connection.").ToString(), *PinType);
			}
		}
	}

	if (!ErrorMsg.IsEmpty())
	{
		Context.MessageLog.Error(*ErrorMsg, Pin);
		return false;
	}

	return true;
}

UEdGraphPin* FKismetCompilerUtilities::GenerateAssignmentNodes(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* CallBeginSpawnNode, UEdGraphNode* SpawnNode, UEdGraphPin* CallBeginResult, const UClass* ForClass )
{
	static FString ObjectParamName = FString(TEXT("Object"));
	static FString ValueParamName = FString(TEXT("Value"));
	static FString PropertyNameParamName = FString(TEXT("PropertyName"));

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	UEdGraphPin* LastThen = CallBeginSpawnNode->GetThenPin();

	// Create 'set var by name' nodes and hook them up
	for (int32 PinIdx = 0; PinIdx < SpawnNode->Pins.Num(); PinIdx++)
	{
		// Only create 'set param by name' node if this pin is linked to something
		UEdGraphPin* OrgPin = SpawnNode->Pins[PinIdx];
		const bool bHasDefaultValue = !OrgPin->DefaultValue.IsEmpty() || !OrgPin->DefaultTextValue.IsEmpty() || OrgPin->DefaultObject;
		if (NULL == CallBeginSpawnNode->FindPin(OrgPin->PinName) &&
			(OrgPin->LinkedTo.Num() > 0 || bHasDefaultValue))
		{
			if( OrgPin->LinkedTo.Num() == 0 )
			{
				UProperty* Property = FindField<UProperty>(ForClass, *OrgPin->PinName);
				// NULL property indicates that this pin was part of the original node, not the 
				// class we're assigning to:
				if( !Property )
				{
					continue;
				}

				// We don't want to generate an assignment node unless the default value 
				// differs from the value in the CDO:
				FString DefaultValueAsString;
					
				if (FBlueprintCompilationManager::GetDefaultValue(ForClass, Property, DefaultValueAsString))
				{
					if (DefaultValueAsString == OrgPin->GetDefaultAsString())
					{
						continue;
					}
				}
				else if(ForClass->ClassDefaultObject)
				{
					FBlueprintEditorUtils::PropertyValueToString(Property, (uint8*)ForClass->ClassDefaultObject, DefaultValueAsString);

					if (DefaultValueAsString == OrgPin->GetDefaultAsString())
					{
						continue;
					}
				}
			}

			UFunction* SetByNameFunction = Schema->FindSetVariableByNameFunction(OrgPin->PinType);
			if (SetByNameFunction)
			{
				UK2Node_CallFunction* SetVarNode = nullptr;
				if (OrgPin->PinType.IsArray())
				{
					SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(SpawnNode, SourceGraph);
				}
				else
				{
					SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(SpawnNode, SourceGraph);
				}
				SetVarNode->SetFromFunction(SetByNameFunction);
				SetVarNode->AllocateDefaultPins();

				// Connect this node into the exec chain
				Schema->TryCreateConnection(LastThen, SetVarNode->GetExecPin());
				LastThen = SetVarNode->GetThenPin();

				// Connect the new actor to the 'object' pin
				UEdGraphPin* ObjectPin = SetVarNode->FindPinChecked(ObjectParamName);
				CallBeginResult->MakeLinkTo(ObjectPin);

				// Fill in literal for 'property name' pin - name of pin is property name
				UEdGraphPin* PropertyNamePin = SetVarNode->FindPinChecked(PropertyNameParamName);
				PropertyNamePin->DefaultValue = OrgPin->PinName;

				UEdGraphPin* ValuePin = SetVarNode->FindPinChecked(ValueParamName);
				if (OrgPin->LinkedTo.Num() == 0 &&
					OrgPin->DefaultValue != FString() &&
					OrgPin->PinType.PinCategory == Schema->PC_Byte &&
					OrgPin->PinType.PinSubCategoryObject.IsValid() &&
					OrgPin->PinType.PinSubCategoryObject->IsA<UEnum>())
				{
					// Pin is an enum, we need to alias the enum value to an int:
					UK2Node_EnumLiteral* EnumLiteralNode = CompilerContext.SpawnIntermediateNode<UK2Node_EnumLiteral>(SpawnNode, SourceGraph);
					EnumLiteralNode->Enum = CastChecked<UEnum>(OrgPin->PinType.PinSubCategoryObject.Get());
					EnumLiteralNode->AllocateDefaultPins();
					EnumLiteralNode->FindPinChecked(Schema->PN_ReturnValue)->MakeLinkTo(ValuePin);

					UEdGraphPin* InPin = EnumLiteralNode->FindPinChecked(UK2Node_EnumLiteral::GetEnumInputPinName());
					check( InPin );
					InPin->DefaultValue = OrgPin->DefaultValue;
				}
				else
				{
					// For non-array struct pins that are not linked, transfer the pin type so that the node will expand an auto-ref that will assign the value by-ref.
					if (OrgPin->PinType.IsArray() == false && OrgPin->PinType.PinCategory == Schema->PC_Struct && OrgPin->LinkedTo.Num() == 0)
					{
						ValuePin->PinType.PinCategory = OrgPin->PinType.PinCategory;
						ValuePin->PinType.PinSubCategory = OrgPin->PinType.PinSubCategory;
						ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
						CompilerContext.MovePinLinksToIntermediate(*OrgPin, *ValuePin);
					}
					else
					{
						CompilerContext.MovePinLinksToIntermediate(*OrgPin, *ValuePin);
						SetVarNode->PinConnectionListChanged(ValuePin);
					}

				}
			}
		}
	}

	return LastThen;
}

void FKismetCompilerUtilities::CreateObjectAssignmentStatement(FKismetFunctionContext& Context, UEdGraphNode* Node, FBPTerminal* SrcTerm, FBPTerminal* DstTerm)
{
	UClass* InputObjClass = Cast<UClass>(SrcTerm->Type.PinSubCategoryObject.Get());
	UClass* OutputObjClass = Cast<UClass>(DstTerm->Type.PinSubCategoryObject.Get());

	const bool bIsOutputInterface = ((OutputObjClass != NULL) && OutputObjClass->HasAnyClassFlags(CLASS_Interface));
	const bool bIsInputInterface = ((InputObjClass != NULL) && InputObjClass->HasAnyClassFlags(CLASS_Interface));

	if (bIsOutputInterface != bIsInputInterface)
	{
		// Create a literal term from the class specified in the node
		FBPTerminal* ClassTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
		ClassTerm->Name = GetNameSafe(OutputObjClass);
		ClassTerm->bIsLiteral = true;
		ClassTerm->Source = DstTerm->Source;
		ClassTerm->ObjectLiteral = OutputObjClass;
		check(Context.Schema);
		ClassTerm->Type.PinCategory = Context.Schema->PC_Class;

		EKismetCompiledStatementType CastOpType = bIsOutputInterface ? KCST_CastObjToInterface : KCST_CastInterfaceToObj;
		FBlueprintCompiledStatement& CastStatement = Context.AppendStatementForNode(Node);
		CastStatement.Type = CastOpType;
		CastStatement.LHS = DstTerm;
		CastStatement.RHS.Add(ClassTerm);
		CastStatement.RHS.Add(SrcTerm);
	}
	else
	{
		FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
		Statement.Type = KCST_Assignment;
		Statement.LHS = DstTerm;
		Statement.RHS.Add(SrcTerm);
	}
}

UProperty* FKismetCompilerUtilities::CreatePrimitiveProperty(UObject* PropertyScope, const FName& ValidatedPropertyName, const FString& PinCategory, const FString& PinSubCategory, UObject* PinSubCategoryObject, UClass* SelfClass, bool bIsWeakPointer, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
{
	const EObjectFlags ObjectFlags = RF_Public;

	UProperty* NewProperty = nullptr;
	//@TODO: Nasty string if-else tree
	if ((PinCategory == Schema->PC_Object) || (PinCategory == Schema->PC_Interface) || (PinCategory == Schema->PC_SoftObject))
	{
		UClass* SubType = (PinSubCategory == Schema->PSC_Self) ? SelfClass : Cast<UClass>(PinSubCategoryObject);

		if (SubType == nullptr)
		{
			// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
			SubType = UObject::StaticClass();
		}

		if (SubType)
		{
			const bool bIsInterface = SubType->HasAnyClassFlags(CLASS_Interface)
				|| ((SubType == SelfClass) && ensure(SelfClass->ClassGeneratedBy) && FBlueprintEditorUtils::IsInterfaceBlueprint(CastChecked<UBlueprint>(SelfClass->ClassGeneratedBy)));

			if (bIsInterface)
			{
				UInterfaceProperty* NewPropertyObj = NewObject<UInterfaceProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				// we want to use this setter function instead of setting the 
				// InterfaceClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				NewPropertyObj->SetInterfaceClass(SubType);
				NewProperty = NewPropertyObj;
			}
			else
			{
				UObjectPropertyBase* NewPropertyObj = NULL;

				if (PinCategory == Schema->PC_SoftObject)
				{
					NewPropertyObj = NewObject<USoftObjectProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				else if (bIsWeakPointer)
				{
					NewPropertyObj = NewObject<UWeakObjectProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				else
				{
					NewPropertyObj = NewObject<UObjectProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				// we want to use this setter function instead of setting the 
				// PropertyClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				NewPropertyObj->SetPropertyClass(SubType);
				NewPropertyObj->SetPropertyFlags(CPF_HasGetValueTypeHash);
				NewProperty = NewPropertyObj;
			}
		}
	}
	else if (PinCategory == Schema->PC_Struct)
	{
		if (UScriptStruct* SubType = Cast<UScriptStruct>(PinSubCategoryObject))
		{
			FString StructureError;
			if (FStructureEditorUtils::EStructureError::Ok == FStructureEditorUtils::IsStructureValid(SubType, NULL, &StructureError))
			{
				UStructProperty* NewPropertyStruct = NewObject<UStructProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				NewPropertyStruct->Struct = SubType;
				NewProperty = NewPropertyStruct;

				if (SubType->StructFlags & STRUCT_HasInstancedReference)
				{
					NewProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
				}

				if (FBlueprintEditorUtils::StructHasGetTypeHash(SubType))
				{
					// tag the type as hashable to avoid crashes in core:
					NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
				}
			}
			else
			{
				MessageLog.Error(
					*FString::Printf(
						*LOCTEXT("InvalidStructForField_Error", "Invalid property '%s' structure '%s' error: %s").ToString(),
						*ValidatedPropertyName.ToString(),
						*SubType->GetName(),
						*StructureError
						));
			}
		}
	}
	else if ((PinCategory == Schema->PC_Class) || (PinCategory == Schema->PC_SoftClass))
	{
		UClass* SubType = Cast<UClass>(PinSubCategoryObject);

		if (SubType == nullptr)
		{
			// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
			SubType = UObject::StaticClass();

			MessageLog.Warning(
				*FString::Printf(
					*LOCTEXT("InvalidClassForField_Error", "Invalid property '%s' class, replaced with Object.  Please fix or remove.").ToString(),
					*ValidatedPropertyName.ToString()
					));
		}

		if (SubType)
		{
			if (PinCategory == Schema->PC_SoftClass)
			{
				USoftClassProperty* SoftClassProperty = NewObject<USoftClassProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				// we want to use this setter function instead of setting the 
				// MetaClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				SoftClassProperty->SetMetaClass(SubType);
				SoftClassProperty->PropertyClass = UClass::StaticClass();
				SoftClassProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
				NewProperty = SoftClassProperty;
			}
			else
			{
				UClassProperty* NewPropertyClass = NewObject<UClassProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				// we want to use this setter function instead of setting the 
				// MetaClass member directly, because it properly handles  
				// placeholder classes (classes that are stubbed in during load)
				NewPropertyClass->SetMetaClass(SubType);
				NewPropertyClass->PropertyClass = UClass::StaticClass();
				NewPropertyClass->SetPropertyFlags(CPF_HasGetValueTypeHash);
				NewProperty = NewPropertyClass;
			}
		}
	}
	else if (PinCategory == Schema->PC_Int)
	{
		NewProperty = NewObject<UIntProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == Schema->PC_Float)
	{
		NewProperty = NewObject<UFloatProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == Schema->PC_Boolean)
	{
		UBoolProperty* BoolProperty = NewObject<UBoolProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		BoolProperty->SetBoolSize(sizeof(bool), true);
		NewProperty = BoolProperty;
	}
	else if (PinCategory == Schema->PC_String)
	{
		NewProperty = NewObject<UStrProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == Schema->PC_Text)
	{
		NewProperty = NewObject<UTextProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else if (PinCategory == Schema->PC_Byte)
	{
		UEnum* Enum = Cast<UEnum>(PinSubCategoryObject);

		if (Enum && Enum->GetCppForm() == UEnum::ECppForm::EnumClass)
		{
			UEnumProperty* EnumProp = NewObject<UEnumProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
			UNumericProperty* UnderlyingProp = NewObject<UByteProperty>(EnumProp, TEXT("UnderlyingType"), ObjectFlags);

			EnumProp->SetEnum(Enum);
			EnumProp->AddCppProperty(UnderlyingProp);

			NewProperty = EnumProp;
		}
		else
		{
			UByteProperty* ByteProp = NewObject<UByteProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
			ByteProp->Enum = Cast<UEnum>(PinSubCategoryObject);

			NewProperty = ByteProp;
		}

		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else if (PinCategory == Schema->PC_Name)
	{
		NewProperty = NewObject<UNameProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}
	else
	{
		// Failed to resolve the type-subtype, create a generic property to survive VM bytecode emission
		NewProperty = NewObject<UIntProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
	}

	return NewProperty;
}

/** Creates a property named PropertyName of type PropertyType in the Scope or returns NULL if the type is unknown, but does *not* link that property in */
UProperty* FKismetCompilerUtilities::CreatePropertyOnScope(UStruct* Scope, const FName& PropertyName, const FEdGraphPinType& Type, UClass* SelfClass, uint64 PropertyFlags, const UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
{
	const EObjectFlags ObjectFlags = RF_Public;

	FName ValidatedPropertyName = PropertyName;

	// Check to see if there's already a object on this scope with the same name, and throw an internal compiler error if so
	// If this happens, it breaks the property link, which causes stack corruption and hard-to-track errors, so better to fail at this point
	{
		if (UObject* ExistingObject = CheckPropertyNameOnScope(Scope, PropertyName))
		{
			const FString ScopeName((Scope != nullptr) ? Scope->GetName() : FString(TEXT("None")));
			const FString ExistingTypeAndPath(ExistingObject->GetFullName(Scope));
			MessageLog.Error(*FString::Printf(TEXT("Internal Compiler Error: Tried to create a property %s in scope %s, but another object (%s) already already exists there."), *PropertyName.ToString(), *ScopeName, *ExistingTypeAndPath));

			// Find a free name, so we can still create the property to make it easier to spot the duplicates, and avoid crashing
			uint32 Counter = 0;
			FName TestName;
			do 
			{
				FString TestNameString = PropertyName.ToString() + FString::Printf(TEXT("_ERROR_DUPLICATE_%d"), Counter++);
				TestName = FName(*TestNameString);

			} while (CheckPropertyNameOnScope(Scope, TestName) != nullptr);

			ValidatedPropertyName = TestName;
		}
	}

	UProperty* NewProperty = nullptr;
	UObject* PropertyScope = nullptr;

	// Handle creating a container property, if necessary
	const bool bIsMapProperty = Type.IsMap();
	const bool bIsSetProperty = Type.IsSet();
	const bool bIsArrayProperty = Type.IsArray();
	UMapProperty* NewMapProperty = nullptr;
	USetProperty* NewSetProperty = nullptr;
	UArrayProperty* NewArrayProperty = nullptr;
	UProperty* NewContainerProperty = nullptr;
	if (bIsMapProperty)
	{
		NewMapProperty = NewObject<UMapProperty>(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewMapProperty;
		NewContainerProperty = NewMapProperty;
	}
	else if (bIsSetProperty)
	{
		NewSetProperty = NewObject<USetProperty>(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewSetProperty;
		NewContainerProperty = NewSetProperty;
	}
	else if( bIsArrayProperty )
	{
		NewArrayProperty = NewObject<UArrayProperty>(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewArrayProperty;
		NewContainerProperty = NewArrayProperty;
	}
	else
	{
		PropertyScope = Scope;
	}

	if (Type.PinCategory == Schema->PC_Delegate)
	{
		if (UFunction* SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Type.PinSubCategoryMemberReference))
		{
			UDelegateProperty* NewPropertyDelegate = NewObject<UDelegateProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewPropertyDelegate->SignatureFunction = SignatureFunction;
			NewProperty = NewPropertyDelegate;
		}
	}
	else if (Type.PinCategory == Schema->PC_MCDelegate)
	{
		UFunction* const SignatureFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Type.PinSubCategoryMemberReference);
		UMulticastDelegateProperty* NewPropertyDelegate = NewObject<UMulticastDelegateProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewPropertyDelegate->SignatureFunction = SignatureFunction;
		NewProperty = NewPropertyDelegate;
	}
	else
	{
		NewProperty = CreatePrimitiveProperty(PropertyScope, ValidatedPropertyName, Type.PinCategory, Type.PinSubCategory, Type.PinSubCategoryObject.Get(), SelfClass, Type.bIsWeakPointer, Schema, MessageLog);
	}

	if (NewContainerProperty && NewProperty && NewProperty->HasAnyPropertyFlags(CPF_ContainsInstancedReference))
	{
		NewContainerProperty->SetPropertyFlags(CPF_ContainsInstancedReference);
	}

	if (bIsMapProperty)
	{
		if (NewProperty)
		{
			if (!NewProperty->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
			{
				MessageLog.Error(
					*FString::Printf(
						*LOCTEXT("MapKeyTypeUnhashable_Error", "Map Property @@ has key type of %s which cannot be hashed and is therefore invalid").ToString(),
						*(Schema->GetCategoryText(Type.PinCategory).ToString())),
					NewMapProperty
				);
			}
			// make the value property:
			// not feeling good about myself..
			// Fix up the array property to have the new type-specific property as its inner, and return the new UArrayProperty
			NewMapProperty->KeyProp = NewProperty;
			// make sure the value property does not collide with the key property:
			FName ValueName = FName( *(ValidatedPropertyName.GetPlainNameString() + FString(TEXT("_Value") )) );
			NewMapProperty->ValueProp = CreatePrimitiveProperty(PropertyScope, ValueName, Type.PinValueType.TerminalCategory, Type.PinValueType.TerminalSubCategory, Type.PinValueType.TerminalSubCategoryObject.Get(), SelfClass, Type.bIsWeakPointer, Schema, MessageLog);;
			if (!NewMapProperty->ValueProp)
			{
				NewProperty->MarkPendingKill();
				NewProperty = nullptr;
				NewMapProperty->MarkPendingKill();
			}
			else
			{
				if (NewMapProperty->ValueProp && NewMapProperty->ValueProp->HasAnyPropertyFlags(CPF_ContainsInstancedReference))
				{
					NewMapProperty->ValueProp->SetPropertyFlags(CPF_ContainsInstancedReference);
				}

				NewProperty = NewMapProperty;
			}
		}
		else
		{
			NewMapProperty->MarkPendingKill();
		}
	}
	else if (bIsSetProperty)
	{
		if (NewProperty)
		{
			if (!NewProperty->HasAnyPropertyFlags(CPF_HasGetValueTypeHash))
			{
				MessageLog.Error(
					*FString::Printf(
						*LOCTEXT("SetKeyTypeUnhashable_Error", "Set Property @@ has contained type of %s which cannot be hashed and is therefore invalid").ToString(), 
						*(Schema->GetCategoryText(Type.PinCategory).ToString())),
					NewSetProperty
				);

				// We need to be able to serialize (for CPFUO to migrate data), so force the 
				// property to hash:
				NewProperty->SetPropertyFlags(CPF_HasGetValueTypeHash);
			}
			NewSetProperty->ElementProp = NewProperty;
			NewProperty = NewSetProperty;
		}
		else
		{
			NewSetProperty->MarkPendingKill();
		}
	}
	else if (bIsArrayProperty)
	{
		if (NewProperty)
		{
			// Fix up the array property to have the new type-specific property as its inner, and return the new UArrayProperty
			NewArrayProperty->Inner = NewProperty;
			NewProperty = NewArrayProperty;
		}
		else
		{
			NewArrayProperty->MarkPendingKill();
		}
	}

	if (NewProperty)
	{
		NewProperty->SetPropertyFlags(PropertyFlags);
	}

	return NewProperty;
}

UObject* FKismetCompilerUtilities::CheckPropertyNameOnScope(UStruct* Scope, const FName& PropertyName)
{
	FString NameStr = PropertyName.ToString();

	if (UObject* ExistingObject = FindObject<UObject>(Scope, *NameStr, false))
	{
		return ExistingObject;
	}

	if (Scope && !Scope->IsA<UFunction>() && (UBlueprintGeneratedClass::GetUberGraphFrameName() != PropertyName))
	{
		if (auto Field = FindField<UProperty>(Scope->GetSuperStruct(), *NameStr))
		{
			return Field;
		}
	}

	return nullptr;
}

/** Checks if the execution path ends with a Return node */
void FKismetCompilerUtilities::ValidateProperEndExecutionPath(FKismetFunctionContext& Context)
{
	struct FRecrursiveHelper
	{
		static bool IsExecutionSequence(const UEdGraphNode* Node)
		{
			return Node && (UK2Node_ExecutionSequence::StaticClass() == Node->GetClass()); // no "SourceNode->IsA<UK2Node_ExecutionSequence>()" because MultiGate is based on ExecutionSequence
		}

		static void CheckPathEnding(const UK2Node* StartingNode, TSet<const UK2Node*>& VisitedNodes, FKismetFunctionContext& InContext, bool bPathShouldEndWithReturn, TSet<const UK2Node*>& BreakableNodesSeeds)
		{
			const UK2Node* CurrentNode = StartingNode;
			while (CurrentNode)
			{
				const UK2Node* SourceNode = CurrentNode;
				CurrentNode = nullptr;

				bool bAlreadyVisited = false;
				VisitedNodes.Add(SourceNode, &bAlreadyVisited);
				if (!bAlreadyVisited && !SourceNode->IsA<UK2Node_FunctionResult>())
				{
					const bool bIsExecutionSequence = IsExecutionSequence(SourceNode); 
					for (auto CurrentPin : SourceNode->Pins)
					{
						if (CurrentPin
							&& (CurrentPin->Direction == EEdGraphPinDirection::EGPD_Output)
							&& (CurrentPin->PinType.PinCategory == InContext.Schema->PC_Exec))
						{
							if (!CurrentPin->LinkedTo.Num())
							{
								if (!bIsExecutionSequence)
								{
									BreakableNodesSeeds.Add(SourceNode);
								}
								if (bPathShouldEndWithReturn && !bIsExecutionSequence)
								{
									InContext.MessageLog.Note(*LOCTEXT("ExecutionEnd_Note", "The execution path doesn't end with a return node. @@").ToString(), CurrentPin);
								}
								continue;
							}
							auto LinkedPin = CurrentPin->LinkedTo[0];
							auto NextNode = ensure(LinkedPin) ? Cast<const UK2Node>(LinkedPin->GetOwningNodeUnchecked()) : nullptr;
							ensure(NextNode);
							if (CurrentNode)
							{
								FRecrursiveHelper::CheckPathEnding(CurrentNode, VisitedNodes, InContext, bPathShouldEndWithReturn && !bIsExecutionSequence, BreakableNodesSeeds);
							}
							CurrentNode = NextNode;
						}
					}
				}
			}
		}

		static void GatherBreakableNodes(const UK2Node* StartingNode, TSet<const UK2Node*>& BreakableNodes, FKismetFunctionContext& InContext)
		{
			const UK2Node* CurrentNode = StartingNode;
			while (CurrentNode)
			{
				const UK2Node* SourceNode = CurrentNode;
				CurrentNode = nullptr;

				bool bAlreadyVisited = false;
				BreakableNodes.Add(SourceNode, &bAlreadyVisited);
				if (!bAlreadyVisited)
				{
					for (auto CurrentPin : SourceNode->Pins)
					{
						if (CurrentPin
							&& (CurrentPin->Direction == EEdGraphPinDirection::EGPD_Input)
							&& (CurrentPin->PinType.PinCategory == InContext.Schema->PC_Exec)
							&& CurrentPin->LinkedTo.Num())
						{
							for (auto LinkedPin : CurrentPin->LinkedTo)
							{
								auto NextNode = ensure(LinkedPin) ? Cast<const UK2Node>(LinkedPin->GetOwningNodeUnchecked()) : nullptr;
								ensure(NextNode);
								if (!FRecrursiveHelper::IsExecutionSequence(NextNode))
								{
									if (CurrentNode)
									{
										GatherBreakableNodes(CurrentNode, BreakableNodes, InContext);
									}
									CurrentNode = NextNode;
								}
							}
						}
					}
				}
			}
		}

		static void GatherBreakableNodesSeedsFromSequences(TSet<const UK2Node_ExecutionSequence*>& UnBreakableExecutionSequenceNodes
			, TSet<const UK2Node*>& BreakableNodesSeeds
			, TSet<const UK2Node*>& BreakableNodes
			, FKismetFunctionContext& InContext)
		{
			for (auto SequenceNode : UnBreakableExecutionSequenceNodes)
			{
				bool bIsBreakable = true;
				// Sequence is breakable when all it's outputs are breakable
				for (auto CurrentPin : SequenceNode->Pins)
				{
					if (CurrentPin
						&& (CurrentPin->Direction == EEdGraphPinDirection::EGPD_Output)
						&& (CurrentPin->PinType.PinCategory == InContext.Schema->PC_Exec)
						&& CurrentPin->LinkedTo.Num())
					{
						auto LinkedPin = CurrentPin->LinkedTo[0];
						auto NextNode = ensure(LinkedPin) ? Cast<const UK2Node>(LinkedPin->GetOwningNodeUnchecked()) : nullptr;
						ensure(NextNode);
						if (!BreakableNodes.Contains(NextNode))
						{
							bIsBreakable = false;
							break;
						}
					}
				}

				if (bIsBreakable)
				{
					bool bWasAlreadyBreakable = false;
					BreakableNodesSeeds.Add(SequenceNode, &bWasAlreadyBreakable);
					ensure(!bWasAlreadyBreakable);
					int32 WasRemoved = UnBreakableExecutionSequenceNodes.Remove(SequenceNode);
					ensure(WasRemoved);
				}
			}
		}

		static void CheckDeadExecutionPath(TSet<const UK2Node*>& BreakableNodesSeeds, FKismetFunctionContext& InContext)
		{
			TSet<const UK2Node_ExecutionSequence*> UnBreakableExecutionSequenceNodes;
			for (auto Node : InContext.SourceGraph->Nodes)
			{
				if (FRecrursiveHelper::IsExecutionSequence(Node))
				{
					UnBreakableExecutionSequenceNodes.Add(Cast<UK2Node_ExecutionSequence>(Node));
				}
			}

			TSet<const UK2Node*> BreakableNodes;
			while (BreakableNodesSeeds.Num())
			{
				for (auto StartingNode : BreakableNodesSeeds)
				{
					GatherBreakableNodes(StartingNode, BreakableNodes, InContext);
				}
				BreakableNodesSeeds.Empty();
				FRecrursiveHelper::GatherBreakableNodesSeedsFromSequences(UnBreakableExecutionSequenceNodes, BreakableNodesSeeds, BreakableNodes, InContext);
			}

			for (auto UnBreakableExecutionSequenceNode : UnBreakableExecutionSequenceNodes)
			{
				bool bUnBreakableOutputWasFound = false;
				for (auto CurrentPin : UnBreakableExecutionSequenceNode->Pins)
				{
					if (CurrentPin
						&& (CurrentPin->Direction == EEdGraphPinDirection::EGPD_Output)
						&& (CurrentPin->PinType.PinCategory == InContext.Schema->PC_Exec)
						&& CurrentPin->LinkedTo.Num())
					{
						if (bUnBreakableOutputWasFound)
						{
							InContext.MessageLog.Note(*LOCTEXT("DeadExecution_Note", "The path is never executed. @@").ToString(), CurrentPin);
							break;
						}

						auto LinkedPin = CurrentPin->LinkedTo[0];
						auto NextNode = ensure(LinkedPin) ? Cast<const UK2Node>(LinkedPin->GetOwningNodeUnchecked()) : nullptr;
						ensure(NextNode);
						if (!BreakableNodes.Contains(NextNode))
						{
							bUnBreakableOutputWasFound = true;
						}
					}
				}
			}
		}
	};

	// Function is designed for multiple return nodes.
	if (!Context.IsEventGraph() && Context.SourceGraph && Context.Schema)
	{
		TArray<UK2Node_FunctionResult*> ReturnNodes;
		Context.SourceGraph->GetNodesOfClass(ReturnNodes);
		if (ReturnNodes.Num() && ensure(Context.EntryPoint))
		{
			TSet<const UK2Node*> VisitedNodes, BreakableNodesSeeds;
			FRecrursiveHelper::CheckPathEnding(Context.EntryPoint, VisitedNodes, Context, true, BreakableNodesSeeds);

			// A non-pure node, that lies on a execution path, that may result with "EndThread" state, is called Breakable.
			// The execution path between the node and Return node can be broken.

			FRecrursiveHelper::CheckDeadExecutionPath(BreakableNodesSeeds, Context);
		}
	}
}

void FKismetCompilerUtilities::DetectValuesReturnedByRef(const UFunction* Func, const UK2Node * Node, FCompilerResultsLog& MessageLog)
{
	for (TFieldIterator<UProperty> PropIt(Func); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParam = *PropIt;
		if (FuncParam->HasAllPropertyFlags(CPF_OutParm) && !FuncParam->HasAllPropertyFlags(CPF_ConstParm))
		{
			const FString MessageStr = FString::Printf(
				*LOCTEXT("WrongRefOutput", "No value will be returned by reference. Parameter '%s'. Node: @@").ToString(),
				*FuncParam->GetName());
			if (FuncParam->IsA<UArrayProperty>()) // array is always passed by reference, see FKismetCompilerContext::CreatePropertiesFromList
			{
				MessageLog.Note(*MessageStr, Node);
			}
			else
			{
				MessageLog.Warning(*MessageStr, Node);
			}
		}
	}
}

bool FKismetCompilerUtilities::IsStatementReducible(EKismetCompiledStatementType StatementType)
{
	switch (StatementType)
	{
	case EKismetCompiledStatementType::KCST_Nop:
	case EKismetCompiledStatementType::KCST_UnconditionalGoto:
	case EKismetCompiledStatementType::KCST_ComputedGoto:
	case EKismetCompiledStatementType::KCST_Return:
	case EKismetCompiledStatementType::KCST_EndOfThread:
	case EKismetCompiledStatementType::KCST_Comment:
	case EKismetCompiledStatementType::KCST_DebugSite:
	case EKismetCompiledStatementType::KCST_WireTraceSite:
	case EKismetCompiledStatementType::KCST_GotoReturn:
	case EKismetCompiledStatementType::KCST_AssignmentOnPersistentFrame:
		return true;
	}
	return false;
}

bool FKismetCompilerUtilities::IsMissingMemberPotentiallyLoading(const UBlueprint* SelfBlueprint, const UStruct* MemberOwner)
{
	bool bCouldBeCompiledInOnLoad = false;
	if (SelfBlueprint && SelfBlueprint->bIsRegeneratingOnLoad)
	{
		if (const UClass* OwnerClass = Cast<UClass>(MemberOwner))
		{
			UBlueprint* OwnerBlueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy);
			bCouldBeCompiledInOnLoad = OwnerBlueprint && !OwnerBlueprint->bHasBeenRegenerated;
		}
	}
	return bCouldBeCompiledInOnLoad;
}

//////////////////////////////////////////////////////////////////////////
// FNodeHandlingFunctor

void FNodeHandlingFunctor::ResolveAndRegisterScopedTerm(FKismetFunctionContext& Context, UEdGraphPin* Net, TIndirectArray<FBPTerminal>& NetArray)
{
	// Determine the scope this takes place in
	UStruct* SearchScope = Context.Function;

	UEdGraphPin* SelfPin = CompilerContext.GetSchema()->FindSelfPin(*(Net->GetOwningNode()), EGPD_Input);
	if (SelfPin != NULL)
	{
		SearchScope = Context.GetScopeFromPinType(SelfPin->PinType, Context.NewClass);
	}

	// Find the variable in the search scope
	UProperty* BoundProperty = FKismetCompilerUtilities::FindPropertyInScope(SearchScope, Net, CompilerContext.MessageLog, CompilerContext.GetSchema(), Context.NewClass);
	if (BoundProperty != NULL)
	{
		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
		// Create the term in the list
		FBPTerminal* Term = new (NetArray) FBPTerminal();
		Term->CopyFromPin(Net, Net->PinName);
		Term->AssociatedVarProperty = BoundProperty;
		Term->bPassedByReference = true;
		Context.NetMap.Add(Net, Term);

		// Check if the property is a local variable and mark it so
		if( SearchScope == Context.Function && BoundProperty->GetOuter() == Context.Function)
		{
			Term->SetVarTypeLocal(true);
		}
		else if (BoundProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly) || (Context.IsConstFunction() && Context.NewClass->IsChildOf(SearchScope)))
		{
			// Read-only variables and variables in const classes are both const
			Term->bIsConst = true;
		}

		// Resolve the context term
		if (SelfPin != NULL)
		{
			FBPTerminal** pContextTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(SelfPin));
			Term->Context = (pContextTerm != NULL) ? *pContextTerm : NULL;
		}
	}
}

FBlueprintCompiledStatement& FNodeHandlingFunctor::GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node, UEdGraphPin* ThenExecPin)
{
	UEdGraphNode* TargetNode = NULL;
	if ((ThenExecPin != NULL) && (ThenExecPin->LinkedTo.Num() > 0))
	{
		TargetNode = ThenExecPin->LinkedTo[0]->GetOwningNode();
	}

	if (Context.bCreateDebugData)
	{
		FBlueprintCompiledStatement& TraceStatement = Context.AppendStatementForNode(&Node);
		TraceStatement.Type = Context.GetWireTraceType();
		TraceStatement.Comment = Node.NodeComment.IsEmpty() ? Node.GetName() : Node.NodeComment;
	}

	FBlueprintCompiledStatement& GotoStatement = Context.AppendStatementForNode(&Node);
	GotoStatement.Type = KCST_UnconditionalGoto;
	Context.GotoFixupRequestMap.Add(&GotoStatement, ThenExecPin);

	return GotoStatement;
}

FBlueprintCompiledStatement& FNodeHandlingFunctor::GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node)
{
	UEdGraphPin* ThenExecPin = CompilerContext.GetSchema()->FindExecutionPin(Node, EGPD_Output);
	return GenerateSimpleThenGoto(Context, Node, ThenExecPin);
}

bool FNodeHandlingFunctor::ValidateAndRegisterNetIfLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	if (Net->LinkedTo.Num() == 0)
	{
		// Make sure the default value is valid
		FString DefaultAllowedResult = CompilerContext.GetSchema()->IsCurrentPinDefaultValid(Net);
		if (DefaultAllowedResult != TEXT(""))
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("InvalidDefaultValue_Error", "Default value '%s' for @@ is invalid: '%s'").ToString(), *(Net->GetDefaultAsString()), *DefaultAllowedResult), Net);
			return false;
		}

		FBPTerminal* LiteralTerm = Context.RegisterLiteral(Net);
		Context.LiteralHackMap.Add(Net, LiteralTerm);
	}

	return true;
}

void FNodeHandlingFunctor::SanitizeName(FString& Name)
{
	// Sanitize the name
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		TCHAR& C = Name[i];

		const bool bGoodChar =
			((C >= 'A') && (C <= 'Z')) || ((C >= 'a') && (C <= 'z')) ||		// A-Z (upper and lowercase) anytime
			(C == '_') ||													// _ anytime
			((i > 0) && (C >= '0') && (C <= '9'));							// 0-9 after the first character

		if (!bGoodChar)
		{
			C = '_';
		}
	}
}

FBPTerminal* FNodeHandlingFunctor::RegisterLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	FBPTerminal* Term = nullptr;
	// Make sure the default value is valid
	FString DefaultAllowedResult = CompilerContext.GetSchema()->IsCurrentPinDefaultValid(Net);
	if (!DefaultAllowedResult.IsEmpty())
	{
		FText ErrorFormat = LOCTEXT("InvalidDefault_Error", "The current value of the '@@' pin is invalid: {0}");
		const FText InvalidReasonText = FText::FromString(DefaultAllowedResult);

		FText DefaultValue = FText::FromString(Net->GetDefaultAsString());
		if (!DefaultValue.IsEmpty())
		{
			ErrorFormat = LOCTEXT("InvalidDefaultVal_Error", "The current value ({1}) of the '@@' pin is invalid: {0}");
		}

		FString ErrorString = FText::Format(ErrorFormat, InvalidReasonText, DefaultValue).ToString();
		CompilerContext.MessageLog.Error(*ErrorString, Net);

		// Skip over these properties if they are container or ref properties, because the backend can't emit valid code for them
		if (Net->PinType.IsContainer() || Net->PinType.bIsReference)
		{
			return nullptr;
		}
	}

	Term = Context.RegisterLiteral(Net);
	Context.NetMap.Add(Net, Term);

	return Term;
}

void FNodeHandlingFunctor::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin->bOrphanedPin)
		{
			if (Pin->bNotConnectable && Pin->LinkedTo.Num() > 0)
			{
				// If it is not connectible due to being orphaned no need to warn as we have other messaging for that
				CompilerContext.MessageLog.Warning(*LOCTEXT("NotConnectablePinLinked", "@@ is linked to another pin but is marked as not connectable. This pin connection will not be compiled.").ToString(), Pin);
			}
			else if (!CompilerContext.GetSchema()->IsMetaPin(*Pin)
				|| (Pin->LinkedTo.Num() == 0 && Pin->DefaultObject && CompilerContext.GetSchema()->IsSelfPin(*Pin) ))
			{
				UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(Pin);

				if (Context.NetMap.Find(Net) == nullptr)
				{
					if ((Net->Direction == EGPD_Input) && (Net->LinkedTo.Num() == 0))
					{
						RegisterLiteral(Context, Net);
					}
					else
					{
						RegisterNet(Context, Pin);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FNetNameMapping
FString FNetNameMapping::MakeBaseName(const UEdGraphPin* Net)
{
	UEdGraphNode* Owner = Net->GetOwningNode();
	FString Part1 = Owner->GetDescriptiveCompiledName();

	return FString::Printf(TEXT("%s_%s"), *Part1, *Net->PinName);
}

FString FNetNameMapping::MakeBaseName(const UEdGraphNode* Net)
{
	return FString::Printf(TEXT("%s"), *Net->GetDescriptiveCompiledName());
}

FString FNetNameMapping::MakeBaseName(const UAnimGraphNode_Base* Net)
{
	return FString::Printf(TEXT("%s_%s"), *Net->GetDescriptiveCompiledName(), *Net->NodeGuid.ToString());
}

//////////////////////////////////////////////////////////////////////////
// FKismetFunctionContext

FKismetFunctionContext::FKismetFunctionContext(FCompilerResultsLog& InMessageLog, const UEdGraphSchema_K2* InSchema, UBlueprintGeneratedClass* InNewClass, UBlueprint* InBlueprint, bool bInGeneratingCpp)
	: Blueprint(InBlueprint)
	, SourceGraph(nullptr)
	, EntryPoint(nullptr)
	, Function(nullptr)
	, NewClass(InNewClass)
	, LastFunctionPropertyStorageLocation(nullptr)
	, MessageLog(InMessageLog)
	, Schema(InSchema)
	, bIsUbergraph(false)
	, bCannotBeCalledFromOtherKismet(false)
	, bIsInterfaceStub(false)
	, bIsConstFunction(false)
	, bEnforceConstCorrectness(false)
	// only need debug-data when running in the editor app:
	, bCreateDebugData(GIsEditor && !IsRunningCommandlet())
	, bIsSimpleStubGraphWithNoParams(false)
	, NetFlags(0)
	, SourceEventFromStubGraph(nullptr)
	, bGeneratingCpp(bInGeneratingCpp)
	, bUseFlowStack(true)
{
	NetNameMap = new FNetNameMapping();
	bAllocatedNetNameMap = true;

	// Prevent debug generation when cooking or running other commandlets
	// Compile-on-load will recreate it if the editor is run
	if (IsRunningCommandlet())
	{
		bCreateDebugData = false;
	}
}

FKismetFunctionContext::~FKismetFunctionContext()
{
	if (bAllocatedNetNameMap)
	{
		delete NetNameMap;
		NetNameMap = NULL;
	}

	for (int32 i = 0; i < AllGeneratedStatements.Num(); ++i)
	{
		delete AllGeneratedStatements[i];
	}
}

void FKismetFunctionContext::SetExternalNetNameMap(FNetNameMapping* NewMap)
{
	if (bAllocatedNetNameMap)
	{
		delete NetNameMap;
		NetNameMap = NULL;
	}

	bAllocatedNetNameMap = false;

	NetNameMap = NewMap;
}

bool FKismetFunctionContext::DoesStatementRequiresSwitch(const FBlueprintCompiledStatement* Statement)
{
	return Statement && (
		Statement->Type == KCST_UnconditionalGoto ||
		Statement->Type == KCST_PushState ||
		Statement->Type == KCST_GotoIfNot ||
		Statement->Type == KCST_ComputedGoto ||
		Statement->Type == KCST_EndOfThread ||
		Statement->Type == KCST_EndOfThreadIfNot ||
		Statement->Type == KCST_GotoReturn ||
		Statement->Type == KCST_GotoReturnIfNot);
}

bool FKismetFunctionContext::MustUseSwitchState(const FBlueprintCompiledStatement* ExcludeThisOne) const
{
	for (auto Node : LinearExecutionList)
	{
		auto StatementList = StatementsPerNode.Find(Node);
		if (StatementList)
		{
			for (auto Statement : (*StatementList))
			{
				if (Statement && (Statement != ExcludeThisOne) && DoesStatementRequiresSwitch(Statement))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void FKismetFunctionContext::MergeAdjacentStates()
{
	for (int32 ExecIndex = 0; ExecIndex < LinearExecutionList.Num(); ++ExecIndex)
	{
		// if the last statement in current node jumps to the first statement in next node, then it's redundant
		const auto CurrentNode = LinearExecutionList[ExecIndex];
		auto CurStatementList = StatementsPerNode.Find(CurrentNode);
		const bool CurrentNodeIsValid = CurrentNode && CurStatementList && CurStatementList->Num();
		const auto LastStatementInCurrentNode = CurrentNodeIsValid ? CurStatementList->Last() : NULL;

		if (LastStatementInCurrentNode
			&& LastStatementInCurrentNode->TargetLabel
			&& (LastStatementInCurrentNode->Type == KCST_UnconditionalGoto)
			&& !LastStatementInCurrentNode->bIsJumpTarget)
		{
			const auto NextNodeIndex = ExecIndex + 1;
			const auto NextNode = LinearExecutionList.IsValidIndex(NextNodeIndex) ? LinearExecutionList[NextNodeIndex] : NULL;
			const auto NextNodeStatements = StatementsPerNode.Find(NextNode);
			const bool bNextNodeValid = NextNode && NextNodeStatements && NextNodeStatements->Num();
			const auto FirstStatementInNextNode = bNextNodeValid ? (*NextNodeStatements)[0] : NULL;
			if (FirstStatementInNextNode == LastStatementInCurrentNode->TargetLabel)
			{
				CurStatementList->RemoveAt(CurStatementList->Num() - 1);
			}
		}
	}

	// Remove unnecessary GotoReturn statements
	// if it's last statement generated by last node (in LinearExecution) then it can be removed
	const auto LastExecutedNode = LinearExecutionList.Num() ? LinearExecutionList.Last() : NULL;
	TArray<FBlueprintCompiledStatement*>* StatementList = StatementsPerNode.Find(LastExecutedNode);
	FBlueprintCompiledStatement* LastStatementInLastNode = (StatementList && StatementList->Num()) ? StatementList->Last() : NULL;
	const bool SafeForNativeCode = !bGeneratingCpp || !MustUseSwitchState(LastStatementInLastNode);
	if (LastStatementInLastNode && SafeForNativeCode && (KCST_GotoReturn == LastStatementInLastNode->Type) && !LastStatementInLastNode->bIsJumpTarget)
	{
		StatementList->RemoveAt(StatementList->Num() - 1);
	}
}

struct FGotoMapUtils
{
	static bool IsUberGraphEventStatement(const FBlueprintCompiledStatement* GotoStatement)
	{
		return GotoStatement
			&& (GotoStatement->Type == KCST_CallFunction) 
			&& (GotoStatement->UbergraphCallIndex == 0);
	}

	static UEdGraphNode* TargetNodeFromPin(const FBlueprintCompiledStatement* GotoStatement, const UEdGraphPin* ExecNet)
	{
		UEdGraphNode* TargetNode = NULL;
		if (ExecNet && GotoStatement)
		{
			if (IsUberGraphEventStatement(GotoStatement))
			{
				TargetNode = ExecNet->GetOwningNode();
			}
			else if (ExecNet->LinkedTo.Num() > 0)
			{
				TargetNode = ExecNet->LinkedTo[0]->GetOwningNode();
			}
		}
		return TargetNode;
	}

	static UEdGraphNode* TargetNodeFromMap(const FBlueprintCompiledStatement* GotoStatement, const TMap< FBlueprintCompiledStatement*, UEdGraphPin* >& GotoFixupRequestMap)
	{
		auto ExecNetPtr = GotoFixupRequestMap.Find(GotoStatement);
		auto ExecNet = ExecNetPtr ? *ExecNetPtr : NULL;
		return TargetNodeFromPin(GotoStatement, ExecNet);
	}
};

void FKismetFunctionContext::ResolveGotoFixups()
{
	if (bCreateDebugData)
	{
		// if we're debugging, go through an insert a wire trace before  
		// every "goto" statement so we can trace what execution pin a node
		// was executed from
		for (auto GotoIt = GotoFixupRequestMap.CreateIterator(); GotoIt; ++GotoIt)
		{
			FBlueprintCompiledStatement* GotoStatement = GotoIt.Key();
			if (FGotoMapUtils::IsUberGraphEventStatement(GotoStatement))
			{
				continue;
			}

			InsertWireTrace(GotoIt.Key(), GotoIt.Value());
		}
	}

	// Resolve the remaining fixups
	for (auto GotoIt = GotoFixupRequestMap.CreateIterator(); GotoIt; ++GotoIt)
	{
		FBlueprintCompiledStatement* GotoStatement = GotoIt.Key();
		const UEdGraphPin* ExecNet = GotoIt.Value();
		const UEdGraphNode* TargetNode = FGotoMapUtils::TargetNodeFromPin(GotoStatement, ExecNet);

		if (TargetNode == NULL)
		{
			// If Execution Flow Stack isn't necessary, then use GotoReturn instead EndOfThread.
			// EndOfThread pops Execution Flow Stack, GotoReturn dosen't.
			GotoStatement->Type = bUseFlowStack
				? ((GotoStatement->Type == KCST_GotoIfNot) ? KCST_EndOfThreadIfNot : KCST_EndOfThread)
				: ((GotoStatement->Type == KCST_GotoIfNot) ? KCST_GotoReturnIfNot : KCST_GotoReturn);
		}
		else
		{
			// Try to resolve the goto
			TArray<FBlueprintCompiledStatement*>* StatementList = StatementsPerNode.Find(TargetNode);

			if ((StatementList == NULL) || (StatementList->Num() == 0))
			{
				MessageLog.Error(TEXT("Statement tried to pass control flow to a node @@ that generates no code"), TargetNode);
				GotoStatement->Type = KCST_CompileError;
			}
			else
			{
				// Wire up the jump target and notify the target that it is targeted
				FBlueprintCompiledStatement& FirstStatement = *((*StatementList)[0]);
				GotoStatement->TargetLabel = &FirstStatement;
				FirstStatement.bIsJumpTarget = true;
			}
		}
	}

	// Clear out the pending fixup map
	GotoFixupRequestMap.Empty();

	//@TODO: Remove any wire debug sites where the next statement is a stack pop
}

void FKismetFunctionContext::FinalSortLinearExecList()
{
	auto K2Schema = Schema;
	LinearExecutionList.RemoveAllSwap([&](UEdGraphNode* CurrentNode)
	{
		auto CurStatementList = StatementsPerNode.Find(CurrentNode);
		return !(CurrentNode && CurStatementList && CurStatementList->Num());
	});

	TSet<UEdGraphNode*> UnsortedExecutionSet(LinearExecutionList);
	LinearExecutionList.Empty();
	TArray<UEdGraphNode*> SortedLinearExecutionList;

	check(EntryPoint);
	SortedLinearExecutionList.Push(EntryPoint);
	UnsortedExecutionSet.Remove(EntryPoint);

	TSet<UEdGraphNode*> NodesToStartNextChain;

	while (UnsortedExecutionSet.Num())
	{
		UEdGraphNode* NextNode = NULL;

		// get last state target
		const auto CurrentNode = SortedLinearExecutionList.Last();
		const auto CurStatementList = StatementsPerNode.Find(CurrentNode);
		const bool CurrentNodeIsValid = CurrentNode && CurStatementList && CurStatementList->Num();
		const auto LastStatementInCurrentNode = CurrentNodeIsValid ? CurStatementList->Last() : NULL;

		// Find next element in current chain
		if (LastStatementInCurrentNode && (LastStatementInCurrentNode->Type == KCST_UnconditionalGoto))
		{
			auto TargetNode = FGotoMapUtils::TargetNodeFromMap(LastStatementInCurrentNode, GotoFixupRequestMap);
			NextNode = UnsortedExecutionSet.Remove(TargetNode) ? TargetNode : NULL;
		}

		if (CurrentNode)
		{
			for (auto Pin : CurrentNode->Pins)
			{
				if (Pin && (EEdGraphPinDirection::EGPD_Output == Pin->Direction) && K2Schema->IsExecPin(*Pin) && Pin->LinkedTo.Num())
				{
					for (auto Link : Pin->LinkedTo)
					{
						auto LinkedNode = Link->GetOwningNodeUnchecked();
						if (LinkedNode && (LinkedNode != NextNode) && UnsortedExecutionSet.Contains(LinkedNode))
						{
							NodesToStartNextChain.Add(LinkedNode);
						}
					}
				}
			}
		}

		// Start next chain if the current is done
		while (NodesToStartNextChain.Num() && !NextNode)
		{
			auto Iter = NodesToStartNextChain.CreateIterator();
			NextNode = UnsortedExecutionSet.Remove(*Iter) ? *Iter : NULL;
			Iter.RemoveCurrent();
		}

		if (!NextNode)
		{
			auto Iter = UnsortedExecutionSet.CreateIterator();
			NextNode = *Iter;
			Iter.RemoveCurrent();
		}

		check(NextNode);
		SortedLinearExecutionList.Push(NextNode);
	}

	LinearExecutionList = SortedLinearExecutionList;
}

bool FKismetFunctionContext::DoesStatementRequiresFlowStack(const FBlueprintCompiledStatement* Statement)
{
	return Statement && (
		(Statement->Type == KCST_EndOfThreadIfNot) ||
		(Statement->Type == KCST_EndOfThread) ||
		(Statement->Type == KCST_PushState));
}

void FKismetFunctionContext::ResolveStatements()
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ResolveCompiledStatements);
	FinalSortLinearExecList();

	static const FBoolConfigValueHelper OptimizeExecutionFlowStack(TEXT("Kismet"), TEXT("bOptimizeExecutionFlowStack"), GEngineIni);
	if (OptimizeExecutionFlowStack)
	{
		bUseFlowStack = AllGeneratedStatements.ContainsByPredicate(&FKismetFunctionContext::DoesStatementRequiresFlowStack);
	}

	ResolveGotoFixups();

	static const FBoolConfigValueHelper OptimizeAdjacentStates(TEXT("Kismet"), TEXT("bOptimizeAdjacentStates"), GEngineIni);
	if (OptimizeAdjacentStates)
	{
		MergeAdjacentStates();
	}
}

struct FEventGraphUtils
{
	static bool IsEntryPointNode(const UK2Node* Node)
	{
		bool bResult = false;
		if (Node)
		{
			bResult |= Node->IsA<UK2Node_Event>();
			bResult |= Node->IsA<UK2Node_Timeline>();
			if (auto CallNode = Cast<const UK2Node_CallFunction>(Node))
			{
				bResult |= CallNode->IsLatentFunction();
			}
		}
		return bResult;
	}

	static void FindEventsCallingTheNodeRecursive(const UK2Node* Node, TSet<const UK2Node*>& Results, TSet<const UK2Node*>& CheckedNodes, const UK2Node* StopOn)
	{
		if (!Node)
		{
			return;
		}

		bool bAlreadyTraversed = false;
		CheckedNodes.Add(Node, &bAlreadyTraversed);
		if (bAlreadyTraversed)
		{
			return;
		}

		if (Node == StopOn)
		{
			return;
		}

		if (IsEntryPointNode(Node))
		{
			Results.Add(Node);
			return;
		}

		const UEdGraphSchema_K2* Schema = CastChecked<const UEdGraphSchema_K2>(Node->GetSchema());
		const bool bIsPure = Node->IsNodePure();
		for (auto Pin : Node->Pins)
		{
			const bool bProperPure		= bIsPure	&& Pin && (Pin->Direction == EEdGraphPinDirection::EGPD_Output);
			const bool bProperNotPure	= !bIsPure	&& Pin && (Pin->Direction == EEdGraphPinDirection::EGPD_Input) && Schema->IsExecPin(*Pin);
			if (bProperPure || bProperNotPure)
			{
				for (auto Link : Pin->LinkedTo)
				{
					auto LinkOwner = Link ? Link->GetOwningNodeUnchecked() : NULL;
					auto NodeToCheck = LinkOwner ? CastChecked<const UK2Node>(LinkOwner) : NULL;
					FindEventsCallingTheNodeRecursive(NodeToCheck, Results, CheckedNodes, StopOn);
				}
			}
		}
	}

	static TSet<const UK2Node*> FindExecutionNodes(const UK2Node* Node, const UK2Node* StopOn)
	{
		TSet<const UK2Node*> Results;
		TSet<const UK2Node*> CheckedNodes;
		FindEventsCallingTheNodeRecursive(Node, Results, CheckedNodes, StopOn);
		return Results;
	}

	static bool PinRepresentsSharedTerminal(const UEdGraphPin& Net, FCompilerResultsLog& MessageLog)
	{
		// TODO: Strange cases..
		if ((Net.Direction != EEdGraphPinDirection::EGPD_Output)
			|| Net.PinType.IsContainer()
			|| Net.PinType.bIsReference
			|| Net.PinType.bIsConst
			|| Net.SubPins.Num())
		{
			return true;
		}

		// Local term must be created by return value. 
		// If the term is from output- by-reference parameter, then it must be persistent between calls.
		// Fix for UE - 23629
		const UK2Node* OwnerNode = Cast<const UK2Node>(Net.GetOwningNodeUnchecked());
		ensure(OwnerNode);
		const UK2Node_CallFunction* CallFunction = Cast<const UK2Node_CallFunction>(OwnerNode);
		if (!CallFunction || (&Net != CallFunction->GetReturnValuePin()))
		{
			return true;
		}

		// If the function call node is an intermediate node resulting from expansion of an async task node, then the return value term must also be persistent.
		const UEdGraphNode* SourceNode = MessageLog.GetSourceNode(OwnerNode);
		if (SourceNode && SourceNode->IsA<UK2Node_BaseAsyncTask>())
		{
			return true;
		}

		// NOT CONNECTED, so it doesn't have to be shared
		if (!Net.LinkedTo.Num())
		{
			return false;
		}

		// Terminals from pure nodes will be recreated anyway, so they can be always local
		if (OwnerNode && OwnerNode->IsNodePure())
		{
			return false;
		}

		// 
		if (IsEntryPointNode(OwnerNode))
		{
			return true;
		}

		// 
		auto SourceEntryPoints = FEventGraphUtils::FindExecutionNodes(OwnerNode, NULL);
		if (1 != SourceEntryPoints.Num())
		{
			return true;
		}

		//
		for (auto Link : Net.LinkedTo)
		{
			auto LinkOwnerNode = Cast<const UK2Node>(Link->GetOwningNodeUnchecked());
			ensure(LinkOwnerNode);
			if (Link->PinType.bIsReference)
			{
				return true;
			}
			auto EventsCallingDestination = FEventGraphUtils::FindExecutionNodes(LinkOwnerNode, OwnerNode);
			if (0 != EventsCallingDestination.Num())
			{
				return true;
			}
		}
		return false;
	}
};

FBPTerminal* FKismetFunctionContext::CreateLocalTerminal(ETerminalSpecification Spec)
{
	FBPTerminal* Result = NULL;
	switch (Spec)
	{
	case ETerminalSpecification::TS_ForcedShared:
		ensure(IsEventGraph());
		Result = new (EventGraphLocals)FBPTerminal();
		break;
	case ETerminalSpecification::TS_Literal:
		Result = new (Literals) FBPTerminal();
		Result->bIsLiteral = true;
		break;
	default:
		const bool bIsLocal = !IsEventGraph();
		Result = new (bIsLocal ? Locals : EventGraphLocals) FBPTerminal();
		Result->SetVarTypeLocal(bIsLocal);
		break;
	}
	return Result;
}

FBPTerminal* FKismetFunctionContext::CreateLocalTerminalFromPinAutoChooseScope(UEdGraphPin* Net, const FString& NewName)
{
	check(Net);
	bool bSharedTerm = IsEventGraph();
	static FBoolConfigValueHelper UseLocalGraphVariables(TEXT("Kismet"), TEXT("bUseLocalGraphVariables"), GEngineIni);
	static FBoolConfigValueHelper UseLocalGraphVariablesInCpp(TEXT("BlueprintNativizationSettings"), TEXT("bUseLocalEventGraphVariables"));

	const bool bUseLocalGraphVariables = UseLocalGraphVariables || (bGeneratingCpp && UseLocalGraphVariablesInCpp);

	const bool OutputPin = EEdGraphPinDirection::EGPD_Output == Net->Direction;
	if (bSharedTerm && bUseLocalGraphVariables && OutputPin)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ChooseTerminalScope);

		// Pin's connections are checked, to tell if created terminal is shared, or if it could be a local variable.
		bSharedTerm = FEventGraphUtils::PinRepresentsSharedTerminal(*Net, MessageLog);
	}
	FBPTerminal* Term = new (bSharedTerm ? EventGraphLocals : Locals) FBPTerminal();
	Term->CopyFromPin(Net, NewName);
	return Term;
}

#undef LOCTEXT_NAMESPACE

//////////////////////////////////////////////////////////////////////////
// FBPTerminal

void FBPTerminal::CopyFromPin(UEdGraphPin* Net, const FString& NewName)
{
	Type = Net->PinType;
	SourcePin = Net;
	Name = NewName;

	bPassedByReference = Net->PinType.bIsReference;

	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Net->GetSchema());
	const bool bStructCategory = Schema && (Schema->PC_Struct == Net->PinType.PinCategory);
	const bool bStructSubCategoryObj = (NULL != Cast<UScriptStruct>(Net->PinType.PinSubCategoryObject.Get()));
	SetContextTypeStruct(bStructCategory && bStructSubCategoryObj);
}


TArray<TSet<UEdGraphNode*>> FKismetCompilerUtilities::FindUnsortedSeparateExecutionGroups(const TArray<UEdGraphNode*>& Nodes)
{
	TArray<UEdGraphNode*> UnprocessedNodes;
	for (UEdGraphNode* Node : Nodes)
	{
		UK2Node* K2Node = Cast<UK2Node>(Node);
		if (K2Node && !K2Node->IsNodePure())
		{
			UnprocessedNodes.Add(Node);
		}
	}

	TSet<UEdGraphNode*> AlreadyProcessed;
	TArray<TSet<UEdGraphNode*>> Result;
	while (UnprocessedNodes.Num())
	{
		Result.Emplace(TSet<UEdGraphNode*>());
		TSet<UEdGraphNode*>& ExecutionGroup = Result.Last();
		TSet<UEdGraphNode*> ToProcess;

		UEdGraphNode* Seed = UnprocessedNodes.Pop();
		ensure(!AlreadyProcessed.Contains(Seed));
		ToProcess.Add(Seed);
		ExecutionGroup.Add(Seed);
		while (ToProcess.Num())
		{
			UEdGraphNode* Node = *ToProcess.CreateIterator();
			// for each execution pin
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->LinkedTo.Num() && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
				{
					for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
					{
						if (!LinkedPin)
						{
							continue;
						}
						UEdGraphNode* LinkedNode = LinkedPin->GetOwningNodeUnchecked();
						const bool bIsAlreadyProcessed = AlreadyProcessed.Contains(LinkedNode);
						const bool bInCurrentExecutionGroup = ExecutionGroup.Contains(LinkedNode);
						ensure(!bIsAlreadyProcessed || bInCurrentExecutionGroup);
						ensure(bInCurrentExecutionGroup || UnprocessedNodes.Contains(LinkedNode));
						if (!bIsAlreadyProcessed)
						{
							ToProcess.Add(LinkedNode);
							ExecutionGroup.Add(LinkedNode);
							UnprocessedNodes.Remove(LinkedNode);
						}
					}
				}
			}

			const int32 WasRemovedFromProcess = ToProcess.Remove(Node);
			ensure(0 != WasRemovedFromProcess);
			bool bAlreadyProcessed = false;
			AlreadyProcessed.Add(Node, &bAlreadyProcessed);
			ensure(!bAlreadyProcessed);
		}

		if (1 == ExecutionGroup.Num())
		{
			UEdGraphNode* TheOnlyNode = *ExecutionGroup.CreateIterator();
			if (!TheOnlyNode || TheOnlyNode->IsA<UK2Node_FunctionEntry>() || TheOnlyNode->IsA<UK2Node_Timeline>())
			{
				Result.Pop();
			}
		}
	}



	return Result;
}
