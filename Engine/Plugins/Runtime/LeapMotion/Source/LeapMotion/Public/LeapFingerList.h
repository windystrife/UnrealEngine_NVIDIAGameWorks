// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapFingerList.generated.h"

/**
* The FingerList class represents a list of Finger objects.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.FingerList.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapFingerList : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapFingerList();
	
	/**
	* Appends the members of the specified FingerList to this FingerList.
	*
	* @param List - A FingerList object containing Finger objects to append to the end of this FingerList.
	* @return the resultant list
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "append", CompactNodeTitle = "", Keywords = "append"), Category = "Leap Finger List")
	ULeapFingerList *Append(const ULeapFingerList *List);

	/**
	* Number of fingers in this list.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger List")
	int32 Count;

	/**
	* Returns a new list containing those fingers in the current list that are extended.
	*
	* @return The list of extended fingers from the current list.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "extended", CompactNodeTitle = "", Keywords = "extended"), Category = "Leap Finger List")
	ULeapFingerList *Extended();

	/**
	* The member of the list that is farthest to the front within the standard Leap Motion frame of reference.
	*
	* @return The frontmost finger, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "frontmost", CompactNodeTitle = "", Keywords = "frontmost"), Category = "Leap Finger List")
	class ULeapFinger *Frontmost();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger List")
	bool IsEmpty;

	/**
	* The member of the list that is farthest to the left within the standard Leap Motion frame of reference 
	*
	* @return The leftmost finger, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "leftmost", CompactNodeTitle = "", Keywords = "leftmost"), Category = "Leap Finger List")
	class ULeapFinger *Leftmost();

	/**
	* Access a list member by its position in the list.
	*
	* @return The Finger object at the specified index.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getPointableById", CompactNodeTitle = "[]", Keywords = "get pointable by id"), Category = "Leap Finger List")
	class ULeapFinger *GetPointableById(int32 Id);

	/**
	* The member of the list that is farthest to the right within the standard Leap Motion frame of reference
	*
	* @return The rightmost finger, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "rightmost", CompactNodeTitle = "", Keywords = "rightmost"), Category = "Leap Finger List")
	class ULeapFinger *Rightmost();

	void SetFingerList(const class Leap::FingerList &Pointables);
	Leap::FingerList* FingerList();

private:
	class FPrivateFingerList* Private;

	UPROPERTY()
	class ULeapFinger* PFrontmost = nullptr;
	UPROPERTY()
	ULeapFinger* PLeftmost = nullptr;
	UPROPERTY()
	ULeapFinger* PRightmost = nullptr;
	UPROPERTY()
	ULeapFinger* PPointableById = nullptr;
	UPROPERTY()
	class ULeapFingerList* PAppendedList = nullptr;
	UPROPERTY()
	ULeapFingerList* PExtendedList = nullptr;
};