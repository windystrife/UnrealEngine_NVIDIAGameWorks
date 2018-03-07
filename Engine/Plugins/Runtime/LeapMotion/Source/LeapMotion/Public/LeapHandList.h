// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapHandList.generated.h"

/**
* The HandList class represents a list of Hand objects.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.HandList.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapHandList : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapHandList();
	
	/**
	* Whether the list is empty.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand List")
	bool IsEmpty;

	/**
	* The number of hands in this list.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand List")
	int32 Count;

	/**
	* The member of the list that is farthest to the front within the standard Leap Motion frame of reference (i.e has the largest X coordinate).
	*
	* @return	The frontmost hand, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getFrontmost", CompactNodeTitle = "", Keywords = "get frontmost"), Category = "Leap Hand List")
	class ULeapHand* Frontmost();

	/**
	* The member of the list that is farthest to the left within the standard Leap Motion frame of reference (i.e has the smallest Y coordinate).
	*
	* @return	The leftmost hand, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getLeftmost", CompactNodeTitle = "", Keywords = "get leftmost"), Category = "Leap Hand List")
	class ULeapHand* Leftmost();

	/**
	* The member of the list that is farthest to the right within the standard Leap Motion frame of reference (i.e has the largest Y coordinate).
	*
	* @return	The rightmost hand, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getRightmost", CompactNodeTitle = "", Keywords = "get rightmost"), Category = "Leap Hand List")
	class ULeapHand* Rightmost();
	
	/**
	* Access a list member by its position in the list.
	*
	* @param	Index	The zero-based list position index.
	* @return	The Hand object at the specified index.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getIndex", CompactNodeTitle = "", Keywords = "get index"), Category = "Leap Hand List")
	class ULeapHand* GetIndex(int32 Index);

	void SetHandList(const class Leap::HandList &Handlist);

private:
	class FPrivateHandList* Private;

	UPROPERTY()
	ULeapHand* PFrontmost = nullptr;
	UPROPERTY()
	ULeapHand* PLeftmost = nullptr;
	UPROPERTY()
	ULeapHand* PRightmost = nullptr;
	UPROPERTY()
	ULeapHand* PIndexHand = nullptr;
};