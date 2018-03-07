// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Widget.h"
#include "Widgets/SNullWidget.h"
#include "Types/NavigationMetaData.h"
#include "Widgets/IToolTip.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SOverlay.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SToolTip.h"
#include "Binding/PropertyBinding.h"
#include "Blueprint/WidgetNavigation.h"
#include "Logging/MessageLog.h"
#include "Blueprint/UserWidget.h"
#include "Slate/SObjectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UMGStyle.h"
#include "Types/ReflectionMetadata.h"
#include "PropertyLocalizationDataGathering.h"
#include "HAL/LowLevelMemTracker.h"

#define LOCTEXT_NAMESPACE "UMG"

/**
* Interface for tool tips.
*/
class FDelegateToolTip : public IToolTip
{
public:

	/**
	* Gets the widget that this tool tip represents.
	*
	* @return The tool tip widget.
	*/
	virtual TSharedRef<class SWidget> AsWidget() override
	{
		return GetContentWidget();
	}

	/**
	* Gets the tool tip's content widget.
	*
	* @return The content widget.
	*/
	virtual TSharedRef<SWidget> GetContentWidget() override
	{
		if ( CachedToolTip.IsValid() )
		{
			return CachedToolTip.ToSharedRef();
		}

		UWidget* Widget = ToolTipWidgetDelegate.Execute();
		if ( Widget )
		{
			CachedToolTip = Widget->TakeWidget();
			return CachedToolTip.ToSharedRef();
		}

		return SNullWidget::NullWidget;
	}

	/**
	* Sets the tool tip's content widget.
	*
	* @param InContentWidget The new content widget to set.
	*/
	virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget) override
	{
		CachedToolTip = InContentWidget;
	}

	/**
	* Checks whether this tool tip has no content to display right now.
	*
	* @return true if the tool tip has no content to display, false otherwise.
	*/
	virtual bool IsEmpty() const override
	{
		return !ToolTipWidgetDelegate.IsBound();
	}

	/**
	* Checks whether this tool tip can be made interactive by the user (by holding Ctrl).
	*
	* @return true if it is an interactive tool tip, false otherwise.
	*/
	virtual bool IsInteractive() const override
	{
		return false;
	}

	virtual void OnClosed() override
	{
		//TODO Notify interface implementing widget of closure

		CachedToolTip.Reset();
	}

	virtual void OnOpening() override
	{
		//TODO Notify interface implementing widget of opening
	}

public:
	UWidget::FGetWidget ToolTipWidgetDelegate;

private:
	TSharedPtr<SWidget> CachedToolTip;
};


#if WITH_EDITORONLY_DATA
namespace
{
	void GatherWidgetForLocalization(const UObject* const Object, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const UWidget* const Widget = CastChecked<UWidget>(Object);

		EPropertyLocalizationGathererTextFlags WidgetGatherTextFlags = GatherTextFlags;

		// If we've instanced this widget from another asset, then we only want to process the widget itself (to process any overrides against the archetype), but skip all of its children
		if (UObject* WidgetGenerator = Widget->WidgetGeneratedBy.Get())
		{
			if (WidgetGenerator->GetOutermost() != Widget->GetOutermost())
			{
				WidgetGatherTextFlags |= EPropertyLocalizationGathererTextFlags::SkipSubObjects;
			}
		}

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromObject(Widget, WidgetGatherTextFlags);
	}
}
#endif


/////////////////////////////////////////////////////
// UWidget

TArray<TSubclassOf<UPropertyBinding>> UWidget::BinderClasses;

UWidget::UWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsEnabled = true;
	bIsVariable = true;
#if WITH_EDITOR
	DesignerFlags = EWidgetDesignFlags::None;
#endif
	Visibility = ESlateVisibility::Visible;
	RenderTransformPivot = FVector2D(0.5f, 0.5f);
	Cursor = EMouseCursor::Default;

#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(UWidget::StaticClass(), &GatherWidgetForLocalization); }
#endif
}

void UWidget::SetRenderTransform(FWidgetTransform Transform)
{
	RenderTransform = Transform;
	UpdateRenderTransform();
}

void UWidget::SetRenderScale(FVector2D Scale)
{
	RenderTransform.Scale = Scale;
	UpdateRenderTransform();
}

void UWidget::SetRenderShear(FVector2D Shear)
{
	RenderTransform.Shear = Shear;
	UpdateRenderTransform();
}

void UWidget::SetRenderAngle(float Angle)
{
	RenderTransform.Angle = Angle;
	UpdateRenderTransform();
}

void UWidget::SetRenderTranslation(FVector2D Translation)
{
	RenderTransform.Translation = Translation;
	UpdateRenderTransform();
}

void UWidget::UpdateRenderTransform()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		if (RenderTransform.IsIdentity())
		{
			SafeWidget->SetRenderTransform(TOptional<FSlateRenderTransform>());
		}
		else
		{
			SafeWidget->SetRenderTransform(RenderTransform.ToSlateRenderTransform());
		}
	}
}

void UWidget::SetRenderTransformPivot(FVector2D Pivot)
{
	RenderTransformPivot = Pivot;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SetRenderTransformPivot(Pivot);
	}
}

bool UWidget::GetIsEnabled() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	return SafeWidget.IsValid() ? SafeWidget->IsEnabled() : bIsEnabled;
}

void UWidget::SetIsEnabled(bool bInIsEnabled)
{
	bIsEnabled = bInIsEnabled;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SetEnabled(bInIsEnabled);
	}
}

void UWidget::SetCursor(EMouseCursor::Type InCursor)
{
	bOverride_Cursor = true;
	Cursor = InCursor;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->SetCursor(Cursor);
	}
}

void UWidget::ResetCursor()
{
	bOverride_Cursor = false;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->SetCursor(TOptional<EMouseCursor::Type>());
	}
}

bool UWidget::IsVisible() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		return SafeWidget->GetVisibility().IsVisible();
	}

	return false;
}

ESlateVisibility UWidget::GetVisibility() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return UWidget::ConvertRuntimeToSerializedVisibility(SafeWidget->GetVisibility());
	}

	return Visibility;
}

void UWidget::SetVisibility(ESlateVisibility InVisibility)
{
	Visibility = InVisibility;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		EVisibility SlateVisibility = UWidget::ConvertSerializedVisibilityToRuntime(InVisibility);
		SafeWidget->SetVisibility(SlateVisibility);
	}
}

EWidgetClipping UWidget::GetClipping() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->GetClipping();
	}

	return Clipping;
}

void UWidget::SetClipping(EWidgetClipping InClipping)
{
	Clipping = InClipping;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SetClipping(InClipping);
	}
}

void UWidget::ForceVolatile(bool bForce)
{
	bIsVolatile = bForce;
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->ForceVolatile(bForce);
	}
}

void UWidget::SetToolTipText(const FText& InToolTipText)
{
	ToolTipText = InToolTipText;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SetToolTipText(InToolTipText);
	}
}

void UWidget::SetToolTip(UWidget* InToolTipWidget)
{
	ToolTipWidget = InToolTipWidget;

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		if ( ToolTipWidget )
		{
			TSharedRef<SToolTip> ToolTip = SNew(SToolTip)
				.TextMargin(FMargin(0))
				.BorderImage(nullptr)
				[
					ToolTipWidget->TakeWidget()
				];

			SafeWidget->SetToolTip(ToolTip);
		}
		else
		{
			SafeWidget->SetToolTip(TSharedPtr<IToolTip>());
		}
	}
}

bool UWidget::IsHovered() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->IsHovered();
	}

	return false;
}

bool UWidget::HasKeyboardFocus() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->HasKeyboardFocus();
	}

	return false;
}

bool UWidget::HasMouseCapture() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->HasMouseCapture();
	}

	return false;
}

void UWidget::SetKeyboardFocus()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( !SafeWidget->SupportsKeyboardFocus() )
		{
			FMessageLog("PIE").Warning(LOCTEXT("ThisWidgetDoesntSupportFocus", "This widget does not support focus.  If this is a UserWidget, you should set bIsFocusable to true."));
		}
#endif

		if ( !FSlateApplication::Get().SetKeyboardFocus(SafeWidget) )
		{
			if ( UWorld* World = GetWorld() )
			{
				if ( ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController() )
				{
					LocalPlayer->GetSlateOperations().SetUserFocus(SafeWidget.ToSharedRef(), EFocusCause::SetDirectly);
				}
			}
		}
	}
}

bool UWidget::HasUserFocus(APlayerController* PlayerController) const
{
	if ( PlayerController == nullptr || !PlayerController->IsLocalPlayerController() )
	{
		return false;
	}

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		FLocalPlayerContext Context(PlayerController);

		if ( ULocalPlayer* LocalPlayer = Context.GetLocalPlayer() )
		{
			// HACK: We use the controller Id as the local player index for focusing widgets in Slate.
			int32 UserIndex = LocalPlayer->GetControllerId();

			TOptional<EFocusCause> FocusCause = SafeWidget->HasUserFocus(UserIndex);
			return FocusCause.IsSet();
		}
	}

	return false;
}

bool UWidget::HasAnyUserFocus() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		TOptional<EFocusCause> FocusCause = SafeWidget->HasAnyUserFocus();
		return FocusCause.IsSet();
	}

	return false;
}

bool UWidget::HasFocusedDescendants() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		return SafeWidget->HasFocusedDescendants();
	}

	return false;
}

bool UWidget::HasUserFocusedDescendants(APlayerController* PlayerController) const
{
	if ( PlayerController == nullptr || !PlayerController->IsLocalPlayerController() )
	{
		return false;
	}

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		FLocalPlayerContext Context(PlayerController);

		if ( ULocalPlayer* LocalPlayer = Context.GetLocalPlayer() )
		{
			// HACK: We use the controller Id as the local player index for focusing widgets in Slate.
			int32 UserIndex = LocalPlayer->GetControllerId();

			return SafeWidget->HasUserFocusedDescendants(UserIndex);
		}
	}

	return false;
}

void UWidget::SetUserFocus(APlayerController* PlayerController)
{
	if ( PlayerController == nullptr || !PlayerController->IsLocalPlayerController() || PlayerController->Player == nullptr )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FMessageLog("PIE").Error(LOCTEXT("NoPlayerControllerToFocus", "The PlayerController is not a valid local player so it can't focus the widget."));
#endif
		return;
	}

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( !SafeWidget->SupportsKeyboardFocus() )
		{
			FMessageLog("PIE").Warning(LOCTEXT("ThisWidgetDoesntSupportFocus", "This widget does not support focus.  If this is a UserWidget, you should set bIsFocusable to true."));
		}
#endif

		FLocalPlayerContext Context(PlayerController);

		if ( ULocalPlayer* LocalPlayer = Context.GetLocalPlayer() )
		{
			// HACK: We use the controller Id as the local player index for focusing widgets in Slate.
			int32 UserIndex = LocalPlayer->GetControllerId();

			if ( !FSlateApplication::Get().SetUserFocus(UserIndex, SafeWidget) )
			{
				LocalPlayer->GetSlateOperations().SetUserFocus(SafeWidget.ToSharedRef());
			}
		}
	}
}

void UWidget::ForceLayoutPrepass()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SafeWidget->SlatePrepass();
	}
}

void UWidget::InvalidateLayoutAndVolatility()
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		SafeWidget->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

FVector2D UWidget::GetDesiredSize() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		return SafeWidget->GetDesiredSize();
	}

	return FVector2D(0, 0);
}

void UWidget::SetNavigationRuleInternal(EUINavigation Direction, EUINavigationRule Rule, FName WidgetToFocus)
{
	if (Navigation == nullptr)
	{
		Navigation = NewObject<UWidgetNavigation>(this);
	}

	FWidgetNavigationData NavigationData;
	NavigationData.Rule = Rule;
	NavigationData.WidgetToFocus = WidgetToFocus;
	switch(Direction)
	{
		case EUINavigation::Up:
			Navigation->Up = NavigationData;
			break;
		case EUINavigation::Down:
			Navigation->Down = NavigationData;
			break;
		case EUINavigation::Left:
			Navigation->Left = NavigationData;
			break;
		case EUINavigation::Right:
			Navigation->Right = NavigationData;
			break;
		case EUINavigation::Next:
			Navigation->Next = NavigationData;
			break;
		case EUINavigation::Previous:
			Navigation->Previous = NavigationData;
			break;
		default:
			break;
	}
}

void UWidget::SetNavigationRule(EUINavigation Direction, EUINavigationRule Rule, FName WidgetToFocus)
{
	SetNavigationRuleInternal(Direction, Rule, WidgetToFocus);
	BuildNavigation();
}

void UWidget::SetAllNavigationRules(EUINavigationRule Rule, FName WidgetToFocus)
{
	SetNavigationRuleInternal(EUINavigation::Up, Rule, WidgetToFocus);
	SetNavigationRuleInternal(EUINavigation::Down, Rule, WidgetToFocus);
	SetNavigationRuleInternal(EUINavigation::Left, Rule, WidgetToFocus);
	SetNavigationRuleInternal(EUINavigation::Right, Rule, WidgetToFocus);
	SetNavigationRuleInternal(EUINavigation::Next, Rule, WidgetToFocus);
	SetNavigationRuleInternal(EUINavigation::Previous, Rule, WidgetToFocus);
	BuildNavigation();
}

UPanelWidget* UWidget::GetParent() const
{
	if ( Slot )
	{
		return Slot->Parent;
	}

	return nullptr;
}

void UWidget::RemoveFromParent()
{
	UPanelWidget* CurrentParent = GetParent();
	if ( CurrentParent )
	{
		CurrentParent->RemoveChild(this);
	}
	else
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( GetCachedWidget().IsValid() )
		{
			FText WarningMessage = FText::Format(LOCTEXT("RemoveFromParentWithNoParent", "UWidget::RemoveFromParent() called on '{0}' which has no UMG parent (if it was added directly to a native Slate widget via TakeWidget() then it must be removed explicitly rather than via RemoveFromParent())"), FText::AsCultureInvariant(GetPathName()));
			// @todo: nickd - we need to switch this back to a warning in engine, but info for games
			FMessageLog("PIE").Info(WarningMessage);
		}
#endif
	}
}

const FGeometry& UWidget::GetCachedGeometry() const
{
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( SafeWidget.IsValid() )
	{
		return SafeWidget->GetCachedGeometry();
	}

	return SNullWidget::NullWidget->GetCachedGeometry();
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UWidget::VerifySynchronizeProperties()
{
	ensureMsgf(bRoutedSynchronizeProperties, TEXT("%s failed to route SynchronizeProperties.  Please call Super::SynchronizeProperties() in your <className>::SynchronizeProperties() function."), *GetFullName());
}
#endif

void UWidget::OnWidgetRebuilt()
{
}

TSharedRef<SWidget> UWidget::TakeWidget()
{
	LLM_SCOPE(ELLMTag::UI);

	return TakeWidget_Private( []( UUserWidget* Widget, TSharedRef<SWidget> Content ) -> TSharedPtr<SObjectWidget> {
		       return SNew( SObjectWidget, Widget )[ Content ];
		   } );
}

TSharedRef<SWidget> UWidget::TakeWidget_Private(ConstructMethodType ConstructMethod)
{
	bool bNewlyCreated = false;
	TSharedPtr<SWidget> PublicWidget;

	// If the underlying widget doesn't exist we need to construct and cache the widget for the first run.
	if (!MyWidget.IsValid())
	{
		PublicWidget = RebuildWidget();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		ensureMsgf(PublicWidget.Get() != &SNullWidget::NullWidget.Get(), TEXT("Don't return SNullWidget from RebuildWidget, because we mutate the state of the return.  Return a SSpacer if you need to return a no-op widget."));
#endif

		MyWidget = PublicWidget;

		bNewlyCreated = true;
	}
	else
	{
		PublicWidget = MyWidget.Pin();
	}

	// If it is a user widget wrap it in a SObjectWidget to keep the instance from being GC'ed
	if (IsA(UUserWidget::StaticClass()))
	{
		TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();

		// If the GC Widget is still valid we still exist in the slate hierarchy, so just return the GC Widget.
		if (SafeGCWidget.IsValid())
		{
			ensure(bNewlyCreated == false);
			PublicWidget = SafeGCWidget;
		}
		else // Otherwise we need to recreate the wrapper widget
		{
			SafeGCWidget = ConstructMethod(Cast<UUserWidget>(this), PublicWidget.ToSharedRef());

			MyGCWidget = SafeGCWidget;
			PublicWidget = SafeGCWidget;
		}
	}

#if WITH_EDITOR
	if (IsDesignTime())
	{
		if (bNewlyCreated)
		{
			TSharedPtr<SWidget> SafeDesignWidget = RebuildDesignWidget(PublicWidget.ToSharedRef());
			if (SafeDesignWidget != PublicWidget)
			{
				DesignWrapperWidget = SafeDesignWidget;
				PublicWidget = SafeDesignWidget;
			}
		}
		else if (DesignWrapperWidget.IsValid())
		{
			PublicWidget = DesignWrapperWidget.Pin();
		}
	}
#endif

	if (bNewlyCreated)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		bRoutedSynchronizeProperties = false;
#endif

		SynchronizeProperties();
		VerifySynchronizeProperties();
		OnWidgetRebuilt();
	}

	return PublicWidget.ToSharedRef();
}

TSharedPtr<SWidget> UWidget::GetCachedWidget() const
{
#if WITH_EDITOR
	if (DesignWrapperWidget.IsValid())
	{
		return DesignWrapperWidget.Pin();
	}
#endif

	if (MyGCWidget.IsValid())
	{
		return MyGCWidget.Pin();
	}

	return MyWidget.Pin();
}

#if WITH_EDITOR

TSharedRef<SWidget> UWidget::RebuildDesignWidget(TSharedRef<SWidget> Content)
{
	return Content;
}

TSharedRef<SWidget> UWidget::CreateDesignerOutline(TSharedRef<SWidget> Content) const
{
	return SNew(SOverlay)
	
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			Content
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.Visibility(HasAnyDesignerFlags(EWidgetDesignFlags::ShowOutline) ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			.BorderImage(FUMGStyle::Get().GetBrush("MarchingAnts"))
		];
}

#endif

APlayerController* UWidget::GetOwningPlayer() const
{
	if ( UWidgetTree* WidgetTree = Cast<UWidgetTree>(GetOuter()) )
	{
		if ( UUserWidget* UserWidget = Cast<UUserWidget>(WidgetTree->GetOuter()) )
		{
			return UserWidget->GetOwningPlayer();
		}
	}

	return nullptr;
}

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "UMGEditor"

void UWidget::SetDesignerFlags(EWidgetDesignFlags::Type NewFlags)
{
	DesignerFlags = ( EWidgetDesignFlags::Type )( DesignerFlags | NewFlags );
}

void UWidget::SetDisplayLabel(const FString& InDisplayLabel)
{
	DisplayLabel = InDisplayLabel;
}

bool UWidget::IsGeneratedName() const
{
	if (!DisplayLabel.IsEmpty())
	{
		return false;
	}

	FString Name = GetName();

	if (Name == GetClass()->GetName() || Name.StartsWith(GetClass()->GetName() + TEXT("_")))
	{
		return true;
	}
	else if (GetClass()->ClassGeneratedBy != nullptr)
	{
		FString BaseNameForBP = GetClass()->GetName();
		BaseNameForBP.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);

		if (Name == BaseNameForBP || Name.StartsWith(BaseNameForBP + TEXT("_")))
		{
			return true;
		}
	}

	return false;
}

FString UWidget::GetLabelMetadata() const
{
	return TEXT("");
}

FText UWidget::GetLabelText() const
{
	return GetDisplayNameBase();
}

FText UWidget::GetLabelTextWithMetadata() const
{
	FText Label = GetDisplayNameBase();

	if (!bIsVariable || !GetLabelMetadata().IsEmpty())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("BaseName"), Label);
		Args.Add(TEXT("Metadata"), FText::FromString(GetLabelMetadata()));
		Label = FText::Format(LOCTEXT("NonVariableLabelFormat", "[{BaseName}]{Metadata}"), Args);
	}

	return Label;
}

FText UWidget::GetDisplayNameBase() const
{
	const bool bHasDisplayLabel = !DisplayLabel.IsEmpty();
	if (IsGeneratedName() && !bIsVariable)
	{
		return GetClass()->GetDisplayNameText();
	}
	else
	{
		return FText::FromString(bHasDisplayLabel ? DisplayLabel : GetName());
	}
}

const FText UWidget::GetPaletteCategory()
{
	return LOCTEXT("Uncategorized", "Uncategorized");
}

const FSlateBrush* UWidget::GetEditorIcon()
{
	return nullptr;
}

EVisibility UWidget::GetVisibilityInDesigner() const
{
	return bHiddenInDesigner ? EVisibility::Collapsed : EVisibility::Visible;
}

void UWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if (SafeWidget.IsValid())
	{
		SynchronizeProperties();
	}
}

void UWidget::SelectByDesigner()
{
	OnSelectedByDesigner();

	UWidget* Parent = GetParent();
	while ( Parent != nullptr )
	{
		Parent->OnDescendantSelectedByDesigner(this);
		Parent = Parent->GetParent();
	}
}

void UWidget::DeselectByDesigner()
{
	OnDeselectedByDesigner();

	UWidget* Parent = GetParent();
	while ( Parent != nullptr )
	{
		Parent->OnDescendantDeselectedByDesigner(this);
		Parent = Parent->GetParent();
	}
}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "UMG"
#endif

bool UWidget::Modify(bool bAlwaysMarkDirty)
{
	bool Modified = Super::Modify(bAlwaysMarkDirty);

	if ( Slot )
	{
		Modified &= Slot->Modify(bAlwaysMarkDirty);
	}

	return Modified;
}

bool UWidget::IsChildOf(UWidget* PossibleParent)
{
	UPanelWidget* Parent = GetParent();
	if ( Parent == nullptr )
	{
		return false;
	}
	else if ( Parent == PossibleParent )
	{
		return true;
	}
	
	return Parent->IsChildOf(PossibleParent);
}

TSharedRef<SWidget> UWidget::RebuildWidget()
{
	ensureMsgf(false, TEXT("You must implement RebuildWidget() in your child class"));
	return SNew(SSpacer);
}

void UWidget::SynchronizeProperties()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bRoutedSynchronizeProperties = true;
#endif

	// We want to apply the bindings to the cached widget, which could be the SWidget, or the SObjectWidget, 
	// in the case where it's a user widget.  We always want to prefer the SObjectWidget so that bindings to 
	// visibility and enabled status are not stomping values setup in the root widget in the User Widget.
	TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
	if ( !SafeWidget.IsValid() )
	{
		return;
	}

#if WITH_EDITOR
	TSharedPtr<SWidget> SafeContentWidget = MyGCWidget.IsValid() ? MyGCWidget.Pin() : MyWidget.Pin();
#endif

#if WITH_EDITOR
	// Always use an enabled and visible state in the designer.
	if ( IsDesignTime() )
	{
		SafeWidget->SetEnabled(true);
		SafeWidget->SetVisibility(BIND_UOBJECT_ATTRIBUTE(EVisibility, GetVisibilityInDesigner));
	}
	else 
#endif
	{
		if ( bOverride_Cursor /*|| CursorDelegate.IsBound()*/ )
		{
			SafeWidget->SetCursor(Cursor);// PROPERTY_BINDING(EMouseCursor::Type, Cursor));
		}

		SafeWidget->SetEnabled(BITFIELD_PROPERTY_BINDING( bIsEnabled ));
		SafeWidget->SetVisibility(OPTIONAL_BINDING_CONVERT(ESlateVisibility, Visibility, EVisibility, ConvertVisibility));
	}

#if WITH_EDITOR
	// In the designer, we need to apply the clip to bounds flag to the real widget, not the designer outline.
	// because we may be changing a critical default set on the base that not actually set on the outline.
	// An example of this, would be changing the clipping bounds on a scrollbox.  The outline never clipped to bounds
	// so unless we tweak the -actual- value on the SScrollBox, the user won't see a difference in how the widget clips.
	SafeContentWidget->SetClipping(Clipping);
#else
	SafeWidget->SetClipping(Clipping);
#endif

	SafeWidget->ForceVolatile(bIsVolatile);

	UpdateRenderTransform();
	SafeWidget->SetRenderTransformPivot(RenderTransformPivot);

	if ( ToolTipWidgetDelegate.IsBound() && !IsDesignTime() )
	{
		TSharedRef<FDelegateToolTip> ToolTip = MakeShareable(new FDelegateToolTip());
		ToolTip->ToolTipWidgetDelegate = ToolTipWidgetDelegate;
		SafeWidget->SetToolTip(ToolTip);
	}
	else if ( ToolTipWidget != nullptr )
	{
		TSharedRef<SToolTip> ToolTip = SNew(SToolTip)
			.TextMargin(FMargin(0))
			.BorderImage(nullptr)
			[
				ToolTipWidget->TakeWidget()
			];

		SafeWidget->SetToolTip(ToolTip);
	}
	else if ( !ToolTipText.IsEmpty() || ToolTipTextDelegate.IsBound() )
	{
		SafeWidget->SetToolTipText(PROPERTY_BINDING(FText, ToolTipText));
	}

#if WITH_EDITOR
	// In editor builds we add metadata to the widget so that once hit with the widget reflector it can report
	// where it comes from, what blueprint, what the name of the widget was...etc.
	SafeWidget->AddMetadata<FReflectionMetaData>(MakeShared<FReflectionMetaData>(GetFName(), GetClass(), this, WidgetGeneratedBy.Get()));
#else

#if !UE_BUILD_SHIPPING
	SafeWidget->AddMetadata<FReflectionMetaData>(MakeShared<FReflectionMetaData>(GetFName(), GetClass(), this, WidgetGeneratedByClass.Get()));
#endif

#endif
}

void UWidget::BuildNavigation()
{
	if ( Navigation != nullptr )
	{
		TSharedPtr<SWidget> SafeWidget = GetCachedWidget();

		if ( SafeWidget.IsValid() )
		{
			TSharedPtr<FNavigationMetaData> MetaData = SafeWidget->GetMetaData<FNavigationMetaData>();
			if ( !MetaData.IsValid() )
			{
				MetaData = MakeShared<FNavigationMetaData>();
				SafeWidget->AddMetadata(MetaData.ToSharedRef());
			}

			Navigation->UpdateMetaData(MetaData.ToSharedRef());
		}
	}
}

UWorld* UWidget::GetWorld() const
{
	// UWidget's are given world scope by their owning user widget.  We can get that through the widget tree that should
	// be the outer of this widget.
	if ( UWidgetTree* OwningTree = Cast<UWidgetTree>(GetOuter()) )
	{
		return OwningTree->GetWorld();
	}

	return nullptr;
}

EVisibility UWidget::ConvertSerializedVisibilityToRuntime(ESlateVisibility Input)
{
	switch ( Input )
	{
	case ESlateVisibility::Visible:
		return EVisibility::Visible;
	case ESlateVisibility::Collapsed:
		return EVisibility::Collapsed;
	case ESlateVisibility::Hidden:
		return EVisibility::Hidden;
	case ESlateVisibility::HitTestInvisible:
		return EVisibility::HitTestInvisible;
	case ESlateVisibility::SelfHitTestInvisible:
		return EVisibility::SelfHitTestInvisible;
	default:
		check(false);
		return EVisibility::Visible;
	}
}

ESlateVisibility UWidget::ConvertRuntimeToSerializedVisibility(const EVisibility& Input)
{
	if ( Input == EVisibility::Visible )
	{
		return ESlateVisibility::Visible;
	}
	else if ( Input == EVisibility::Collapsed )
	{
		return ESlateVisibility::Collapsed;
	}
	else if ( Input == EVisibility::Hidden )
	{
		return ESlateVisibility::Hidden;
	}
	else if ( Input == EVisibility::HitTestInvisible )
	{
		return ESlateVisibility::HitTestInvisible;
	}
	else if ( Input == EVisibility::SelfHitTestInvisible )
	{
		return ESlateVisibility::SelfHitTestInvisible;
	}
	else
	{
		check(false);
		return ESlateVisibility::Visible;
	}
}

FSizeParam UWidget::ConvertSerializedSizeParamToRuntime(const FSlateChildSize& Input)
{
	switch ( Input.SizeRule )
	{
	default:
	case ESlateSizeRule::Automatic:
		return FAuto();
	case ESlateSizeRule::Fill:
		return FStretch(Input.Value);
	}

	return FAuto();
}

UWidget* UWidget::FindChildContainingDescendant(UWidget* Root, UWidget* Descendant)
{
	if ( Root == nullptr )
	{
		return nullptr;
	}

	UWidget* Parent = Descendant->GetParent();

	while ( Parent != nullptr )
	{
		// If the Descendant's parent is the root, then the child containing the descendant is the descendant.
		if ( Parent == Root )
		{
			return Descendant;
		}

		Descendant = Parent;
		Parent = Parent->GetParent();
	}

	return nullptr;
}

//bool UWidget::BindProperty(const FName& DestinationProperty, UObject* SourceObject, const FName& SourceProperty)
//{
//	UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>(GetClass(), FName(*( DestinationProperty.ToString() + TEXT("Delegate") )));
//
//	if ( DelegateProperty )
//	{
//		FDynamicPropertyPath BindingPath(SourceProperty.ToString());
//		return AddBinding(DelegateProperty, SourceObject, BindingPath);
//	}
//
//	return false;
//}

TSubclassOf<UPropertyBinding> UWidget::FindBinderClassForDestination(UProperty* Property)
{
	if ( BinderClasses.Num() == 0 )
	{
		for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{
			if ( ClassIt->IsChildOf(UPropertyBinding::StaticClass()) )
			{
				BinderClasses.Add(*ClassIt);
			}
		}
	}

	for ( int32 ClassIndex = 0; ClassIndex < BinderClasses.Num(); ClassIndex++ )
	{
		if ( GetDefault<UPropertyBinding>(BinderClasses[ClassIndex])->IsSupportedDestination(Property))
		{
			return BinderClasses[ClassIndex];
		}
	}

	return nullptr;
}

static UPropertyBinding* GenerateBinder(UDelegateProperty* DelegateProperty, UObject* Container, UObject* SourceObject, const FDynamicPropertyPath& BindingPath)
{
	FScriptDelegate* ScriptDelegate = DelegateProperty->GetPropertyValuePtr_InContainer(Container);
	if ( ScriptDelegate )
	{
		// Only delegates that take no parameters have native binders.
		UFunction* SignatureFunction = DelegateProperty->SignatureFunction;
		if ( SignatureFunction->NumParms == 1 )
		{
			if ( UProperty* ReturnProperty = SignatureFunction->GetReturnProperty() )
			{
				TSubclassOf<UPropertyBinding> BinderClass = UWidget::FindBinderClassForDestination(ReturnProperty);
				if ( BinderClass != nullptr )
				{
					UPropertyBinding* Binder = NewObject<UPropertyBinding>(Container, BinderClass);
					Binder->SourceObject = SourceObject;
					Binder->SourcePath = BindingPath;
					Binder->Bind(ReturnProperty, ScriptDelegate);

					return Binder;
				}
			}
		}
	}

	return nullptr;
}

bool UWidget::AddBinding(UDelegateProperty* DelegateProperty, UObject* SourceObject, const FDynamicPropertyPath& BindingPath)
{
	if ( UPropertyBinding* Binder = GenerateBinder(DelegateProperty, this, SourceObject, BindingPath) )
	{
		// Remove any existing binding object for this property.
		for ( int32 BindingIndex = 0; BindingIndex < NativeBindings.Num(); BindingIndex++ )
		{
			if ( NativeBindings[BindingIndex]->DestinationProperty == DelegateProperty->GetFName() )
			{
				NativeBindings.RemoveAt(BindingIndex);
				break;
			}
		}

		NativeBindings.Add(Binder);

		// Only notify the bindings have changed if we've already create the underlying slate widget.
		if ( MyWidget.IsValid() )
		{
			OnBindingChanged(DelegateProperty->GetFName());
		}

		return true;
	}

	return false;
}

void UWidget::OnBindingChanged(const FName& Property)
{

}

#undef LOCTEXT_NAMESPACE
