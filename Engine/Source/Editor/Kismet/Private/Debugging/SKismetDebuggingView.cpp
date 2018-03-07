// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Debugging/SKismetDebuggingView.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "Editor/EditorEngine.h"
#include "EngineGlobals.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node.h"
#include "Engine/Breakpoint.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/DebuggerCommands.h"
#include "Debugging/KismetDebugCommands.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "DebugViewUI"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintDebuggingView, Log, All);

//////////////////////////////////////////////////////////////////////////

namespace KismetDebugViewConstants
{
	const FName ColumnId_Name( "Name" );
	const FName ColumnId_Value( "Value" );
	const FText ColumnText_Name( NSLOCTEXT("DebugViewUI", "Name", "Name") );
	const FText ColumnText_Value( NSLOCTEXT("DebugViewUI", "Value", "Value") );
}

//////////////////////////////////////////////////////////////////////////
// FDebugLineItem

FText FDebugLineItem::GetDisplayName() const
{
	return FText::GetEmpty();
}

FText FDebugLineItem::GetDescription() const
{
	return FText::GetEmpty();
}

TSharedRef<SWidget> FDebugLineItem::GenerateNameWidget()
{
	return SNew(STextBlock)
		.Text(this, &FDebugLineItem::GetDisplayName);
}

TSharedRef<SWidget> FDebugLineItem::GenerateValueWidget()
{
	return SNew(STextBlock)
		.Text(this, &FDebugLineItem::GetDescription);
}

UBlueprint* FDebugLineItem::GetBlueprintForObject(UObject* ParentObject)
{
	UBlueprint* ParentBlueprint = NULL;

	if (ParentObject != NULL)
	{
		ParentBlueprint = Cast<UBlueprint>(ParentObject);
		if (ParentBlueprint == NULL)
		{
			ParentBlueprint = Cast<UBlueprint>(ParentObject->GetClass()->ClassGeneratedBy);
		}
	}

	return ParentBlueprint;
}

UBlueprintGeneratedClass* FDebugLineItem::GetClassForObject(UObject* ParentObject)
{
	if (ParentObject != NULL)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(ParentObject))
		{
			return Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass);
		}
		else if (UBlueprintGeneratedClass* Result = Cast<UBlueprintGeneratedClass>(ParentObject))
		{
			return Result;
		}
		else
		{
			return Cast<UBlueprintGeneratedClass>(ParentObject->GetClass());
		}
	}

	return NULL;
}

// O(# children)
void FDebugLineItem::EnsureChildIsAdded(TArray<FDebugTreeItemPtr>& ChildrenMirrors, TArray<FDebugTreeItemPtr>& OutChildren, const FDebugLineItem& Item)
{
	for (int32 i = 0; i < ChildrenMirrors.Num(); ++i)
	{
		TSharedPtr< FDebugLineItem > MirrorItem = ChildrenMirrors[i];

		if (MirrorItem->Type == Item.Type)
		{
			if (Item.Compare(MirrorItem.Get()))
			{
				OutChildren.Add(MirrorItem);
				return;
			}
		}
	}

	TSharedPtr< FDebugLineItem > Result = MakeShareable(Item.Duplicate());
	ChildrenMirrors.Add(Result);

	OutChildren.Add(Result);
}

//////////////////////////////////////////////////////////////////////////
// FMessageLineItem

struct FMessageLineItem : public FDebugLineItem
{
protected:
	FString Message;
public:
	// Message line
	FMessageLineItem(const FString& InMessage)
		: FDebugLineItem(DLT_Message)
		, Message(InMessage)
	{
	}
protected:
	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		FMessageLineItem* Other = (FMessageLineItem*)BaseOther;
		return Message == Other->Message;
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		return new FMessageLineItem(Message);
	}

	virtual FText GetDescription() const override
	{
		return FText::FromString(Message);
	}
};

//////////////////////////////////////////////////////////////////////////
// FLatentActionLineItem

struct FLatentActionLineItem : public FDebugLineItem
{
protected:
	int32 UUID;
	TWeakObjectPtr< UObject > ParentObjectRef;
public:
	FLatentActionLineItem(int32 InUUID, UObject* ParentObject)
		: FDebugLineItem(DLT_LatentAction)
	{
		UUID = InUUID;
		check(UUID != INDEX_NONE);
		ParentObjectRef = ParentObject;
	}
protected:
	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		FLatentActionLineItem* Other = (FLatentActionLineItem*)BaseOther;
		return (ParentObjectRef.Get() == Other->ParentObjectRef.Get()) &&
			(UUID == Other->UUID);
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		return new FLatentActionLineItem(UUID, ParentObjectRef.Get());
	}
protected:
	virtual TSharedRef<SWidget> GenerateNameWidget() override;
	virtual FText GetDescription() const override;
	virtual FText GetDisplayName() const override;
	void OnNavigateToLatentNode();

	class UEdGraphNode* FindAssociatedNode() const;
};

FText FLatentActionLineItem::GetDescription() const
{
	if (UObject* ParentObject = ParentObjectRef.Get())
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(ParentObject, EGetWorldErrorMode::ReturnNull))
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			return FText::FromString(LatentActionManager.GetDescription(ParentObject, UUID));
		}
	}

	return LOCTEXT("NullObject", "Object has been destroyed");
}

TSharedRef<SWidget> FLatentActionLineItem::GenerateNameWidget()
{
	return
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SImage)
			. Image(FEditorStyle::GetBrush(TEXT("Kismet.LatentActionIcon")))
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
				SNew(SHyperlink)
			.Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
			.OnNavigate(this, &FLatentActionLineItem::OnNavigateToLatentNode)
			.Text(this, &FLatentActionLineItem::GetDisplayName)
			.ToolTipText( LOCTEXT("NavLatentActionLoc_Tooltip", "Navigate to the latent action location") )
		];
}

UEdGraphNode* FLatentActionLineItem::FindAssociatedNode() const
{
	if (UBlueprintGeneratedClass* Class = GetClassForObject(ParentObjectRef.Get()))
	{
		return Class->GetDebugData().FindNodeFromUUID(UUID);
	}

	return NULL;
}

FText FLatentActionLineItem::GetDisplayName() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ID"), UUID);
	if (UK2Node* Node = Cast<UK2Node>(FindAssociatedNode()))
	{
		Args.Add(TEXT("Title"), Node->GetCompactNodeTitle());

		return FText::Format(LOCTEXT("ID", "{Title} (ID: {ID})"), Args);
	}
	else
	{
		return FText::Format(LOCTEXT("LatentAction", "Latent action # {ID}"), Args);
	}
}

void FLatentActionLineItem::OnNavigateToLatentNode( )
{
	if (UEdGraphNode* Node = FindAssociatedNode())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
	}
}

//////////////////////////////////////////////////////////////////////////
// FWatchLineItem 

struct FWatchLineItem : public FDebugLineItem
{
protected:
	TWeakObjectPtr< UObject > ParentObjectRef;
	FEdGraphPinReference ObjectRef;
public:
	FWatchLineItem(UEdGraphPin* PinToWatch, UObject* ParentObject)
		: FDebugLineItem(DLT_Watch)
	{
		ObjectRef = PinToWatch;
		ParentObjectRef = ParentObject;
	}

	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		FWatchLineItem* Other = (FWatchLineItem*)BaseOther;
		return (ParentObjectRef.Get() == Other->ParentObjectRef.Get()) &&
			(ObjectRef.Get() == Other->ObjectRef.Get());
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		return new FWatchLineItem(ObjectRef.Get(), ParentObjectRef.Get());
	}	

	virtual void MakeMenu(class FMenuBuilder& MenuBuilder) override
	{
		if (UEdGraphPin* WatchedPin = ObjectRef.Get())
		{
			FUIAction ClearThisWatch(
				FExecuteAction::CreateStatic( &FDebuggingActionCallbacks::ClearWatch, WatchedPin )
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ClearWatch", "Stop watching"),
				LOCTEXT("ClearWatch_ToolTip", "Stop watching this variable"),
				FSlateIcon(),
				ClearThisWatch);
		}
	}
protected:
	virtual FText GetDescription() const override;
	virtual FText GetDisplayName() const override;
	virtual TSharedRef<SWidget> GenerateNameWidget() override;

	void OnNavigateToWatchLocation();
};

FText FWatchLineItem::GetDisplayName() const
{
	if (UEdGraphPin* PinToWatch = ObjectRef.Get())
	{
		if (UBlueprint* Blueprint = GetBlueprintForObject(ParentObjectRef.Get()))
		{
			if (UProperty* Property = FKismetDebugUtilities::FindClassPropertyForPin(Blueprint, PinToWatch))
			{
				return FText::FromString(UEditorEngine::GetFriendlyName(Property));
			}
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("PinWatchName"), FText::FromString(PinToWatch->GetName()));
		return FText::Format(LOCTEXT("DisplayNameNoProperty", "{PinWatchName} (no prop)"), Args);
	}
	else
	{
		return FText::GetEmpty();
	}
}

FText FWatchLineItem::GetDescription() const
{
	if (UEdGraphPin* PinToWatch = ObjectRef.Get())
	{
		// Try to determine the blueprint that generated the watch
		UBlueprint* ParentBlueprint = GetBlueprintForObject(ParentObjectRef.Get());

		// Find a valid property mapping and display the current value
		UObject* ParentObject = ParentObjectRef.Get();
		if ((ParentBlueprint != ParentObject) && (ParentBlueprint != NULL))
		{
			FString WatchString;
			const FKismetDebugUtilities::EWatchTextResult WatchStatus = FKismetDebugUtilities::GetWatchText(WatchString, ParentBlueprint, ParentObject, PinToWatch);

			switch (WatchStatus)
			{
			case FKismetDebugUtilities::EWTR_Valid:
				return FText::FromString(WatchString);

			case FKismetDebugUtilities::EWTR_NotInScope:
				return LOCTEXT("NotInScope", "Not in scope");

			case FKismetDebugUtilities::EWTR_NoProperty:
				return LOCTEXT("UnknownProperty", "No debug data");

			default:
			case FKismetDebugUtilities::EWTR_NoDebugObject:
				return LOCTEXT("NoDebugObject", "No debug object");
			}			
		}
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> FWatchLineItem::GenerateNameWidget()
{
	return SNew(SHorizontalBox)
		
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SImage)
			.Image(FEditorStyle::GetBrush(TEXT("Kismet.WatchIcon")))
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
				SNew(SHyperlink)
			.Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
			.OnNavigate(this, &FWatchLineItem::OnNavigateToWatchLocation)
			.Text(this, &FWatchLineItem::GetDisplayName)
			.ToolTipText( LOCTEXT("NavWatchLoc", "Navigate to the watch location") )
		];
}

void FWatchLineItem::OnNavigateToWatchLocation( )
{
	if (UEdGraphPin* ObjectToFocus = ObjectRef.Get())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnPin(ObjectToFocus);
	}
}

//////////////////////////////////////////////////////////////////////////
// FBreakpointLineItem

struct FBreakpointLineItem : public FDebugLineItem
{
protected:
	TWeakObjectPtr< UObject > ParentObjectRef;
	TWeakObjectPtr< UBreakpoint > BreakpointRef;
public:
	FBreakpointLineItem(UBreakpoint* BreakpointToWatch, UObject* ParentObject)
		: FDebugLineItem(DLT_Breakpoint)
	{
		BreakpointRef = BreakpointToWatch;
		ParentObjectRef = ParentObject;
	}

	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		FBreakpointLineItem* Other = (FBreakpointLineItem*)BaseOther;
		return (ParentObjectRef.Get() == Other->ParentObjectRef.Get()) &&
			(BreakpointRef.Get() == Other->BreakpointRef.Get());
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		return new FBreakpointLineItem(BreakpointRef.Get(), ParentObjectRef.Get());
	}	

	virtual void MakeMenu(class FMenuBuilder& MenuBuilder) override
	{
		UBreakpoint* Breakpoint = BreakpointRef.Get();
		UBlueprint* ParentBlueprint = GetBlueprintForObject(ParentObjectRef.Get());

		// By default, we don't allow actions to execute when in debug mode. 
		// Create an empty action to always allow execution for these commands (they are allowed in debug mode)
		FCanExecuteAction AlwaysAllowExecute;

		if (Breakpoint != NULL)
		{
			const bool bNewEnabledState = !Breakpoint->IsEnabledByUser();

			FUIAction ToggleThisBreakpoint(
				FExecuteAction::CreateStatic( &FDebuggingActionCallbacks::SetBreakpointEnabled, Breakpoint, bNewEnabledState ),
				AlwaysAllowExecute
				);

			if (bNewEnabledState)
			{
				// Enable
				MenuBuilder.AddMenuEntry(
					LOCTEXT("EnableBreakpoint", "Enable breakpoint"),
					LOCTEXT("EnableBreakpoint_ToolTip", "Enable this breakpoint; the debugger will appear when this node is about to be executed."),
					FSlateIcon(),
					ToggleThisBreakpoint);
			}
			else
			{
				// Disable
				MenuBuilder.AddMenuEntry(
					LOCTEXT("DisableBreakpoint", "Disable breakpoint"),
					LOCTEXT("DisableBreakpoint_ToolTip", "Disable this breakpoint."),
					FSlateIcon(),
					ToggleThisBreakpoint);
			}
		}

		if ((Breakpoint != NULL) && (ParentBlueprint != NULL))
		{
			FUIAction ClearThisBreakpoint(
				FExecuteAction::CreateStatic( &FDebuggingActionCallbacks::ClearBreakpoint, Breakpoint, ParentBlueprint ),
				AlwaysAllowExecute
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ClearBreakpoint", "Remove breakpoint"),
				LOCTEXT("ClearBreakpoint_ToolTip", "Remove the breakpoint from this node."),
				FSlateIcon(),
				ClearThisBreakpoint);
		}
	}
protected:
	virtual TSharedRef<SWidget> GenerateNameWidget() override
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				. OnClicked(this, &FBreakpointLineItem::OnUserToggledEnabled)
				. ToolTipText(LOCTEXT("ToggleBreakpointButton_ToolTip", "Toggle this breakpoint"))
				. ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				. ContentPadding(0.0f)
				[
					SNew(SImage)
					. Image(this, &FBreakpointLineItem::GetStatusImage)
					. ToolTipText(this, &FBreakpointLineItem::GetStatusTooltip)
				]
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			. VAlign(VAlign_Center)
			[
				SNew(SHyperlink)
				. Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
				. Text(this, &FBreakpointLineItem::GetLocationDescription)
				. ToolTipText( LOCTEXT("NavBreakpointLoc", "Navigate to the breakpoint location") )
				. OnNavigate(this, &FBreakpointLineItem::OnNavigateToBreakpointLocation)
			];
	}
	
	
	FText GetLocationDescription() const;
	FReply OnUserToggledEnabled();

	void OnNavigateToBreakpointLocation();

	const FSlateBrush* GetStatusImage() const;
	FText GetStatusTooltip() const;
};

FText FBreakpointLineItem::GetLocationDescription() const
{
	if (UBreakpoint* MyBreakpoint = BreakpointRef.Get())
	{
		return MyBreakpoint->GetLocationDescription();
	}
	return FText::GetEmpty();
}

FReply FBreakpointLineItem::OnUserToggledEnabled()
{
	if (UBreakpoint* MyBreakpoint = BreakpointRef.Get())
	{
		FKismetDebugUtilities::SetBreakpointEnabled(MyBreakpoint, !MyBreakpoint->IsEnabledByUser());
	}
	return FReply::Handled();
}

void FBreakpointLineItem::OnNavigateToBreakpointLocation()
{
	if (UBreakpoint* MyBreakpoint = BreakpointRef.Get())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(MyBreakpoint->GetLocation());
	}
}

const FSlateBrush* FBreakpointLineItem::GetStatusImage() const
{
	if (UBreakpoint* MyBreakpoint = BreakpointRef.Get())
	{
		if (MyBreakpoint->IsEnabledByUser())
		{
			return FEditorStyle::GetBrush(FKismetDebugUtilities::IsBreakpointValid(MyBreakpoint) ? TEXT("Kismet.Breakpoint.EnabledAndValid") : TEXT("Kismet.Breakpoint.EnabledAndInvalid"));
		}
		else
		{
			return FEditorStyle::GetBrush(TEXT("Kismet.Breakpoint.Disabled"));
		}
	}

	return FEditorStyle::GetDefaultBrush();
}

FText FBreakpointLineItem::GetStatusTooltip() const
{
	if (UBreakpoint* MyBreakpoint = BreakpointRef.Get())
	{
		if (!FKismetDebugUtilities::IsBreakpointValid(MyBreakpoint))
		{
			return LOCTEXT("Breakpoint_NoHit", "This breakpoint will not be hit because its node generated no code");
		}
		else
		{
			return MyBreakpoint->IsEnabledByUser() ? LOCTEXT("ActiveBreakpoint", "Active breakpoint") : LOCTEXT("InactiveBreakpoint", "Inactive breakpoint");
		}
	}
	else
	{
		return LOCTEXT("NoBreakpoint", "No Breakpoint");
	}
}

//////////////////////////////////////////////////////////////////////////
// FParentLineItem

class FParentLineItem : public FDebugLineItem
{
protected:
	// The parent object
	TWeakObjectPtr<UObject> ObjectRef;

	// List of children 
	TArray<FDebugTreeItemPtr> ChildrenMirrors;
public:
	FParentLineItem(UObject* Object)
		: FDebugLineItem(DLT_Parent)
	{
		ObjectRef = Object;
	}

	virtual UObject* GetParentObject() override
	{
		return ObjectRef.Get();
	}

	virtual void GatherChildren(TArray<FDebugTreeItemPtr>& OutChildren) override
	{
		if (UObject* ParentObject = ObjectRef.Get())
		{
			UBlueprint* ParentBP = FDebugLineItem::GetBlueprintForObject(ParentObject);
			if ((ParentBP != NULL) && (ParentBP == ParentObject))
			{
				// Create children for each watch
				for (int32 WatchIndex = 0; WatchIndex < ParentBP->WatchedPins.Num(); ++WatchIndex)
				{
					UEdGraphPin* WatchedPin = ParentBP->WatchedPins[WatchIndex].Get();

					EnsureChildIsAdded(ChildrenMirrors, OutChildren, FWatchLineItem(WatchedPin, ParentObject));
				}

				// Create children for each breakpoint
				for (int32 BreakpointIndex = 0; BreakpointIndex < ParentBP->Breakpoints.Num(); ++BreakpointIndex)
				{
					UBreakpoint* Breakpoint = ParentBP->Breakpoints[BreakpointIndex];

					EnsureChildIsAdded(ChildrenMirrors, OutChildren, FBreakpointLineItem(Breakpoint, ParentObject));
				}

				// Make sure there is something there, to let the user know if there is nothing
				if (OutChildren.Num() == 0)
				{
					EnsureChildIsAdded(ChildrenMirrors, OutChildren, FMessageLineItem( LOCTEXT("NoWatchesOrBreakpoints", "No watches or breakpoints").ToString() ));
				}
			}
			else
			{
				if (ParentBP != NULL)
				{
					// Create children for each watch
					for (int32 WatchIndex = 0; WatchIndex < ParentBP->WatchedPins.Num(); ++WatchIndex)
					{
						UEdGraphPin* WatchedPin = ParentBP->WatchedPins[WatchIndex].Get();

						EnsureChildIsAdded(ChildrenMirrors, OutChildren, FWatchLineItem(WatchedPin, ParentObject));
					}

					// Create children for each breakpoint
					for (int32 BreakpointIndex = 0; BreakpointIndex < ParentBP->Breakpoints.Num(); ++BreakpointIndex)
					{
						UBreakpoint* Breakpoint = ParentBP->Breakpoints[BreakpointIndex];

						EnsureChildIsAdded(ChildrenMirrors, OutChildren, FBreakpointLineItem(Breakpoint, ParentObject));
					}
				}

				// It could also have active latent behaviors
				if (UWorld* World = GEngine->GetWorldFromContextObject(ParentObject, EGetWorldErrorMode::ReturnNull))
				{
					FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

					// Get the current list of action UUIDs
					TSet<int32> UUIDSet;
					LatentActionManager.GetActiveUUIDs(ParentObject, /*inout*/ UUIDSet);

					// Add the new ones
					for (TSet<int32>::TConstIterator RemainingIt(UUIDSet); RemainingIt; ++RemainingIt)
					{
						const int32 UUID = *RemainingIt;
						EnsureChildIsAdded(ChildrenMirrors, OutChildren, FLatentActionLineItem(UUID, ParentObject));
					}
				}

				// Make sure there is something there, to let the user know if there is nothing
				if (OutChildren.Num() == 0)
				{
					EnsureChildIsAdded(ChildrenMirrors, OutChildren, FMessageLineItem( LOCTEXT("NoDebugInfo", "No debugging info").ToString() ));
				}
			}
			//@TODO: try to get at TArray<struct FDebugDisplayProperty> DebugProperties in UGameViewportClient, if available
		}
	}

protected:
	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		FParentLineItem* Other = (FParentLineItem*)BaseOther;
		return ObjectRef.Get() == Other->ObjectRef.Get();
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		return new FParentLineItem(ObjectRef.Get());
	}

	virtual FText GetDisplayName() const override
	{
		UObject* Object = ObjectRef.Get();
		AActor* Actor = Cast<AActor>(Object);

		if (Actor != NULL)
		{
			 return FText::FromString(Actor->GetActorLabel());
		}
		else
		{
			return (Object != NULL) ? FText::FromString(Object->GetName()) : LOCTEXT("Null", "(null)");
		}
	}

	virtual void MakeMenu(class FMenuBuilder& MenuBuilder) override
	{
		if (UBlueprint* BP = Cast<UBlueprint>(ObjectRef.Get()))
		{
			if (BP->WatchedPins.Num() > 0)
			{
				FUIAction ClearAllWatches(
					FExecuteAction::CreateStatic( &FDebuggingActionCallbacks::ClearWatches, BP )
					);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("ClearWatches", "Clear all watches"),
					LOCTEXT("ClearWatches_ToolTip", "Clear all watches in this blueprint"),
					FSlateIcon(),
					ClearAllWatches);
			}

			if (BP->Breakpoints.Num() > 0)
			{
				FUIAction ClearAllBreakpoints(
					FExecuteAction::CreateStatic( &FDebuggingActionCallbacks::ClearBreakpoints, BP )
					);

				MenuBuilder.AddMenuEntry(
					LOCTEXT("ClearBreakpoints", "Remove all breakpoints"),
					LOCTEXT("ClearBreakpoints_ToolTip", "Clear all breakpoints in this blueprint"),
					FSlateIcon(),
					ClearAllBreakpoints);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FTraceStackChildItem

class FTraceStackChildItem : public FDebugLineItem
{
protected:
	int32 StackIndex;
public:
	FTraceStackChildItem(int32 InStackIndex)
		: FDebugLineItem(DLT_TraceStackChild)
	{
		StackIndex = InStackIndex;
	}
protected:
	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		check(false);
		return false;
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		check(false);
		return NULL;
	}

	UEdGraphNode* GetNode() const
	{
		const TSimpleRingBuffer<FKismetTraceSample>& TraceStack = FKismetDebugUtilities::GetTraceStack();
		if (StackIndex < TraceStack.Num())
		{
			const FKismetTraceSample& Sample = TraceStack(StackIndex);
			UObject* ObjectContext = Sample.Context.Get();

			FString ContextName = (ObjectContext != NULL) ? ObjectContext->GetName() : LOCTEXT("ObjectDoesNotExist", "(object no longer exists)").ToString();
			FString NodeName = TEXT(" ");

			if (ObjectContext != NULL)
			{
				// Try to find the node that got executed
				UEdGraphNode* Node = FKismetDebugUtilities::FindSourceNodeForCodeLocation(ObjectContext, Sample.Function.Get(), Sample.Offset);
				return Node;
			}
		}

		return NULL;
	}

	virtual FText GetDisplayName() const override
	{
		UEdGraphNode* Node = GetNode();
		if (Node != NULL)
		{
			return Node->GetNodeTitle(ENodeTitleType::ListView);
		}
		else
		{
			return LOCTEXT("Unknown", "(unknown)");
		}
	}

	// Index icon and node name
	virtual TSharedRef<SWidget> GenerateNameWidget() override
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage)
				. Image( FEditorStyle::GetBrush((StackIndex > 0) ? TEXT("Kismet.Trace.PreviousIndex") : TEXT("Kismet.Trace.CurrentIndex")) )
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			. VAlign(VAlign_Center)
			[
				SNew(SHyperlink)
				. Text(this, &FTraceStackChildItem::GetDisplayName)
				. Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
				. ToolTipText(LOCTEXT("NavigateToDebugTraceLocationHyperlink_ToolTip", "Navigate to the trace location"))
				. OnNavigate(this, &FTraceStackChildItem::OnNavigateToNode)
			];
	}

	// Visit time and actor name
	virtual TSharedRef<SWidget> GenerateValueWidget() override
	{
		return SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SHyperlink)
				. Text(this, &FTraceStackChildItem::GetContextObjectName)
				. Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
				. ToolTipText( LOCTEXT("SelectActor_Tooltip", "Select this actor") )
				. OnNavigate(this, &FTraceStackChildItem::OnSelectContextObject)
			]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(this, &FTraceStackChildItem::GetVisitTime)
			];
	}

	FText GetVisitTime() const
	{
		const TSimpleRingBuffer<FKismetTraceSample>& TraceStack = FKismetDebugUtilities::GetTraceStack();
		if (StackIndex < TraceStack.Num())
		{
			static const FNumberFormattingOptions TimeFormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(2)
				.SetMaximumFractionalDigits(2);
			return FText::Format(LOCTEXT("VisitTimeFmt", " @ {0} s"), FText::AsNumber(TraceStack(StackIndex).ObservationTime - GStartTime, &TimeFormatOptions));
		}

		return FText::GetEmpty();
	}

	FText GetContextObjectName() const
	{
		const TSimpleRingBuffer<FKismetTraceSample>& TraceStack = FKismetDebugUtilities::GetTraceStack();

		UObject* ObjectContext = (StackIndex < TraceStack.Num()) ? TraceStack(StackIndex).Context.Get() : NULL;
		
		return (ObjectContext != NULL) ? FText::FromString(ObjectContext->GetName()) : LOCTEXT("ObjectDoesNotExist", "(object no longer exists)");
	}

	void OnNavigateToNode()
	{
		if (UEdGraphNode* Node = GetNode())
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
		}		
	}

	void OnSelectContextObject()
	{
		const TSimpleRingBuffer<FKismetTraceSample>& TraceStack = FKismetDebugUtilities::GetTraceStack();

		UObject* ObjectContext = (StackIndex < TraceStack.Num()) ? TraceStack(StackIndex).Context.Get() : NULL;

		// Add the object to the selection set
		if (AActor* Actor = Cast<AActor>(ObjectContext))
		{
			GEditor->SelectActor(Actor, true, true, true);
		}
		else
		{
			UE_LOG(LogBlueprintDebuggingView, Warning, TEXT("Cannot select the non-actor object '%s'"), (ObjectContext != NULL) ? *ObjectContext->GetName() : TEXT("(null)") );
		}
	}

};

//////////////////////////////////////////////////////////////////////////
// FTraceStackParentItem

class FTraceStackParentItem : public FDebugLineItem
{
protected:
	// List of children 
	TArray< TSharedPtr<FTraceStackChildItem> > ChildrenMirrors;
public:
	FTraceStackParentItem()
		: FDebugLineItem(DLT_TraceStackParent)
	{
	}

	virtual void GatherChildren(TArray<FDebugTreeItemPtr>& OutChildren) override
	{
		const TSimpleRingBuffer<FKismetTraceSample>& TraceStack = FKismetDebugUtilities::GetTraceStack();
		const int32 NumVisible = TraceStack.Num();

		// Create any new stack entries that are needed
		for (int32 i = ChildrenMirrors.Num(); i < NumVisible; ++i)
		{
			ChildrenMirrors.Add(MakeShareable( new FTraceStackChildItem(i) ));
		}

		// Add the visible stack entries as children
		for (int32 i = 0; i < NumVisible; ++i)
		{
			OutChildren.Add(ChildrenMirrors[i]);
		}
	}
protected:
	virtual FText GetDisplayName() const override
	{
		return LOCTEXT("ExecutionTrace", "Execution Trace");
	}

	virtual bool Compare(const FDebugLineItem* BaseOther) const override
	{
		check(false);
		return false;
	}

	virtual FDebugLineItem* Duplicate() const override
	{
		check(false);
		return NULL;
	}
};

//////////////////////////////////////////////////////////////////////////
// SDebugLineItem

class SDebugLineItem : public SMultiColumnTableRow< FDebugTreeItemPtr >
{
protected:
	FDebugTreeItemPtr ItemToEdit;
public:
	SLATE_BEGIN_ARGS(SDebugLineItem){}
	SLATE_END_ARGS()

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == KismetDebugViewConstants::ColumnId_Name)
		{
			return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SExpanderArrow, SharedThis(this) )
				]
			+SHorizontalBox::Slot()
				. FillWidth(1.0f)
				[
					ItemToEdit->GenerateNameWidget()
				];
		}
		else if (ColumnName == KismetDebugViewConstants::ColumnId_Value)
		{
			return ItemToEdit->GenerateValueWidget();
		}
		else
		{
			return SNew(STextBlock)
				. Text( LOCTEXT("Error", "Error") );
		}
	}

	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, FDebugTreeItemPtr InItemToEdit)
	{
		ItemToEdit = InItemToEdit;
		SMultiColumnTableRow<FDebugTreeItemPtr>::Construct( FSuperRowType::FArguments(), OwnerTableView );	
	}
};


//////////////////////////////////////////////////////////////////////////
// SKismetDebuggingView

TSharedRef<ITableRow> SKismetDebuggingView::OnGenerateRowForWatchTree(FDebugTreeItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( SDebugLineItem, OwnerTable, InItem );
}

void SKismetDebuggingView::OnGetChildrenForWatchTree(FDebugTreeItemPtr InParent, TArray<FDebugTreeItemPtr>& OutChildren)
{
	InParent->GatherChildren(OutChildren);
}

TSharedPtr<SWidget> SKismetDebuggingView::OnMakeContextMenu()
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection("DebugActions", LOCTEXT("DebugActionsMenuHeading", "Debug Actions"));
	{
		const TArray<FDebugTreeItemPtr> SelectionList = DebugTreeView->GetSelectedItems();

		for (int32 SelIndex = 0; SelIndex < SelectionList.Num(); ++SelIndex)
		{
			FDebugTreeItemPtr Ptr = SelectionList[SelIndex];
			Ptr->MakeMenu(MenuBuilder);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText SKismetDebuggingView::GetTopText() const
{
	const bool bIsDebugging = GEditor->PlayWorld != nullptr;
	UBlueprint* BlueprintObj = BlueprintToWatchPtr.Get();
	UObject* DebuggedObject = (BlueprintObj != nullptr) ? BlueprintObj->GetObjectBeingDebugged() : nullptr;

	TSet<UObject*> NewRootSet;
	if (bIsDebugging && (BlueprintObj != nullptr) && (DebuggedObject != nullptr))
	{
		return FText::Format(LOCTEXT("ShowDebugForObject", "Showing debug info for {0}"), FText::FromName(DebuggedObject->GetFName()));
	}
	else if (!bIsDebugging && (BlueprintObj != nullptr))
	{
		return LOCTEXT("ShowDebugForBlueprint", "Showing debug info for this blueprint");
	}
	else
	{
		return LOCTEXT("ShowDebugForActors", "Showing debug info for selected actors");
	}
}

EVisibility SKismetDebuggingView::IsDebuggerVisible() const
{
	return (GEditor->PlayWorld != NULL) ? EVisibility::Visible : EVisibility::Collapsed;
}

void SKismetDebuggingView::Construct(const FArguments& InArgs)
{	
	BlueprintToWatchPtr = InArgs._BlueprintToWatch;

	// Build the debug toolbar
	FToolBarBuilder DebugToolbarBuilder(FPlayWorldCommands::GlobalPlayWorldActions, FMultiBoxCustomization::None );
	FPlayWorldCommands::BuildToolbar(DebugToolbarBuilder);

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.Visibility( this, &SKismetDebuggingView::IsDebuggerVisible )
			.BorderImage( FEditorStyle::GetBrush( TEXT("NoBorder") ) )
			[
				DebugToolbarBuilder.MakeWidget()
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text( this, &SKismetDebuggingView::GetTopText )
		]
		+SVerticalBox::Slot()
		. FillHeight(1.0f)
		[
			SAssignNew( DebugTreeView, STreeView< FDebugTreeItemPtr > )
			.TreeItemsSource( &RootTreeItems )
			.SelectionMode( ESelectionMode::Single )
			.OnGetChildren( this, &SKismetDebuggingView::OnGetChildrenForWatchTree )
			.OnGenerateRow( this, &SKismetDebuggingView::OnGenerateRowForWatchTree ) 
			.OnContextMenuOpening( this, &SKismetDebuggingView::OnMakeContextMenu )
			. HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(KismetDebugViewConstants::ColumnId_Name)
				.DefaultLabel(KismetDebugViewConstants::ColumnText_Name)
				+ SHeaderRow::Column(KismetDebugViewConstants::ColumnId_Value)
				.DefaultLabel(KismetDebugViewConstants::ColumnText_Value)
			)
		]
	];

	TraceStackItem = MakeShareable(new FTraceStackParentItem);
}

void SKismetDebuggingView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Gather the old root set
	TSet<UObject*> OldRootSet;
	for (int32 i = 0; i < RootTreeItems.Num(); ++i)
	{
		if (UObject* OldObject = RootTreeItems[i]->GetParentObject())
		{
			OldRootSet.Add(OldObject);
		}
	}

	// Gather what we'd like to be the new root set
	const bool bIsDebugging = GEditor->PlayWorld != nullptr;
	UBlueprint* BlueprintObj = BlueprintToWatchPtr.Get();
	UObject* DebuggedObject = (BlueprintObj != nullptr) ? BlueprintObj->GetObjectBeingDebugged() : nullptr;

	TSet<UObject*> NewRootSet;
	if (bIsDebugging && (BlueprintObj != nullptr) && (DebuggedObject != nullptr))
	{
		// If we have a currently debugged object & we were launched from a specific Kismet window, display the debugged object
		NewRootSet.Add(DebuggedObject);
	}
	else if (!bIsDebugging && (BlueprintObj != nullptr))
	{
		// If not debugging and summoned from a specific Kismet window, just display the currently open blueprint
		NewRootSet.Add(BlueprintObj);
	}
	else
	{
		// Get the set of objects being debugged
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for (FObjectsBeingDebuggedIterator Iter; Iter; ++Iter)
		{
			NewRootSet.Add(*Iter);
		}
	}

	// This will pull anything out of Old that is also New (sticking around), so afterwards Old is a list of things to remove
	RootTreeItems.Empty();
	for (TSet<UObject*>::TIterator NewIter(NewRootSet); NewIter; ++NewIter)
	{
		UObject* ObjectToAdd = *NewIter;

		if (OldRootSet.Contains(ObjectToAdd))
		{
			OldRootSet.Remove(ObjectToAdd);
			RootTreeItems.Add(ObjectToTreeItemMap.FindChecked(ObjectToAdd));
		}
		else
		{
			FDebugTreeItemPtr NewPtr = FDebugTreeItemPtr(new FParentLineItem(ObjectToAdd));
			ObjectToTreeItemMap.Add(ObjectToAdd, NewPtr);
			RootTreeItems.Add(NewPtr);

			// Autoexpand newly selected items
			DebugTreeView->SetItemExpansion(NewPtr, true);
		}
	}

	// Remove the old root set items that didn't get used again
	for (TSet<UObject*>::TIterator DeadIter(OldRootSet); DeadIter; ++DeadIter)
	{
		UObject* ObjectToRemove = *DeadIter;
		ObjectToTreeItemMap.Remove(ObjectToRemove);
	}

	// Add a message if there are no actors selected
	if (RootTreeItems.Num() == 0)
	{
		RootTreeItems.Add(MakeShareable(new FMessageLineItem(
			bIsDebugging ? LOCTEXT("NoActorsSelected", "No actors selected").ToString() : LOCTEXT("NoPIEorSIE", "Not running PIE or SIE").ToString())));
	}

	// Show the trace stack when debugging
	if (bIsDebugging)
	{
		RootTreeItems.Add(TraceStackItem);
	}

	// Refresh the list
	DebugTreeView->RequestTreeRefresh();
}


#undef LOCTEXT_NAMESPACE
