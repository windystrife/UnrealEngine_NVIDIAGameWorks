// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DependsNode.h"
#include "AssetRegistryPrivate.h"

void FDependsNode::PrintNode() const
{
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Printing DependsNode: %s ***"), *Identifier.ToString());
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Dependencies:"));
	PrintDependencies();
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Referencers:"));
	PrintReferencers();
}

void FDependsNode::PrintDependencies() const
{
	TSet<const FDependsNode*> VisitedNodes;

	PrintDependenciesRecursive(TEXT(""), VisitedNodes);
}

void FDependsNode::PrintReferencers() const
{
	TSet<const FDependsNode*> VisitedNodes;

	PrintReferencersRecursive(TEXT(""), VisitedNodes);
}

void FDependsNode::GetDependencies(TArray<FDependsNode*>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	IterateOverDependencies([&OutDependencies](FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
	{
		OutDependencies.Add(InDependency);
	}, 
	InDependencyType);
}

void FDependsNode::GetDependencies(TArray<FAssetIdentifier>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	IterateOverDependencies([&OutDependencies](const FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
	{
		OutDependencies.Add(InDependency->GetIdentifier());
	},
	InDependencyType);
}

void FDependsNode::GetReferencers(TArray<FDependsNode*>& OutReferencers, EAssetRegistryDependencyType::Type InDependencyType) const
{
	for (FDependsNode* Referencer : Referencers)
	{
		bool bShouldAdd = false;
		// If type specified, filter
		if (InDependencyType != EAssetRegistryDependencyType::All)
		{
			IterateOverDependencies([&bShouldAdd,this](const FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
			{
				if (InDependency == this)
				{
					bShouldAdd = true;
				}
			}, InDependencyType);
		}
		else
		{
			bShouldAdd = true;
		}

		if (bShouldAdd)
		{
			OutReferencers.Add(Referencer);
		}
	}
}

void FDependsNode::AddDependency(FDependsNode* InDependency, EAssetRegistryDependencyType::Type InDependencyType, bool bGuaranteedUnique)
{
	IterateOverDependencyArrays([InDependency,&bGuaranteedUnique](TArray<FDependsNode*>& InArray, EAssetRegistryDependencyType::Type)
	{
		if (bGuaranteedUnique)
		{
			InArray.Add(InDependency);
		}
		else
		{
			InArray.AddUnique(InDependency);
		}
	}, InDependencyType);
}

void FDependsNode::RemoveDependency(FDependsNode* InDependency)
{
	IterateOverDependencyArrays([InDependency](TArray<FDependsNode*>& InArray, EAssetRegistryDependencyType::Type)
	{
		InArray.Remove(InDependency);
	});
}

void FDependsNode::ClearDependencies()
{
	IterateOverDependencyArrays([](TArray<FDependsNode*>& InArray, EAssetRegistryDependencyType::Type)
	{
		InArray.Empty();
	});
}

void FDependsNode::RemoveManageReferencesToNode()
{
	EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::Manage;
	// Iterate referencers array, possibly removing 
	for (int32 i = Referencers.Num() - 1; i >= 0; i--)
	{
		bool bStillExists = false;
		Referencers[i]->IterateOverDependencyArrays([InDependencyType, this, &bStillExists](TArray<FDependsNode*>& InArray, EAssetRegistryDependencyType::Type CurrentType)
		{
			int32 FoundIndex = InArray.Find(this);

			if (FoundIndex != INDEX_NONE)
			{
				if (CurrentType & InDependencyType)
				{
					InArray.RemoveAt(FoundIndex);
				}
				else
				{
					// Reference of another type still exists
					bStillExists = true;
				}
			}
		}, EAssetRegistryDependencyType::All);

		if (!bStillExists)
		{
			Referencers.RemoveAt(i);
		}
	}
}

void FDependsNode::PrintDependenciesRecursive(const FString& Indent, TSet<const FDependsNode*>& VisitedNodes) const
{
	if ( this == NULL )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%sNULL"), *Indent);
	}
	else if ( VisitedNodes.Contains(this) )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s[CircularReferenceTo]%s"), *Indent, *Identifier.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s%s"), *Indent, *Identifier.ToString());
		VisitedNodes.Add(this);

		IterateOverDependencies([&Indent, &VisitedNodes](FDependsNode* InDependency, EAssetRegistryDependencyType::Type)
		{
			InDependency->PrintDependenciesRecursive(Indent + TEXT("  "), VisitedNodes);
		},
		EAssetRegistryDependencyType::All
		);
	}
}

void FDependsNode::PrintReferencersRecursive(const FString& Indent, TSet<const FDependsNode*>& VisitedNodes) const
{
	if ( this == NULL )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%sNULL"), *Indent);
	}
	else if ( VisitedNodes.Contains(this) )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s[CircularReferenceTo]%s"), *Indent, *Identifier.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s%s"), *Indent, *Identifier.ToString());
		VisitedNodes.Add(this);

		for (FDependsNode* Node : Referencers)
		{
			Node->PrintReferencersRecursive(Indent + TEXT("  "), VisitedNodes);
		}
	}
}

int32 FDependsNode::GetConnectionCount() const
{
	return HardDependencies.Num() + SoftDependencies.Num() + NameDependencies.Num() + ManageDependencies.Num() + Referencers.Num();
}