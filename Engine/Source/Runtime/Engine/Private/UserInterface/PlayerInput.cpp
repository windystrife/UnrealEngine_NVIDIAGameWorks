// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlayerInput.cpp: Unreal input system.
=============================================================================*/

#include "GameFramework/PlayerInput.h"
#include "Misc/CommandLine.h"
#include "Components/InputComponent.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/PlayerController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputSettings.h"

bool bExecutingBindCommand = false;

/** for debug rendering */
static float DebugUnSmoothedMouseX = 0.f;
static float DebugSmoothedMouseX = 0.f;
static float DebugUnSmoothedMouseY = 0.f;
static float DebugSmoothedMouseY = 0.f;
static float DebugSmoothedMouseSensitivity = 8.f;

const TArray<FInputActionKeyMapping> UPlayerInput::NoKeyMappings;
const TArray<FInputAxisKeyMapping> UPlayerInput::NoAxisMappings;
TArray<FInputActionKeyMapping> UPlayerInput::EngineDefinedActionMappings;
TArray<FInputAxisKeyMapping> UPlayerInput::EngineDefinedAxisMappings;

/** Runtime struct that gathers up the different kinds of delegates that might be issued */
struct FDelegateDispatchDetails
{
	uint32 EventIndex;
	uint32 FoundIndex;

	FInputActionUnifiedDelegate ActionDelegate;
	const FInputActionBinding* SourceAction;
	FInputChord Chord;
	TEnumAsByte<EInputEvent> KeyEvent;

	FInputTouchUnifiedDelegate TouchDelegate;
	FVector TouchLocation;
	ETouchIndex::Type FingerIndex;

	FInputGestureUnifiedDelegate GestureDelegate;
	float GestureValue;

	FDelegateDispatchDetails(const uint32 InEventIndex, const uint32 InFoundIndex, const FInputChord& InChord, const FInputActionUnifiedDelegate& InDelegate, const EInputEvent InKeyEvent, const FInputActionBinding* InSourceAction = NULL)
		: EventIndex(InEventIndex)
		, FoundIndex(InFoundIndex)
		, ActionDelegate(InDelegate)
		, SourceAction(InSourceAction)
		, Chord(InChord)
		, KeyEvent(InKeyEvent)
	{}

	FDelegateDispatchDetails(const uint32 InEventIndex, const uint32 InFoundIndex, const FInputTouchUnifiedDelegate& InDelegate, const FVector InLocation, const ETouchIndex::Type InFingerIndex)
		: EventIndex(InEventIndex)
		, FoundIndex(InFoundIndex)
		, TouchDelegate(InDelegate)
		, TouchLocation(InLocation)
		, FingerIndex(InFingerIndex)
	{}

	FDelegateDispatchDetails(const uint32 InEventIndex, const uint32 InFoundIndex, const FInputGestureUnifiedDelegate& InDelegate, const float InValue)
		: EventIndex(InEventIndex)
		, FoundIndex(InFoundIndex)
		, GestureDelegate(InDelegate)
		, GestureValue(InValue)
	{}
};

UPlayerInput::UPlayerInput()
	: Super()
{
	SetFlags(RF_Transactional);
	MouseSamplingTotal = +0.0083f;
	MouseSamples = 1;
}

void UPlayerInput::PostInitProperties()
{
	Super::PostInitProperties();
	ForceRebuildingKeyMaps(true);
}

void UPlayerInput::FlushPressedKeys()
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
	if ( LocalPlayer != NULL )
	{
		TArray<FKey> PressedKeys;

		for (TMap<FKey,FKeyState>::TIterator It(KeyStateMap); It; ++It)
		{
			const FKeyState& KeyState = It.Value();
			if (KeyState.bDown)
			{
				PressedKeys.Add(It.Key());
			}
		}

		// we may have gotten here as a result of executing an input bind.  in order to ensure that the simulated IE_Released events
		// we're about to fire are actually propagated to the game, we need to clear the bExecutingBindCommand flag
		if ( PressedKeys.Num() > 0 )
		{
			bExecutingBindCommand = false;

			for ( int32 KeyIndex = 0; KeyIndex < PressedKeys.Num(); KeyIndex++ )
			{
				FKey& Key = PressedKeys[KeyIndex];
				InputKey(Key, IE_Released, 0, Key.IsGamepadKey());
			}
		}
	}

	UWorld* World = GetWorld();
	check(World);
	float TimeSeconds = World->GetRealTimeSeconds();
	for (TMap<FKey,FKeyState>::TIterator It(KeyStateMap); It; ++It)
	{
		FKeyState& KeyState = It.Value();
		KeyState.RawValue = FVector(0.f, 0.f, 0.f);
		KeyState.bDown = false;
		KeyState.bDownPrevious = false;
		KeyState.LastUpDownTransitionTime = TimeSeconds;
	}
}

void UPlayerInput::FlushPressedActionBindingKeys(FName ActionName)
{
	//need an action name and a local player to move forward
	APlayerController* PlayerController = (ActionName != NAME_None) ? GetOuterAPlayerController() : nullptr;
	ULocalPlayer* LocalPlayer = PlayerController ? Cast<ULocalPlayer>(PlayerController->Player) : nullptr;
	if (!LocalPlayer)
	{
		return;
	}

	//there can't be more than 32 keys...
	TArray<FKey, TInlineAllocator<32>> AssociatedPressedKeys;

	//grab the action key details
	if (const FActionKeyDetails* KeyDetails = ActionKeyMap.Find(ActionName))
	{
		//go through the details
		for (const FInputActionKeyMapping& KeyMapping : KeyDetails->Actions)
		{
			//grab out key state
			if (const FKeyState* KeyState = KeyStateMap.Find(KeyMapping.Key))
			{
				//if the key is down, add it to the associated keys array
				if (KeyState->bDown)
				{
					AssociatedPressedKeys.AddUnique(KeyMapping.Key);
				}
			}
		}
	}

	//if there are no keys, nothing to do here
	if (AssociatedPressedKeys.Num() > 0)
	{
		// we may have gotten here as a result of executing an input bind.  in order to ensure that the simulated IE_Released events
		// we're about to fire are actually propagated to the game, we need to clear the bExecutingBindCommand flag
		bExecutingBindCommand = false;

		//go through all the keys, releasing them
		for (const FKey& Key : AssociatedPressedKeys)
		{
			InputKey(Key, IE_Released, 0, Key.IsGamepadKey());
		}

		UWorld* World = GetWorld();
		check(World);
		float TimeSeconds = World->GetRealTimeSeconds();

		//go through the details
		for (const FKey& Key : AssociatedPressedKeys)
		{
			//grab out key state
			if (FKeyState* KeyState = KeyStateMap.Find(Key))
			{
				KeyState->RawValue = FVector(0.f, 0.f, 0.f);
				KeyState->bDown = false;
				KeyState->bDownPrevious = false;
				KeyState->LastUpDownTransitionTime = TimeSeconds;
			}
		}
	}
}

bool UPlayerInput::InputKey(FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	// first event associated with this key, add it to the map
	FKeyState& KeyState = KeyStateMap.FindOrAdd(Key);
	UWorld* World = GetWorld();
	check(World);

	switch(Event)
	{
	case IE_Pressed:
	case IE_Repeat:
		KeyState.RawValueAccumulator.X = AmountDepressed;
		KeyState.EventAccumulator[Event].Add(++EventCount);
		if (KeyState.bDownPrevious == false)
		{
			// check for doubleclick
			// note, a tripleclick will currently count as a 2nd double click.
			const float WorldRealTimeSeconds = World->GetRealTimeSeconds();
			if ((WorldRealTimeSeconds - KeyState.LastUpDownTransitionTime) < GetDefault<UInputSettings>()->DoubleClickTime)
			{
				KeyState.EventAccumulator[IE_DoubleClick].Add(++EventCount);
			}

			// just went down
			KeyState.LastUpDownTransitionTime = WorldRealTimeSeconds;
		}
		break;
	case IE_Released:
		KeyState.RawValueAccumulator.X = 0.f;
		KeyState.EventAccumulator[IE_Released].Add(++EventCount);
		break;
	case IE_DoubleClick:
		KeyState.RawValueAccumulator.X = AmountDepressed;
		KeyState.EventAccumulator[IE_Pressed].Add(++EventCount);
		KeyState.EventAccumulator[IE_DoubleClick].Add(++EventCount);
		break;
	}
	KeyState.SampleCountAccumulator++;

#if !UE_BUILD_SHIPPING
	CurrentEvent		= Event;

	const FString Command = GetBind(Key);
	if(Command.Len())
	{
		return ExecInputCommands(World, *Command,*GLog);
	}
#endif

	if( Event == IE_Pressed )
	{
		return IsKeyHandledByAction( Key );
	}

	return true;
}

bool UPlayerInput::InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad )
{
	ensure((Key != EKeys::MouseX && Key != EKeys::MouseY) || NumSamples > 0);

	// first event associated with this key, add it to the map
	FKeyState& KeyState = KeyStateMap.FindOrAdd(Key);

	// look for event edges
	if (KeyState.Value.X == 0.f && Delta != 0.f)
	{
		KeyState.EventAccumulator[IE_Pressed].Add(++EventCount);
	}
	else if (KeyState.Value.X != 0.f && Delta == 0.f)
	{
		KeyState.EventAccumulator[IE_Released].Add(++EventCount);
	}
	else
	{
		KeyState.EventAccumulator[IE_Repeat].Add(++EventCount);
	}

	// accumulate deltas until processed next
	KeyState.SampleCountAccumulator += NumSamples;
	KeyState.RawValueAccumulator.X += Delta;

#if !UE_BUILD_SHIPPING
	CurrentEvent		= IE_Axis;

	const FString Command = GetBind(Key);
	if(Command.Len())
	{
		UWorld* World = GetWorld();
		check(World);
		ExecInputCommands( World, *Command,*GLog);
		return true;
	}
#endif

	return false;
}

bool UPlayerInput::InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) 
{
	// get the current state of each finger
	checkf(TouchpadIndex == 0, TEXT("We currently assume one touchpad in UPlayerInput::InputTouch. If this triggers, add support for multiple pads"));

	// if the handle is out of bounds, we can't handle it
	if (Handle >= EKeys::NUM_TOUCH_KEYS)
	{
		return false;
	}

	// update position
	Touches[Handle].X = TouchLocation.X;
	Touches[Handle].Y = TouchLocation.Y;

	// update touched/untouched flag
	// > 0 means that it's currently held down (anything besides an end message is held down)
	Touches[Handle].Z = (Type == ETouchType::Ended) ? 0.0f : 1.0f;

	// hook up KeyState for InputComponent
	FKeyState& KeyState = KeyStateMap.FindOrAdd(EKeys::TouchKeys[Handle]);
	switch(Type)
	{
	case ETouchType::Began:
		KeyState.EventAccumulator[IE_Pressed].Add(++EventCount);
		// store current touch location paired with event id
		TouchEventLocations.Add(EventCount, Touches[Handle]);
		if (KeyState.bDownPrevious == false)
		{
			UWorld* World = GetWorld();
			check(World);

			// check for doubleclick
			// note, a tripleclick will currently count as a 2nd double click.
			const float WorldRealTimeSeconds = World->GetRealTimeSeconds();
			if ((WorldRealTimeSeconds - KeyState.LastUpDownTransitionTime) < GetDefault<UInputSettings>()->DoubleClickTime)
			{
				KeyState.EventAccumulator[IE_DoubleClick].Add(++EventCount);
				// store current touch location paired with event id
				TouchEventLocations.Add(EventCount, Touches[Handle]);
			}

			// just went down
			KeyState.LastUpDownTransitionTime = WorldRealTimeSeconds;
		}
		break;

	case ETouchType::Ended:
		KeyState.EventAccumulator[IE_Released].Add(++EventCount);
		// store current touch location paired with event id
		TouchEventLocations.Add(EventCount, Touches[Handle]);
		break;

	default:
		KeyState.EventAccumulator[IE_Repeat].Add(++EventCount);
		// store current touch location paired with event id
		TouchEventLocations.Add(EventCount, Touches[Handle]);
		break;
	}

	// accumulate deltas until processed next
	KeyState.SampleCountAccumulator++;
	KeyState.RawValueAccumulator = KeyState.Value = KeyState.RawValue = FVector(TouchLocation.X, TouchLocation.Y, 0);

	// for now, if we have a player, assume it sucks up all touch input
	// @todo: Here we could look for game touch zones, etc, and only return true if some non-Slate in-game zone used the touch
	return true;
}

bool UPlayerInput::InputMotion(const FVector& InTilt, const FVector& InRotationRate, const FVector& InGravity, const FVector& InAcceleration) 
{
	FKeyState& KeyStateTilt = KeyStateMap.FindOrAdd(EKeys::Tilt);
	KeyStateTilt.RawValue = KeyStateTilt.Value = InTilt;
	KeyStateTilt.EventAccumulator[IE_Repeat].Add(++EventCount);

	FKeyState& KeyStateRotationRate = KeyStateMap.FindOrAdd(EKeys::RotationRate);
	KeyStateRotationRate.RawValue = KeyStateRotationRate.Value = InRotationRate;
	KeyStateRotationRate.EventAccumulator[IE_Repeat].Add(++EventCount);

	FKeyState& KeyStateGravity = KeyStateMap.FindOrAdd(EKeys::Gravity);
	KeyStateGravity.RawValue = KeyStateGravity.Value = InGravity;
	KeyStateGravity.EventAccumulator[IE_Repeat].Add(++EventCount);

	FKeyState& KeyStateAcceleration = KeyStateMap.FindOrAdd(EKeys::Acceleration);
	KeyStateAcceleration.RawValue = KeyStateAcceleration.Value = InAcceleration;
	KeyStateAcceleration.EventAccumulator[IE_Repeat].Add(++EventCount);

	// for now, if we have a player, assume it sucks up all motion input
	// @todo: Here we could look for game kismet, etc, and only return true if some non-Slate in-game handler used the touch
	return true;
}

bool UPlayerInput::InputGesture(const FKey Gesture, const EInputEvent Event, const float Value)
{
	FKeyState& KeyState = KeyStateMap.FindOrAdd(Gesture);

	KeyState.Value.X = KeyState.RawValue.X = KeyState.RawValueAccumulator.X = Value;
	KeyState.EventAccumulator[Event].Add(++EventCount);

	return true;
}

void UPlayerInput::UpdatePinchStartDistance()
{
	GestureRecognizer.SetAnchorDistanceSquared(FVector2D(Touches[ETouchIndex::Touch1]), FVector2D(Touches[ETouchIndex::Touch2]));
}

bool UPlayerInput::GetAxisProperties(const FKey AxisKey, FInputAxisProperties& OutAxisProperties)
{
	ConditionalInitAxisProperties();

	const FInputAxisProperties* const AxisProps = AxisProperties.Find(AxisKey);
	if (AxisProps)
	{
		OutAxisProperties = *AxisProps;
		return true;
	}

	return false;
}

void UPlayerInput::SetAxisProperties(const FKey AxisKey, const FInputAxisProperties& InAxisProperties)
{
	for (FInputAxisConfigEntry& AxisConfigEntry : AxisConfig)
	{
		if (AxisConfigEntry.AxisKeyName == AxisKey)
		{
			AxisConfigEntry.AxisProperties = InAxisProperties;
		}
	}

	AxisProperties.Reset();
}

float UPlayerInput::GetMouseSensitivity()
{
	FInputAxisProperties MouseAxisProps;
	if (GetAxisProperties(EKeys::MouseX, MouseAxisProps) || GetAxisProperties(EKeys::MouseY, MouseAxisProps))
	{
		return MouseAxisProps.Sensitivity;
	}

	return 1.f;
}

void UPlayerInput::SetMouseSensitivity(const float Sensitivity)
{
	FInputAxisProperties MouseAxisProps;
	if (GetAxisProperties(EKeys::MouseX, MouseAxisProps))
	{
		MouseAxisProps.Sensitivity = Sensitivity;
		SetAxisProperties(EKeys::MouseX, MouseAxisProps);
	}
	if (GetAxisProperties(EKeys::MouseY, MouseAxisProps))
	{
		MouseAxisProps.Sensitivity = Sensitivity;
		SetAxisProperties(EKeys::MouseY, MouseAxisProps);
	}
}

bool UPlayerInput::GetInvertAxis(const FName AxisName)
{
	ConditionalBuildKeyMappings();

	bool bAxisInverted = false;

	FAxisKeyDetails* KeyDetails = AxisKeyMap.Find(AxisName);
	if (KeyDetails)
	{
		bAxisInverted = KeyDetails->bInverted;
	}
	return bAxisInverted;
}

void UPlayerInput::InvertAxis(const FName AxisName)
{
	bool bInverted = true;

	if (AxisKeyMap.Num() > 0)
	{
		FAxisKeyDetails* KeyDetails = AxisKeyMap.Find(AxisName);
		if (KeyDetails)
		{
			bInverted = (KeyDetails->bInverted = !KeyDetails->bInverted);
		}
		if (bInverted)
		{
			InvertedAxis.Add(AxisName);
		}
		else
		{
			for (int32 InvertIndex = InvertedAxis.Num() - 1; InvertIndex >= 0; --InvertIndex)
			{
				InvertedAxis.RemoveAtSwap(InvertIndex);
			}
		}
	}
	else
	{
		bool bFound = false;
		for (int32 InvertIndex = InvertedAxis.Num() - 1; InvertIndex >= 0; --InvertIndex)
		{
			if (InvertedAxis[InvertIndex] == AxisName)
			{
				bFound = true;
				InvertedAxis.RemoveAtSwap(InvertIndex);
			}
		}
		if (!bFound)
		{
			InvertedAxis.Add(AxisName);
		}
	}
	SaveConfig();
}

bool UPlayerInput::GetInvertAxisKey(const FKey AxisKey)
{
	bool bAxisInverted = false;
	FInputAxisProperties AxisKeyProperties;
	if (GetAxisProperties(AxisKey, AxisKeyProperties))
	{
		bAxisInverted = AxisKeyProperties.bInvert;
	}

	return bAxisInverted;
}

void UPlayerInput::InvertAxisKey(const FKey AxisKey)
{
	ConditionalInitAxisProperties();

	FInputAxisProperties AxisKeyProperties;
	if (GetAxisProperties(AxisKey, AxisKeyProperties))
	{
		AxisKeyProperties.bInvert = !AxisKeyProperties.bInvert;
		SetAxisProperties(AxisKey, AxisKeyProperties);
	}
}

struct FAxisDelegate
{
	UObject* Obj;
	UFunction* Func;

	bool operator==(FAxisDelegate const& Other) const
	{
		return (Obj == Other.Obj) && (Func == Other.Func);
	}

	friend inline uint32 GetTypeHash(FAxisDelegate const& D)
	{
		return (PTRINT)D.Obj + (PTRINT)D.Func;
	}
};

void UPlayerInput::AddActionMapping(const FInputActionKeyMapping& KeyMapping)
{
	ActionMappings.AddUnique(KeyMapping);
	ActionKeyMap.Reset();
	bKeyMapsBuilt = false;
}

void UPlayerInput::RemoveActionMapping(const FInputActionKeyMapping& KeyMapping)
{
	for (int32 ActionIndex = ActionMappings.Num() - 1; ActionIndex >= 0; --ActionIndex)
	{
		if (ActionMappings[ActionIndex] == KeyMapping)
		{
			ActionMappings.RemoveAtSwap(ActionIndex);
			ActionKeyMap.Reset();
			bKeyMapsBuilt = false;
			// we don't break because the mapping may have been in the array twice
		}
	}
}

void UPlayerInput::AddAxisMapping(const FInputAxisKeyMapping& KeyMapping)
{
	AxisMappings.AddUnique(KeyMapping);
	AxisKeyMap.Reset();
	bKeyMapsBuilt = false;
}

void UPlayerInput::RemoveAxisMapping(const FInputAxisKeyMapping& InKeyMapping)
{
	for (int32 AxisIndex = AxisMappings.Num() - 1; AxisIndex >= 0; --AxisIndex)
	{
		const FInputAxisKeyMapping& KeyMapping = AxisMappings[AxisIndex];
		if (KeyMapping.AxisName == InKeyMapping.AxisName
			&& KeyMapping.Key == InKeyMapping.Key)
		{
			AxisMappings.RemoveAtSwap(AxisIndex);
			AxisKeyMap.Reset();
			bKeyMapsBuilt = false;
			// we don't break because the mapping may have been in the array twice
		}
	}
}

void UPlayerInput::AddEngineDefinedActionMapping(const FInputActionKeyMapping& ActionMapping)
{
	EngineDefinedActionMappings.AddUnique(ActionMapping);
	for (TObjectIterator<UPlayerInput> It; It; ++It)
	{
		It->ActionKeyMap.Reset();
		It->bKeyMapsBuilt = false;
	}
}

void UPlayerInput::AddEngineDefinedAxisMapping(const FInputAxisKeyMapping& AxisMapping)
{
	EngineDefinedAxisMappings.AddUnique(AxisMapping);
	for (TObjectIterator<UPlayerInput> It; It; ++It)
	{
		It->AxisKeyMap.Reset();
		It->bKeyMapsBuilt = false;
	}
}

void UPlayerInput::ForceRebuildingKeyMaps(const bool bRestoreDefaults)
{
	if (bRestoreDefaults)
	{
		AxisConfig = GetDefault<UInputSettings>()->AxisConfig;
		AxisMappings = GetDefault<UInputSettings>()->AxisMappings;
		ActionMappings = GetDefault<UInputSettings>()->ActionMappings;
	}

	ActionKeyMap.Reset();
	AxisKeyMap.Reset();
	AxisProperties.Reset();
	bKeyMapsBuilt = false;
}

void UPlayerInput::ConditionalBuildKeyMappings_Internal()
{
	if (ActionKeyMap.Num() == 0)
	{
		struct
		{
			void Build(const TArray<FInputActionKeyMapping>& Mappings, TMap<FName, FActionKeyDetails>& KeyMap)
			{
				for (const FInputActionKeyMapping& ActionMapping : Mappings)
				{
					TArray<FInputActionKeyMapping>& KeyMappings = KeyMap.FindOrAdd(ActionMapping.ActionName).Actions;
					KeyMappings.AddUnique(ActionMapping);
				}
			}
		} ActionMappingsUtility;

		ActionMappingsUtility.Build(ActionMappings, ActionKeyMap);
		ActionMappingsUtility.Build(EngineDefinedActionMappings, ActionKeyMap);
	}

	if (AxisKeyMap.Num() == 0)
	{
		struct
		{
			void Build(const TArray<FInputAxisKeyMapping>& Mappings, TMap<FName, FAxisKeyDetails>& AxisMap)
			{
				for (const FInputAxisKeyMapping& AxisMapping : Mappings)
				{
					bool bAdd = true;
					FAxisKeyDetails& KeyDetails = AxisMap.FindOrAdd(AxisMapping.AxisName);
					for (const FInputAxisKeyMapping& KeyMapping : KeyDetails.KeyMappings)
					{
						if (KeyMapping.Key == AxisMapping.Key)
						{
							UE_LOG(LogInput, Error, TEXT("Duplicate mapping of key %s for axis %s"), *KeyMapping.Key.ToString(), *AxisMapping.AxisName.ToString());
							bAdd = false;
							break;
						}
					}
					if (bAdd)
					{
						KeyDetails.KeyMappings.Add(AxisMapping);
					}
				}
			}
		} AxisMappingsUtility;

		AxisMappingsUtility.Build(AxisMappings, AxisKeyMap);
		AxisMappingsUtility.Build(EngineDefinedAxisMappings, AxisKeyMap);

		// Apply the axis inversions
		for (int32 InvertedAxisIndex = 0; InvertedAxisIndex < InvertedAxis.Num(); ++InvertedAxisIndex)
		{
			FAxisKeyDetails* KeyDetails = AxisKeyMap.Find(InvertedAxis[InvertedAxisIndex]);
			if (KeyDetails)
			{
				KeyDetails->bInverted = true;
			}
		}
	}

	bKeyMapsBuilt = true;
}

void UPlayerInput::GetChordsForKeyMapping(const FInputActionKeyMapping& KeyMapping, const FInputActionBinding& ActionBinding, const bool bGamePaused, TArray<FDelegateDispatchDetails>& FoundChords, TArray<FKey>& KeysToConsume)
{
	bool bConsumeInput = false;

	checkSlow(!EventIndices.Num());

	// test modifier conditions and ignore the event if they failed
	if (	(KeyMapping.bAlt == false || IsAltPressed())
		&&	(KeyMapping.bCtrl == false || IsCtrlPressed())
		&&	(KeyMapping.bShift == false || IsShiftPressed())
		&&	(KeyMapping.bCmd == false || IsCmdPressed())
		&& 	KeyEventOccurred(KeyMapping.Key, ActionBinding.KeyEvent, EventIndices))
	{
		bool bAddDelegate = true;

		// look through the found chords and determine if this is masked (or masks) anything in the array
		const FInputChord Chord(KeyMapping.Key, KeyMapping.bShift, KeyMapping.bCtrl, KeyMapping.bAlt, KeyMapping.bCmd);
		for (int32 ChordIndex = FoundChords.Num() - 1; ChordIndex >= 0; --ChordIndex)
		{
			FInputChord::ERelationshipType ChordRelationship = Chord.GetRelationship(FoundChords[ChordIndex].Chord);

			if (ChordRelationship == FInputChord::Masks)
			{
				// If we mask the found one, then remove it from the list
				FoundChords.RemoveAtSwap(ChordIndex);
			}
			else if (ChordRelationship == FInputChord::Masked)
			{
				bAddDelegate = false;
				break;
			}
		}

		if (bAddDelegate)
		{
			check(EventIndices.Num() > 0);
			FDelegateDispatchDetails FoundChord(  
										EventIndices[0]
										, FoundChords.Num()
										, Chord
										, ((!bGamePaused || ActionBinding.bExecuteWhenPaused) ? ActionBinding.ActionDelegate : FInputActionUnifiedDelegate())
										, ActionBinding.KeyEvent
										, &ActionBinding);
			FoundChords.Add(FoundChord);

			for (int32 EventsIndex = 1; EventsIndex < EventIndices.Num(); ++EventsIndex)
			{
				FoundChord.EventIndex = EventIndices[EventsIndex];
				FoundChords.Add(FoundChord);
			}
			bConsumeInput = true;
		}
	}
	if (ActionBinding.bConsumeInput && (bConsumeInput || !(KeyMapping.bAlt || KeyMapping.bCtrl || KeyMapping.bShift || KeyMapping.bCmd || ActionBinding.KeyEvent == EInputEvent::IE_DoubleClick)))
	{
		KeysToConsume.AddUnique(KeyMapping.Key);
	}

	EventIndices.Reset();
}

void UPlayerInput::GetChordsForAction(const FInputActionBinding& ActionBinding, const bool bGamePaused, TArray<FDelegateDispatchDetails>& FoundChords, TArray<FKey>& KeysToConsume)
{
	ConditionalBuildKeyMappings();

	FActionKeyDetails* KeyDetails = ActionKeyMap.Find(ActionBinding.ActionName);
	if (KeyDetails)
	{
		for (const FInputActionKeyMapping& KeyMapping : KeyDetails->Actions)
		{
			if (KeyMapping.Key == EKeys::AnyKey)
			{
				for (auto KeyStateIt(KeyStateMap.CreateConstIterator()); KeyStateIt; ++KeyStateIt)
				{
					if (!KeyStateIt.Key().IsFloatAxis() && !KeyStateIt.Key().IsVectorAxis() && !KeyStateIt.Value().bConsumed)
					{
						FInputActionKeyMapping SubKeyMapping(KeyMapping);
						SubKeyMapping.Key = KeyStateIt.Key();
						GetChordsForKeyMapping(SubKeyMapping, ActionBinding, bGamePaused, FoundChords, KeysToConsume);
					}
				}
			}
			else if ( !IsKeyConsumed(KeyMapping.Key) )
			{
				GetChordsForKeyMapping(KeyMapping, ActionBinding, bGamePaused, FoundChords, KeysToConsume);
			}
		}
	}
}

void UPlayerInput::GetChordForKey(const FInputKeyBinding& KeyBinding, const bool bGamePaused, TArray<FDelegateDispatchDetails>& FoundChords, TArray<FKey>& KeysToConsume)
{
	bool bConsumeInput = false;

	if (KeyBinding.Chord.Key == EKeys::AnyKey)
	{
		for (auto KeyStateIt(KeyStateMap.CreateConstIterator()); KeyStateIt; ++KeyStateIt)
		{
			if (!KeyStateIt.Key().IsFloatAxis() && !KeyStateIt.Key().IsVectorAxis() && !KeyStateIt.Value().bConsumed)
			{
				FInputKeyBinding SubKeyBinding(KeyBinding);
				SubKeyBinding.Chord.Key = KeyStateIt.Key();
				GetChordForKey(SubKeyBinding, bGamePaused, FoundChords, KeysToConsume);
			}
		}
	}
	else if ( !IsKeyConsumed(KeyBinding.Chord.Key) )
	{
		checkSlow(!EventIndices.Num());

		// test modifier conditions and ignore the event if they failed
		if (	(KeyBinding.Chord.bAlt == false || IsAltPressed())
			&&	(KeyBinding.Chord.bCtrl == false || IsCtrlPressed())
			&&	(KeyBinding.Chord.bShift == false || IsShiftPressed())
			&&	(KeyBinding.Chord.bCmd == false || IsCmdPressed())
			&& 	KeyEventOccurred(KeyBinding.Chord.Key, KeyBinding.KeyEvent, EventIndices))
		{
			bool bAddDelegate = true;

			// look through the found chords and determine if this is masked (or masks) anything in the array
			for (int32 ChordIndex = FoundChords.Num() - 1; ChordIndex >= 0; --ChordIndex)
			{
				FInputChord::ERelationshipType ChordRelationship = KeyBinding.Chord.GetRelationship(FoundChords[ChordIndex].Chord);

				if (ChordRelationship == FInputChord::Masks)
				{
					// If we mask the found one, then remove it from the list
					FoundChords.RemoveAtSwap(ChordIndex);
				}
				else if (ChordRelationship == FInputChord::Masked)
				{
					bAddDelegate = false;
					break;
				}
			}

			if (bAddDelegate)
			{
				check(EventIndices.Num() > 0);
				FDelegateDispatchDetails FoundChord(
											EventIndices[0]
											, FoundChords.Num()
											, KeyBinding.Chord
											, ((!bGamePaused || KeyBinding.bExecuteWhenPaused) ? KeyBinding.KeyDelegate : FInputActionUnifiedDelegate())
											, KeyBinding.KeyEvent);
				FoundChords.Add(FoundChord);
				for (int32 EventsIndex = 1; EventsIndex < EventIndices.Num(); ++EventsIndex)
				{
					FoundChord.EventIndex = EventIndices[EventsIndex];
					FoundChords.Add(FoundChord);
				}
				bConsumeInput = true;
			}

			EventIndices.Reset();
		}
	}
	if (KeyBinding.bConsumeInput && (bConsumeInput || !(KeyBinding.Chord.bAlt || KeyBinding.Chord.bCtrl || KeyBinding.Chord.bShift || KeyBinding.Chord.bCmd || KeyBinding.KeyEvent == EInputEvent::IE_DoubleClick)))
	{
		KeysToConsume.AddUnique(KeyBinding.Chord.Key);
	}
}

float UPlayerInput::DetermineAxisValue(const FInputAxisBinding& AxisBinding, const bool bGamePaused, TArray<FKey>& KeysToConsume)
{
	ConditionalBuildKeyMappings();

	float AxisValue = 0.f;

	FAxisKeyDetails* KeyDetails = AxisKeyMap.Find(AxisBinding.AxisName);
	if (KeyDetails)
	{
		for (int32 AxisIndex = 0; AxisIndex < KeyDetails->KeyMappings.Num(); ++AxisIndex)
		{
			const FInputAxisKeyMapping& KeyMapping = (KeyDetails->KeyMappings)[AxisIndex];
			if ( !IsKeyConsumed(KeyMapping.Key) )
			{
				if (!bGamePaused || AxisBinding.bExecuteWhenPaused)
				{
					AxisValue += GetKeyValue(KeyMapping.Key) * KeyMapping.Scale;
				}

				if (AxisBinding.bConsumeInput)
				{
					KeysToConsume.AddUnique(KeyMapping.Key);
				}
			}
		}

		if (KeyDetails->bInverted)
		{
			AxisValue *= -1.f;
		}
	}

	return AxisValue;
}

void UPlayerInput::ProcessNonAxesKeys(FKey InKey, FKeyState* KeyState)
{
	KeyState->Value.X = MassageAxisInput(InKey, KeyState->RawValue.X);

	int32 const PressDelta = KeyState->EventCounts[IE_Pressed].Num() - KeyState->EventCounts[IE_Released].Num();
	if (PressDelta < 0)
	{
		// If this is negative, we definitely released
		KeyState->bDown = false;
	}
	else if (PressDelta > 0)
	{
		// If this is positive, we definitely pressed
		KeyState->bDown = true;
	}
	else
	{
		// If this is 0, we maintain state
		KeyState->bDown = KeyState->bDownPrevious;
	}
}

void UPlayerInput::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	// @todo, if input is coming in asynchronously, we should buffer anyhting that comes in during playerinput() and not include it
	// in this processing batch

	APlayerController* PlayerController = GetOuterAPlayerController();

	PlayerController->PreProcessInput(DeltaTime, bGamePaused);

	// copy data from accumulators to the real values
	for (TMap<FKey,FKeyState>::TIterator It(KeyStateMap); It; ++It)
	{
		FKeyState* const KeyState = &It.Value();
		FKey const Key = It.Key();

		for (uint8 EventIndex = 0; EventIndex < IE_MAX; ++EventIndex)
		{
			KeyState->EventCounts[EventIndex].Reset();
			Exchange(KeyState->EventCounts[EventIndex], KeyState->EventAccumulator[EventIndex]);
		}

		if ( (KeyState->SampleCountAccumulator > 0) || Key.ShouldUpdateAxisWithoutSamples() )
		{
			// if we had no samples, we'll assume the state hasn't changed
			// except for some axes, where no samples means the mouse stopped moving
			KeyState->RawValue = KeyState->RawValueAccumulator;
			if (KeyState->SampleCountAccumulator == 0)
			{
				KeyState->EventCounts[IE_Released].Add(++EventCount);
			}
		}

		//@todo there must be some way of directly getting the mouse sampling rate from directinput
		if ( ( Key == EKeys::MouseX ) && KeyState->RawValue.X != 0.f )
		{
		 	// calculate sampling time
		 	// make sure not first non-zero sample
		 	if ( SmoothedMouse[0] != 0 )
		 	{
		 		// not first non-zero
		 		MouseSamplingTotal += FApp::GetDeltaTime();
		 		MouseSamples += KeyState->SampleCountAccumulator;
		 	}
		}
		
		// will just copy for non-axes
		ProcessNonAxesKeys(Key, KeyState);

		// reset the accumulators
		KeyState->RawValueAccumulator = FVector(0, 0, 0);
		KeyState->SampleCountAccumulator = 0;
	}
	EventCount = 0;
	
	// We collect axis contributions by delegate, so we can sum up 
	// contributions from multiple bindings.
	struct FAxisDelegateDetails
	{
		FInputAxisUnifiedDelegate Delegate;
		float Value;

		FAxisDelegateDetails(const FInputAxisUnifiedDelegate& InDelegate, const float InValue)
			: Delegate(InDelegate)
			, Value(InValue)
		{
		}
	};
	struct FVectorAxisDelegateDetails
	{
		FInputVectorAxisUnifiedDelegate Delegate;
		FVector Value;

		FVectorAxisDelegateDetails(const FInputVectorAxisUnifiedDelegate& InDelegate, const FVector InValue)
			: Delegate(InDelegate)
			, Value(InValue)
		{
		}
	};
	static TArray<FAxisDelegateDetails> AxisDelegates;
	static TArray<FVectorAxisDelegateDetails> VectorAxisDelegates;
	static TArray<FDelegateDispatchDetails> NonAxisDelegates;
	static TArray<FKey> KeysToConsume;
	static TArray<FDelegateDispatchDetails> FoundChords;


	// must be called non-recursively and on the game thread
	check(IsInGameThread() && !AxisDelegates.Num() && !VectorAxisDelegates.Num() && !NonAxisDelegates.Num() && !KeysToConsume.Num() && !FoundChords.Num() && !EventIndices.Num());

	struct FDelegateDispatchDetailsSorter
	{
		bool operator()( const FDelegateDispatchDetails& A, const FDelegateDispatchDetails& B ) const
		{
			return (A.EventIndex == B.EventIndex ? A.FoundIndex < B.FoundIndex : A.EventIndex < B.EventIndex);
		}
	};

	int32 StackIndex = InputComponentStack.Num()-1;

	// Walk the stack, top to bottom
	for ( ; StackIndex >= 0; --StackIndex)
	{
		UInputComponent* const IC = InputComponentStack[StackIndex];
		if (IC)
		{
			check(!KeysToConsume.Num() && !FoundChords.Num() && !EventIndices.Num())

			for (int32 ActionIndex=0; ActionIndex<IC->GetNumActionBindings(); ++ActionIndex)
			{
				GetChordsForAction(IC->GetActionBinding(ActionIndex), bGamePaused, FoundChords, KeysToConsume);
			}

			for (int32 KeyIndex=0; KeyIndex<IC->KeyBindings.Num(); ++KeyIndex)
			{
				GetChordForKey(IC->KeyBindings[KeyIndex], bGamePaused, FoundChords, KeysToConsume);
			}

			FoundChords.Sort(FDelegateDispatchDetailsSorter());

			for (int32 ChordIndex=0; ChordIndex < FoundChords.Num(); ++ChordIndex)
			{
				const FDelegateDispatchDetails& FoundChord = FoundChords[ChordIndex];
				bool bFireDelegate = true;
				// If this is a paired action (implements both pressed and released) then we ensure that only one chord is
				// handling the pairing
				if (FoundChord.SourceAction && FoundChord.SourceAction->bPaired)
				{
					FActionKeyDetails& KeyDetails = ActionKeyMap.FindChecked(FoundChord.SourceAction->ActionName);
					if (!KeyDetails.CapturingChord.Key.IsValid() || KeyDetails.CapturingChord == FoundChord.Chord || !IsPressed(KeyDetails.CapturingChord.Key))
					{
						if (FoundChord.SourceAction->KeyEvent == IE_Pressed)
						{
							KeyDetails.CapturingChord = FoundChord.Chord;
						}
						else
						{
							KeyDetails.CapturingChord.Key = EKeys::Invalid;
						}
					}
					else
					{
						bFireDelegate = false;
					}
				}

				if (bFireDelegate && FoundChords[ChordIndex].ActionDelegate.IsBound())
				{
					FoundChords[ChordIndex].FoundIndex = NonAxisDelegates.Num();
					NonAxisDelegates.Add(FoundChords[ChordIndex]);
				}
			}

			for (int32 TouchBindingIndex=0; TouchBindingIndex<IC->TouchBindings.Num(); ++TouchBindingIndex)
			{
				FInputTouchBinding& TB = IC->TouchBindings[TouchBindingIndex];

				for (int32 TouchIndex = 0; TouchIndex < EKeys::NUM_TOUCH_KEYS; TouchIndex++)
				{
					const FKey& TouchKey = EKeys::TouchKeys[TouchIndex];
					if (!IsKeyConsumed(TouchKey) && KeyEventOccurred(TouchKey, TB.KeyEvent, EventIndices))
					{
						if (TB.bExecuteWhenPaused || !bGamePaused)
						{
							check(EventIndices.Num() > 0);
							FVector *TouchedLocation = TouchEventLocations.Find(EventIndices[0]);
							FDelegateDispatchDetails TouchInfo(EventIndices[0], NonAxisDelegates.Num(), TB.TouchDelegate, TouchedLocation != nullptr ? *TouchedLocation : FVector(-1.0f, -1.0f, 0.0f), (ETouchIndex::Type)TouchIndex);
							NonAxisDelegates.Add(TouchInfo);
							for (int32 EventsIndex = 1; EventsIndex < EventIndices.Num(); ++EventsIndex)
							{
								TouchInfo.EventIndex = EventIndices[EventsIndex];
								TouchedLocation = TouchEventLocations.Find(TouchInfo.EventIndex);
								TouchInfo.TouchLocation = TouchedLocation != nullptr ? *TouchedLocation : FVector(-1.0f, -1.0f, 0.0f);
								NonAxisDelegates.Add(TouchInfo);
							}
						}
						if (TB.bConsumeInput)
						{
							KeysToConsume.AddUnique(TouchKey);
						}
					}
				}

				EventIndices.Reset();
			}

			// look for any gestures that happened
			for (FInputGestureBinding& GB : IC->GestureBindings)
			{
				// treat gestures as fire-and-forget, so by convention we assume if they happen, it was a "pressed" event
				if (!IsKeyConsumed(GB.GestureKey) && KeyEventOccurred(GB.GestureKey, IE_Pressed, EventIndices))
				{
					FKeyState const* const KeyState = KeyStateMap.Find(GB.GestureKey);
					if (KeyState)
					{
						check(EventIndices.Num() > 0);
						FDelegateDispatchDetails GestureInfo(EventIndices[0], NonAxisDelegates.Num(), GB.GestureDelegate, KeyState->Value.X);
						NonAxisDelegates.Add(GestureInfo);

						if (GB.bConsumeInput)
						{
							KeysToConsume.AddUnique(GB.GestureKey);
						}
					}
				}

				EventIndices.Reset();
			}

			// Run though game axis bindings and accumulate axis values
			for (FInputAxisBinding& AB : IC->AxisBindings)
			{
				AB.AxisValue = DetermineAxisValue(AB, bGamePaused, KeysToConsume);
				if (AB.AxisDelegate.IsBound())
				{
					AxisDelegates.Add(FAxisDelegateDetails(AB.AxisDelegate, AB.AxisValue));
				}
			}
			for (FInputAxisKeyBinding& AxisKeyBinding : IC->AxisKeyBindings)
			{
				if (!IsKeyConsumed(AxisKeyBinding.AxisKey))
				{
					if (!bGamePaused || AxisKeyBinding.bExecuteWhenPaused)
					{
						AxisKeyBinding.AxisValue = GetKeyValue(AxisKeyBinding.AxisKey);
					}
					else
					{
						AxisKeyBinding.AxisValue = 0.f;
					}

					if (AxisKeyBinding.bConsumeInput)
					{
						KeysToConsume.AddUnique(AxisKeyBinding.AxisKey);
					}
				}

				if (AxisKeyBinding.AxisDelegate.IsBound())
				{
					AxisDelegates.Add(FAxisDelegateDetails(AxisKeyBinding.AxisDelegate, AxisKeyBinding.AxisValue));
				}
			}
			for (FInputVectorAxisBinding& VectorAxisBinding : IC->VectorAxisBindings)
			{
				if (!IsKeyConsumed(VectorAxisBinding.AxisKey))
				{
					if (!bGamePaused || VectorAxisBinding.bExecuteWhenPaused)
					{
						VectorAxisBinding.AxisValue = GetVectorKeyValue(VectorAxisBinding.AxisKey);
					}
					else
					{
						VectorAxisBinding.AxisValue = FVector::ZeroVector;
					}

					if (VectorAxisBinding.bConsumeInput)
					{
						KeysToConsume.AddUnique(VectorAxisBinding.AxisKey);
					}
				}

				if (VectorAxisBinding.AxisDelegate.IsBound())
				{
					VectorAxisDelegates.Add(FVectorAxisDelegateDetails(VectorAxisBinding.AxisDelegate, VectorAxisBinding.AxisValue));
				}
			}

			if (IC->bBlockInput)
			{
				// stop traversing the stack, all input has been consumed by this InputComponent
				--StackIndex;
				KeysToConsume.Reset();
				FoundChords.Reset();
				break;
			}

			// we do this after finishing the whole component, so we don't consume a key while there might be more bindings to it
			for (int32 KeyIndex=0; KeyIndex<KeysToConsume.Num(); ++KeyIndex)
			{
				ConsumeKey(KeysToConsume[KeyIndex]);
			}
			KeysToConsume.Reset();
			FoundChords.Reset();
		}
	}

	for ( ; StackIndex >= 0; --StackIndex)
	{
		UInputComponent* IC = InputComponentStack[StackIndex];
		if (IC)
		{
			for (FInputAxisBinding& AxisBinding : IC->AxisBindings)
			{
				AxisBinding.AxisValue = 0.f;
			}
			for (FInputAxisKeyBinding& AxisKeyBinding : IC->AxisKeyBindings)
			{
				AxisKeyBinding.AxisValue = 0.f;
			}
			for (FInputVectorAxisBinding& VectorAxisBinding : IC->VectorAxisBindings)
			{
				VectorAxisBinding.AxisValue = FVector::ZeroVector;
			}
		}
	}

	// Dispatch the delegates in the order they occurred
	NonAxisDelegates.Sort(FDelegateDispatchDetailsSorter());
	for (const FDelegateDispatchDetails& Details : NonAxisDelegates)
	{
		if (Details.ActionDelegate.IsBound())
		{
			Details.ActionDelegate.Execute(Details.Chord.Key);
		}
		else if (Details.TouchDelegate.IsBound())
		{
			Details.TouchDelegate.Execute(Details.FingerIndex, Details.TouchLocation);
		}
		else if (Details.GestureDelegate.IsBound())
		{
			Details.GestureDelegate.Execute(Details.GestureValue);
		}
	}
	// Now dispatch delegates for summed axes
	for (const FAxisDelegateDetails& Details : AxisDelegates)
	{
		if (Details.Delegate.IsBound())
		{
			Details.Delegate.Execute(Details.Value);
		}
	}
	for (const FVectorAxisDelegateDetails& Details : VectorAxisDelegates)
	{
		if (Details.Delegate.IsBound())
		{
			Details.Delegate.Execute(Details.Value);
		}
	}

	PlayerController->PostProcessInput(DeltaTime, bGamePaused);

	FinishProcessingPlayerInput();
	AxisDelegates.Reset();
	VectorAxisDelegates.Reset();
	NonAxisDelegates.Reset();
	TouchEventLocations.Reset();
}

void UPlayerInput::DiscardPlayerInput()
{
	FinishProcessingPlayerInput();
}

void UPlayerInput::FinishProcessingPlayerInput()
{
	// finished processing input for this frame, clean up for next update
	for (TMap<FKey,FKeyState>::TIterator It(KeyStateMap); It; ++It)
	{
		FKeyState& KeyState = It.Value();
		KeyState.bDownPrevious = KeyState.bDown;
		KeyState.bConsumed = false;
	}
}

void UPlayerInput::ClearSmoothing()
{
	for ( int32 i=0; i<2; i++ )
	{
		ZeroTime[i] = 0.f;
		SmoothedMouse[i] = 0.f;
	}

	const UPlayerInput* DefaultPlayerInput = GetDefault<UPlayerInput>();

   	MouseSamplingTotal = DefaultPlayerInput->MouseSamplingTotal;
	MouseSamples = DefaultPlayerInput->MouseSamples;
}


float UPlayerInput::SmoothMouse(float aMouse, uint8& SampleCount, int32 Index)
{
	check(Index >= 0);
	check(Index < ARRAY_COUNT(ZeroTime));

	UWorld* World = GetWorld();
	if (World)
	{
		check(World->GetWorldSettings());
		const float EffectiveTimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();
		if (EffectiveTimeDilation != LastTimeDilation)
		{
			LastTimeDilation = EffectiveTimeDilation;
			ClearSmoothing();
		}
	}

	const float DeltaTime = FApp::GetDeltaTime();

	if (DeltaTime < 0.25f)
	{
		check(MouseSamples > 0);

		// this is seconds/sample
		const float MouseSamplingTime = MouseSamplingTotal/MouseSamples;
		check(MouseSamplingTime > 0.0f);

		if ( aMouse == 0 )
		{
			// no mouse movement received
			ZeroTime[Index] += DeltaTime;		// increment length of time we've been at zero
			if ( ZeroTime[Index] < MouseSamplingTime )
			{
				// zero mouse movement is possibly because less than the mouse sampling interval has passed
				aMouse = SmoothedMouse[Index] * DeltaTime/MouseSamplingTime;
			}
			else
			{
				SmoothedMouse[Index] = 0;
			}
		}
		else
		{
			ZeroTime[Index] = 0;
			if ( SmoothedMouse[Index] != 0 )
			{
				// this isn't the first tick with non-zero mouse movement
				if ( DeltaTime < MouseSamplingTime * (SampleCount + 1) )
				{
					check(SampleCount > 0);

					// smooth mouse movement so samples/tick is constant
					aMouse = aMouse * DeltaTime/(MouseSamplingTime * SampleCount);
				}
				else
				{
					// fewer samples, so going slow
					// use number of samples we should have had for sample count
					SampleCount = DeltaTime/MouseSamplingTime;
				}
			}

			check(SampleCount > 0);
			SmoothedMouse[Index] = aMouse/SampleCount;
		}
	}
	else
	{
		// if we had an abnormally long frame, clear everything so it doesn't distort the results
		ClearSmoothing();
	}

	SampleCount = 0;

	return aMouse;
}

void UPlayerInput::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if (Canvas)
	{
		FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
		DisplayDebugManager.SetDrawColor(FColor::Red);
		DisplayDebugManager.DrawString(FString::Printf(TEXT("INPUT %s"), *GetName()));

		UWorld* World = GetWorld();
		check(World);
		const float WorldRealTimeSeconds = World->GetRealTimeSeconds();
		for (TMap<FKey,FKeyState>::TIterator It(KeyStateMap); It; ++It)
		{
			FKeyState const* const KeyState = &It.Value();
			FKey const Key = It.Key();

			// used cached mouse values, since they were flushed already
			FString Str = FString::Printf(TEXT(" %s: %.2f (raw %.2f)"), *(Key.GetDisplayName().ToString()), KeyState->Value.X, KeyState->RawValue.X);

			if ( KeyState->bDown || (KeyState->Value.X != 0.f) )
			{
				if (!Key.IsFloatAxis())
				{
					Str += FString::Printf(TEXT(" time: %.2f"), WorldRealTimeSeconds - KeyState->LastUpDownTransitionTime);
				}
				DisplayDebugManager.SetDrawColor(FColor(180, 255, 180));
				DisplayDebugManager.DrawString(Str);
			}
			else
			{
				DisplayDebugManager.SetDrawColor(FColor(180, 180, 180));
				DisplayDebugManager.DrawString(Str);
			}
		}

		float const DetectedMouseSampleHz = MouseSamples / MouseSamplingTotal;

		DisplayDebugManager.SetDrawColor(FColor::White);
		DisplayDebugManager.DrawString(FString::Printf(TEXT("MouseSampleRate: %.2f"), DetectedMouseSampleHz));
		DisplayDebugManager.DrawString(FString::Printf(TEXT("MouseX ZeroTime: %.2f, Smoothed: %.2f"), ZeroTime[0], SmoothedMouse[0]));
		DisplayDebugManager.DrawString(FString::Printf(TEXT("MouseY ZeroTime: %.2f, Smoothed: %.2f"), ZeroTime[1], SmoothedMouse[1]));

		if ( (ZeroTime[0] > 2.f && ZeroTime[1] > 2.f) && GetDefault<UInputSettings>()->bEnableMouseSmoothing )
		{
			// return to center of screen
			DebugSmoothedMouseX = DebugUnSmoothedMouseX = Canvas->SizeX * 0.5f;
			DebugSmoothedMouseY = DebugUnSmoothedMouseY = Canvas->SizeY * 0.5f;
		}
		else
		{
			float const CenterX = Canvas->SizeX * 0.5f;
			float const CenterY = Canvas->SizeY * 0.5f;

			static const float DEBUGSMOOTHMOUSE_REGIONSIZE = 700;

			// clamp mouse smoothing debug cursor
			DebugSmoothedMouseX = FMath::Clamp(DebugSmoothedMouseX, CenterX-DEBUGSMOOTHMOUSE_REGIONSIZE, CenterX+DEBUGSMOOTHMOUSE_REGIONSIZE);
			DebugSmoothedMouseY = FMath::Clamp(DebugSmoothedMouseY, CenterY-DEBUGSMOOTHMOUSE_REGIONSIZE, CenterY+DEBUGSMOOTHMOUSE_REGIONSIZE);
			DebugUnSmoothedMouseX = FMath::Clamp(DebugUnSmoothedMouseX, CenterX-DEBUGSMOOTHMOUSE_REGIONSIZE, CenterX+DEBUGSMOOTHMOUSE_REGIONSIZE);
			DebugUnSmoothedMouseY = FMath::Clamp(DebugUnSmoothedMouseY, CenterY-DEBUGSMOOTHMOUSE_REGIONSIZE, CenterY+DEBUGSMOOTHMOUSE_REGIONSIZE);
		}

		// draw YELLOW box for SMOOTHED mouse loc 
		FCanvasTileItem TileItem( FVector2D(DebugSmoothedMouseX, DebugSmoothedMouseY), GWhiteTexture, FVector2D(8.0f,8.0f), FLinearColor::Yellow );
		Canvas->DrawItem( TileItem );

		// draw WHITE box for UNSMOOTHED mouse loc
		TileItem.SetColor( FLinearColor::White );
		TileItem.Size = FVector2D( 5.0f, 5.0f );
		TileItem.Position = FVector2D(DebugUnSmoothedMouseX, DebugUnSmoothedMouseY);
		Canvas->DrawItem( TileItem );
	}
}

bool UPlayerInput::WasJustPressed( FKey InKey ) const
{
	if (InKey == EKeys::AnyKey)
	{
		// Is there any key that has just been pressed
		for (const TPair<FKey, FKeyState>& KeyStatePair : KeyStateMap)
		{
			if (!KeyStatePair.Key.IsFloatAxis() && !KeyStatePair.Key.IsVectorAxis() && KeyStatePair.Value.EventCounts[IE_Pressed].Num() > 0)
			{
				return true;
			}
		}
	}
	else if (FKeyState const* const KeyState = KeyStateMap.Find(InKey))
	{
		return (KeyState->EventCounts[IE_Pressed].Num() > 0);
	}

	return false;
}


bool UPlayerInput::WasJustReleased( FKey InKey ) const
{
	if (InKey == EKeys::AnyKey)
	{
		// Is there any key that has just been released
		for (const TPair<FKey, FKeyState>& KeyStatePair : KeyStateMap)
		{
			if (!KeyStatePair.Key.IsFloatAxis() && !KeyStatePair.Key.IsVectorAxis() && KeyStatePair.Value.EventCounts[IE_Released].Num() > 0)
			{
				return true;
			}
		}
	}
	else if (FKeyState const* const KeyState = KeyStateMap.Find(InKey))
	{
		return (KeyState->EventCounts[IE_Released].Num() > 0);
	}

	return false;
}

float UPlayerInput::GetTimeDown( FKey InKey ) const
{
	UE_CLOG(InKey == EKeys::AnyKey, LogInput, Warning, TEXT("GetTimeDown cannot return a meaningful result for AnyKey"));
	UWorld* World = GetWorld();	
	float DownTime = 0.f;
	if( World )
	{
		FKeyState const* const KeyState = KeyStateMap.Find(InKey);
		if (KeyState && KeyState->bDown)
		{
			DownTime = (World->GetRealTimeSeconds() - KeyState->LastUpDownTransitionTime);
		}
	}

	return DownTime;
}

bool UPlayerInput::IsKeyConsumed( FKey InKey ) const
{
	if (InKey == EKeys::AnyKey)
	{
		// Is there any key that is consumed
		for (const TPair<FKey, FKeyState>& KeyStatePair : KeyStateMap)
		{
			if (KeyStatePair.Value.bConsumed)
			{
				return true;
			}
		}
	}
	else if (FKeyState const* const KeyState = KeyStateMap.Find(InKey))
	{
		return KeyState->bConsumed;
	}

	return false;
}

float UPlayerInput::GetKeyValue( FKey InKey ) const
{
	UE_CLOG(InKey == EKeys::AnyKey, LogInput, Warning, TEXT("GetKeyValue cannot return a meaningful result for AnyKey"));
	FKeyState const* const KeyState = KeyStateMap.Find(InKey);
	return KeyState ? KeyState->Value.X : 0.f;
}

float UPlayerInput::GetRawKeyValue( FKey InKey ) const
{
	UE_CLOG(InKey == EKeys::AnyKey, LogInput, Warning, TEXT("GetRawKeyValue cannot return a meaningful result for AnyKey"));
	FKeyState const* const KeyState = KeyStateMap.Find(InKey);
	return KeyState ? KeyState->RawValue.X : 0.f;
}

FVector UPlayerInput::GetVectorKeyValue( FKey InKey ) const
{
	UE_CLOG(InKey == EKeys::AnyKey, LogInput, Warning, TEXT("GetVectorKeyValue cannot return a meaningful result for AnyKey"));
	FKeyState const* const KeyState = KeyStateMap.Find(InKey);
	return KeyState ? KeyState->RawValue : FVector(0,0,0);
}

bool UPlayerInput::IsPressed( FKey InKey ) const
{
	if (InKey == EKeys::AnyKey)
	{
		// Is there any key that is down
		for (const TPair<FKey, FKeyState>& KeyStatePair : KeyStateMap)
		{
			if (!KeyStatePair.Key.IsFloatAxis() && !KeyStatePair.Key.IsVectorAxis() && KeyStatePair.Value.bDown)
			{
				return true;
			}
		}
	}
	else if (FKeyState const* const KeyState = KeyStateMap.Find(InKey))
	{
		return KeyState->bDown;
	}

	return false;
}


float UPlayerInput::MassageAxisInput(FKey Key, float RawValue)
{
	float NewVal = RawValue;
	
	ConditionalInitAxisProperties();

	// no massaging for buttons atm, might want to support it for things like pressure-sensitivity at some point

	FInputAxisProperties const* const AxisProps = AxisProperties.Find(Key);
	if (AxisProps)
	{
		// deal with axis deadzone
		if (AxisProps->DeadZone > 0.f)
		{
			// We need to translate and scale the input to the +/- 1 range after removing the dead zone.
			if( NewVal > 0 )
			{
				NewVal = FMath::Max( 0.f, NewVal - AxisProps->DeadZone ) / (1.f - AxisProps->DeadZone);
			}
			else
			{
				NewVal = -FMath::Max( 0.f, -NewVal - AxisProps->DeadZone ) / (1.f - AxisProps->DeadZone);
			}
		}
		
		// apply any exponent curvature while we're in the [0..1] range
		if (AxisProps->Exponent != 1.f)
		{
			NewVal = FMath::Sign(NewVal) * FMath::Pow(FMath::Abs(NewVal), AxisProps->Exponent);
		}

		// now apply any scaling (sensitivity)
		NewVal *= AxisProps->Sensitivity;

		if (AxisProps->bInvert)
		{
			NewVal *= -1.f;
		}
	}

	// special handling for mouse input
	if ( (Key == EKeys::MouseX) || (Key == EKeys::MouseY) )
	{
		const UInputSettings* DefaultInputSettings = GetDefault<UInputSettings>();

		// Take FOV into account (lower FOV == less sensitivity).
		APlayerController const* const PlayerController = GetOuterAPlayerController();
		float const FOVScale = (DefaultInputSettings->bEnableFOVScaling && PlayerController->PlayerCameraManager) ? (DefaultInputSettings->FOVScale*PlayerController->PlayerCameraManager->GetFOVAngle()) : 1.0f;
		NewVal *= FOVScale;

		// debug
		if (Key == EKeys::MouseX)
		{
			DebugUnSmoothedMouseX += NewVal * DebugSmoothedMouseSensitivity;
		}
		else
		{
			DebugUnSmoothedMouseY += -NewVal * DebugSmoothedMouseSensitivity;
		}

		// mouse smoothing 
		if (DefaultInputSettings->bEnableMouseSmoothing)
		{
			FKeyState* const KeyState = KeyStateMap.Find(Key);
			if (KeyState)
			{
				NewVal = SmoothMouse( NewVal, KeyState->SampleCountAccumulator, (Key == EKeys::MouseX ? 0 : 1) );
			}
		}

		// debug
		if (Key == EKeys::MouseX)
		{
			DebugSmoothedMouseX += NewVal * DebugSmoothedMouseSensitivity;
		}
		else
		{
			DebugSmoothedMouseY += -NewVal * DebugSmoothedMouseSensitivity;
		}
	}

	return NewVal;
}

void UPlayerInput::Tick(float DeltaTime)
{
	ConditionalInitAxisProperties();
	GestureRecognizer.DetectGestures(Touches, this, DeltaTime);
}

void UPlayerInput::ConsumeKey(FKey Key)
{
	FKeyState* const KeyState = KeyStateMap.Find(Key);
	if (KeyState)
	{
		KeyState->bConsumed = true;
	}
}

bool UPlayerInput::KeyEventOccurred(FKey Key, EInputEvent Event, TArray<uint32>& InEventIndices) const
{
	FKeyState const* const KeyState = KeyStateMap.Find(Key);
	if (KeyState && KeyState->EventCounts[Event].Num() > 0)
	{
		InEventIndices = KeyState->EventCounts[Event];
		return true;
	}

	return false;
}

bool UPlayerInput::IsAltPressed() const
{
	return IsPressed(EKeys::LeftAlt) || IsPressed(EKeys::RightAlt);
}

bool UPlayerInput::IsCtrlPressed() const
{
#if PLATFORM_MAC
	return IsPressed(EKeys::LeftCommand) || IsPressed(EKeys::RightCommand);
#else
	return IsPressed(EKeys::LeftControl) || IsPressed(EKeys::RightControl);
#endif
}

bool UPlayerInput::IsShiftPressed() const
{
	return IsPressed(EKeys::LeftShift) || IsPressed(EKeys::RightShift);
}

bool UPlayerInput::IsCmdPressed() const
{
#if PLATFORM_MAC
	return IsPressed(EKeys::LeftControl) || IsPressed(EKeys::RightControl);
#else
	return IsPressed(EKeys::LeftCommand) || IsPressed(EKeys::RightCommand);
#endif
}

void UPlayerInput::ConditionalInitAxisProperties()
{
	// Initialize AxisProperties map if needed.
	if ( AxisProperties.Num() == 0 )
	{
		// move stuff from config structure to our runtime structure
		for (const FInputAxisConfigEntry& AxisConfigEntry : AxisConfig)
		{
			const FKey AxisKey = AxisConfigEntry.AxisKeyName;
			if (AxisKey.IsValid())
			{
				AxisProperties.Add(AxisKey, AxisConfigEntry.AxisProperties);
			}
		}
	}
}

bool UPlayerInput::IsKeyHandledByAction( FKey Key ) const
{
	for (const FInputActionKeyMapping& Mapping : ActionMappings)
	{
		if( (Mapping.Key == Key || Mapping.Key == EKeys::AnyKey) && 
			(Mapping.bAlt == false || IsAltPressed()) &&
			(Mapping.bCtrl == false || IsCtrlPressed()) &&
			(Mapping.bShift == false || IsShiftPressed()) &&
			(Mapping.bCmd == false || IsCmdPressed()) )
		{
			return true;
		}
	}

	return false;
}

const TArray<FInputActionKeyMapping>& UPlayerInput::GetKeysForAction(const FName ActionName)
{
	ConditionalBuildKeyMappings();

	const FActionKeyDetails* KeyDetails = ActionKeyMap.Find(ActionName);
	if (KeyDetails)
	{
		return KeyDetails->Actions;
	}

	return UPlayerInput::NoKeyMappings;
}

const TArray<FInputAxisKeyMapping>& UPlayerInput::GetKeysForAxis(const FName AxisName)
{
	ConditionalBuildKeyMappings();

	const FAxisKeyDetails* KeyDetails = AxisKeyMap.Find(AxisName);
	if (KeyDetails)
	{
		return KeyDetails->KeyMappings;
	}

	return UPlayerInput::NoAxisMappings;
}

#if !UE_BUILD_SHIPPING
bool UPlayerInput::ExecInputCommands( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	bool bResult = false;

	int32 CmdLen = FCString::Strlen(Cmd);
	TCHAR* Line = (TCHAR*)FMemory::Malloc(sizeof(TCHAR)*(CmdLen+1));

	while( FParse::Line( &Cmd, Line, CmdLen+1) )		// The FParse::Line function expects the full array size, including the NULL character.
	{
		const TCHAR* Str = Line;
		if(CurrentEvent == IE_Pressed || (CurrentEvent == IE_Released && FParse::Command(&Str,TEXT("OnRelease"))))
		{
			APlayerController*	Actor = Cast<APlayerController>(GetOuter());
			UPlayer* Player = Actor ? Actor->Player : NULL;
			if(ProcessConsoleExec(Str,Ar,this))
			{
				bResult = true;
				continue;
			}
			else if(Actor && Exec(Actor->GetWorld(), Str,Ar))
			{
				bResult = true;
				continue;
			}
			else if(Player && Player->Exec( Actor->GetWorld(), Str,Ar))
			{
				bResult = true;
				continue;
			}
		}
		else
		{
			bResult |= Exec(InWorld, Str,Ar);
		}
	}

	FMemory::Free(Line);

	return bResult;
}

bool UPlayerInput::Exec(UWorld* InWorld, const TCHAR* Str,FOutputDevice& Ar)
{
	TCHAR Temp[256];

	if( FParse::Command( &Str, TEXT("KEYBINDING") ) && FParse::Token( Str, Temp, ARRAY_COUNT(Temp), 0 ) )
	{
		FKey const Key(Temp);
		if (Key.IsValid())
		{
			for(uint32 BindIndex = 0;BindIndex < (uint32)DebugExecBindings.Num();BindIndex++)
			{
				if (DebugExecBindings[BindIndex].Key == Key)
				{
					Ar.Logf(TEXT("%s"),*DebugExecBindings[BindIndex].Command);
					break;
				}
			}
		}

		return 1;
	}
	else if( !bExecutingBindCommand && FParse::Token( Str, Temp, ARRAY_COUNT(Temp), 0 ) )
	{
		FKey const Key(Temp);
		if(Key.IsValid())
		{
			for(int32 BindIndex = DebugExecBindings.Num() - 1; BindIndex >= 0; BindIndex--)
			{
				if(DebugExecBindings[BindIndex].Key == Key)
				{
					bExecutingBindCommand = true;
					bool bResult = ExecInputCommands(GetWorld(), *DebugExecBindings[BindIndex].Command,Ar);
					bExecutingBindCommand = false;
					return bResult;
				}
			}
		}
	}

	return 0;
}

FString UPlayerInput::GetBind(FKey Key)
{
	static bool bDebugExecBindingsAllowed = !FParse::Param( FCommandLine::Get(), TEXT("NoDebugExecBindings") );

	if ( bDebugExecBindingsAllowed )
	{
		const bool bControlPressed = IsCtrlPressed();
		const bool bShiftPressed = IsShiftPressed();
		const bool bAltPressed = IsAltPressed();
		const bool bCmdPressed = IsCmdPressed();

		for ( int32 BindIndex = DebugExecBindings.Num() - 1; BindIndex >= 0; BindIndex-- )
		{
			const FKeyBind&	Bind = DebugExecBindings[BindIndex];
			if ( Bind.Key == Key && !Bind.bDisabled )
			{
				// if the modifier key pressed [or this key-bind doesn't require that key], and the key-bind isn't
				// configured to ignore they modifier key, we've found a match.
				if ((!Bind.Control || bControlPressed) && (!Bind.Shift || bShiftPressed) && (!Bind.Alt || bAltPressed) && (!Bind.Cmd || bCmdPressed)
					&&	(!Bind.bIgnoreCtrl || !bControlPressed) && (!Bind.bIgnoreShift || !bShiftPressed) && (!Bind.bIgnoreAlt || !bAltPressed) && (!Bind.bIgnoreCmd || !bCmdPressed))
				{
					return DebugExecBindings[BindIndex].Command;
				}
			}
		}
	}

	return TEXT("");
}

FKeyBind UPlayerInput::GetExecBind(FString const& ExecCommand)
{
	FKeyBind Binding;
	for( auto InputBindingIt = DebugExecBindings.CreateConstIterator(); InputBindingIt; ++InputBindingIt )
	{
		if(InputBindingIt->Command == ExecCommand)
		{
			Binding = *InputBindingIt;
			break;
		}
	}
	return Binding;
}
#endif

void UPlayerInput::SetBind(FName BindName, const FString& Command)
{
#if !UE_BUILD_SHIPPING
	FKey const BindKey(BindName);
	if (BindKey.IsValid())
	{
		FString CommandMod = Command;
		if ( CommandMod.Left(1) == TEXT("\"") && CommandMod.Right(1) == ("\"") )
		{
			CommandMod = CommandMod.Mid(1, CommandMod.Len() - 2);
		}

		for(int32 BindIndex = DebugExecBindings.Num()-1;BindIndex >= 0;BindIndex--)
		{
			if (DebugExecBindings[BindIndex].Key == BindKey)
			{
				DebugExecBindings[BindIndex].Command = CommandMod;
				SaveConfig();
				return;
			}
		}

		FKeyBind NewBind;
		NewBind.Key = BindKey;
		NewBind.Command = CommandMod;
		DebugExecBindings.Add(NewBind);
		SaveConfig();
	}
#endif
}

class UWorld* UPlayerInput::GetWorld() const
{
	check(GetOuterAPlayerController());
	UWorld* World = GetOuterAPlayerController()->GetWorld();
	return World;
}
