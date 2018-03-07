// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionComment.generated.h"

struct FPropertyChangedEvent;

UCLASS(MinimalAPI)
class UMaterialExpressionComment : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 SizeX;

	UPROPERTY()
	int32 SizeY;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionComment, meta=(MultiLine=true))
	FString Text;

	/** Color to style comment with */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionComment)
	FLinearColor CommentColor;

	/** Size of the text in the comment box */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionComment, meta=(ClampMin=1, ClampMax=1000))
	int32 FontSize;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual bool Modify( bool bAlwaysMarkDirty=true ) override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif // WITH_EDITOR
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
	//~ End UMaterialExpression Interface
};



