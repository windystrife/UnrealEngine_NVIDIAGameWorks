// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class ULevel;
class ULevelStreaming;

/**
 * A set of static methods for common editor operations that operate on ULevel objects.
 */
class ENGINE_API FLevelUtils
{
public:
	///////////////////////////////////////////////////////////////////////////
	// Given a ULevel, find the corresponding ULevelStreaming.

	/**
	 * Returns the streaming level corresponding to the specified ULevel, or NULL if none exists.
	 *
	 * @param		Level		The level to query.
	 * @return					The level's streaming level, or NULL if none exists.
	 */
	static ULevelStreaming* FindStreamingLevel(const ULevel* Level);

	/**
	 * Returns the streaming level by package name, or NULL if none exists.
	 *
	 * @param		InWorld			World to look in for the streaming level
	 * @param		PackageName		Name of the package containing the ULevel to query
	 * @return						The level's streaming level, or NULL if none exists.
	 */
	static ULevelStreaming* FindStreamingLevel(UWorld* InWorld, const TCHAR* PackageName);


	///////////////////////////////////////////////////////////////////////////
	// Locking/unlocking levels for edit.

	/**
	 * Returns true if the specified level is locked for edit, false otherwise.
	 *
	 * @param	Level		The level to query.
	 * @return				true if the level is locked, false otherwise.
	 */
	static bool IsLevelLocked(ULevel* Level);
	static bool IsLevelLocked(AActor* Actor);

	/**
	 * Sets a level's edit lock.
	 *
	 * @param	Level		The level to modify.
	 */
	static void ToggleLevelLock(ULevel* Level);

	///////////////////////////////////////////////////////////////////////////
	// Controls whether the level is loaded in editor.

	/**
	 * Returns true if the level is currently loaded in the editor, false otherwise.
	 *
	 * @param	Level		The level to query.
	 * @return				true if the level is loaded, false otherwise.
	 */
	static bool IsLevelLoaded(ULevel* Level);

	/**
	 * Flags an unloaded level for loading.
	 *
	 * @param	Level		The level to modify.
	 */
	static void MarkLevelForLoading(ULevel* Level);

	/**
	 * Flags a loaded level for unloading.
	 *
	 * @param	Level		The level to modify.
	 */
	static void MarkLevelForUnloading(ULevel* Level);

	///////////////////////////////////////////////////////////////////////////
	// Level visibility.

	/**
	 * Returns true if the specified level is visible in the editor, false otherwise.
	 *
	 * @param	StreamingLevel		The level to query.
	 */
	static bool IsLevelVisible(const ULevelStreaming* StreamingLevel);

	/**
	 * Returns true if the specified level is visible in the editor, false otherwise.
	 *
	 * @param	Level		The level to query.
	 */
	static bool IsLevelVisible(const ULevel* Level);


	/**
	 * Transforms the level to a new world space
	 *
	 * @param	Level				The level to Transform.
	 * @param	LevelTransform		How to Transform the level. 
	 * @param	bDoPostEditMove		Whether to call PostEditMove on actors after transforming
	 */
	static void ApplyLevelTransform( ULevel* Level, const FTransform& LevelTransform, bool bDoPostEditMove = true );

#if WITH_EDITOR
	///////////////////////////////////////////////////////////////////////////
	// Level - editor transforms.

	/**
	 * Calls PostEditMove on all the actors in the level
	 *
	 * @param	Level		The level.
	 */
	static void ApplyPostEditMove( ULevel* Level );

	/**
	 * Sets a new LevelEditorTransform on a streaming level .
	 *
	 * @param	StreamingLevel		The level.
	 * @param	Transform			The new transform.
	 * @param	bDoPostEditMove		Whether to call PostEditMove on actors after transforming
	 */
	static void SetEditorTransform(ULevelStreaming* StreamingLevel, const FTransform& Transform, bool bDoPostEditMove = true);

	/**
	 * Apply the LevelEditorTransform on a level.
	 *
	 * @param	StreamingLevel		The level.
	 * @param   bDoPostEditMove		Whether to call PostEditMove on actors after transforming
	 */
	static void ApplyEditorTransform(const ULevelStreaming* StreamingLevel, bool bDoPostEditMove = true);

	/**
	 * Remove the LevelEditorTransform from a level.
	 *
	 * @param	StreamingLevel		The level.
	 * @param	bDoPostEditMove		Whether to call PostEditMove on actors after transforming
	 */
	static void RemoveEditorTransform(const ULevelStreaming* StreamingLevel, bool bDoPostEditMove = true);

	/**
	* Returns true if we are moving a level
	*/
	static bool IsMovingLevel();

private:

	// Flag to mark if we are currently finalizing a level offset
	static bool bMovingLevel;

#endif // WITH_EDITOR
};

