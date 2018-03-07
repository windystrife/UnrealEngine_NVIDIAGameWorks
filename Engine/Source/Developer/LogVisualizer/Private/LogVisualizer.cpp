// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "EngineGlobals.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "LogVisualizerSettings.h"
#include "VisualLoggerDatabase.h"
#include "VisualLoggerCameraController.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"
#endif

TSharedPtr< struct FLogVisualizer > FLogVisualizer::StaticInstance;
FColor FLogVisualizerColorPalette[] = {
	FColor(0xff00A480),
	FColorList::Aquamarine,
	FColorList::Cyan,
	FColorList::Brown,
	FColorList::Green,
	FColorList::Orange,
	FColorList::Magenta,
	FColorList::BrightGold,
	FColorList::NeonBlue,
	FColorList::MediumSlateBlue,
	FColorList::SpicyPink,
	FColorList::SpringGreen,
	FColorList::SteelBlue,
	FColorList::SummerSky,
	FColorList::Violet,
	FColorList::VioletRed,
	FColorList::YellowGreen,
	FColor(0xff62E200),
	FColor(0xff1F7B67),
	FColor(0xff62AA2A),
	FColor(0xff70227E),
	FColor(0xff006B53),
	FColor(0xff409300),
	FColor(0xff5D016D),
	FColor(0xff34D2AF),
	FColor(0xff8BF13C),
	FColor(0xffBC38D3),
	FColor(0xff5ED2B8),
	FColor(0xffA6F16C),
	FColor(0xffC262D3),
	FColor(0xff0F4FA8),
	FColor(0xff00AE68),
	FColor(0xffDC0055),
	FColor(0xff284C7E),
	FColor(0xff21825B),
	FColor(0xffA52959),
	FColor(0xff05316D),
	FColor(0xff007143),
	FColor(0xff8F0037),
	FColor(0xff4380D3),
	FColor(0xff36D695),
	FColor(0xffEE3B80),
	FColor(0xff6996D3),
	FColor(0xff60D6A7),
	FColor(0xffEE6B9E)
};


void FLogVisualizer::Initialize()
{
	StaticInstance = MakeShareable(new FLogVisualizer);
	Get().TimeSliderController = MakeShareable(new FVisualLoggerTimeSliderController(FVisualLoggerTimeSliderArgs()));
}

void FLogVisualizer::Shutdown()
{
	StaticInstance.Reset();
}

void FLogVisualizer::Reset()
{
	TimeSliderController->SetTimesliderArgs(FVisualLoggerTimeSliderArgs());
}

FLogVisualizer& FLogVisualizer::Get()
{
	return *StaticInstance;
}

FLinearColor FLogVisualizer::GetColorForCategory(int32 Index) const
{
	if (Index >= 0 && Index < sizeof(FLogVisualizerColorPalette) / sizeof(FLogVisualizerColorPalette[0]))
	{
		return FLogVisualizerColorPalette[Index];
	}

	static bool bReateColorList = false;
	static FColorList StaticColor;
	if (!bReateColorList)
	{
		bReateColorList = true;
		StaticColor.CreateColorMap();
	}
	return StaticColor.GetFColorByIndex(Index);
}

FLinearColor FLogVisualizer::GetColorForCategory(const FString& InFilterName) const
{
	static TArray<FString> Filters;
	int32 CategoryIndex = Filters.Find(InFilterName);
	if (CategoryIndex == INDEX_NONE)
	{
		CategoryIndex = Filters.Add(InFilterName);
	}

	return GetColorForCategory(CategoryIndex);
}

UWorld* FLogVisualizer::GetWorld(UObject* OptionalObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(OptionalObject, EGetWorldErrorMode::ReturnNull);
#if WITH_EDITOR
	if (!World && GIsEditor)
	{
		UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
		// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
		World = EEngine != nullptr && EEngine->PlayWorld != nullptr ? EEngine->PlayWorld : EEngine->GetEditorWorldContext().World();
	}
	else 
#endif
	if (!World && !GIsEditor)
	{
		World = GEngine->GetWorld();
	}

	if (World == nullptr)
	{
		World = GWorld;
	}

	return World;
}

void FLogVisualizer::UpdateCameraPosition(FName RowName, int32 ItemIndes)
{
	const FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	auto &Entries = DBRow.GetItems();
	if (DBRow.GetCurrentItemIndex() == INDEX_NONE || Entries.IsValidIndex(DBRow.GetCurrentItemIndex()) == false)
	{
		return;
	}

	UWorld* World = GetWorld();
	
	FVector CurrentLocation = Entries[DBRow.GetCurrentItemIndex()].Entry.Location;

	FVector Extent(150);
	bool bFoundActor = false;
	FName OwnerName = Entries[DBRow.GetCurrentItemIndex()].OwnerName;
	for (FActorIterator It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetFName() == OwnerName)
		{
			FVector Orgin;
			Actor->GetActorBounds(false, Orgin, Extent);
			bFoundActor = true;
			break;
		}
	}


	const float DefaultCameraDistance = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->DefaultCameraDistance;
	Extent = Extent.SizeSquared() < FMath::Square(DefaultCameraDistance) ? FVector(DefaultCameraDistance) : Extent;

#if WITH_EDITOR
	UEditorEngine *EEngine = Cast<UEditorEngine>(GEngine);
	if (GIsEditor && EEngine != NULL)
	{
		for (auto ViewportClient : EEngine->AllViewportClients)
		{
			ViewportClient->FocusViewportOnBox(FBox::BuildAABB(CurrentLocation, Extent));
		}
	}
	else if (AVisualLoggerCameraController::IsEnabled(World) && AVisualLoggerCameraController::Instance.IsValid() && AVisualLoggerCameraController::Instance->GetSpectatorPawn())
	{
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(AVisualLoggerCameraController::Instance->Player);
		if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport)
		{

			FViewport* Viewport = LocalPlayer->ViewportClient->Viewport;

			FBox BoundingBox = FBox::BuildAABB(CurrentLocation, Extent);
			const FVector Position = BoundingBox.GetCenter();
			float Radius = BoundingBox.GetExtent().Size();
			
			FViewportCameraTransform ViewTransform;
			ViewTransform.TransitionToLocation(Position, nullptr, true);

			float NewOrthoZoom;
			const float AspectRatio = 1.777777f;
			CA_SUPPRESS(6326);
			uint32 MinAxisSize = (AspectRatio > 1.0f) ? Viewport->GetSizeXY().Y : Viewport->GetSizeXY().X;
			float Zoom = Radius / (MinAxisSize / 2.0f);

			NewOrthoZoom = Zoom * (Viewport->GetSizeXY().X*15.0f);
			NewOrthoZoom = FMath::Clamp<float>(NewOrthoZoom, 250, MAX_FLT);
			ViewTransform.SetOrthoZoom(NewOrthoZoom);

			AVisualLoggerCameraController::Instance->GetSpectatorPawn()->TeleportTo(ViewTransform.GetLocation(), ViewTransform.GetRotation(), false, true);
		}
	}
#endif
}

int32 FLogVisualizer::GetNextItem(FName RowName, int32 MoveDistance)
{
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	int32 NewItemIndex = DBRow.GetCurrentItemIndex();

	int32 Index = 0;
	auto &Entries = DBRow.GetItems();
	while (true)
	{
		NewItemIndex++;
		if (Entries.IsValidIndex(NewItemIndex))
		{
			if (DBRow.IsItemVisible(NewItemIndex) == true && ++Index == MoveDistance)
			{
				break;
			}
		}
		else
		{
			NewItemIndex = FMath::Clamp(NewItemIndex, 0, Entries.Num() - 1);
			break;
		}
	}

	return NewItemIndex;
}

int32 FLogVisualizer::GetPreviousItem(FName RowName, int32 MoveDistance)
{
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	int32 NewItemIndex = DBRow.GetCurrentItemIndex();

	int32 Index = 0;
	auto &Entries = DBRow.GetItems();
	while (true)
	{
		NewItemIndex--;
		if (Entries.IsValidIndex(NewItemIndex))
		{
			if (DBRow.IsItemVisible(NewItemIndex) == true && ++Index == MoveDistance)
			{
				break;
			}
		}
		else
		{
			NewItemIndex = FMath::Clamp(NewItemIndex, 0, Entries.Num() - 1);
			break;
		}
	}
	return NewItemIndex;
}

void FLogVisualizer::GotoNextItem(FName RowName, int32 MoveDistance)
{
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	const int32 NewItemIndex = GetNextItem(RowName, MoveDistance);

	if (NewItemIndex != DBRow.GetCurrentItemIndex())
	{
		auto &Entries = DBRow.GetItems();
		float NewTimeStamp = Entries[NewItemIndex].Entry.TimeStamp;
	}
}

void FLogVisualizer::GotoPreviousItem(FName RowName, int32 MoveDistance)
{
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	const int32 NewItemIndex = GetPreviousItem(RowName, MoveDistance);

	if (NewItemIndex != DBRow.GetCurrentItemIndex())
	{
		auto &Entries = DBRow.GetItems();
		float NewTimeStamp = Entries[NewItemIndex].Entry.TimeStamp;
	}
}

void FLogVisualizer::GotoFirstItem(FName RowName)
{
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	int32 NewItemIndex = DBRow.GetCurrentItemIndex();

	auto &Entries = DBRow.GetItems();
	for (int32 Index = 0; Index <= DBRow.GetCurrentItemIndex(); Index++)
	{
		if (DBRow.IsItemVisible(Index))
		{
			NewItemIndex = Index;
			break;
		}
	}

	if (NewItemIndex != DBRow.GetCurrentItemIndex())
	{
		//DBRow.MoveTo(NewItemIndex);
		TimeSliderController->CommitScrubPosition(Entries[NewItemIndex].Entry.TimeStamp, false);
	}
}

void FLogVisualizer::GotoLastItem(FName RowName)
{
	FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
	int32 NewItemIndex = DBRow.GetCurrentItemIndex();

	auto &Entries = DBRow.GetItems();
	for (int32 Index = Entries.Num() - 1; Index >= DBRow.GetCurrentItemIndex(); Index--)
	{
		if (DBRow.IsItemVisible(Index))
		{
			NewItemIndex = Index;
			break;
		}
	}


	if (NewItemIndex != DBRow.GetCurrentItemIndex())
	{
		//DBRow.MoveTo(NewItemIndex);
		TimeSliderController->CommitScrubPosition(Entries[NewItemIndex].Entry.TimeStamp, false);
	}
}

