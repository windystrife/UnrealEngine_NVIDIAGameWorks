// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GameplayDebuggerSettings.h: Declares the UGameplayDebuggerSettings class.
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "LogVisualizerSettings.generated.h"

struct FVisualLoggerDBRow;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFilterCategoryAdded, FString, ELogVerbosity::Type);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFilterCategoryRemoved, FString);

struct FVisualLoggerDBRow;

USTRUCT()
struct FCategoryFilter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config)
	FString CategoryName;

	UPROPERTY(config)
	int32 LogVerbosity;

	UPROPERTY(config)
	uint32 Enabled : 1;

	uint32 bIsInUse : 1;
};

USTRUCT()
struct FVisualLoggerFiltersData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config)
	FString SearchBoxFilter;
	
	UPROPERTY(config)
	FString ObjectNameFilter;
	
	UPROPERTY(config)
	TArray<FCategoryFilter> Categories;
	
	UPROPERTY(config)
	TArray<FString> SelectedClasses;
};

USTRUCT()
struct FVisualLoggerFilters : public FVisualLoggerFiltersData
{
	GENERATED_USTRUCT_BODY()

	FOnFilterCategoryAdded OnFilterCategoryAdded;
	FOnFilterCategoryRemoved OnFilterCategoryRemoved;

	static FVisualLoggerFilters& Get();
	static void Initialize();
	static void Shutdown();

	void Reset();
	void InitWith(const FVisualLoggerFiltersData& NewFiltersData);

	bool MatchCategoryFilters(FString String, ELogVerbosity::Type Verbosity = ELogVerbosity::All);

	bool MatchSearchString(FString String) { return SearchBoxFilter == String; }
	void SetSearchString(FString InString) { SearchBoxFilter = InString; }
	FString GetSearchString() { return SearchBoxFilter; }

	void AddCategory(FString InName, ELogVerbosity::Type InVerbosity);
	void RemoveCategory(FString InName);
	FCategoryFilter& GetCategoryByName(const FString& InName);
	FCategoryFilter& GetCategoryByName(const FName& InName);

	void DeactivateAllButThis(const FString& InName);
	void EnableAllCategories();

	bool MatchObjectName(FString String);
	void SelectObject(FString ObjectName);
	void RemoveObjectFromSelection(FString ObjectName);
	const TArray<FString>& GetSelectedObjects() const;

	void DisableGraphData(FName GraphName, FName DataName, bool SetAsDisabled);
	bool IsGraphDataDisabled(FName GraphName, FName DataName);

protected:
	void OnNewItemHandler(const FVisualLoggerDBRow& BDRow, int32 ItemIndex);

private:
	static TSharedPtr< struct FVisualLoggerFilters > StaticInstance;
	TMap<FName, FCategoryFilter*>	FastCategoryFilterMap;
	TArray<FName> DisabledGraphDatas;
};

struct FCategoryFiltersManager;

UCLASS(config = EditorPerProjectUserSettings)
class LOGVISUALIZER_API ULogVisualizerSettings : public UObject
{
	GENERATED_UCLASS_BODY()
	friend struct FCategoryFiltersManager;

public:
	DECLARE_EVENT_OneParam(ULogVisualizerSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged() { return SettingChangedEvent; }

	/**Whether to show trivial logs, i.e. the ones with only one entry.*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bIgnoreTrivialLogs;

	/**Threshold for trivial Logs*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger", meta = (EditCondition = "bIgnoreTrivialLogs", ClampMin = "0", ClampMax = "10", UIMin = "0", UIMax = "10"))
	int32 TrivialLogsThreshold;

	/**Whether to show the recent data or not. Property disabled for now.*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bStickToRecentData;

	/**Whether to reset current data or not for each new session.*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bResetDataWithNewSession;

	/**Whether to show histogram labels inside graph or outside. Property disabled for now.*/
	UPROPERTY(VisibleAnywhere, config, Category = "VisualLogger")
	bool bShowHistogramLabelsOutside;

	/** Camera distance used to setup location during reaction on log item double click */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger", meta = (ClampMin = "10", ClampMax = "1000", UIMin = "10", UIMax = "1000"))
	float DefaultCameraDistance;

	/**Whether to search/filter categories or to get text vlogs into account too */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bSearchInsideLogs;

	/** Background color for 2d graphs visualization */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	FColor GraphsBackgroundColor;

	/**Whether to store all filter settings on exit*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bPresistentFilters;

	/** Whether to extreme values on graph (data has to be provided for extreme values) */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bDrawExtremesOnGraphs;

	/** Whether to use PlayersOnly during Pause or not */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bUsePlayersOnlyForPause;

	/** Whether to dump Navigation Octree on Stop recording or not */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bLogNavOctreeOnStop;

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	class UMaterial* GetDebugMeshMaterial();

	void SavePresistentData();

	void ClearPresistentData();

	void LoadPresistentData();

protected:
	UPROPERTY(config)
	FVisualLoggerFiltersData PresistentFilters;

	/** A material used to render debug meshes with kind of flat shading, mostly used by Visual Logger tool. */
	UPROPERTY()
	class UMaterial* DebugMeshMaterialFakeLight;

	/** @todo document */
	UPROPERTY(config)
	FString DebugMeshMaterialFakeLightName;
private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;

};
