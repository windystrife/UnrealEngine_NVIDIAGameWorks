// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Brush.generated.h"

class UBrushBuilder;

//-----------------------------------------------------------------------------
// Variables.
UENUM()
enum ECsgOper
{
	/** Active brush. (deprecated, do not use.) */
	CSG_Active,
	/** Add to world. (deprecated, do not use.) */
	CSG_Add,
	/** Subtract from world. (deprecated, do not use.) */
	CSG_Subtract,
	/** Form from intersection with world. */
	CSG_Intersect,
	/** Form from negative intersection with world. */
	CSG_Deintersect,
	CSG_None,
	CSG_MAX,
};


UENUM()
enum EBrushType
{
	/** Default/builder brush. */
	Brush_Default UMETA(Hidden),
	/** Add to world. */
	Brush_Add UMETA(DisplayName=Additive),
	/** Subtract from world. */
	Brush_Subtract UMETA(DisplayName=Subtractive),
	Brush_MAX,
};


// Selection information for geometry mode
USTRUCT()
struct FGeomSelection
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Type;    // EGeometrySelectionType_

	UPROPERTY()
	int32 Index;    // Index into the geometry data structures

	UPROPERTY()
	int32 SelectionIndex;    // The selection index of this item


	FGeomSelection()
		: Type(0)
		, Index(0)
		, SelectionIndex(0)
	{ }

	friend ENGINE_API FArchive& operator<<(FArchive& Ar,FGeomSelection& MyGeomSelection)
	{
		return Ar << MyGeomSelection.Type << MyGeomSelection.Index << MyGeomSelection.SelectionIndex;
	}
};


UCLASS(hidecategories=(Object, Collision, Display, Rendering, Physics, Input, Blueprint), showcategories=("Input|MouseInput", "Input|TouchInput"), NotBlueprintable, ConversionRoot)
class ENGINE_API ABrush
	: public AActor
{
	GENERATED_UCLASS_BODY()

	/** Type of brush */
	UPROPERTY(EditAnywhere, Category=Brush)
	TEnumAsByte<enum EBrushType> BrushType;

	// Information.
	UPROPERTY()
	FColor BrushColor;

	UPROPERTY()
	int32 PolyFlags;

	UPROPERTY()
	uint32 bColored:1;

	UPROPERTY()
	uint32 bSolidWhenSelected:1;

	/** If true, this brush class can be placed using the class browser like other simple class types */
	UPROPERTY()
	uint32 bPlaceableFromClassBrowser:1;

	/** If true, this brush is a builder or otherwise does not need to be loaded into the game */
	UPROPERTY()
	uint32 bNotForClientOrServer:1;

	UPROPERTY(Instanced)
	class UModel* Brush;

private:
	UPROPERTY(Category = Collision, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UBrushComponent* BrushComponent;
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category=BrushBuilder)
	class UBrushBuilder* BrushBuilder;
#endif

	/** Flag set when we are in a manipulation (scaling, translation, brush builder param change etc.) */
	UPROPERTY()
	uint32 bInManipulation:1;

	/**
	 * Stores selection information from geometry mode.  This is the only information that we can't
	 * regenerate by looking at the source brushes following an undo operation.
	 */
	UPROPERTY()
	TArray<struct FGeomSelection> SavedSelections;
	
#if WITH_EDITOR
	/** Delegate used for notifications when PostRegisterAllComponents is called for a Brush */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBrushRegistered, ABrush*);
	/** Function to get the 'brush registered' delegate */
	static FOnBrushRegistered& GetOnBrushRegisteredDelegate()
	{
		return OnBrushRegistered;
	}
#endif

public:
	
	// UObject interface.
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual bool Modify(bool bAlwaysMarkDirty = false) override;

	virtual bool NeedsLoadForClient() const override
	{ 
		return !IsNotForClientOrServer(); 
	}

	virtual bool NeedsLoadForServer() const override
	{ 
		return !IsNotForClientOrServer(); 
	}

public:
	
	// AActor interface
	virtual bool IsLevelBoundsRelevant() const override;
	virtual void RebuildNavigationData();

#if WITH_EDITOR
	virtual void Destroyed() override;
	virtual void PostRegisterAllComponents() override;
	virtual void CheckForErrors() override;
	virtual void SetIsTemporarilyHiddenInEditor( bool bIsHidden ) override;

public:

	virtual void InitPosRotScale();
	virtual void CopyPosRotScaleFrom( ABrush* Other );

	static void SetSuppressBSPRegeneration(bool bSuppress) { bSuppressBSPRegeneration = bSuppress; }

private:

	/** An array to keep track of all the levels that need rebuilding. This is checked via NeedsRebuild() in the editor tick and triggers a csg rebuild. */
	static TArray< TWeakObjectPtr< ULevel > > LevelsToRebuild;

	/** Delegate called when PostRegisterAllComponents is called for a Brush */
	static FOnBrushRegistered OnBrushRegistered;

	/** Global bool to suppress automatic BSP regeneration */
	static bool bSuppressBSPRegeneration;

public:

	/**
	 * Called to see if any of the levels need rebuilding
	 *
	 * @param	OutLevels if specified, provides a copy of the levels array
	 * @return	true if the csg needs to be rebuilt on the next editor tick.	
	 */
	static bool NeedsRebuild(TArray< TWeakObjectPtr< ULevel > >* OutLevels = nullptr)
	{
		LevelsToRebuild.RemoveAllSwap([](const TWeakObjectPtr<ULevel>& Level) { return !Level.IsValid(); });

		if (OutLevels)
		{
			*OutLevels = LevelsToRebuild;
		}
		
		return(LevelsToRebuild.Num() > 0);
	}

	/**
	 * Called upon finishing the csg rebuild to clear the rebuild bool.
	 */
	static void OnRebuildDone()
	{
		LevelsToRebuild.Empty();
	}

	/**
	 * Called to make not of the level that needs rebuilding
	 *
	 * @param	InLevel The level that needs rebuilding
	 */
	static void SetNeedRebuild(ULevel* InLevel){if(InLevel){LevelsToRebuild.AddUnique(InLevel);}}
#endif//WITH_EDITOR

	/** @return true if this is a static brush */
	virtual bool IsStaticBrush() const;

	/** @return false */
	virtual bool IsVolumeBrush() const { return false; }
	
	/** @return false */
	virtual bool IsBrushShape() const { return false; }

	// ABrush interface.

	/** Figures out the best color to use for this brushes wireframe drawing.	*/
	virtual FColor GetWireColor() const;

	/**
	 * Return if true if this brush is not used for gameplay (i.e. builder brush)
	 * 
	 * @return	true if brush is not for client or server
	 */
	FORCEINLINE bool IsNotForClientOrServer() const
	{
		return bNotForClientOrServer;
	}

	/** Indicate that this brush need not be loaded on client or servers	 */
	FORCEINLINE void SetNotForClientOrServer()
	{
		bNotForClientOrServer = true;
	}

	/** Indicate that brush need should be loaded on client or servers	 */
	FORCEINLINE void ClearNotForClientOrServer()
	{
		bNotForClientOrServer = false;
	}

#if WITH_EDITORONLY_DATA
	/** @return the brush builder that created the current brush shape */
	const UBrushBuilder* GetBrushBuilder() const { return BrushBuilder; }
#endif

public:
	/** Returns BrushComponent subobject **/
	class UBrushComponent* GetBrushComponent() const { return BrushComponent; }

#if WITH_EDITOR
	/** Debug purposes only; an attempt to catch the cause of UE-36265 */
	static const TCHAR* GGeometryRebuildCause;
#endif
};
