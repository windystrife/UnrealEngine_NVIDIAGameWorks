// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SObjectNameEditableTextBox.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"
#include "Editor/EditorEngine.h"

const float SObjectNameEditableTextBox::HighlightRectLeftOffset = 0.0f;
const float SObjectNameEditableTextBox::HighlightRectRightOffset = 0.0f;
const float SObjectNameEditableTextBox::HighlightTargetSpringConstant = 25.0f;
const float SObjectNameEditableTextBox::HighlightTargetEffectDuration = 0.5f;
const float SObjectNameEditableTextBox::HighlightTargetOpacity = 0.8f;
const float SObjectNameEditableTextBox::CommittingAnimOffsetPercent = 0.2f;

#define LOCTEXT_NAMESPACE "EditorWidgets"

void SObjectNameEditableTextBox::Construct( const FArguments& InArgs )
{
	LastCommittedTime = 0.0;
	bUpdateHighlightSpring = false;

	Objects = InArgs._Objects;

	ChildSlot
		[
			SAssignNew(TextBox, SEditableTextBox)
			.Text( this, &SObjectNameEditableTextBox::GetNameText )
			.ToolTipText( this, &SObjectNameEditableTextBox::GetNameTooltipText )
			.Visibility( this, &SObjectNameEditableTextBox::GetNameVisibility )
			.HintText( this, &SObjectNameEditableTextBox::GetNameHintText )
			.OnTextCommitted( this, &SObjectNameEditableTextBox::OnNameTextCommitted )
			.IsReadOnly( this, &SObjectNameEditableTextBox::CannotEditNameText )
			.SelectAllTextWhenFocused( this, &SObjectNameEditableTextBox::CanEditNameText )
			.OnTextChanged(this, &SObjectNameEditableTextBox::OnTextChanged )
			.RevertTextOnEscape( true )
		];
}

EActiveTimerReturnType SObjectNameEditableTextBox::UpdateHighlightSpringState( double InCurrentTime, float InDeltaTime )
{
	if ( (float)(InCurrentTime - LastCommittedTime) <= HighlightTargetEffectDuration )
	{
		bUpdateHighlightSpring = true;
	}
	else
	{
		bUpdateHighlightSpring = false;
	}

	return bUpdateHighlightSpring ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

void SObjectNameEditableTextBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	const bool bShouldAppearFocused = HasKeyboardFocus();

	if ( bUpdateHighlightSpring || bShouldAppearFocused )
	{
		// Update highlight 'target' effect
		const float HighlightLeftX = HighlightRectLeftOffset;
		const float HighlightRightX = HighlightRectRightOffset + AllottedGeometry.GetLocalSize().X;

		HighlightTargetLeftSpring.SetTarget( HighlightLeftX );
		HighlightTargetRightSpring.SetTarget( HighlightRightX );

		HighlightTargetLeftSpring.Tick( InDeltaTime );
		HighlightTargetRightSpring.Tick( InDeltaTime );
	}
}

int32 SObjectNameEditableTextBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 StartLayer = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	const int32 TextLayer = 1;

	// See if a disabled effect should be used
	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	ESlateDrawEffect DrawEffects = (bEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const double CurrentTime = FSlateApplication::Get().GetCurrentTime();

	// Draw highlight targeting effect
	const float TimeSinceHighlightInteraction = (float)( CurrentTime - LastCommittedTime );
	if( TimeSinceHighlightInteraction <= HighlightTargetEffectDuration )
	{
		// Compute animation progress
		float EffectAlpha = FMath::Clamp( TimeSinceHighlightInteraction / HighlightTargetEffectDuration, 0.0f, 1.0f );
		EffectAlpha = 1.0f - EffectAlpha * EffectAlpha;  // Inverse square falloff (looks nicer!)

		float EffectOpacity = EffectAlpha;

		// Figure out a universally visible highlight color.
		FLinearColor HighlightTargetColorAndOpacity = ( (FLinearColor::White - ColorAndOpacity.Get())*0.5f + FLinearColor(+0.4f, +0.1f, -0.2f)) * InWidgetStyle.GetColorAndOpacityTint();
		HighlightTargetColorAndOpacity.A = HighlightTargetOpacity * EffectOpacity * 255.0f;

		// Compute the bounds offset of the highlight target from where the highlight target spring
		// extents currently lie.  This is used to "grow" or "shrink" the highlight as needed.
		const float CommittingAnimOffset = CommittingAnimOffsetPercent * AllottedGeometry.GetLocalSize().Y;

		// Choose an offset amount depending on whether we're highlighting, or clearing highlight
		const float EffectOffset = EffectAlpha * CommittingAnimOffset;

		const float HighlightLeftX = HighlightTargetLeftSpring.GetPosition() - EffectOffset;
		const float HighlightRightX = HighlightTargetRightSpring.GetPosition() + EffectOffset;
		const float HighlightTopY = 0.0f - EffectOffset;
		const float HighlightBottomY = AllottedGeometry.GetLocalSize().Y + EffectOffset;

		const FVector2D DrawPosition = FVector2D( HighlightLeftX, HighlightTopY );
		const FVector2D DrawSize = FVector2D( HighlightRightX - HighlightLeftX, HighlightBottomY - HighlightTopY );

		const FSlateBrush* StyleInfo = FEditorStyle::GetBrush("DetailsView.NameChangeCommitted");

		// NOTE: We rely on scissor clipping for the highlight rectangle
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + TextLayer,
			AllottedGeometry.ToPaintGeometry( DrawPosition, DrawSize ),	// Position, Size, Scale
			StyleInfo,													// Style
			DrawEffects,												// Effects to use
			HighlightTargetColorAndOpacity );							// Color
	}

	return LayerId + TextLayer;
}

FText SObjectNameEditableTextBox::GetNameText() const
{
	FString Result;

	if (Objects.Num() == 1)
	{
		Result = GetObjectDisplayName(Objects[0]);
	}
	else if (Objects.Num() > 1)
	{
		if (!UserSetCommonName.IsEmpty())
		{
			Result = UserSetCommonName;
		}
	}

	return FText::FromString(Result);
}

FText SObjectNameEditableTextBox::GetNameTooltipText() const
{
	FText Result = FText::GetEmpty();

	if (Objects.Num() == 0)
	{
		Result = LOCTEXT("EditableActorLabel_NoObjectsTooltip", "Nothing selected");
	}
	else if (Objects.Num() == 1 && Objects[0].IsValid())
	{
		if (CanEditNameText())
		{
			Result = FText::Format(LOCTEXT("EditableActorLabel_ActorTooltipFmt", "Rename the selected {0}"), FText::FromString(Objects[0].Get()->GetClass()->GetName()));
		}
		else if (Objects[0].Get()->IsA(AActor::StaticClass()))
		{
			Result = LOCTEXT("EditableActorLabel_NoEditActorTooltip", "Can't rename selected actor (its label isn't editable)");
		}
		else
		{
			Result = LOCTEXT("EditableActorLabel_NoEditObjectTooltip", "Can't rename selected object (only actors can have editable labels)");
		}
	}
	else if (Objects.Num() > 1)
	{
		if (CanEditNameText())
		{
			Result = LOCTEXT("EditableActorLabel_MultiActorTooltip", "Rename multiple selected actors at once");
		}
		else
		{
			Result = LOCTEXT("EditableActorLabel_NoEditMultiObjectTooltip", "Can't rename selected objects (one or more aren't actors with editable labels)");
		}
	}

	return Result;
}

EVisibility SObjectNameEditableTextBox::GetNameVisibility() const
{
	if (Objects.Num() == 1 && Objects[0].IsValid())
	{
		if (CanEditNameText())
		{
			return EVisibility::Visible;
		}
		else if (Objects[0].Get()->IsA(AActor::StaticClass()))
		{
			return EVisibility::Visible;
		}
		else
		{
			return EVisibility::Collapsed;
		}
	}
	else if (Objects.Num() > 1)
	{
		if (CanEditNameText())
		{
			return EVisibility::Visible;
		}
		else
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Collapsed;
}

FText SObjectNameEditableTextBox::GetNameHintText() const
{
	FText Result;

	if (Objects.Num() == 0)
	{
		Result = LOCTEXT("EditableActorLabel_NoObjectsHint", "<Nothing Selected>");
	}
	else if (Objects.Num() == 1 && Objects[0].IsValid())
	{
		Result = FText::Format(LOCTEXT("EditableActorLabel_MultiObjectsHint_SameType", "<Selected {0}>"), FText::FromName(Objects[0].Get()->GetClass()->GetFName()));
	}
	else if (Objects.Num() > 1)
	{
		Result = LOCTEXT("EditableActorLabel_MultiObjectsHint_DifferentTypes", "<Selected Objects>");
	}

	return Result;
}

void SObjectNameEditableTextBox::OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	// Don't apply the change if the TextCommit type is OnCleared - this will only be the case if the keyboard focus was cleared due to
	// Enter being pressed, in which case we will already have been here once with a TextCommit type of OnEnter.
	FText ErrorMessage;
	if (InTextCommit != ETextCommit::OnCleared && FActorEditorUtils::ValidateActorName(NewText, ErrorMessage))
	{
		FText TrimmedText = FText::TrimPrecedingAndTrailing(NewText);

		if (!TrimmedText.IsEmpty())
		{
			if (Objects.Num() == 1)
			{
				// Apply the change to the selected actor
				AActor* Actor = Cast<AActor>(Objects[0].Get());
				if(Actor != NULL)
				{
					const FScopedTransaction Transaction( LOCTEXT("RenameActorTransaction", "Rename Actor") );

					if (Actor->IsActorLabelEditable())
					{
						FActorLabelUtilities::RenameExistingActor(Actor, TrimmedText.ToString());
						LastCommittedTime = FSlateApplication::Get().GetCurrentTime();
						RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SObjectNameEditableTextBox::UpdateHighlightSpringState ) );
					}
				}
			}
			else if (Objects.Num() > 1)
			{
				const FScopedTransaction Transaction( LOCTEXT("RenameActorsTransaction", "Rename Multiple Actors") );

				UserSetCommonName = TrimmedText.ToString();

				for (int32 i=0; i<Objects.Num(); i++)
				{
					// Apply the change to the selected actor
					if(Objects[i].IsValid() && Objects[i].Get()->IsA(AActor::StaticClass()))
					{
						AActor* Actor = (AActor*)Objects[i].Get();
						if (Actor->IsActorLabelEditable())
						{
							FActorLabelUtilities::RenameExistingActor(Actor, TrimmedText.ToString());
							LastCommittedTime = FSlateApplication::Get().GetCurrentTime();
							RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SObjectNameEditableTextBox::UpdateHighlightSpringState ) );
						}
					}
				}
			}
		}
		// Remove ourselves from the window focus so we don't get automatically reselected when scrolling around the context menu.
		TSharedPtr< SWindow > ParentWindow = FSlateApplication::Get().FindWidgetWindow( SharedThis(this) );
		if( ParentWindow.IsValid() )
		{
			ParentWindow->SetWidgetToFocusOnActivate( NULL );
		}
	}

	// Clear Error 
	TextBox->SetError(FText::GetEmpty());
}

void SObjectNameEditableTextBox::OnTextChanged( const FText& InLabel )
{
	FText OutErrorMessage;

	if(!FActorEditorUtils::ValidateActorName(InLabel, OutErrorMessage))
	{
		TextBox->SetError(OutErrorMessage);
	}
	else
	{
		TextBox->SetError( FText::GetEmpty() );
	}
}

bool SObjectNameEditableTextBox::CanEditNameText() const
{
	bool Result = false;

	if (Objects.Num() > 0)
	{
		Result = true;
		for (int32 i=0; i<Objects.Num(); i++)
		{
			if(Objects[i].IsValid())
			{
				if (Objects[i].Get()->IsA(AActor::StaticClass()))
				{
					AActor* Actor = (AActor*)Objects[i].Get();
					if (!Actor->IsActorLabelEditable())
					{
						// can't edit the name when a non-editable actor is selected
						Result = false;
						break;
					}
				}
				else
				{
					// can't edit the name when a non-actor is selected
					Result = false;
					break;
				}
			}
		}
	}

	return Result;
}

FString SObjectNameEditableTextBox::GetObjectDisplayName(TWeakObjectPtr<UObject> Object)
{
	FString Result;
	if(Object.IsValid())
	{
		UObject* ObjectPtr = Object.Get();
		if (ObjectPtr->IsA(AActor::StaticClass()))
		{
			Result =  ((AActor*)ObjectPtr)->GetActorLabel();
		}
		else
		{
			Result = ObjectPtr->GetName();
		}
	}
	return Result;
}

// No code beyond this point
#undef LOCTEXT_NAMESPACE
