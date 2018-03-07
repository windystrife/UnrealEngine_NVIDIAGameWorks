// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/UserInterfaceSettings.h"

#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"

#include "Engine/DPICustomScalingRule.h"

#define LOCTEXT_NAMESPACE "Engine"

UUserInterfaceSettings::UUserInterfaceSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RenderFocusRule(ERenderFocusRule::NavigationOnly)
	, ApplicationScale(1)
	, bLoadWidgetsOnDedicatedServer(true)
{
	SectionName = TEXT("UI");
}

void UUserInterfaceSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if ( !DefaultCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::Default, DefaultCursor_DEPRECATED);
		DefaultCursor_DEPRECATED.Reset();
	}

	if ( !TextEditBeamCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::TextEditBeam, TextEditBeamCursor_DEPRECATED);
		TextEditBeamCursor_DEPRECATED.Reset();
	}

	if ( !CrosshairsCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::Crosshairs, CrosshairsCursor_DEPRECATED);
		CrosshairsCursor_DEPRECATED.Reset();
	}

	if ( !HandCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::Hand, HandCursor_DEPRECATED);
		HandCursor_DEPRECATED.Reset();
	}

	if ( !GrabHandCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::GrabHand, GrabHandCursor_DEPRECATED);
		GrabHandCursor_DEPRECATED.Reset();
	}

	if ( !GrabHandClosedCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::GrabHandClosed, GrabHandClosedCursor_DEPRECATED);
		GrabHandClosedCursor_DEPRECATED.Reset();
	}

	if ( !SlashedCircleCursor_DEPRECATED.IsNull() )
	{
		SoftwareCursors.Add(EMouseCursor::SlashedCircle, SlashedCircleCursor_DEPRECATED);
		SlashedCircleCursor_DEPRECATED.Reset();
	}

	// Allow the assets to be replaced in the editor, but make sure they're part of the root set in cooked games
#if WITH_EDITOR
	if ( IsTemplate() == false )
	{
		ForceLoadResources();
	}
#else
	ForceLoadResources();
#endif
}

float UUserInterfaceSettings::GetDPIScaleBasedOnSize(FIntPoint Size) const
{
	float Scale = 1.0f;

	if ( UIScaleRule == EUIScalingRule::Custom )
	{
		if ( CustomScalingRuleClassInstance == nullptr )
		{
			CustomScalingRuleClassInstance = CustomScalingRuleClass.TryLoadClass<UDPICustomScalingRule>();

			if ( CustomScalingRuleClassInstance == nullptr )
			{
				FMessageLog("MapCheck").Error()
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("CustomScalingRule_NotFound", "Project Settings - User Interface Custom Scaling Rule '{0}' could not be found."), FText::FromString(CustomScalingRuleClass.ToString()))));
				return 1;
			}
		}
		
		if ( CustomScalingRule == nullptr )
		{
			CustomScalingRule = CustomScalingRuleClassInstance->GetDefaultObject<UDPICustomScalingRule>();
		}

		Scale = CustomScalingRule->GetDPIScaleBasedOnSize(Size);
	}
	else
	{
		int32 EvalPoint = 0;
		switch ( UIScaleRule )
		{
		case EUIScalingRule::ShortestSide:
			EvalPoint = FMath::Min(Size.X, Size.Y);
			break;
		case EUIScalingRule::LongestSide:
			EvalPoint = FMath::Max(Size.X, Size.Y);
			break;
		case EUIScalingRule::Horizontal:
			EvalPoint = Size.X;
			break;
		case EUIScalingRule::Vertical:
			EvalPoint = Size.Y;
			break;
		}

		const FRichCurve* DPICurve = UIScaleCurve.GetRichCurveConst();
		Scale = DPICurve->Eval((float)EvalPoint, 1.0f);
	}

	return FMath::Max(Scale * ApplicationScale, 0.01f);
}

void UUserInterfaceSettings::ForceLoadResources()
{
	bool bShouldLoadCurors = true;

	if (IsRunningCommandlet())
	{
		bShouldLoadCurors = false;
	}
	else if (IsRunningDedicatedServer())
	{
		bShouldLoadCurors = bLoadWidgetsOnDedicatedServer;
	}

	if (bShouldLoadCurors)
	{
		TArray<UObject*> LoadedClasses;
		for ( auto& Entry : SoftwareCursors )
		{
			LoadedClasses.Add(Entry.Value.TryLoad());
		}

		for (int32 i = 0; i < LoadedClasses.Num(); ++i)
		{
			UObject* Cursor = LoadedClasses[i];
			if (Cursor)
			{
				CursorClasses.Add(Cursor);
			}
			else
			{
				UE_LOG(LogLoad, Warning, TEXT("UUserInterfaceSettings::ForceLoadResources: Failed to load cursor resource %d."), i);
			}
		}

		CustomScalingRuleClassInstance = CustomScalingRuleClass.TryLoadClass<UDPICustomScalingRule>();
	}
}

#undef LOCTEXT_NAMESPACE
