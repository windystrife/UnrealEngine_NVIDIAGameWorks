// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PersonaUtils.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "ComponentAssetBroker.h"
#include "Animation/AnimInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimNodeBase.h"

namespace PersonaUtils
{

USceneComponent* GetComponentForAttachedObject(USceneComponent* PreviewComponent, UObject* Object, const FName& AttachedTo)
{
	if (PreviewComponent)
	{
		for (USceneComponent* ChildComponent : PreviewComponent->GetAttachChildren())
		{
			UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(ChildComponent);

			if (Asset == Object && ChildComponent->GetAttachSocketName() == AttachedTo)
			{
				return ChildComponent;
			}
		}
	}
	return nullptr;
}

int32 CopyPropertiesToCDO(UAnimInstance* InAnimInstance, const FCopyOptions& Options)
{
	check(InAnimInstance != nullptr);

	UAnimInstance* SourceInstance = InAnimInstance;
	UClass* AnimInstanceClass = SourceInstance->GetClass();
	UAnimInstance* TargetInstance = CastChecked<UAnimInstance>(AnimInstanceClass->GetDefaultObject());
	
	const bool bIsPreviewing = ( Options.Flags & ECopyOptions::PreviewOnly ) != 0;

	int32 CopiedPropertyCount = 0;

	// Copy properties from the instance to the CDO
	TSet<UObject*> ModifiedObjects;
	for( UProperty* Property = AnimInstanceClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
	{
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsBlueprintReadonly = !!(Options.Flags & ECopyOptions::FilterBlueprintReadOnly) && !!( Property->PropertyFlags & CPF_BlueprintReadOnly );
		const bool bIsIdentical = Property->Identical_InContainer(SourceInstance, TargetInstance);
		const bool bIsAnimGraphNodeProperty = Property->IsA<UStructProperty>() && Cast<UStructProperty>(Property)->Struct->IsChildOf(FAnimNode_Base::StaticStruct());

		if( !bIsAnimGraphNodeProperty && !bIsTransient && !bIsIdentical && !bIsBlueprintReadonly)
		{
			const bool bIsSafeToCopy = !( Options.Flags & ECopyOptions::OnlyCopyEditOrInterpProperties ) || ( Property->HasAnyPropertyFlags( CPF_Edit | CPF_Interp ) );
			if( bIsSafeToCopy )
			{
				if (!Options.CanCopyProperty(*Property, *SourceInstance))
				{
					continue;
				}

				if( !bIsPreviewing )
				{
					if( !ModifiedObjects.Contains(TargetInstance) )
					{
						// Start modifying the target object
						TargetInstance->Modify();
						ModifiedObjects.Add(TargetInstance);
					}

					if( Options.Flags & ECopyOptions::CallPostEditChangeProperty )
					{
						TargetInstance->PreEditChange(Property);
					}

					EditorUtilities::CopySingleProperty(SourceInstance, TargetInstance, Property);

					if( Options.Flags & ECopyOptions::CallPostEditChangeProperty )
					{
						FPropertyChangedEvent PropertyChangedEvent(Property);
						TargetInstance->PostEditChangeProperty(PropertyChangedEvent);
					}
				}

				++CopiedPropertyCount;
			}
		}
	}

	if (!bIsPreviewing && CopiedPropertyCount > 0 && AnimInstanceClass->HasAllClassFlags(CLASS_CompiledFromBlueprint))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(CastChecked<UBlueprint>(AnimInstanceClass->ClassGeneratedBy));
	}

	return CopiedPropertyCount;
}

}
