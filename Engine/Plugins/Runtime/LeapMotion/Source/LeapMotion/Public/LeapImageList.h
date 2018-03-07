// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapImageList.generated.h"

/**
* The ImageList class represents a list of Image objects.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.ImageList.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapImageList : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapImageList();
	
	/**
	* Whether the list is empty.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image List")
	bool IsEmpty;

	/**
	* The number of images in this list.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image List")
	int32 Count;
	
	/**
	* Access a list member by its position in the list.
	*
	* @param	Index	The zero-based list position index.
	* @return	The Image object at the specified index.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getIndex", CompactNodeTitle = "", Keywords = "get index"), Category = "Leap Image List")
	class ULeapImage* GetIndex(int32 Index);

	ULeapImage* operator[](int Index);

	void SetLeapImageList(const class Leap::ImageList &LeapImageList);

private:
	class FPrivateLeapImageList* Private;

	UPROPERTY()
	ULeapImage* PIndexImage1 = nullptr;
	UPROPERTY()
	ULeapImage* PIndexImage2 = nullptr;
};