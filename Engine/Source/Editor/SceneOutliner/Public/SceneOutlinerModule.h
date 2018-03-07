// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerPublicTypes.h"

class ISceneOutlinerColumn;
class FOutlinerFilter;

/**
 * Implements the Scene Outliner module.
 */
class FSceneOutlinerModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a scene outliner widget
	 *
	 * @param	InitOptions						Programmer-driven configuration for this widget instance
	 * @param	MakeContentMenuWidgetDelegate	Optional delegate to execute to build a context menu when right clicking on actors
	 * @param	OnActorPickedDelegate			Optional callback when an actor is selected in 'actor picking' mode
	 *
	 * @return	New scene outliner widget
	 */
	virtual TSharedRef< ISceneOutliner > CreateSceneOutliner(
		const SceneOutliner::FInitializationOptions& InitOptions,
		const FOnActorPicked& OnActorPickedDelegate ) const;

	/**
	 * Creates a scene outliner widget
	 *
	 * @param	InitOptions						Programmer-driven configuration for this widget instance
	 * @param	MakeContentMenuWidgetDelegate	Optional delegate to execute to build a context menu when right clicking on actors
	 * @param	OnItemPickedDelegate			Optional callback when an item is selected in 'picking' mode
	 *
	 * @return	New scene outliner widget
	 */
	virtual TSharedRef< ISceneOutliner > CreateSceneOutliner(
		const SceneOutliner::FInitializationOptions& InitOptions,
		const FOnSceneOutlinerItemPicked& OnItemPickedDelegate ) const;

public:
	/** Register a new type of column available to all scene outliners */
	template< typename T >
	void RegisterColumnType()
	{
		auto ID = T::GetID();
		if ( !ColumnMap.Contains( ID ) )
		{
			auto CreateColumn = []( ISceneOutliner& Outliner ){
				return TSharedRef< ISceneOutlinerColumn >( MakeShareable( new T(Outliner) ) );
			};

			ColumnMap.Add( ID, FCreateSceneOutlinerColumn::CreateStatic( CreateColumn ) );
		}
	}

	/** Register a new type of default column available to all scene outliners */
	template< typename T >
	void RegisterDefaultColumnType(SceneOutliner::FDefaultColumnInfo InDefaultColumnInfo)
	{
		auto ID = T::GetID();
		if ( !ColumnMap.Contains( ID ) )
		{
			auto CreateColumn = []( ISceneOutliner& Outliner ){
				return TSharedRef< ISceneOutlinerColumn >( MakeShareable( new T(Outliner) ) );
			};

			ColumnMap.Add( ID, FCreateSceneOutlinerColumn::CreateStatic( CreateColumn ) );
			DefaultColumnMap.Add( ID, InDefaultColumnInfo );
		}
	}

	/** Unregister a previously registered column type */
	template< typename T >
	void UnRegisterColumnType()
	{
		ColumnMap.Remove( T::GetID() );
		DefaultColumnMap.Remove( T::GetID() );
	}

	/** Factory a new column from the specified name. Returns null if no type has been registered under that name. */
	TSharedPtr< ISceneOutlinerColumn > FactoryColumn( FName ID, ISceneOutliner& Outliner ) const
	{
		if ( auto* Factory = ColumnMap.Find( ID ) )
		{
			return Factory->Execute(Outliner);
		}
		
		return nullptr;
	}

	/** Map of column type name -> default column info */
	TMap< FName, SceneOutliner::FDefaultColumnInfo> DefaultColumnMap;


	/** Additional outliner filters */
	TMap< FName, SceneOutliner::FOutlinerFilterInfo > OutlinerFilterInfoMap;

private:

	/** Map of column type name -> factory delegate */
	TMap< FName, FCreateSceneOutlinerColumn > ColumnMap;

public:

	// IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
