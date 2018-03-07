// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapPointableList.generated.h"

/**
* The PointableList class represents a list of Pointable objects.
* 
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.PointableList.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapPointableList : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapPointableList();
	
	/**
	* Appends the members of the specified PointableList to this PointableList.
	*
	* @param	List	A PointableList object containing Pointable objects to append to the end of this PointableList.
	* @return	A PointableList object with the current and appended list
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "append", CompactNodeTitle = "", Keywords = "append"), Category = "Leap Pointable List")
	ULeapPointableList* Append(class ULeapPointableList *List);

	/**
	* Appends the members of the specified FingerList to this PointableList.
	*
	* @param	List	A FingerList object containing Finger objects to append to the end of this PointableList.
	* @return	A PointableList object with the current and appended list
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "appendFingers", CompactNodeTitle = "", Keywords = "append"), Category = "Leap Pointable List")
	ULeapPointableList* AppendFingers(class ULeapFingerList *List);

	/**
	* Appends the members of the specified ToolList to this PointableList.
	*
	* @param	List	A ToolList object containing Tool objects to append to the end of this PointableList.
	* @return	A PointableList object with the current and appended list
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "appendTools", CompactNodeTitle = "", Keywords = "append"), Category = "Leap Pointable List")
	ULeapPointableList* AppendTools(class ULeapToolList *List);

	/**
	* The number of pointable entities in this list.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable List")
	int32 Count;

	/**
	* Returns a new list containing those members of the current list that are extended.
	*
	* @return	The list of tools and extended fingers from the current list.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "extended", CompactNodeTitle = "", Keywords = "extended"), Category = "Leap Pointable List")
	ULeapPointableList *Extended();

	/**
	* The member of the list that is farthest to the front within the standard Leap Motion 
	* frame of reference (i.e has the largest X coordinate).
	*
	* @return	The frontmost pointable, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "frontmost", CompactNodeTitle = "", Keywords = "frontmost"), Category = "Leap Pointable List")
	class ULeapPointable *Frontmost();

	/**
	* Reports whether the list is empty.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable List")
	bool IsEmpty;

	/**
	* The member of the list that is farthest to the left within the standard Leap Motion
	* frame of reference (i.e has the smallest Y coordinate).
	*
	* @return	The leftmost pointable, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "leftmost", CompactNodeTitle = "", Keywords = "leftmost"), Category = "Leap Pointable List")
	class ULeapPointable *Leftmost();

	/**
	* Access a list member by its position in the list.
	*
	* @param	Index	position in the list.
	* @return The Pointable object at the specified index.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getPointableByIndex", CompactNodeTitle = "[]", Keywords = "get pointable by index"), Category = "Leap Pointable List")
	class ULeapPointable *GetPointableByIndex(int32 Index);

	/**
	* The member of the list that is farthest to the right within the standard Leap Motion 
	* frame of reference (i.e has the largest Y coordinate).
	*
	* @return	The rightmost pointable, or invalid if list is empty.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "rightmost", CompactNodeTitle = "", Keywords = "rightmost"), Category = "Leap Pointable List")
	class ULeapPointable *Rightmost();

	void SetPointableList(const class Leap::PointableList &Pointables);

private:
	class FPrivatePointableList* Private;

	UPROPERTY()
	ULeapPointable* PLeftmost = nullptr;
	UPROPERTY()
	ULeapPointable* PRightmost = nullptr;
	UPROPERTY()
	ULeapPointable* PFrontmost = nullptr;
	UPROPERTY()
	ULeapPointable* PPointableByIndex = nullptr;
	UPROPERTY()
	ULeapPointableList* PAppendedList = nullptr;
	UPROPERTY()
	ULeapPointableList* PExtendedList = nullptr;
};