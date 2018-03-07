// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundBase.h"
#include "Sound/SlateSound.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Components/NamedSlot.h"
#include "Slate/SObjectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Animation/UMGSequencePlayer.h"
#include "UObject/UnrealType.h"
#include "Blueprint/WidgetNavigation.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Interfaces/ITargetPlatform.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UObject/EditorObjectVersion.h"
#include "UMGPrivate.h"
#include "UObject/UObjectHash.h"
#include "PropertyPortFlags.h"

DECLARE_CYCLE_STAT(TEXT("UserWidget Create"), STAT_CreateWidget, STATGROUP_Slate);

#define LOCTEXT_NAMESPACE "UMG"

bool UUserWidget::bTemplateInitializing = false;
uint32 UUserWidget::bInitializingFromWidgetTree = 0;

static FGeometry NullGeometry;
static FSlateRect NullRect;
static FSlateWindowElementList NullElementList;
static FWidgetStyle NullStyle;

FPaintContext::FPaintContext()
	: AllottedGeometry(NullGeometry)
	, MyCullingRect(NullRect)
	, OutDrawElements(NullElementList)
	, LayerId(0)
	, WidgetStyle(NullStyle)
	, bParentEnabled(true)
	, MaxLayer(0)
{
}

/////////////////////////////////////////////////////
// UUserWidget
UUserWidget::UUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCanEverTick(true)
	, bCanEverPaint(true)
{
	ViewportAnchors = FAnchors(0, 0, 1, 1);
	Visibility = ESlateVisibility::SelfHitTestInvisible;

	bInitialized = false;
	bSupportsKeyboardFocus_DEPRECATED = true;
	bIsFocusable = false;
	ColorAndOpacity = FLinearColor::White;
	ForegroundColor = FSlateColor::UseForeground();

#if WITH_EDITORONLY_DATA
	DesignTimeSize = FVector2D(100, 100);
	PaletteCategory = LOCTEXT("UserCreated", "User Created");
	DesignSizeMode = EDesignPreviewSizeMode::FillScreen;
#endif
}

UWidgetBlueprintGeneratedClass* UUserWidget::GetWidgetTreeOwningClass()
{
	UWidgetBlueprintGeneratedClass* RootBGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass());
	UWidgetBlueprintGeneratedClass* BGClass = RootBGClass;

	while ( BGClass )
	{
		//TODO NickD: This conditional post load shouldn't be needed any more once the Fast Widget creation path is the only path!
		// Force post load on the generated class so all subobjects are done (specifically the widget tree).
		BGClass->ConditionalPostLoad();

		const bool bNoRootWidget = !BGClass->HasTemplate() && ( ( nullptr == BGClass->WidgetTree ) || ( nullptr == BGClass->WidgetTree->RootWidget ) );

		if ( bNoRootWidget )
		{
			UWidgetBlueprintGeneratedClass* SuperBGClass = Cast<UWidgetBlueprintGeneratedClass>(BGClass->GetSuperClass());
			if ( SuperBGClass )
			{
				BGClass = SuperBGClass;
				continue;
			}
			else
			{
				// If we reach a super class that isn't a UWidgetBlueprintGeneratedClass, return the root class.
				return RootBGClass;
			}
		}

		return BGClass;
	}

	return nullptr;
}

void UUserWidget::TemplateInit()
{
	TGuardValue<bool> InitGuard(bTemplateInitializing, true);
	TemplateInitInner();

	ForEachObjectWithOuter(this, [] (UObject* Child)
	{
		// Make sure to clear the entire hierarchy of the transient flag, we don't want some errant widget tree
		// to be culled from serialization accidentally.
		if ( UWidgetTree* InnerWidgetTree = Cast<UWidgetTree>(Child) )
		{
			InnerWidgetTree->ClearFlags(RF_Transient | RF_DefaultSubObject);
		}
	}, true);
}

void UUserWidget::TemplateInitInner()
{
	UWidgetBlueprintGeneratedClass* WidgetClass = GetWidgetTreeOwningClass();

	FObjectDuplicationParameters Parameters(WidgetClass->WidgetTree, this);
	Parameters.FlagMask = RF_Transactional;
	Parameters.PortFlags = PPF_DuplicateVerbatim;

	WidgetTree = (UWidgetTree*)StaticDuplicateObjectEx(Parameters);
	bCookedWidgetTree = true;

	if ( ensure(WidgetTree) )
	{
		for ( UWidgetAnimation* Animation : WidgetClass->Animations )
		{
			//UWidgetAnimation* DuplicatedAnimation = NewObject<UWidgetAnimation>(this, Animation->GetFName());
			//DuplicatedAnimation->MovieScene = Animation->MovieScene;
			//DuplicatedAnimation->AnimationBindings = Animation->AnimationBindings;
			UWidgetAnimation* DuplicatedAnimation = DuplicateObject<UWidgetAnimation>(Animation, this);

			if ( DuplicatedAnimation->GetMovieScene() )
			{
				// Find property with the same name as the template and assign the new widget to it.
				UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetClass, DuplicatedAnimation->GetMovieScene()->GetFName());
				if ( Prop )
				{
					Prop->SetObjectPropertyValue_InContainer(this, DuplicatedAnimation);
				}
			}
		}

		WidgetTree->ForEachWidget([this, WidgetClass] (UWidget* Widget) {

			Widget->WidgetGeneratedByClass = WidgetClass;

			// TODO UMG Make this an FName
			FString VariableName = Widget->GetName();

			// Find property with the same name as the template and assign the new widget to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetClass, *VariableName);
			if ( Prop )
			{
				Prop->SetObjectPropertyValue_InContainer(this, Widget);
#if UE_BUILD_DEBUG
				UObject* Value = Prop->GetObjectPropertyValue_InContainer(this);
				check(Value == Widget);
#endif
			}

			// Initialize Navigation Data
			if ( Widget->Navigation )
			{
				Widget->Navigation->ResolveExplictRules(WidgetTree);
			}

			if ( UUserWidget* UserWidget = Cast<UUserWidget>(Widget) )
			{
				UserWidget->TemplateInitInner();
			}
		});

		// Initialize the named slots!
		const bool bReparentToWidgetTree = true;
		InitializeNamedSlots(bReparentToWidgetTree);
	}
}

bool UUserWidget::VerifyTemplateIntegrity(TArray<FText>& OutErrors)
{
	bool bIsTemplateSafe = true;

	TArray<UObject*> ClonableSubObjectsSet;
	ClonableSubObjectsSet.Add(this);
	GetObjectsWithOuter(this, ClonableSubObjectsSet, true, RF_NoFlags, EInternalObjectFlags::PendingKill);

	TMap<FName, UObject*> QuickLookup;

	for ( UObject* Obj : ClonableSubObjectsSet )
	{
		QuickLookup.Add(Obj->GetFName(), Obj);

		for ( TFieldIterator<UObjectPropertyBase> PropIt(Obj->GetClass()); PropIt; ++PropIt )
		{
			UObjectPropertyBase* ObjProp = *PropIt;

			// If the property is transient, ignore it, we're not serializing it, so it shouldn't
			// be a problem if it's not instanced.
			if ( ObjProp->HasAnyPropertyFlags(CPF_Transient) )
			{
				continue;
			}

			UObject* ExternalObject = ObjProp->GetObjectPropertyValue_InContainer(Obj);

			// If the UObject property references any object in the tree, ensure that it's referenceable back.
			if ( ExternalObject )
			{
				if ( ExternalObject->IsIn(this) || ExternalObject == this )
				{
					if ( ObjProp->HasAllPropertyFlags(CPF_InstancedReference) )
					{
						continue;
					}

					OutErrors.Add(FText::Format(LOCTEXT("TemplatingFailed", "Fast CreateWidget Warning!  This class can not be created using the fast path, because the property {0} on {1} references {2}.  Please add the 'Instanced' flag to this property."),
						FText::FromString(ObjProp->GetName()), FText::FromString(ObjProp->GetOwnerClass()->GetName()), FText::FromString(ExternalObject->GetName())));

					bIsTemplateSafe = false;
				}
			}
		}
	}

	// See if a matching name appeared
	if ( UWidgetBlueprintGeneratedClass* TemplateClass = GetWidgetTreeOwningClass() )
	{
		TemplateClass->WidgetTree->ForEachWidgetAndDescendants([&OutErrors, &QuickLookup, &bIsTemplateSafe, TemplateClass] (UWidget* Widget) {
			
			if ( !QuickLookup.Contains(Widget->GetFName()) )
			{
				OutErrors.Add(FText::Format(LOCTEXT("MissingOriginWidgetInTemplate", "Widget '{0}' Missing From Template For {1}."),
					FText::FromString(Widget->GetPathName(TemplateClass->WidgetTree)), FText::FromString(TemplateClass->GetName())));

				bIsTemplateSafe = false;
			}

		});
	}

	return VerifyTemplateIntegrity(this, OutErrors) && bIsTemplateSafe;
}

bool UUserWidget::VerifyTemplateIntegrity(UUserWidget* TemplateRoot, TArray<FText>& OutErrors)
{
	bool bIsTemplateSafe = true;

	if ( WidgetTree == nullptr )
	{
		OutErrors.Add(FText::Format(LOCTEXT("NoWidgetTree", "Null Widget Tree {0}"), FText::FromString(GetName())));
		bIsTemplateSafe = false;
	}

	if ( bCookedWidgetTree == false )
	{
		OutErrors.Add(FText::Format(LOCTEXT("NoCookedWidgetTree", "No Cooked Widget Tree! {0}"), FText::FromString(GetName())));
		bIsTemplateSafe = false;
	}

	UClass* TemplateClass = GetClass();
	if ( WidgetTree != nullptr )
	{
		WidgetTree->ForEachWidget([this, TemplateClass, &bIsTemplateSafe, &OutErrors, TemplateRoot] (UWidget* Widget) {

			FName VariableFName = Widget->GetFName();

			// Find property with the same name as the template and assign the new widget to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(TemplateClass, VariableFName);
			if ( Prop )
			{
				UObject* Value = Prop->GetObjectPropertyValue_InContainer(this);
				if ( Value != Widget )
				{
					OutErrors.Add(FText::Format(LOCTEXT("WidgetTreeVerify", "Property in widget template did not load correctly, {0}."),
						FText::FromName(Prop->GetFName())));

					bIsTemplateSafe = false;
				}
			}

			UUserWidget* UserWidget = Cast<UUserWidget>(Widget);
			if ( UserWidget )
			{
				bIsTemplateSafe &= UserWidget->VerifyTemplateIntegrity(TemplateRoot, OutErrors);
			}
		});
	}

	return bIsTemplateSafe;
}

bool UUserWidget::CanInitialize() const
{
#if (WITH_EDITOR || UE_BUILD_DEBUG)
	if ( HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) )
	{
		return false;
	}

	// If this object is outered to an archetype or CDO, don't initialize the user widget.  That leads to a complex
	// and confusing serialization that when re-initialized later causes problems when copies of the template are made.
	for ( const UObjectBaseUtility* It = this; It; It = It->GetOuter() )
	{
		if ( It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) )
		{
			return false;
		}
	}
#endif

	return true;
}

bool UUserWidget::Initialize()
{
	// We don't want to initialize the widgets going into the widget templates, they're being setup in a
	// different way, and don't need to be initialized in their template form.
	ensure(bTemplateInitializing == false);

	// If it's not initialized initialize it, as long as it's not the CDO, we never initialize the CDO.
	if ( !bInitialized && ensure(CanInitialize()) )
	{
		bInitialized = true;

		// Only do this if this widget is of a blueprint class
		if ( UWidgetBlueprintGeneratedClass* BGClass = GetWidgetTreeOwningClass() )
		{
			BGClass->InitializeWidget(this);
		}
		else
		{
			InitializeNativeClassData();
		}

		if ( WidgetTree == nullptr )
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}

		if ( bCookedWidgetTree == false )
		{
			WidgetTree->SetFlags(RF_Transient);

			const bool bReparentToWidgetTree = false;
			InitializeNamedSlots(bReparentToWidgetTree);
		}

		return true;
	}

	return false;
}

void UUserWidget::InitializeNamedSlots(bool bReparentToWidgetTree)
{
	for ( FNamedSlotBinding& Binding : NamedSlotBindings )
	{
		if ( UWidget* BindingContent = Binding.Content )
		{
			UObjectPropertyBase* NamedSlotProperty = FindField<UObjectPropertyBase>(GetClass(), Binding.Name);
			if ( ensure(NamedSlotProperty) )
			{
				UNamedSlot* NamedSlot = Cast<UNamedSlot>(NamedSlotProperty->GetObjectPropertyValue_InContainer(this));
				if ( ensure(NamedSlot) )
				{
					NamedSlot->ClearChildren();
					NamedSlot->AddChild(BindingContent);

					//if ( bReparentToWidgetTree )
					//{
					//	FName NewName = MakeUniqueObjectName(WidgetTree, BindingContent->GetClass(), BindingContent->GetFName());
					//	BindingContent->Rename(*NewName.ToString(), WidgetTree, REN_DontCreateRedirectors | REN_DoNotDirty);
					//}
				}
			}
		}
	}
}

void UUserWidget::DuplicateAndInitializeFromWidgetTree(UWidgetTree* InWidgetTree)
{
	TScopeCounter<uint32> ScopeInitializingFromWidgetTree(bInitializingFromWidgetTree);

	if ( ensure(InWidgetTree) )
	{
		FObjectDuplicationParameters Parameters(InWidgetTree, this);

		// Set to be transient and strip public flags
		Parameters.ApplyFlags = RF_Transient | RF_DuplicateTransient;
		Parameters.FlagMask = Parameters.FlagMask & ~( RF_Public | RF_DefaultSubObject );

		WidgetTree = Cast<UWidgetTree>(StaticDuplicateObjectEx(Parameters));
	}
}

void UUserWidget::BeginDestroy()
{
	Super::BeginDestroy();

	// If anyone ever calls BeginDestroy explicitly on a widget we need to immediately remove it from
	// the the parent as it may be owned currently by a slate widget.  As long as it's the viewport we're
	// fine.
	RemoveFromParent();

	// If it's not owned by the viewport we need to take more extensive measures.  If the GC widget still
	// exists after this point we should just reset the widget, which will forcefully cause the SObjectWidget
	// to lose access to this UObject.
	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->ResetWidget();
	}
}

void UUserWidget::PostEditImport()
{
	Super::PostEditImport();

	//Initialize();
}

void UUserWidget::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	
	if ( bInitializingFromWidgetTree )
	{
		Initialize();
	}
}

void UUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	UWidget* RootWidget = GetRootWidget();
	if ( RootWidget )
	{
		RootWidget->ReleaseSlateResources(bReleaseChildren);
	}
}

void UUserWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// We get the GCWidget directly because MyWidget could be the fullscreen host widget if we've been added
	// to the viewport.
	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		TAttribute<FLinearColor> ColorBinding = PROPERTY_BINDING(FLinearColor, ColorAndOpacity);
		TAttribute<FSlateColor> ForegroundColorBinding = PROPERTY_BINDING(FSlateColor, ForegroundColor);

		SafeGCWidget->SetColorAndOpacity(ColorBinding);
		SafeGCWidget->SetForegroundColor(ForegroundColorBinding);
		SafeGCWidget->SetPadding(Padding);
	}
}

void UUserWidget::SetColorAndOpacity(FLinearColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;

	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->SetColorAndOpacity(ColorAndOpacity);
	}
}

void UUserWidget::SetForegroundColor(FSlateColor InForegroundColor)
{
	ForegroundColor = InForegroundColor;

	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->SetForegroundColor(ForegroundColor);
	}
}

void UUserWidget::SetPadding(FMargin InPadding)
{
	Padding = InPadding;

	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->SetPadding(Padding);
	}
}

UWorld* UUserWidget::GetWorld() const
{
	if ( UWorld* LastWorld = CachedWorld.Get() )
	{
		return LastWorld;
	}

	if ( HasAllFlags(RF_ClassDefaultObject) )
	{
		// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
		return nullptr;
	}

	// Use the Player Context's world, if a specific player context is given, otherwise fall back to
	// following the outer chain.
	if ( PlayerContext.IsValid() )
	{
		if ( UWorld* World = PlayerContext.GetWorld() )
		{
			CachedWorld = World;
			return World;
		}
	}

	// Could be a GameInstance, could be World, could also be a WidgetTree, so we're just going to follow
	// the outer chain to find the world we're in.
	UObject* Outer = GetOuter();

	while ( Outer )
	{
		UWorld* World = Outer->GetWorld();
		if ( World )
		{
			CachedWorld = World;
			return World;
		}

		Outer = Outer->GetOuter();
	}

	return nullptr;
}

UUMGSequencePlayer* UUserWidget::GetOrAddPlayer(UWidgetAnimation* InAnimation)
{
	if (InAnimation)
	{
		// @todo UMG sequencer - Restart animations which have had Play called on them?
		UUMGSequencePlayer** FoundPlayer = nullptr;
		for ( UUMGSequencePlayer * Player : ActiveSequencePlayers )
		{
			// We need to make sure we haven't stopped the animation, otherwise it'll get cancelled on the next frame.
			if (Player->GetAnimation() == InAnimation
			 && !StoppedSequencePlayers.Contains(Player))
			{
				FoundPlayer = &Player;
				break;
			}
		}

		if (!FoundPlayer)
		{
			UUMGSequencePlayer* NewPlayer = NewObject<UUMGSequencePlayer>(this, NAME_None, RF_Transient);
			ActiveSequencePlayers.Add(NewPlayer);

			NewPlayer->OnSequenceFinishedPlaying().AddUObject(this, &UUserWidget::OnAnimationFinishedPlaying);

			NewPlayer->InitSequencePlayer(*InAnimation, *this);

			return NewPlayer;
		}
		else
		{
			return *FoundPlayer;
		}
	}

	return nullptr;
}

void UUserWidget::Invalidate()
{
	TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (CachedWidget.IsValid())
	{
		CachedWidget->Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void UUserWidget::PlayAnimation( UWidgetAnimation* InAnimation, float StartAtTime, int32 NumberOfLoops, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed)
{
	FScopedNamedEvent NamedEvent(FColor::Emerald, "Widget::PlayAnimation");

	UUMGSequencePlayer* Player = GetOrAddPlayer(InAnimation);
	if (Player)
	{
		Player->Play(StartAtTime, NumberOfLoops, PlayMode, PlaybackSpeed);

		Invalidate();

		OnAnimationStarted(InAnimation);
	}

	//return Player;
}

void UUserWidget::PlayAnimationTo(UWidgetAnimation* InAnimation, float StartAtTime, float EndAtTime, int32 NumberOfLoops, EUMGSequencePlayMode::Type PlayMode, float PlaybackSpeed)
{
	FScopedNamedEvent NamedEvent(FColor::Emerald, "Widget::PlayAnimationTo");

	UUMGSequencePlayer* Player = GetOrAddPlayer(InAnimation);
	if (Player)
	{
		Player->PlayTo(StartAtTime, EndAtTime, NumberOfLoops, PlayMode, PlaybackSpeed);

		Invalidate();

		OnAnimationStarted(InAnimation);
	}

	//return Player;
}

void UUserWidget::StopAnimation(const UWidgetAnimation* InAnimation)
{
	if(InAnimation)
	{
		// @todo UMG sequencer - Restart animations which have had Play called on them?
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; } );

		if(FoundPlayer)
		{
			(*FoundPlayer)->Stop();
		}
	}
}

float UUserWidget::PauseAnimation(const UWidgetAnimation* InAnimation)
{
	if ( InAnimation )
	{
		// @todo UMG sequencer - Restart animations which have had Play called on them?
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&] (const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });

		if ( FoundPlayer )
		{
			( *FoundPlayer )->Pause();
			return (float)( *FoundPlayer )->GetTimeCursorPosition();
		}
	}

	return 0;
}

float UUserWidget::GetAnimationCurrentTime(const UWidgetAnimation* InAnimation) const
{
	if (InAnimation)
	{
		const UUMGSequencePlayer*const* FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });
		if (FoundPlayer)
		{
			return (float)(*FoundPlayer)->GetTimeCursorPosition();
		}
	}

	return 0;
}

bool UUserWidget::IsAnimationPlaying(const UWidgetAnimation* InAnimation) const
{
	if (InAnimation)
	{
		UUMGSequencePlayer* const* FoundPlayer = ActiveSequencePlayers.FindByPredicate(
			[ &](const UUMGSequencePlayer* Player)
		{
			return Player->GetAnimation() == InAnimation;
		});

		if (FoundPlayer)
		{
			return (*FoundPlayer)->GetPlaybackStatus() == EMovieScenePlayerStatus::Playing;
		}
	}

	return false;
}

bool UUserWidget::IsAnyAnimationPlaying() const
{
	return ActiveSequencePlayers.Num() > 0;
}

void UUserWidget::SetNumLoopsToPlay(const UWidgetAnimation* InAnimation, int32 InNumLoopsToPlay)
{
	if (InAnimation)
	{
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });

		if (FoundPlayer)
		{
			(*FoundPlayer)->SetNumLoopsToPlay(InNumLoopsToPlay);
		}
	}
}

void UUserWidget::SetPlaybackSpeed(const UWidgetAnimation* InAnimation, float PlaybackSpeed)
{
	if (InAnimation)
	{
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });

		if (FoundPlayer)
		{
			(*FoundPlayer)->SetPlaybackSpeed(PlaybackSpeed);
		}
	}
}

void UUserWidget::ReverseAnimation(const UWidgetAnimation* InAnimation)
{
	if (InAnimation)
	{
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });

		if (FoundPlayer)
		{
			(*FoundPlayer)->Reverse();
		}
	}
}

bool UUserWidget::IsAnimationPlayingForward(const UWidgetAnimation* InAnimation)
{
	if (InAnimation)
	{
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });

		if (FoundPlayer)
		{
			return (*FoundPlayer)->IsPlayingForward();
		}
	}

	return true;
}

void UUserWidget::OnAnimationFinishedPlaying(UUMGSequencePlayer& Player)
{
	OnAnimationFinished( Player.GetAnimation() );

	if ( Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped )
	{
		StoppedSequencePlayers.Add(&Player);
	}
}

void UUserWidget::PlaySound(USoundBase* SoundToPlay)
{
	if (SoundToPlay)
	{
		FSlateSound NewSound;
		NewSound.SetResourceObject(SoundToPlay);
		FSlateApplication::Get().PlaySound(NewSound);
	}
}

UWidget* UUserWidget::GetWidgetHandle(TSharedRef<SWidget> InWidget)
{
	return WidgetTree->FindWidget(InWidget);
}

TSharedRef<SWidget> UUserWidget::RebuildWidget()
{
	check(!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject));

	// In the event this widget is replaced in memory by the blueprint compiler update
	// the widget won't be properly initialized, so we ensure it's initialized and initialize
	// it if it hasn't been.
	if ( !bInitialized )
	{
		Initialize();
	}

	// Setup the player context on sub user widgets, if we have a valid context
	if (PlayerContext.IsValid())
	{
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( UUserWidget* UserWidget = Cast<UUserWidget>(Widget) )
			{
				UserWidget->SetPlayerContext(PlayerContext);
			}
		});
	}

	// Add the first component to the root of the widget surface.
	TSharedRef<SWidget> UserRootWidget = WidgetTree->RootWidget ? WidgetTree->RootWidget->TakeWidget() : TSharedRef<SWidget>(SNew(SSpacer));

	return UserRootWidget;
}

void UUserWidget::OnWidgetRebuilt()
{
	// When a user widget is rebuilt we can safely initialize the navigation now since all the slate
	// widgets should be held onto by a smart pointer at this point.
	WidgetTree->ForEachWidget([&] (UWidget* Widget) {
		Widget->BuildNavigation();
	});

	if (!IsDesignTime())
	{
		// Notify the widget to run per-construct.
		NativePreConstruct();

		// Notify the widget that it has been constructed.
		NativeConstruct();
	}
#if WITH_EDITOR
	else if ( HasAnyDesignerFlags(EWidgetDesignFlags::ExecutePreConstruct) )
	{
		NativePreConstruct();
	}
#endif
}

TSharedPtr<SWidget> UUserWidget::GetSlateWidgetFromName(const FName& Name) const
{
	UWidget* WidgetObject = WidgetTree->FindWidget(Name);
	if ( WidgetObject )
	{
		return WidgetObject->GetCachedWidget();
	}

	return TSharedPtr<SWidget>();
}

UWidget* UUserWidget::GetWidgetFromName(const FName& Name) const
{
	return WidgetTree->FindWidget(Name);
}

void UUserWidget::GetSlotNames(TArray<FName>& SlotNames) const
{
	// Only do this if this widget is of a blueprint class
	if ( UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()) )
	{
		SlotNames.Append(BGClass->NamedSlots);
	}
	else // For non-blueprint widget blueprints we have to go through the widget tree to locate the named slots dynamically.
	{
		TArray<FName> NamedSlots;
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( Widget && Widget->IsA<UNamedSlot>() )
			{
				NamedSlots.Add(Widget->GetFName());
			}
		});
	}
}

UWidget* UUserWidget::GetContentForSlot(FName SlotName) const
{
	for ( const FNamedSlotBinding& Binding : NamedSlotBindings )
	{
		if ( Binding.Name == SlotName )
		{
			return Binding.Content;
		}
	}

	return nullptr;
}

void UUserWidget::SetContentForSlot(FName SlotName, UWidget* Content)
{
	bool bFoundExistingSlot = false;

	// Find the binding in the existing set and replace the content for that binding.
	for ( int32 BindingIndex = 0; BindingIndex < NamedSlotBindings.Num(); BindingIndex++ )
	{
		FNamedSlotBinding& Binding = NamedSlotBindings[BindingIndex];

		if ( Binding.Name == SlotName )
		{
			bFoundExistingSlot = true;

			if ( Content )
			{
				Binding.Content = Content;
			}
			else
			{
				NamedSlotBindings.RemoveAt(BindingIndex);
			}

			break;
		}
	}

	if ( !bFoundExistingSlot && Content )
	{
		// Add the new binding to the list of bindings.
		FNamedSlotBinding NewBinding;
		NewBinding.Name = SlotName;
		NewBinding.Content = Content;

		NamedSlotBindings.Add(NewBinding);
	}

	// Dynamically insert the new widget into the hierarchy if it exists.
	if ( WidgetTree )
	{
		UNamedSlot* NamedSlot = Cast<UNamedSlot>(WidgetTree->FindWidget(SlotName));
		if ( NamedSlot )
		{
			NamedSlot->ClearChildren();

			if ( Content )
			{
				NamedSlot->AddChild(Content);
			}
		}
	}
}

UWidget* UUserWidget::GetRootWidget() const
{
	if ( WidgetTree )
	{
		return WidgetTree->RootWidget;
	}

	return nullptr;
}

void UUserWidget::AddToViewport(int32 ZOrder)
{
	AddToScreen(nullptr, ZOrder);
}

bool UUserWidget::AddToPlayerScreen(int32 ZOrder)
{
	if ( ULocalPlayer* LocalPlayer = GetOwningLocalPlayer() )
	{
		AddToScreen(LocalPlayer, ZOrder);
		return true;
	}

	FMessageLog("PIE").Error(LOCTEXT("AddToPlayerScreen_NoPlayer", "AddToPlayerScreen Failed.  No Owning Player!"));
	return false;
}

void UUserWidget::AddToScreen(ULocalPlayer* Player, int32 ZOrder)
{
	if ( !FullScreenWidget.IsValid() )
	{
		if ( UPanelWidget* ParentPanel = GetParent() )
		{
			FMessageLog("PIE").Error(FText::Format(LOCTEXT("WidgetAlreadyHasParent", "The widget '{0}' already has a parent widget.  It can't also be added to the viewport!"),
				FText::FromString(GetClass()->GetName())));
			return;
		}

		// First create and initialize the variable so that users calling this function twice don't
		// attempt to add the widget to the viewport again.
		TSharedRef<SConstraintCanvas> FullScreenCanvas = SNew(SConstraintCanvas);
		FullScreenWidget = FullScreenCanvas;

		TSharedRef<SWidget> UserSlateWidget = TakeWidget();

		FullScreenCanvas->AddSlot()
			.Offset(BIND_UOBJECT_ATTRIBUTE(FMargin, GetFullScreenOffset))
			.Anchors(BIND_UOBJECT_ATTRIBUTE(FAnchors, GetAnchorsInViewport))
			.Alignment(BIND_UOBJECT_ATTRIBUTE(FVector2D, GetAlignmentInViewport))
			[
				UserSlateWidget
			];

		// If this is a game world add the widget to the current worlds viewport.
		UWorld* World = GetWorld();
		if ( World && World->IsGameWorld() )
		{
			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				if ( Player )
				{
					ViewportClient->AddViewportWidgetForPlayer(Player, FullScreenCanvas, ZOrder);
				}
				else
				{
					// We add 10 to the zorder when adding to the viewport to avoid 
					// displaying below any built-in controls, like the virtual joysticks on mobile builds.
					ViewportClient->AddViewportWidgetContent(FullScreenCanvas, ZOrder + 10);
				}

				// Just in case we already hooked this delegate, remove the handler.
				FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

				// Widgets added to the viewport are automatically removed if the persistent level is unloaded.
				FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UUserWidget::OnLevelRemovedFromWorld);
			}
		}
	}
	else
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("WidgetAlreadyOnScreen", "The widget '{0}' was already added to the screen."),
			FText::FromString(GetClass()->GetName())));
	}
}

void UUserWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	// If the InLevel is null, it's a signal that the entire world is about to disappear, so
	// go ahead and remove this widget from the viewport, it could be holding onto too many
	// dangerous actor references that won't carry over into the next world.
	if ( InLevel == nullptr && InWorld == GetWorld() )
	{
		RemoveFromParent();
	}
}

void UUserWidget::RemoveFromViewport()
{
	RemoveFromParent();
}

void UUserWidget::RemoveFromParent()
{
	if ( FullScreenWidget.IsValid() )
	{
		TSharedPtr<SWidget> WidgetHost = FullScreenWidget.Pin();

		// If this is a game world add the widget to the current worlds viewport.
		UWorld* World = GetWorld();
		if ( World && World->IsGameWorld() )
		{
			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				TSharedRef<SWidget> WidgetHostRef = WidgetHost.ToSharedRef();

				ViewportClient->RemoveViewportWidgetContent(WidgetHostRef);

				if ( ULocalPlayer* LocalPlayer = GetOwningLocalPlayer() )
				{
					ViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, WidgetHostRef);
				}

				FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
			}
		}
	}
	else
	{
		Super::RemoveFromParent();
	}
}

bool UUserWidget::GetIsVisible() const
{
	return FullScreenWidget.IsValid();
}

bool UUserWidget::IsInViewport() const
{
	return FullScreenWidget.IsValid();
}

void UUserWidget::SetPlayerContext(const FLocalPlayerContext& InPlayerContext)
{
	PlayerContext = InPlayerContext;
}

const FLocalPlayerContext& UUserWidget::GetPlayerContext() const
{
	return PlayerContext;
}

ULocalPlayer* UUserWidget::GetOwningLocalPlayer() const
{
	if (PlayerContext.IsValid())
	{
		return PlayerContext.GetLocalPlayer();
	}
	return nullptr;
}

void UUserWidget::SetOwningLocalPlayer(ULocalPlayer* LocalPlayer)
{
	if ( LocalPlayer )
	{
		PlayerContext = FLocalPlayerContext(LocalPlayer, GetWorld());
	}
}

APlayerController* UUserWidget::GetOwningPlayer() const
{
	return PlayerContext.IsValid() ? PlayerContext.GetPlayerController() : nullptr;
}

void UUserWidget::SetOwningPlayer(APlayerController* LocalPlayerController)
{
	if (LocalPlayerController && LocalPlayerController->IsLocalController())
	{
		PlayerContext = FLocalPlayerContext(LocalPlayerController);
	}
}

class APawn* UUserWidget::GetOwningPlayerPawn() const
{
	if ( APlayerController* PC = GetOwningPlayer() )
	{
		return PC->GetPawn();
	}

	return nullptr;
}

void UUserWidget::SetPositionInViewport(FVector2D Position, bool bRemoveDPIScale )
{
	if ( bRemoveDPIScale )
	{
		float Scale = UWidgetLayoutLibrary::GetViewportScale(this);

		ViewportOffsets.Left = Position.X / Scale;
		ViewportOffsets.Top = Position.Y / Scale;
	}
	else
	{
		ViewportOffsets.Left = Position.X;
		ViewportOffsets.Top = Position.Y;
	}

	ViewportAnchors = FAnchors(0, 0);
}

void UUserWidget::SetDesiredSizeInViewport(FVector2D DesiredSize)
{
	ViewportOffsets.Right = DesiredSize.X;
	ViewportOffsets.Bottom = DesiredSize.Y;

	ViewportAnchors = FAnchors(0, 0);
}

void UUserWidget::SetAnchorsInViewport(FAnchors Anchors)
{
	ViewportAnchors = Anchors;
}

void UUserWidget::SetAlignmentInViewport(FVector2D Alignment)
{
	ViewportAlignment = Alignment;
}

FMargin UUserWidget::GetFullScreenOffset() const
{
	// If the size is zero, and we're not stretched, then use the desired size.
	FVector2D FinalSize = FVector2D(ViewportOffsets.Right, ViewportOffsets.Bottom);
	if ( FinalSize.IsZero() && !ViewportAnchors.IsStretchedVertical() && !ViewportAnchors.IsStretchedHorizontal() )
	{
		TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
		if ( CachedWidget.IsValid() )
		{
			FinalSize = CachedWidget->GetDesiredSize();
		}
	}

	return FMargin(ViewportOffsets.Left, ViewportOffsets.Top, FinalSize.X, FinalSize.Y);
}

FAnchors UUserWidget::GetAnchorsInViewport() const
{
	return ViewportAnchors;
}

FVector2D UUserWidget::GetAlignmentInViewport() const
{
	return ViewportAlignment;
}

void UUserWidget::RemoveObsoleteBindings(const TArray<FName>& NamedSlots)
{
	for (int32 BindingIndex = 0; BindingIndex < NamedSlotBindings.Num(); BindingIndex++)
	{
		const FNamedSlotBinding& Binding = NamedSlotBindings[BindingIndex];

		if (!NamedSlots.Contains(Binding.Name))
		{
			NamedSlotBindings.RemoveAt(BindingIndex);
			BindingIndex--;
		}
	}
}

#if WITH_EDITOR

const FText UUserWidget::GetPaletteCategory()
{
	return PaletteCategory;
}

void UUserWidget::SetDesignerFlags(EWidgetDesignFlags::Type NewFlags)
{
	Super::SetDesignerFlags(NewFlags);

	if ( ensure(WidgetTree) )
	{
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			Widget->SetDesignerFlags(NewFlags);
		});
	}
}

void UUserWidget::OnDesignerChanged(const FDesignerChangedEventArgs& EventArgs)
{
	Super::OnDesignerChanged(EventArgs);

	if ( ensure(WidgetTree) )
	{
		WidgetTree->ForEachWidget([&EventArgs] (UWidget* Widget) {
			Widget->OnDesignerChanged(EventArgs);
		});
	}
}

void UUserWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if ( PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
		if ( SafeWidget.IsValid() )
		{
			// Re-Run execute PreConstruct when we get a post edit property change, to do something
			// akin to running Sync Properties, so users don't have to recompile to see updates.
			NativePreConstruct();
		}
	}
}

#endif

void UUserWidget::OnAnimationStarted_Implementation(const UWidgetAnimation* Animation)
{

}

void UUserWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{

}

// Native handling for SObjectWidget

void UUserWidget::NativePreConstruct()
{
	PreConstruct(IsDesignTime());
}

void UUserWidget::NativeConstruct()
{
	Construct();
}

void UUserWidget::NativeDestruct()
{
	StopListeningForAllInputActions();
	Destruct();
}

void UUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	GInitRunaway();

	TickActionsAndAnimation(MyGeometry, InDeltaTime);

	if ( bCanEverTick )
	{
		Tick(MyGeometry, InDeltaTime);
	}
}

void UUserWidget::TickActionsAndAnimation(const FGeometry& MyGeometry, float InDeltaTime)
{
	if ( IsDesignTime() )
	{
		return;
	}

	// Update active movie scenes, none will be removed here, but new
	// ones can be added during the tick, if a player ends and triggers
	// starting another animation
	for ( int32 Index = 0; Index < ActiveSequencePlayers.Num(); Index++ )
	{
		UUMGSequencePlayer* Player = ActiveSequencePlayers[Index];
		Player->Tick( InDeltaTime );
	}

	const bool bWasPlayingAnimation = IsPlayingAnimation();

	// The process of ticking the players above can stop them so we remove them after all players have ticked
	for ( UUMGSequencePlayer* StoppedPlayer : StoppedSequencePlayers )
	{
		ActiveSequencePlayers.RemoveSwap(StoppedPlayer);
	}

	StoppedSequencePlayers.Empty();

	// If we're no longer playing animations invalidate layout so that we recache the volatility of the widget.
	if ( bWasPlayingAnimation && IsPlayingAnimation() == false )
	{
		Invalidate();
	}

	UWorld* World = GetWorld();
	if ( World )
	{
		// Update any latent actions we have for this actor
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		LatentActionManager.ProcessLatentActions(this, InDeltaTime);
	}
}

void UUserWidget::ListenForInputAction( FName ActionName, TEnumAsByte< EInputEvent > EventType, bool bConsume, FOnInputAction Callback )
{
	if ( !InputComponent )
	{
		InitializeInputComponent();
	}

	if ( InputComponent )
	{
		FInputActionBinding NewBinding( ActionName, EventType.GetValue() );
		NewBinding.bConsumeInput = bConsume;
		NewBinding.ActionDelegate.GetDelegateForManualSet().BindUObject( this, &ThisClass::OnInputAction, Callback );

		InputComponent->AddActionBinding( NewBinding );
	}
}

void UUserWidget::StopListeningForInputAction( FName ActionName, TEnumAsByte< EInputEvent > EventType )
{
	if ( InputComponent )
	{
		for ( int32 ExistingIndex = InputComponent->GetNumActionBindings() - 1; ExistingIndex >= 0; --ExistingIndex )
		{
			const FInputActionBinding& ExistingBind = InputComponent->GetActionBinding( ExistingIndex );
			if ( ExistingBind.ActionName == ActionName && ExistingBind.KeyEvent == EventType )
			{
				InputComponent->RemoveActionBinding( ExistingIndex );
			}
		}
	}
}

void UUserWidget::StopListeningForAllInputActions()
{
	if ( InputComponent )
	{
		for ( int32 ExistingIndex = InputComponent->GetNumActionBindings() - 1; ExistingIndex >= 0; --ExistingIndex )
		{
			InputComponent->RemoveActionBinding( ExistingIndex );
		}

		UnregisterInputComponent();

		InputComponent->ClearActionBindings();
		InputComponent->MarkPendingKill();
		InputComponent = nullptr;
	}
}

bool UUserWidget::IsListeningForInputAction( FName ActionName ) const
{
	bool bResult = false;
	if ( InputComponent )
	{
		for ( int32 ExistingIndex = InputComponent->GetNumActionBindings() - 1; ExistingIndex >= 0; --ExistingIndex )
		{
			const FInputActionBinding& ExistingBind = InputComponent->GetActionBinding( ExistingIndex );
			if ( ExistingBind.ActionName == ActionName )
			{
				bResult = true;
				break;
			}
		}
	}

	return bResult;
}

void UUserWidget::RegisterInputComponent()
{
	if ( InputComponent )
	{
		if ( APlayerController* Controller = GetOwningPlayer() )
		{
			Controller->PushInputComponent(InputComponent);
		}
	}
}

void UUserWidget::UnregisterInputComponent()
{
	if ( InputComponent )
	{
		if ( APlayerController* Controller = GetOwningPlayer() )
		{
			Controller->PopInputComponent(InputComponent);
		}
	}
}

void UUserWidget::SetInputActionPriority( int32 NewPriority )
{
	if ( InputComponent )
	{
		Priority = NewPriority;
		InputComponent->Priority = Priority;
	}
}

void UUserWidget::SetInputActionBlocking( bool bShouldBlock )
{
	if ( InputComponent )
	{
		bStopAction = bShouldBlock;
		InputComponent->bBlockInput = bStopAction;
	}
}

void UUserWidget::OnInputAction( FOnInputAction Callback )
{
	if ( GetIsEnabled() )
	{
		Callback.ExecuteIfBound();
	}
}

void UUserWidget::InitializeInputComponent()
{
	if ( APlayerController* Controller = GetOwningPlayer() )
	{
		InputComponent = NewObject< UInputComponent >( this, NAME_None, RF_Transient );
		InputComponent->bBlockInput = bStopAction;
		InputComponent->Priority = Priority;
		Controller->PushInputComponent( InputComponent );
	}
	else
	{
		FMessageLog("PIE").Info(FText::Format(LOCTEXT("NoInputListeningWithoutPlayerController", "Unable to listen to input actions without a player controller in {0}."), FText::FromName(GetClass()->GetFName())));
	}
}

void UUserWidget::NativePaint( FPaintContext& InContext ) const
{
	if ( bCanEverPaint )
	{
		OnPaint( InContext );
	}
}

bool UUserWidget::NativeIsInteractable() const
{
	return IsInteractable();
}

bool UUserWidget::NativeSupportsKeyboardFocus() const
{
	return bIsFocusable;
}

FReply UUserWidget::NativeOnFocusReceived( const FGeometry& InGeometry, const FFocusEvent& InFocusEvent )
{
	return OnFocusReceived( InGeometry, InFocusEvent ).NativeReply;
}

void UUserWidget::NativeOnFocusLost( const FFocusEvent& InFocusEvent )
{
	OnFocusLost( InFocusEvent );
}

void UUserWidget::NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		const bool bDecendantNewlyFocused = NewWidgetPath.ContainsWidget(SafeGCWidget.ToSharedRef());
		if ( bDecendantNewlyFocused )
		{
			const bool bDecendantPreviouslyFocused = PreviousFocusPath.ContainsWidget(SafeGCWidget.ToSharedRef());
			if ( !bDecendantPreviouslyFocused )
			{
				NativeOnAddedToFocusPath( InFocusEvent );
			}
		}
		else
		{
			NativeOnRemovedFromFocusPath( InFocusEvent );
		}
	}
}

void UUserWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	OnAddedToFocusPath(InFocusEvent);
}

void UUserWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	OnRemovedFromFocusPath(InFocusEvent);
}

FNavigationReply UUserWidget::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// No Blueprint Support At This Time

	return InDefaultReply;
}

FReply UUserWidget::NativeOnKeyChar( const FGeometry& InGeometry, const FCharacterEvent& InCharEvent )
{
	return OnKeyChar( InGeometry, InCharEvent ).NativeReply;
}

FReply UUserWidget::NativeOnPreviewKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	return OnPreviewKeyDown( InGeometry, InKeyEvent ).NativeReply;
}

FReply UUserWidget::NativeOnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	return OnKeyDown( InGeometry, InKeyEvent ).NativeReply;
}

FReply UUserWidget::NativeOnKeyUp( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	return OnKeyUp( InGeometry, InKeyEvent ).NativeReply;
}

FReply UUserWidget::NativeOnAnalogValueChanged( const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent )
{
	return OnAnalogValueChanged( InGeometry, InAnalogEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown( InGeometry, InMouseEvent ).NativeReply;
}

FReply UUserWidget::NativeOnPreviewMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnPreviewMouseButtonDown( InGeometry, InMouseEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMouseButtonUp( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonUp(InGeometry, InMouseEvent).NativeReply;
}

FReply UUserWidget::NativeOnMouseMove( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseMove( InGeometry, InMouseEvent ).NativeReply;
}

void UUserWidget::NativeOnMouseEnter( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	OnMouseEnter( InGeometry, InMouseEvent );
}

void UUserWidget::NativeOnMouseLeave( const FPointerEvent& InMouseEvent )
{
	OnMouseLeave( InMouseEvent );
}

FReply UUserWidget::NativeOnMouseWheel( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseWheel( InGeometry, InMouseEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMouseButtonDoubleClick( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDoubleClick( InGeometry, InMouseEvent ).NativeReply;
}

void UUserWidget::NativeOnDragDetected( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation )
{
	OnDragDetected( InGeometry, InMouseEvent, OutOperation);
}

void UUserWidget::NativeOnDragEnter( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	OnDragEnter( InGeometry, InDragDropEvent, InOperation );
}

void UUserWidget::NativeOnDragLeave( const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	OnDragLeave( InDragDropEvent, InOperation );
}

bool UUserWidget::NativeOnDragOver( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	return OnDragOver( InGeometry, InDragDropEvent, InOperation );
}

bool UUserWidget::NativeOnDrop( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	return OnDrop( InGeometry, InDragDropEvent, InOperation );
}

void UUserWidget::NativeOnDragCancelled( const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	OnDragCancelled( InDragDropEvent, InOperation );
}

FReply UUserWidget::NativeOnTouchGesture( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchGesture( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnTouchStarted( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchStarted( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnTouchMoved( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchMoved( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnTouchEnded( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchEnded( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMotionDetected( const FGeometry& InGeometry, const FMotionEvent& InMotionEvent )
{
	return OnMotionDetected( InGeometry, InMotionEvent ).NativeReply;
}

FCursorReply UUserWidget::NativeOnCursorQuery( const FGeometry& InGeometry, const FPointerEvent& InCursorEvent )
{
	return FCursorReply::Unhandled();
}

FNavigationReply UUserWidget::NativeOnNavigation(const FGeometry& InGeometry, const FNavigationEvent& InNavigationEvent)
{
	return FNavigationReply::Escape();
}
	
void UUserWidget::NativeOnMouseCaptureLost()
{
	OnMouseCaptureLost();
}

bool UUserWidget::ShouldSerializeWidgetTree(const ITargetPlatform* TargetPlatform) const
{
	if ( UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()) )
	{
		// Non-templateable user widgets can not preserve their hierarchy.
		if ( !BGClass->HasTemplate() )
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	// Don't store it on the CDO.
	if ( HasAllFlags(RF_ClassDefaultObject) )
	{
		return false;
	}

	// We preserve widget trees on Archetypes (that are not the CDO).
	if ( HasAllFlags(RF_ArchetypeObject) )
	{
		return true;
	}

	// We also preserve widget trees if you're a sub-object of an archetype.
	for ( const UObjectBaseUtility* It = this; It; It = It->GetOuter() )
	{
		if ( It->HasAllFlags(RF_ArchetypeObject) )
		{
			return true;
		}
	}

	return false;
}

bool UUserWidget::IsAsset() const
{
	return false;
}

void UUserWidget::PreSave(const class ITargetPlatform* TargetPlatform)
{
	if ( WidgetTree )
	{
		if ( ShouldSerializeWidgetTree(TargetPlatform) )
		{
			bCookedWidgetTree = true;
			WidgetTree->ClearFlags(RF_Transient);
		}
		else
		{
			bCookedWidgetTree = false;
			WidgetTree->SetFlags(RF_Transient);
		}
	}
	else
	{
		bCookedWidgetTree = false;
		ensure(ShouldSerializeWidgetTree(TargetPlatform) == false);
	}

	// Remove bindings that are no longer contained in the class.
	if ( UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()) )
	{
		RemoveObsoleteBindings(BGClass->NamedSlots);
	}

	Super::PreSave(TargetPlatform);
}

void UUserWidget::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UUserWidget* DefeaultWidget = Cast<UUserWidget>(GetClass()->GetDefaultObject());
		bCanEverTick = DefeaultWidget->bCanEverTick;
		bCanEverPaint = DefeaultWidget->bCanEverPaint;
	}
#else
	if ( HasAnyFlags(RF_ArchetypeObject) && !HasAllFlags(RF_ClassDefaultObject) )
	{
		if ( UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()) )
		{
			WidgetClass->SetTemplate(this);
		}
	}
#endif
}

void UUserWidget::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	if ( Ar.IsLoading() )
	{
		if ( Ar.UE4Ver() < VER_UE4_USERWIDGET_DEFAULT_FOCUSABLE_FALSE )
		{
			bIsFocusable = bSupportsKeyboardFocus_DEPRECATED;
		}
	}

#if UE_BUILD_DEBUG
	if ( Ar.IsCooking() )
	{
		if ( HasAllFlags(RF_ArchetypeObject) && !HasAllFlags(RF_ClassDefaultObject) )
		{
			if ( bCookedWidgetTree )
			{
				UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Saving Cooked Template"), *GetClass()->GetName());
			}
			else
			{
				UE_LOG(LogUMG, Warning, TEXT("Widget Class %s - Unable To Cook Template"), *GetClass()->GetName());
			}
		}
	}
#endif
}

/////////////////////////////////////////////////////

UUserWidget* UUserWidget::NewWidgetObject(UObject* Outer, UClass* UserWidgetClass, FName WidgetName, EObjectFlags Flags)
{
	UWidgetBlueprintGeneratedClass* WBGC = Cast<UWidgetBlueprintGeneratedClass>(UserWidgetClass);
	if ( WBGC && WBGC->HasTemplate() )
	{
		if ( UUserWidget* Template = WBGC->GetTemplate() )
		{
#if UE_BUILD_DEBUG
			UE_LOG(LogUMG, Log, TEXT("Widget Class %s - Using Fast CreateWidget Path."), *UserWidgetClass->GetName());
#endif

			FObjectInstancingGraph ObjectInstancingGraph;
			UUserWidget* NewUserWidget = NewObject<UUserWidget>(Outer, UserWidgetClass, WidgetName, Flags, Template, false, &ObjectInstancingGraph);

			return NewUserWidget;
		}
		else
		{
#if !WITH_EDITOR && (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
			UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Using Slow CreateWidget path because no template found."), *UserWidgetClass->GetName());
#endif
		}
	}
	else
	{
#if !WITH_EDITOR && (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
		UE_LOG(LogUMG, Warning, TEXT("Widget Class %s - Using Slow CreateWidget path because this class could not be templated."), *UserWidgetClass->GetName());
#endif
	}

	return NewObject<UUserWidget>(Outer, UserWidgetClass, WidgetName, Flags);
}

UUserWidget* UUserWidget::CreateWidgetOfClass(UClass* UserWidgetClass, UGameInstance* InGameInstance, UWorld* InWorld, APlayerController* InOwningPlayer)
{
	SCOPE_CYCLE_COUNTER(STAT_CreateWidget);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Only do this on a non-shipping or test build.
	if ( !CreateWidgetHelpers::ValidateUserWidgetClass(UserWidgetClass) )
	{
		return nullptr;
	}
#endif

	UObject* Outer = nullptr;
	ULocalPlayer* PlayerContext = nullptr;
	UWorld* World = InWorld;

	if ( InOwningPlayer )
	{
		if ( !InOwningPlayer->IsLocalPlayerController() )
		{
			const FText FormatPattern = LOCTEXT("NotLocalPlayer", "Only Local Player Controllers can be assigned to widgets. {PlayerController} is not a Local Player Controller.");
			FFormatNamedArguments FormatPatternArgs;
			FormatPatternArgs.Add(TEXT("PlayerController"), FText::FromName(InOwningPlayer->GetFName()));
			FMessageLog("PIE").Error(FText::Format(FormatPattern, FormatPatternArgs));
			return nullptr;
		}

		if ( !InOwningPlayer->Player )
		{
			const FText FormatPattern = LOCTEXT("NoPlayer", "CreateWidget cannot be used on Player Controller with no attached player. {PlayerController} has no Player attached.");
			FFormatNamedArguments FormatPatternArgs;
			FormatPatternArgs.Add(TEXT("PlayerController"), FText::FromName(InOwningPlayer->GetFName()));
			FMessageLog("PIE").Error(FText::Format(FormatPattern, FormatPatternArgs));
			return nullptr;
		}

		// Assign the outer to the game instance if it exists, otherwise use the player controller's world
		World = InOwningPlayer->GetWorld();

		Outer = World->GetGameInstance() ? StaticCast<UObject*>(World->GetGameInstance()) : StaticCast<UObject*>(World);
		PlayerContext = CastChecked<ULocalPlayer>(InOwningPlayer->Player);
	}
	else if ( InWorld )
	{
		// Assign the outer to the game instance if it exists, otherwise use the world
		Outer = InWorld->GetGameInstance() ? StaticCast<UObject*>(InWorld->GetGameInstance()) : StaticCast<UObject*>(InWorld);
		PlayerContext = InWorld->GetFirstLocalPlayerFromController();
	}
	else if ( InGameInstance )
	{
		Outer = InGameInstance;
		PlayerContext = InGameInstance->GetFirstGamePlayer();
	}

	if ( Outer == nullptr )
	{
		FMessageLog("PIE").Error(FText::Format(LOCTEXT("OuterNull", "Unable to create the widget {0}, no outer provided."), FText::FromName(UserWidgetClass->GetFName())));
		return nullptr;
	}

	UUserWidget* NewWidget = UUserWidget::NewWidgetObject(Outer, UserWidgetClass);

	if ( PlayerContext )
	{
		NewWidget->SetPlayerContext(FLocalPlayerContext(PlayerContext, World));
	}

	NewWidget->Initialize();

	return NewWidget;
}

/////////////////////////////////////////////////////

bool CreateWidgetHelpers::ValidateUserWidgetClass(const UClass* UserWidgetClass)
{
	if (UserWidgetClass == nullptr)
	{
		FMessageLog("PIE").Error(LOCTEXT("WidgetClassNull", "CreateWidget called with a null class."));
		return false;
	}

	if (!UserWidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		const FText FormatPattern = LOCTEXT("NotUserWidget", "CreateWidget can only be used on UUserWidget children. {UserWidgetClass} is not a UUserWidget.");
		FFormatNamedArguments FormatPatternArgs;
		FormatPatternArgs.Add(TEXT("UserWidgetClass"), FText::FromName(UserWidgetClass->GetFName()));
		FMessageLog("PIE").Error(FText::Format(FormatPattern, FormatPatternArgs));
		return false;
	}

	if (UserWidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated))
	{
		const FText FormatPattern = LOCTEXT("NotValidClass", "Abstract, Deprecated or Replaced classes are not allowed to be used to construct a user widget. {UserWidgetClass} is one of these.");
		FFormatNamedArguments FormatPatternArgs;
		FormatPatternArgs.Add(TEXT("UserWidgetClass"), FText::FromName(UserWidgetClass->GetFName()));
		FMessageLog("PIE").Error(FText::Format(FormatPattern, FormatPatternArgs));
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE