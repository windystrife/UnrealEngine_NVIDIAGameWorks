// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdGraphNode_Reference.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformFilemanager.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"

//////////////////////////////////////////////////////////////////////////
// UEdGraphNode_Reference

UEdGraphNode_Reference::UEdGraphNode_Reference(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DependencyPin = NULL;
	ReferencerPin = NULL;
	bIsCollapsed = false;
	bIsPackage = false;
	bIsPrimaryAsset = false;
	bUsesThumbnail = false;
}

void UEdGraphNode_Reference::SetupReferenceNode(const FIntPoint& NodeLoc, const TArray<FAssetIdentifier>& NewIdentifiers, const FAssetData& InAssetData)
{
	check(NewIdentifiers.Num() > 0);

	NodePosX = NodeLoc.X;
	NodePosY = NodeLoc.Y;

	Identifiers = NewIdentifiers;
	const FAssetIdentifier& First = NewIdentifiers[0];
	FString ShortPackageName = FPackageName::GetLongPackageAssetName(First.PackageName.ToString());

	bIsCollapsed = false;
	bIsPackage = true;
	
	FPrimaryAssetId PrimaryAssetID = NewIdentifiers[0].GetPrimaryAssetId();
	if (PrimaryAssetID.IsValid())
	{
		ShortPackageName = PrimaryAssetID.ToString();
		bIsPackage = false;
		bIsPrimaryAsset = true;
	}
	else if (NewIdentifiers[0].IsValue())
	{
		ShortPackageName = FString::Printf(TEXT("%s::%s"), *First.ObjectName.ToString(), *First.ValueName.ToString());
		bIsPackage = false;
	}

	if (NewIdentifiers.Num() == 1 )
	{
		if (bIsPackage)
		{
			NodeComment = First.PackageName.ToString();
		}
		NodeTitle = FText::FromString(ShortPackageName);
	}
	else
	{
		NodeComment = FText::Format(LOCTEXT("ReferenceNodeMultiplePackagesTitle", "{0} nodes"), FText::AsNumber(NewIdentifiers.Num())).ToString();
		NodeTitle = FText::Format(LOCTEXT("ReferenceNodeMultiplePackagesComment", "{0} and {1} others"), FText::FromString(ShortPackageName), FText::AsNumber(NewIdentifiers.Num() - 1));
	}
	
	CacheAssetData(InAssetData);
	AllocateDefaultPins();
}

void UEdGraphNode_Reference::SetReferenceNodeCollapsed(const FIntPoint& NodeLoc, int32 InNumReferencesExceedingMax)
{
	NodePosX = NodeLoc.X;
	NodePosY = NodeLoc.Y;

	Identifiers.Empty();
	bIsCollapsed = true;
	bUsesThumbnail = false;
	NodeComment = FText::Format(LOCTEXT("ReferenceNodeCollapsedMessage", "{0} other nodes"), FText::AsNumber(InNumReferencesExceedingMax)).ToString();

	NodeTitle = LOCTEXT("ReferenceNodeCollapsedTitle", "Collapsed nodes");
	CacheAssetData(FAssetData());
	AllocateDefaultPins();
}

void UEdGraphNode_Reference::AddReferencer(UEdGraphNode_Reference* ReferencerNode)
{
	UEdGraphPin* ReferencerDependencyPin = ReferencerNode->GetDependencyPin();

	if ( ensure(ReferencerDependencyPin) )
	{
		ReferencerDependencyPin->bHidden = false;
		ReferencerPin->bHidden = false;
		ReferencerPin->MakeLinkTo(ReferencerDependencyPin);
	}
}

FAssetIdentifier UEdGraphNode_Reference::GetIdentifier() const
{
	if (Identifiers.Num() > 0)
	{
		return Identifiers[0];
	}

	return FAssetIdentifier();
}

void UEdGraphNode_Reference::GetAllIdentifiers(TArray<FAssetIdentifier>& OutIdentifiers) const
{
	OutIdentifiers.Append(Identifiers);
}

void UEdGraphNode_Reference::GetAllPackageNames(TArray<FName>& OutPackageNames) const
{
	for (const FAssetIdentifier& AssetId : Identifiers)
	{
		if (AssetId.IsPackage())
		{
			OutPackageNames.AddUnique(AssetId.PackageName);
		}
	}
}

UEdGraph_ReferenceViewer* UEdGraphNode_Reference::GetReferenceViewerGraph() const
{
	return Cast<UEdGraph_ReferenceViewer>( GetGraph() );
}

FText UEdGraphNode_Reference::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NodeTitle;
}

FLinearColor UEdGraphNode_Reference::GetNodeTitleColor() const
{
	if (bIsPrimaryAsset)
	{
		return FLinearColor(0.2f, 0.8f, 0.2f);
	}
	else if (bIsPackage)
	{
		return FLinearColor(0.4f, 0.62f, 1.0f);
	}
	else if (bIsCollapsed)
	{
		return FLinearColor(0.55f, 0.55f, 0.55f);
	}
	else 
	{
		return FLinearColor(0.0f, 0.55f, 0.62f);
	}
}

FText UEdGraphNode_Reference::GetTooltipText() const
{
	FString TooltipString;
	for (const FAssetIdentifier& AssetId : Identifiers)
	{
		if (!TooltipString.IsEmpty())
		{
			TooltipString.Append(TEXT("\n"));
		}
		TooltipString.Append(AssetId.ToString());
	}
	return FText::FromString(TooltipString);
}

void UEdGraphNode_Reference::AllocateDefaultPins()
{
	ReferencerPin = CreatePin( EEdGraphPinDirection::EGPD_Input, FString(), FString(), nullptr, FString() );
	DependencyPin = CreatePin( EEdGraphPinDirection::EGPD_Output, FString(), FString(), nullptr, FString() );

	ReferencerPin->bHidden = true;
	DependencyPin->bHidden = true;
}

UObject* UEdGraphNode_Reference::GetJumpTargetForDoubleClick() const
{
	if (Identifiers.Num() > 0 )
	{
		GetReferenceGraph()->SetGraphRoot(Identifiers, FIntPoint(NodePosX, NodePosY));
		GetReferenceGraph()->RebuildGraph();
	}
	return NULL;
}

UEdGraphPin* UEdGraphNode_Reference::GetDependencyPin()
{
	return DependencyPin;
}

UEdGraphPin* UEdGraphNode_Reference::GetReferencerPin()
{
	return ReferencerPin;
}

void UEdGraphNode_Reference::CacheAssetData(const FAssetData& AssetData)
{
	if ( AssetData.IsValid() && IsPackage() )
	{
		bUsesThumbnail = true;
		CachedAssetData = AssetData;
	}
	else
	{
		CachedAssetData = FAssetData();
		bUsesThumbnail = false;

		if (Identifiers.Num() == 1 )
		{
			const FString PackageNameStr = Identifiers[0].PackageName.ToString();
			if ( FPackageName::IsValidLongPackageName(PackageNameStr, true) )
			{
				if ( PackageNameStr.StartsWith(TEXT("/Script")) )
				{
					CachedAssetData.AssetClass = FName(TEXT("Code"));
				}
				else
				{
					const FString PotentiallyMapFilename = FPackageName::LongPackageNameToFilename(PackageNameStr, FPackageName::GetMapPackageExtension());
					const bool bIsMapPackage = FPlatformFileManager::Get().GetPlatformFile().FileExists(*PotentiallyMapFilename);
					if ( bIsMapPackage )
					{
						CachedAssetData.AssetClass = FName(TEXT("World"));
					}
				}
			}
		}
		else
		{
			CachedAssetData.AssetClass = FName(TEXT("Multiple Nodes"));
		}
	}

}

FAssetData UEdGraphNode_Reference::GetAssetData() const
{
	return CachedAssetData;
}

bool UEdGraphNode_Reference::UsesThumbnail() const
{
	return bUsesThumbnail;
}

bool UEdGraphNode_Reference::IsPackage() const
{
	return bIsPackage;
}

bool UEdGraphNode_Reference::IsCollapsed() const
{
	return bIsCollapsed;
}

#undef LOCTEXT_NAMESPACE
