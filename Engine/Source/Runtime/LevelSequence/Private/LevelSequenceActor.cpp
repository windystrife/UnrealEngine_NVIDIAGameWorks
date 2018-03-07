// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"
#include "LevelSequenceBurnIn.h"

#if WITH_EDITOR
	#include "PropertyCustomizationHelpers.h"
	#include "ActorPickerMode.h"
	#include "SceneOutlinerFilters.h"
#endif

ALevelSequenceActor::ALevelSequenceActor(const FObjectInitializer& Init)
	: Super(Init)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;

#if WITH_EDITORONLY_DATA
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics() : DecalTexture(TEXT("/Engine/EditorResources/S_LevelSequence")) {}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
			SpriteComponent->SetupAttachment(RootComponent);
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->bAbsoluteScale = true;
			SpriteComponent->bReceivesDecals = false;
			SpriteComponent->bHiddenInGame = true;
		}
	}
#endif //WITH_EDITORONLY_DATA

	BindingOverrides = Init.CreateDefaultSubobject<UMovieSceneBindingOverrides>(this, "BindingOverrides");
	BurnInOptions = Init.CreateDefaultSubobject<ULevelSequenceBurnInOptions>(this, "BurnInOptions");
	PrimaryActorTick.bCanEverTick = true;
	bAutoPlay = false;
}

void ALevelSequenceActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!SequencePlayer)
	{
		InitializePlayer();
	}
}

void ALevelSequenceActor::BeginPlay()
{
	Super::BeginPlay();
	if (!SequencePlayer)
	{
		InitializePlayer();
	}
	if (SequencePlayer)
	{
		SequencePlayer->BeginPlay();
	}
}


#if WITH_EDITOR

bool ALevelSequenceActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	ULevelSequence* LevelSequenceAsset = GetSequence(true);

	if (LevelSequenceAsset)
	{
		Objects.Add(LevelSequenceAsset);
	}

	Super::GetReferencedContentObjects(Objects);

	return true;
}

#endif //WITH_EDITOR


void ALevelSequenceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (SequencePlayer)
	{
		SequencePlayer->Update(DeltaSeconds);
	}
}

void ALevelSequenceActor::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Fix sprite component so that it's attached to the root component. In the past, the sprite component was the root component.
	UBillboardComponent* SpriteComponent = FindComponentByClass<UBillboardComponent>();
	if (SpriteComponent && SpriteComponent->GetAttachParent() != RootComponent)
	{
		SpriteComponent->SetupAttachment(RootComponent);
	}
#endif
}

#if WITH_EDITOR

void FBoundActorProxy::Initialize(TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	ReflectedProperty = InPropertyHandle;

	UObject* Object = nullptr;
	ReflectedProperty->GetValue(Object);
	BoundActor = Cast<AActor>(Object);

	ReflectedProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FBoundActorProxy::OnReflectedPropertyChanged));
}

void FBoundActorProxy::OnReflectedPropertyChanged()
{
	UObject* Object = nullptr;
	ReflectedProperty->GetValue(Object);
	BoundActor = Cast<AActor>(Object);
}

TSharedPtr<FStructOnScope> ALevelSequenceActor::GetObjectPickerProxy(TSharedPtr<IPropertyHandle> ObjectPropertyHandle)
{
	TSharedRef<FStructOnScope> Struct = MakeShared<FStructOnScope>(FBoundActorProxy::StaticStruct());
	reinterpret_cast<FBoundActorProxy*>(Struct->GetStructMemory())->Initialize(ObjectPropertyHandle);
	return Struct;
}

void ALevelSequenceActor::UpdateObjectFromProxy(FStructOnScope& Proxy, IPropertyHandle& ObjectPropertyHandle)
{
	UObject* BoundActor = reinterpret_cast<FBoundActorProxy*>(Proxy.GetStructMemory())->BoundActor;
	ObjectPropertyHandle.SetValue(BoundActor);
}

#endif


void ALevelSequenceActor::OnSequenceLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result, bool bInitializePlayer)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		if (bInitializePlayer)
		{
			InitializePlayer();
		}
	}
}

ULevelSequence* ALevelSequenceActor::GetSequence(bool bLoad, bool bInitializePlayer) const
{
	if (LevelSequence.IsValid())
	{
		ULevelSequence* Sequence = Cast<ULevelSequence>(LevelSequence.ResolveObject());
		if (Sequence)
		{
			return Sequence;
		}

		if (bLoad)
		{
			if (IsAsyncLoading())
			{
				LoadPackageAsync(LevelSequence.GetLongPackageName(), FLoadPackageAsyncDelegate::CreateUObject(this, &ALevelSequenceActor::OnSequenceLoaded, bInitializePlayer));
				return nullptr;
			}
			else
			{
				return Cast<ULevelSequence>(LevelSequence.TryLoad());
			}
		}
	}

	return nullptr;
}


void ALevelSequenceActor::SetSequence(ULevelSequence* InSequence)
{
	if (!SequencePlayer || !SequencePlayer->IsPlaying())
	{
		LevelSequence = InSequence;
		InitializePlayer();
	}
}

void ALevelSequenceActor::SetEventReceivers(TArray<AActor*> InAdditionalReceivers)
{
	AdditionalEventReceivers = InAdditionalReceivers;
	if (SequencePlayer)
	{
		SequencePlayer->SetEventReceivers(TArray<UObject*>(AdditionalEventReceivers));
	}
}

void ALevelSequenceActor::InitializePlayer()
{
	ULevelSequence* LevelSequenceAsset = GetSequence(true, true);

	if (GetWorld()->IsGameWorld() && (LevelSequenceAsset != nullptr))
	{
		PlaybackSettings.BindingOverrides = BindingOverrides;

		SequencePlayer = NewObject<ULevelSequencePlayer>(this, "AnimationPlayer");
		SequencePlayer->Initialize(LevelSequenceAsset, GetWorld(), PlaybackSettings);
		SequencePlayer->SetEventReceivers(TArray<UObject*>(AdditionalEventReceivers));

		RefreshBurnIn();

		if (bAutoPlay)
		{
			SequencePlayer->Play();
		}
	}
}

void ALevelSequenceActor::RefreshBurnIn()
{
 	if (!SequencePlayer)
	{
		return;
	}

	if (BurnInInstance)
	{
		BurnInInstance->RemoveFromViewport();
		BurnInInstance = nullptr;
	}
	
	if (BurnInOptions && BurnInOptions->bUseBurnIn)
	{
		// Create the burn-in if necessary
		UClass* Class = BurnInOptions->BurnInClass.TryLoadClass<ULevelSequenceBurnIn>();
		if (Class)
		{
			BurnInInstance = CreateWidget<ULevelSequenceBurnIn>(GetWorld(), Class);
			if (BurnInInstance)
			{
				// Ensure we have a valid settings object if possible
				BurnInOptions->ResetSettings();

				BurnInInstance->SetSettings(BurnInOptions->Settings);
				BurnInInstance->TakeSnapshotsFrom(*this);
				BurnInInstance->AddToViewport();
			}
		}
	}
}

ULevelSequenceBurnInOptions::ULevelSequenceBurnInOptions(const FObjectInitializer& Init)
	: Super(Init)
	, bUseBurnIn(false)
	, BurnInClass(TEXT("/Engine/Sequencer/DefaultBurnIn.DefaultBurnIn_C"))
	, Settings(nullptr)
{
}

void ULevelSequenceBurnInOptions::ResetSettings()
{
	UClass* Class = BurnInClass.TryLoadClass<ULevelSequenceBurnIn>();
	if (Class)
	{
		TSubclassOf<ULevelSequenceBurnInInitSettings> SettingsClass = Cast<ULevelSequenceBurnIn>(Class->GetDefaultObject())->GetSettingsClass();
		if (SettingsClass)
		{
			if (!Settings || !Settings->IsA(SettingsClass))
			{
				if (Settings)
				{
					Settings->Rename(*MakeUniqueObjectName(this, ULevelSequenceBurnInInitSettings::StaticClass(), "Settings_EXPIRED").ToString());
				}
				
				Settings = NewObject<ULevelSequenceBurnInInitSettings>(this, SettingsClass, "Settings");
				Settings->SetFlags(GetMaskedFlags(RF_PropagateToSubObjects));
			}
		}
		else
		{
			Settings = nullptr;
		}
	}
	else
	{
		Settings = nullptr;
	}
}

#if WITH_EDITOR

void ULevelSequenceBurnInOptions::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelSequenceBurnInOptions, bUseBurnIn) || PropertyName == GET_MEMBER_NAME_CHECKED(ULevelSequenceBurnInOptions, BurnInClass))
	{
		ResetSettings();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR
