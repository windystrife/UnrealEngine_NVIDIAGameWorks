// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/TransformPropertySection.h"
#include "FloatCurveKeyArea.h"
#include "ISectionLayoutBuilder.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "SequencerSectionPainter.h"
#include "MultiBoxBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FTransformSection"

void FTransformSection::AssignProperty(FName PropertyName, const FString& PropertyPath)
{
	PropertyBindings = FTrackInstancePropertyBindings(PropertyName, PropertyPath);
}

int32 FTransformSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	return InPainter.PaintSectionBackground();
}

void FTransformSection::GenerateSectionLayout(ISectionLayoutBuilder& LayoutBuilder) const
{
	static const FLinearColor BlueKeyAreaColor(0.0f, 0.0f, 0.7f, 0.5f);
	static const FLinearColor GreenKeyAreaColor(0.0f, 0.7f, 0.0f, 0.5f);
	static const FLinearColor RedKeyAreaColor(0.7f, 0.0f, 0.0f, 0.5f);

	UMovieScene3DTransformSection* TransformSection = CastChecked<UMovieScene3DTransformSection>(Section.Get());
	EMovieSceneTransformChannel Channels = TransformSection->GetMask().GetChannels();

	// Helper function to tidy up TAttribute::Create syntax
	typedef TOptional<float> (FTransformSection::*ValueGetter)(EAxis::Type) const;

	auto MakeExternalValue = [this](ValueGetter Getter, EAxis::Type Axis) -> TAttribute<TOptional<float>>
	{
		return TAttribute<TOptional<float>>::Create(TAttribute<TOptional<float>>::FGetter::CreateRaw(this, Getter, Axis));
	};

	// This generates the tree structure for the transform section
	if (EnumHasAnyFlags(Channels, EMovieSceneTransformChannel::Translation))
	{
		LayoutBuilder.PushCategory("Location", LOCTEXT("LocationArea", "Location"));

		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::TranslationX))
		{
			TSharedRef<FFloatCurveKeyArea> TranslationXKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetTranslationCurve(EAxis::X),
				MakeExternalValue(&FTransformSection::GetTranslationValue, EAxis::X), TransformSection, RedKeyAreaColor);
			LayoutBuilder.AddKeyArea("Location.X", LOCTEXT("LocXArea", "X"), TranslationXKeyArea );
		}
		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::TranslationY))
		{
			TSharedRef<FFloatCurveKeyArea> TranslationYKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetTranslationCurve(EAxis::Y),
				MakeExternalValue(&FTransformSection::GetTranslationValue, EAxis::Y), TransformSection, GreenKeyAreaColor);
			LayoutBuilder.AddKeyArea("Location.Y", LOCTEXT("LocYArea", "Y"), TranslationYKeyArea );
		}
		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::TranslationZ))
		{
			TSharedRef<FFloatCurveKeyArea> TranslationZKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetTranslationCurve(EAxis::Z),
				MakeExternalValue(&FTransformSection::GetTranslationValue, EAxis::Z), TransformSection, BlueKeyAreaColor);
			LayoutBuilder.AddKeyArea("Location.Z", LOCTEXT("LocZArea", "Z"), TranslationZKeyArea );
		}

		LayoutBuilder.PopCategory();
	}

	if (EnumHasAnyFlags(Channels, EMovieSceneTransformChannel::Rotation))
	{
		LayoutBuilder.PushCategory( "Rotation", LOCTEXT("RotationArea", "Rotation") );

		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::RotationX))
		{
			TSharedRef<FFloatCurveKeyArea> RotationXKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetRotationCurve(EAxis::X),
				MakeExternalValue(&FTransformSection::GetRotationValue, EAxis::X), TransformSection, RedKeyAreaColor);
			LayoutBuilder.AddKeyArea("Rotation.X", LOCTEXT("RotXArea", "X"), RotationXKeyArea );
		}
		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::RotationY))
		{
			TSharedRef<FFloatCurveKeyArea> RotationYKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetRotationCurve(EAxis::Y),
				MakeExternalValue(&FTransformSection::GetRotationValue, EAxis::Y), TransformSection, GreenKeyAreaColor);
			LayoutBuilder.AddKeyArea("Rotation.Y", LOCTEXT("RotYArea", "Y"), RotationYKeyArea );
		}
		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::RotationZ))
		{
			TSharedRef<FFloatCurveKeyArea> RotationZKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetRotationCurve(EAxis::Z),
				MakeExternalValue(&FTransformSection::GetRotationValue, EAxis::Z), TransformSection, BlueKeyAreaColor);
			LayoutBuilder.AddKeyArea("Rotation.Z", LOCTEXT("RotZArea", "Z"), RotationZKeyArea );
		}

		LayoutBuilder.PopCategory();
	}

	if (EnumHasAnyFlags(Channels, EMovieSceneTransformChannel::Scale))
	{
		LayoutBuilder.PushCategory( "Scale", LOCTEXT("ScaleArea", "Scale") );

		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::ScaleX))
		{
			TSharedRef<FFloatCurveKeyArea> ScaleXKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetScaleCurve(EAxis::X),
				MakeExternalValue(&FTransformSection::GetScaleValue, EAxis::X), TransformSection, RedKeyAreaColor);
			LayoutBuilder.AddKeyArea("Scale.X", LOCTEXT("ScaleXArea", "X"), ScaleXKeyArea );
		}
		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::ScaleY))
		{
			TSharedRef<FFloatCurveKeyArea> ScaleYKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetScaleCurve(EAxis::Y),
				MakeExternalValue(&FTransformSection::GetScaleValue, EAxis::Y), TransformSection, GreenKeyAreaColor);
			LayoutBuilder.AddKeyArea("Scale.Y", LOCTEXT("ScaleYArea", "Y"), ScaleYKeyArea );
		}
		if (EnumHasAllFlags(Channels, EMovieSceneTransformChannel::ScaleZ))
		{
			TSharedRef<FFloatCurveKeyArea> ScaleZKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetScaleCurve(EAxis::Z),
				MakeExternalValue(&FTransformSection::GetScaleValue, EAxis::Z), TransformSection, BlueKeyAreaColor);
			LayoutBuilder.AddKeyArea("Scale.Z", LOCTEXT("ScaleZArea", "Z"), ScaleZKeyArea );
		}

		LayoutBuilder.PopCategory();
	}

	if (EnumHasAnyFlags(Channels, EMovieSceneTransformChannel::Weight))
	{
		TSharedRef<FFloatCurveKeyArea> WeightKeyArea = MakeShared<FFloatCurveKeyArea>(&TransformSection->GetManualWeightCurve(), TransformSection);
		LayoutBuilder.AddKeyArea("Weight", LOCTEXT("WeightArea", "Weight"), WeightKeyArea );
	}
}

void FTransformSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding)
{
	UMovieScene3DTransformSection* TransformSection = CastChecked<UMovieScene3DTransformSection>(Section.Get());
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();

	auto MakeUIAction = [=](EMovieSceneTransformChannel ChannelsToToggle)
	{
		return FUIAction(
			FExecuteAction::CreateLambda([=]
				{
					FScopedTransaction Transaction(LOCTEXT("SetActiveChannelsTransaction", "Set Active Channels"));
					TransformSection->Modify();
					EMovieSceneTransformChannel Channels = TransformSection->GetMask().GetChannels();

					if (EnumHasAllFlags(Channels, ChannelsToToggle) || (Channels & ChannelsToToggle) == EMovieSceneTransformChannel::None)
					{
						TransformSection->SetMask(TransformSection->GetMask().GetChannels() ^ ChannelsToToggle);
					}
					else
					{
						TransformSection->SetMask(TransformSection->GetMask().GetChannels() | ChannelsToToggle);
					}

					SequencerPtr->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				}
			),
			FCanExecuteAction(),
			FGetActionCheckState::CreateLambda([=]
			{
				EMovieSceneTransformChannel Channels = TransformSection->GetMask().GetChannels();
				if (EnumHasAllFlags(Channels, ChannelsToToggle))
				{
					return ECheckBoxState::Checked;
				}
				else if (EnumHasAnyFlags(Channels, ChannelsToToggle))
				{
					return ECheckBoxState::Undetermined;
				}
				return ECheckBoxState::Unchecked;
			})
		);
	};

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("TransformChannelsText", "Active Channels"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("AllTranslation", "Translation"), LOCTEXT("AllTranslation_ToolTip", "Causes this section to affect the translation of the transform"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("TranslationX", "X"), LOCTEXT("TranslationX_ToolTip", "Causes this section to affect the X channel of the transform's translation"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::TranslationX), NAME_None, EUserInterfaceActionType::ToggleButton);
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("TranslationY", "Y"), LOCTEXT("TranslationY_ToolTip", "Causes this section to affect the Y channel of the transform's translation"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::TranslationY), NAME_None, EUserInterfaceActionType::ToggleButton);
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("TranslationZ", "Z"), LOCTEXT("TranslationZ_ToolTip", "Causes this section to affect the Z channel of the transform's translation"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::TranslationZ), NAME_None, EUserInterfaceActionType::ToggleButton);
			}),
			MakeUIAction(EMovieSceneTransformChannel::Translation),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);

		MenuBuilder.AddSubMenu(
			LOCTEXT("AllRotation", "Rotation"), LOCTEXT("AllRotation_ToolTip", "Causes this section to affect the rotation of the transform"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("RotationX", "Roll (X)"), LOCTEXT("RotationX_ToolTip", "Causes this section to affect the roll (X) channel the transform's rotation"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::RotationX), NAME_None, EUserInterfaceActionType::ToggleButton);
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("RotationY", "Pitch (Y)"), LOCTEXT("RotationY_ToolTip", "Causes this section to affect the pitch (Y) channel the transform's rotation"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::RotationY), NAME_None, EUserInterfaceActionType::ToggleButton);
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("RotationZ", "Yaw (Z)"), LOCTEXT("RotationZ_ToolTip", "Causes this section to affect the yaw (Z) channel the transform's rotation"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::RotationZ), NAME_None, EUserInterfaceActionType::ToggleButton);
			}),
			MakeUIAction(EMovieSceneTransformChannel::Rotation),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);

		MenuBuilder.AddSubMenu(
			LOCTEXT("AllScale", "Scale"), LOCTEXT("AllScale_ToolTip", "Causes this section to affect the scale of the transform"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("ScaleX", "X"), LOCTEXT("ScaleX_ToolTip", "Causes this section to affect the X channel of the transform's scale"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::ScaleX), NAME_None, EUserInterfaceActionType::ToggleButton);
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("ScaleY", "Y"), LOCTEXT("ScaleY_ToolTip", "Causes this section to affect the Y channel of the transform's scale"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::ScaleY), NAME_None, EUserInterfaceActionType::ToggleButton);
				SubMenuBuilder.AddMenuEntry(
					LOCTEXT("ScaleZ", "Z"), LOCTEXT("ScaleZ_ToolTip", "Causes this section to affect the Z channel of the transform's scale"),
					FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::ScaleZ), NAME_None, EUserInterfaceActionType::ToggleButton);
			}),
			MakeUIAction(EMovieSceneTransformChannel::Scale),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Weight", "Weight"), LOCTEXT("Weight_ToolTip", "Causes this section to be applied with a user-specified weight curve"),
			FSlateIcon(), MakeUIAction(EMovieSceneTransformChannel::Weight), NAME_None, EUserInterfaceActionType::ToggleButton);
	}
	MenuBuilder.EndSection();
}

TOptional<FTransform> FTransformSection::GetCurrentValue() const
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (ensure(PropertyBindings.IsSet()) && Sequencer.IsValid())
	{
		for (TWeakObjectPtr<> WeakObject : Sequencer->FindBoundObjects(ObjectBinding, Sequencer->GetFocusedTemplateID()))
		{
			if (UObject* Object = WeakObject.Get())
			{
				return PropertyBindings->GetCurrentValue<FTransform>(*Object);
			}
		}
	}
	return TOptional<FTransform>();
}

TOptional<float> FTransformSection::GetTranslationValue(EAxis::Type Axis) const
{
	FTransform Transform = GetCurrentValue().Get(FTransform::Identity);
	switch (Axis)
	{
	case EAxis::X:		return Transform.GetTranslation().X;
	case EAxis::Y:		return Transform.GetTranslation().Y;
	case EAxis::Z:		return Transform.GetTranslation().Z;
	}
	return TOptional<float>();
}

TOptional<float> FTransformSection::GetRotationValue(EAxis::Type Axis) const
{
	FTransform Transform = GetCurrentValue().Get(FTransform::Identity);
	switch (Axis)
	{
	case EAxis::X:		return Transform.GetRotation().Rotator().Roll;
	case EAxis::Y:		return Transform.GetRotation().Rotator().Pitch;
	case EAxis::Z:		return Transform.GetRotation().Rotator().Yaw;
	}
	return TOptional<float>();
}

TOptional<float> FTransformSection::GetScaleValue(EAxis::Type Axis) const
{
	FTransform Transform = GetCurrentValue().Get(FTransform::Identity);
	switch (Axis)
	{
	case EAxis::X:		return Transform.GetScale3D().X;
	case EAxis::Y:		return Transform.GetScale3D().Y;
	case EAxis::Z:		return Transform.GetScale3D().Z;
	}
	return TOptional<float>();
}

#undef LOCTEXT_NAMESPACE
