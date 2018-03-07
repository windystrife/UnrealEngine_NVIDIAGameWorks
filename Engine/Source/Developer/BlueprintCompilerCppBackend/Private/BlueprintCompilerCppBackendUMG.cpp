// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Evaluation/MovieSceneTrackImplementation.h"
#include "Evaluation/MovieSceneEvalTemplate.h"

void FBackendHelperUMG::WidgetFunctionsInHeader(FEmitterLocalContext& Context)
{
	if (Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s(TArray<FName>& SlotNames) const override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s(const class ITargetPlatform* TargetPlatform) override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, PreSave)));
		Context.Header.AddLine(TEXT("virtual void InitializeNativeClassData() override;"));
	}
}

void FBackendHelperUMG::AdditionalHeaderIncludeForWidget(FEmitterLocalContext& Context)
{
	if (Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(TEXT("#include \"Runtime/UMG/Public/UMG.h\""));
	}
}

void FBackendHelperUMG::CreateClassSubobjects(FEmitterLocalContext& Context, bool bCreate, bool bInitialize)
{
	if (auto WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		if (WidgetClass->WidgetTree)
		{
			ensure(WidgetClass->WidgetTree->GetOuter() == Context.GetCurrentlyGeneratedClass());
			FEmitDefaultValueHelper::HandleClassSubobject(Context, WidgetClass->WidgetTree, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, bCreate, bInitialize);
		}
		for (auto Anim : WidgetClass->Animations)
		{
			ensure(Anim->GetOuter() == Context.GetCurrentlyGeneratedClass());

			// We need the same regeneration like for cooking. See UMovieSceneSequence::Serialize
			Anim->EvaluationTemplate.Regenerate(Anim->TemplateParameters);

			FEmitDefaultValueHelper::HandleClassSubobject(Context, Anim, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, bCreate, bInitialize);
		}
	}
}

void FBackendHelperUMG::EmitWidgetInitializationFunctions(FEmitterLocalContext& Context)
{
	if (auto WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.ResetPropertiesForInaccessibleStructs();

		const FString CppClassName = FEmitHelper::GetCppName(WidgetClass);

		auto GenerateLocalProperty = [](FEmitterLocalContext& InContext, UProperty* InProperty, const uint8* DataPtr) -> FString
		{
			check(InProperty && DataPtr);
			const FString NativeName = InContext.GenerateUniqueLocalName();
			
			const uint32 CppTemplateTypeFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst | EPropertyExportCPPFlags::CPPF_NoRef;
			const FString Target = InContext.ExportCppDeclaration(InProperty, EExportedDeclaration::Local, CppTemplateTypeFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);

			InContext.AddLine(FString::Printf(TEXT("%s %s;"), *Target, *NativeName));
			FEmitDefaultValueHelper::InnerGenerate(InContext, InProperty, NativeName, DataPtr, nullptr, true);
			return NativeName;
		};

		{	// GetSlotNames
			Context.AddLine(FString::Printf(TEXT("void %s::%s(TArray<FName>& SlotNames) const"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();

			const FString LocalNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("NamedSlots")), reinterpret_cast<const uint8*>(&WidgetClass->NamedSlots));
			Context.AddLine(FString::Printf(TEXT("SlotNames.Append(%s);"), *LocalNativeName));

			Context.DecreaseIndent();
			Context.AddLine(TEXT("}"));
		}

		{	// InitializeNativeClassData
			Context.AddLine(FString::Printf(TEXT("void %s::InitializeNativeClassData()"), *CppClassName));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();

			const FString WidgetTreeStr = Context.FindGloballyMappedObject(WidgetClass->WidgetTree, UWidgetTree::StaticClass(), true);
			ensure(!WidgetTreeStr.IsEmpty());
			const FString AnimationsArrayNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("Animations")), reinterpret_cast<const uint8*>(&WidgetClass->Animations));
			const FString BindingsArrayNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("Bindings")), reinterpret_cast<const uint8*>(&WidgetClass->Bindings));
					
			Context.AddLine(FString::Printf(TEXT("UWidgetBlueprintGeneratedClass::%s(this, GetClass(), %s, %s, %s, %s);")
				, GET_FUNCTION_NAME_STRING_CHECKED(UWidgetBlueprintGeneratedClass, InitializeWidgetStatic)
				, WidgetClass->HasTemplate() ? TEXT("true") : TEXT("false")
				, *WidgetTreeStr
				, *AnimationsArrayNativeName
				, *BindingsArrayNativeName));

			Context.DecreaseIndent();
			Context.AddLine(TEXT("}"));
		}

		// PreSave
		Context.AddLine(FString::Printf(TEXT("void %s::%s(const class ITargetPlatform* TargetPlatform)"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, PreSave)));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("Super::%s(TargetPlatform);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, PreSave)));
		Context.AddLine(TEXT("TArray<FName> LocalNamedSlots;"));
		Context.AddLine(FString::Printf(TEXT("%s(LocalNamedSlots);"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
		Context.AddLine(TEXT("RemoveObsoleteBindings(LocalNamedSlots);")); //RemoveObsoleteBindings is protected - no check
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
}

bool FBackendHelperUMG::SpecialStructureConstructorUMG(const UStruct* Struct, const uint8* ValuePtr, /*out*/ FString* OutResult)
{
	check(ValuePtr || !OutResult);

	if (FSectionEvaluationData::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FSectionEvaluationData* SectionEvaluationData = reinterpret_cast<const FSectionEvaluationData*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FSectionEvaluationData(%d, %s)")
				, SectionEvaluationData->ImplIndex
				, *FEmitHelper::FloatToString(SectionEvaluationData->ForcedTime));
		}
		return true;
	}

	if (FMovieSceneSegment::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FMovieSceneSegment* MovieSceneSegment = reinterpret_cast<const FMovieSceneSegment*>(ValuePtr);
			FString RangeStr;
			FEmitDefaultValueHelper::SpecialStructureConstructor(TBaseStructure<FFloatRange>::Get()
				, reinterpret_cast<const uint8*>(&MovieSceneSegment->Range)
				, &RangeStr);
			FString SegmentsInitializerList;
			for (const FSectionEvaluationData& SectionEvaluationData : MovieSceneSegment->Impls)
			{
				if (!SegmentsInitializerList.IsEmpty())
				{
					SegmentsInitializerList += TEXT(", ");
				}
				FString SectionEvaluationDataStr;
				FBackendHelperUMG::SpecialStructureConstructorUMG(FSectionEvaluationData::StaticStruct()
					, reinterpret_cast<const uint8*>(&SectionEvaluationData)
					, &SectionEvaluationDataStr);
				SegmentsInitializerList += SectionEvaluationDataStr;
			}
			*OutResult = FString::Printf(TEXT("FMovieSceneSegment(%s, {%s})"), *RangeStr, *SegmentsInitializerList);
		}
		return true;
	}
	return false;
}

bool FBackendHelperUMG::IsTInlineStruct(UScriptStruct* OuterStruct)
{
	return (OuterStruct == FMovieSceneTrackImplementationPtr::StaticStruct())
		|| (OuterStruct == FMovieSceneEvalTemplatePtr::StaticStruct());
}

UScriptStruct* FBackendHelperUMG::InlineValueStruct(UScriptStruct* OuterStruct, const uint8* ValuePtr)
{ 
	if (OuterStruct == FMovieSceneTrackImplementationPtr::StaticStruct())
	{
		const FMovieSceneTrackImplementation* MovieSceneTrackImplementation = reinterpret_cast<const FMovieSceneTrackImplementationPtr*>(ValuePtr)->GetPtr();
		if (MovieSceneTrackImplementation)
		{
			return &(MovieSceneTrackImplementation->GetScriptStruct());
		}
	}

	if (OuterStruct == FMovieSceneEvalTemplatePtr::StaticStruct())
	{
		const FMovieSceneEvalTemplate* MovieSceneEvalTemplate = reinterpret_cast<const FMovieSceneEvalTemplatePtr*>(ValuePtr)->GetPtr();
		if (MovieSceneEvalTemplate)
		{
			return &(MovieSceneEvalTemplate->GetScriptStruct());
		}
	}
	return nullptr;
}

const uint8* FBackendHelperUMG::InlineValueData(UScriptStruct* OuterStruct, const uint8* ValuePtr)
{ 
	if (ValuePtr)
	{
		if (OuterStruct == FMovieSceneTrackImplementationPtr::StaticStruct())
		{
			return reinterpret_cast<const uint8*>(reinterpret_cast<const FMovieSceneTrackImplementationPtr*>(ValuePtr)->GetPtr());
		}
		if (OuterStruct == FMovieSceneEvalTemplatePtr::StaticStruct())
		{
			return reinterpret_cast<const uint8*>(reinterpret_cast<const FMovieSceneEvalTemplatePtr*>(ValuePtr)->GetPtr());
		}
	}
	return nullptr;
}
