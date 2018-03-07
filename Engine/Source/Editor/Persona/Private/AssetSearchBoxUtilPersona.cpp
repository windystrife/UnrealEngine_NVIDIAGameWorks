// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetSearchBoxUtilPersona.h"
#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "SAssetSearchBox.h"
#include "Engine/SkeletalMeshSocket.h"

void SAssetSearchBoxForBones::Construct( const FArguments& InArgs, const class UObject* Outer, TSharedPtr<class IPropertyHandle> BoneNameProperty )
{
	check(Outer);

	// get the currently chosen bone/socket, if any
	const FText PropertyName = BoneNameProperty->GetPropertyDisplayName();
	FString CurValue;
	BoneNameProperty->GetValue(CurValue);
	if( CurValue == FString("None") )
	{
		CurValue.Empty();
	}

	const USkeleton* Skeleton = NULL;

	TArray<FString> PossibleSuggestions;
	if( const USkeletalMesh* SkeletalMesh = Cast<const USkeletalMesh>(Outer) )
	{
		if( InArgs._IncludeSocketsForSuggestions.Get() )
		{
			for( auto SocketIt=const_cast<USkeletalMesh*>(SkeletalMesh)->GetMeshOnlySocketList().CreateConstIterator(); SocketIt; ++SocketIt )
			{
				PossibleSuggestions.Add((*SocketIt)->SocketName.ToString());
			}
		}
		Skeleton = SkeletalMesh->Skeleton;
	}
	else
	{
		Skeleton = Cast<const USkeleton>(Outer);
	}
	if( Skeleton && InArgs._IncludeSocketsForSuggestions.Get() )
	{
		for( auto SocketIt=Skeleton->Sockets.CreateConstIterator(); SocketIt; ++SocketIt )
		{
			PossibleSuggestions.Add((*SocketIt)->SocketName.ToString());
		}
	}
	check(Skeleton);

	const TArray<FMeshBoneInfo>& Bones = Skeleton->GetReferenceSkeleton().GetRefBoneInfo();
	for( auto BoneIt=Bones.CreateConstIterator(); BoneIt; ++BoneIt )
	{
		PossibleSuggestions.Add(BoneIt->Name.ToString());
	}

	// create the asset search box
	ChildSlot
	[
		SNew(SAssetSearchBox)
		.InitialText(FText::FromString(CurValue))
		.HintText(InArgs._HintText)
		.OnTextCommitted(InArgs._OnTextCommitted)
		.PossibleSuggestions(PossibleSuggestions)
		.DelayChangeNotificationsWhileTyping( true )
		.MustMatchPossibleSuggestions(InArgs._MustMatchPossibleSuggestions)
	];
}

void SAssetSearchBoxForCurves::Construct(const FArguments& InArgs, const class USkeleton* InSkeleton, TSharedPtr<class IPropertyHandle> CurveNameProperty)
{
	check(InSkeleton);

	// get the currently chosen curve, if any
	const FText PropertyName = CurveNameProperty->GetPropertyDisplayName();
	FString CurValue;
	CurveNameProperty->GetValue(CurValue);
	if (CurValue == FString("None"))
	{
		CurValue.Empty();
	}

	Skeleton = InSkeleton;
	
	// create the asset search box
	ChildSlot
		[
			SNew(SAssetSearchBox)
			.InitialText(FText::FromString(CurValue))
			.HintText(InArgs._HintText)
			.OnTextCommitted(InArgs._OnTextCommitted)
			.PossibleSuggestions(this, &SAssetSearchBoxForCurves::GetCurveSearchSuggestions)
			.DelayChangeNotificationsWhileTyping(true)
			.MustMatchPossibleSuggestions(InArgs._MustMatchPossibleSuggestions)
		];
}

TArray<FString> SAssetSearchBoxForCurves::GetCurveSearchSuggestions() const
{
	TArray<FString> PossibleSuggestions;
	if (USkeleton* Skel = Skeleton.Get())
	{
		if (const FSmartNameMapping* Mapping = Skel->GetSmartNameContainer(USkeleton::AnimCurveMappingName))
		{
			TArray<FName> Names; 
			Mapping->FillNameArray(Names);
			for (const FName& Name : Names)
			{
				PossibleSuggestions.Add(Name.ToString());
			}
		}
	}

	return PossibleSuggestions;
}
