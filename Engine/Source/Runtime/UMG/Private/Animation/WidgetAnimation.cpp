// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/WidgetAnimation.h"
#include "UObject/Package.h"
#include "Components/Visual.h"
#include "Blueprint/UserWidget.h"
#include "MovieScene.h"
#include "Components/PanelSlot.h"


#define LOCTEXT_NAMESPACE "UWidgetAnimation"


/* UWidgetAnimation structors
 *****************************************************************************/

UWidgetAnimation::UWidgetAnimation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{
	bParentContextsAreSignificant = false;
}


/* UWidgetAnimation interface
 *****************************************************************************/

#if WITH_EDITOR

UWidgetAnimation* UWidgetAnimation::GetNullAnimation()
{
	static UWidgetAnimation* NullAnimation = nullptr;

	if (!NullAnimation)
	{
		NullAnimation = NewObject<UWidgetAnimation>(GetTransientPackage(), NAME_None);
		NullAnimation->AddToRoot();
		NullAnimation->MovieScene = NewObject<UMovieScene>(NullAnimation, FName("No Animation"));
		NullAnimation->MovieScene->AddToRoot();
	}

	return NullAnimation;
}

#endif


float UWidgetAnimation::GetStartTime() const
{
	return MovieScene->GetPlaybackRange().GetLowerBoundValue();
}


float UWidgetAnimation::GetEndTime() const
{
	return MovieScene->GetPlaybackRange().GetUpperBoundValue();
}


/* UMovieSceneAnimation overrides
 *****************************************************************************/


void UWidgetAnimation::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	UUserWidget* PreviewWidget = CastChecked<UUserWidget>(Context);

	// If it's the Root Widget
	if (&PossessedObject == PreviewWidget)
	{
		FWidgetAnimationBinding NewBinding;
		{
			NewBinding.AnimationGuid = ObjectId;
			NewBinding.WidgetName = PossessedObject.GetFName();
			NewBinding.bIsRootWidget = true;
		}

		AnimationBindings.Add(NewBinding);
		return;
	}
	
	UPanelSlot* PossessedSlot = Cast<UPanelSlot>(&PossessedObject);

	if ((PossessedSlot != nullptr) && (PossessedSlot->Content != nullptr))
	{
		// Save the name of the widget containing the slots. This is the object
		// to look up that contains the slot itself (the thing we are animating).
		FWidgetAnimationBinding NewBinding;
		{
			NewBinding.AnimationGuid = ObjectId;
			NewBinding.SlotWidgetName = PossessedSlot->GetFName();
			NewBinding.WidgetName = PossessedSlot->Content->GetFName();
			NewBinding.bIsRootWidget = false;
		}

		AnimationBindings.Add(NewBinding);
	}
	else if (PossessedSlot == nullptr)
	{
		FWidgetAnimationBinding NewBinding;
		{
			NewBinding.AnimationGuid = ObjectId;
			NewBinding.WidgetName = PossessedObject.GetFName();
			NewBinding.bIsRootWidget = false;
		}

		AnimationBindings.Add(NewBinding);
	}
}


bool UWidgetAnimation::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	if (InPlaybackContext == nullptr)
	{
		return false;
	}

	UUserWidget* PreviewWidget = CastChecked<UUserWidget>(InPlaybackContext);

	if (&Object == PreviewWidget)
	{
		return true;
	}

	UPanelSlot* Slot = Cast<UPanelSlot>(&Object);

	if ((Slot != nullptr) && (Slot->Content == nullptr))
	{
		// can't possess empty slots.
		return false;
	}

	return (Object.IsA<UVisual>() && Object.IsIn(PreviewWidget));
}

void UWidgetAnimation::LocateBoundObjects(const FGuid& ObjectId, UObject* InContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	if (InContext == nullptr)
	{
		return;
	}

	const FWidgetAnimationBinding* Binding = AnimationBindings.FindByPredicate(
		[ObjectId](const FWidgetAnimationBinding& In){
			return In.AnimationGuid == ObjectId;
		}
	);

	UUserWidget* PreviewWidget = CastChecked<UUserWidget>(InContext);
	UObject* FoundObject = Binding ? Binding->FindRuntimeObject(*PreviewWidget->WidgetTree, *PreviewWidget) : nullptr;

	if (FoundObject)
	{
		OutObjects.Add(FoundObject);
	}
}


UMovieScene* UWidgetAnimation::GetMovieScene() const
{
	return MovieScene;
}


UObject* UWidgetAnimation::GetParentObject(UObject* Object) const
{
	UPanelSlot* Slot = Cast<UPanelSlot>(Object);

	if (Slot != nullptr)
	{
		// The slot is actually the child of the panel widget in the hierarchy,
		// but we want it to show up as a sub-object of the widget it contains
		// in the timeline so we return the content's GUID.
		return Slot->Content;
	}

	return nullptr;
}

void UWidgetAnimation::UnbindPossessableObjects(const FGuid& ObjectId)
{
	// mark dirty
	Modify();

	// remove animation bindings
	AnimationBindings.RemoveAll([&](const FWidgetAnimationBinding& Binding) {
		return Binding.AnimationGuid == ObjectId;
	});
}


#undef LOCTEXT_NAMESPACE
