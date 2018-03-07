// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "EngineDefines.h"

class AActor;
class UCanvas;
struct FLogEntryItem;

enum class ECreateIfNeeded : int8
{
	Invalid = -1,
	DontCreate = 0,
	Create = 1,
};

// flags describing VisualLogger device's features
namespace EVisualLoggerDeviceFlags
{
	enum Type
	{
		NoFlags = 0,
		CanSaveToFile = 1,
		StoreLogsLocally = 2,
	};
}

// version for vlog binary file format
namespace EVisualLoggerVersion
{
	enum Type
	{
		Initial = 0,
		HistogramGraphsSerialization = 1,
		AddedOwnerClassName = 2,
		StatusCategoryWithChildren = 3,
		TransformationForShapes = 4,
		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	extern ENGINE_API const FGuid GUID;
}

//types of shape elements
enum class EVisualLoggerShapeElement : uint8
{
	Invalid = 0,
	SinglePoint, // individual points. 
	Segment, // pairs of points 
	Path,	// sequence of point
	Box,
	Cone,
	Cylinder,
	Capsule,
	Polygon,
	Mesh,
	NavAreaMesh, // convex based mesh with min and max Z values
	// note that in order to remain backward compatibility in terms of log
	// serialization new enum values need to be added at the end
};

#if ENABLE_VISUAL_LOG
struct ENGINE_API FVisualLogEventBase
{
	const FString Name;
	const FString FriendlyDesc;
	const ELogVerbosity::Type Verbosity;

	FVisualLogEventBase(const FString& InName, const FString& InFriendlyDesc, ELogVerbosity::Type InVerbosity)
		: Name(InName), FriendlyDesc(InFriendlyDesc), Verbosity(InVerbosity)
	{
	}
};

struct ENGINE_API FVisualLogEvent
{
	FString Name;
	FString UserFriendlyDesc;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TMap<FName, int32>	 EventTags;
	int32 Counter;
	int64 UserData;
	FName TagName;

	FVisualLogEvent() : Counter(1) { /* Empty */ }
	FVisualLogEvent(const FVisualLogEventBase& Event);
	FVisualLogEvent& operator=(const FVisualLogEventBase& Event);
};

struct ENGINE_API FVisualLogLine
{
	FString Line;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	int32 UniqueId;
	int64 UserData;
	FName TagName;

	FVisualLogLine() { /* Empty */ }
	FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine);
	FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine, int64 InUserData);
};

struct ENGINE_API FVisualLogStatusCategory
{
	TArray<FString> Data;
	FString Category;
	int32 UniqueId;
	TArray<FVisualLogStatusCategory> Children;

	FVisualLogStatusCategory();
	explicit FVisualLogStatusCategory(const FString& InCategory/* = TEXT("")*/)
		: Category(InCategory)
	{
	}

	void Add(const FString& Key, const FString& Value);
	bool GetDesc(int32 Index, FString& Key, FString& Value) const;
	void AddChild(const FVisualLogStatusCategory& Child);
};

struct ENGINE_API FVisualLogShapeElement
{
	FString Description;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TArray<FVector> Points;
	FMatrix TransformationMatrix;
	int32 UniqueId;
	EVisualLoggerShapeElement Type;
	uint8 Color;
	union
	{
		uint16 Thicknes;
		uint16 Radius;
	};

	FVisualLogShapeElement(EVisualLoggerShapeElement InType = EVisualLoggerShapeElement::Invalid);
	FVisualLogShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness, const FName& InCategory);
	void SetColor(const FColor& InColor);
	EVisualLoggerShapeElement GetType() const;
	void SetType(EVisualLoggerShapeElement InType);
	FColor GetFColor() const;
};

struct ENGINE_API FVisualLogHistogramSample
{
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	FName GraphName;
	FName DataName;
	FVector2D SampleValue;
	int32 UniqueId;
};

struct ENGINE_API FVisualLogDataBlock
{
	FName TagName;
	FName Category;
	TEnumAsByte<ELogVerbosity::Type> Verbosity;
	TArray<uint8>	Data;
	int32 UniqueId;
};
#endif  //ENABLE_VISUAL_LOG

struct ENGINE_API FVisualLogEntry
{
#if ENABLE_VISUAL_LOG
	float TimeStamp;
	FVector Location;
	uint8 bIsClassWhitelisted : 1;
	uint8 bIsObjectWhitelisted : 1;	
	uint8 bIsAllowedToLog : 1;

	TArray<FVisualLogEvent> Events;
	TArray<FVisualLogLine> LogLines;
	TArray<FVisualLogStatusCategory> Status;
	TArray<FVisualLogShapeElement> ElementsToDraw;
	TArray<FVisualLogHistogramSample>	HistogramSamples;
	TArray<FVisualLogDataBlock>	DataBlocks;

	FVisualLogEntry() { Reset(); }
	FVisualLogEntry(const FVisualLogEntry& Entry);
	FVisualLogEntry(const AActor* InActor, TArray<TWeakObjectPtr<UObject> >* Children);
	FVisualLogEntry(float InTimeStamp, FVector InLocation, const UObject* Object, TArray<TWeakObjectPtr<UObject> >* Children);

	void Reset();
	void UpdateAllowedToLog();

	void AddText(const FString& TextLine, const FName& CategoryName, ELogVerbosity::Type Verbosity);
	// path
	void AddElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// location
	void AddElement(const FVector& Point, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// segment
	void AddElement(const FVector& Start, const FVector& End, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// box
	void AddElement(const FBox& Box, const FMatrix& Matrix, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// Cone
	void AddElement(const FVector& Orgin, const FVector& Direction, float Length, float AngleWidth, float AngleHeight, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// Cylinder
	void AddElement(const FVector& Start, const FVector& End, float Radius, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""), uint16 Thickness = 0);
	// capsule
	void AddElement(const FVector& Center, float HalfHeight, float Radius, const FQuat & Rotation, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// custom element
	void AddElement(const FVisualLogShapeElement& Element);
	// NavAreaMesh
	void AddElement(const TArray<FVector>& ConvexPoints, float MinZ, float MaxZ, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// 3d Mesh
	void AddElement(const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// 2d convex
	void AddConvexElement(const TArray<FVector>& Points, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FColor& Color = FColor::White, const FString& Description = TEXT(""));
	// histogram sample
	void AddHistogramData(const FVector2D& DataSample, const FName& CategoryName, ELogVerbosity::Type Verbosity, const FName& GraphName, const FName& DataName);
	// Custom data block
	FVisualLogDataBlock& AddDataBlock(const FString& TagName, const TArray<uint8>& BlobDataArray, const FName& CategoryName, ELogVerbosity::Type Verbosity);
	// Event
	int32 AddEvent(const FVisualLogEventBase& Event);
	// find index of status category
	int32 FindStatusIndex(const FString& CategoryName);


#endif // ENABLE_VISUAL_LOG
};

#if  ENABLE_VISUAL_LOG

/**
 * Interface for Visual Logger Device
 */
class ENGINE_API FVisualLogDevice
{
public:
	struct FVisualLogEntryItem
	{
		FVisualLogEntryItem() {}
		FVisualLogEntryItem(FName InOwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) : OwnerName(InOwnerName), OwnerClassName(InOwnerClassName), Entry(LogEntry) { }

		FName OwnerName;
		FName OwnerClassName;
		FVisualLogEntry Entry;
	};

	virtual ~FVisualLogDevice() { }
	virtual void Serialize(const UObject* LogOwner, FName OwnerName, FName InOwnerClassName, const FVisualLogEntry& LogEntry) = 0;
	virtual void Cleanup(bool bReleaseMemory = false) { /* Empty */ }
	virtual void StartRecordingToFile(float TImeStamp) { /* Empty */ }
	virtual void StopRecordingToFile(float TImeStamp) { /* Empty */ }
	virtual void DiscardRecordingToFile() { /* Empty */ }
	virtual void SetFileName(const FString& InFileName) { /* Empty */ }
	virtual void GetRecordedLogs(TArray<FVisualLogDevice::FVisualLogEntryItem>& OutLogs)  const { /* Empty */ }
	virtual bool HasFlags(int32 InFlags) const { return false; }
};

struct ENGINE_API FVisualLoggerCategoryVerbosityPair
{
	FVisualLoggerCategoryVerbosityPair(FName Category, ELogVerbosity::Type InVerbosity) : CategoryName(Category), Verbosity(InVerbosity) {}

	FName CategoryName;
	ELogVerbosity::Type Verbosity;
};

inline bool operator==(const FVisualLoggerCategoryVerbosityPair& A, const FVisualLoggerCategoryVerbosityPair& B)
{
	return A.CategoryName == B.CategoryName
		&& A.Verbosity == B.Verbosity;
}

struct ENGINE_API FVisualLoggerHelpers
{
	static FString GenerateTemporaryFilename(const FString& FileExt);
	static FString GenerateFilename(const FString& TempFileName, const FString& Prefix, float StartRecordingTime, float EndTimeStamp);
	static FArchive& Serialize(FArchive& Ar, FName& Name);
	static FArchive& Serialize(FArchive& Ar, TArray<FVisualLogDevice::FVisualLogEntryItem>& RecordedLogs);
	static void GetCategories(const FVisualLogEntry& RecordedLogs, TArray<FVisualLoggerCategoryVerbosityPair>& OutCategories);
	static void GetHistogramCategories(const FVisualLogEntry& RecordedLogs, TMap<FString, TArray<FString> >& OutCategories);
};

struct IVisualLoggerEditorInterface
{
	virtual const FName& GetRowClassName(FName RowName) const = 0;
	virtual int32 GetSelectedItemIndex(FName RowName) const = 0;
	virtual const TArray<FVisualLogDevice::FVisualLogEntryItem>& GetRowItems(FName RowName) = 0;
	virtual const FVisualLogDevice::FVisualLogEntryItem& GetSelectedItem(FName RowName) const = 0;

	virtual const TArray<FName>& GetSelectedRows() const = 0;
	virtual bool IsRowVisible(FName RowName) const = 0;
	virtual bool IsItemVisible(FName RowName, int32 ItemIndex) const = 0;
	virtual UWorld* GetWorld() const = 0;
	virtual AActor* GetHelperActor(UWorld* InWorld = nullptr) const = 0;

	virtual bool MatchCategoryFilters(const FString& String, ELogVerbosity::Type Verbosity = ELogVerbosity::All) = 0;
};

class FVisualLogExtensionInterface
{
public:
	virtual ~FVisualLogExtensionInterface() { }

	virtual void ResetData(IVisualLoggerEditorInterface* EdInterface) = 0;
	virtual void DrawData(IVisualLoggerEditorInterface* EdInterface, UCanvas* Canvas) = 0;

	virtual void OnItemsSelectionChanged(IVisualLoggerEditorInterface* EdInterface) {};
	virtual void OnLogLineSelectionChanged(IVisualLoggerEditorInterface* EdInterface, TSharedPtr<struct FLogEntryItem> SelectedItem, int64 UserData) {};
};

ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogDevice::FVisualLogEntryItem& FrameCacheItem);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogDataBlock& Data);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogHistogramSample& Sample);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogShapeElement& Element);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogEvent& Event);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogLine& LogLine);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogStatusCategory& Status);
ENGINE_API  FArchive& operator<<(FArchive& Ar, FVisualLogEntry& LogEntry);

inline
bool operator==(const FVisualLogEvent& Left, const FVisualLogEvent& Right) 
{ 
	return Left.Name == Right.Name; 
}

inline
FVisualLogEvent::FVisualLogEvent(const FVisualLogEventBase& Event)
: Counter(1)
{
	Name = Event.Name;
	UserFriendlyDesc = Event.FriendlyDesc;
	Verbosity = Event.Verbosity;
}

inline
FVisualLogEvent& FVisualLogEvent::operator= (const FVisualLogEventBase& Event)
{
	Name = Event.Name;
	UserFriendlyDesc = Event.FriendlyDesc;
	Verbosity = Event.Verbosity;
	return *this;
}

inline
FVisualLogLine::FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine)
: Line(InLine)
, Category(InCategory)
, Verbosity(InVerbosity)
, UserData(0)
{

}

inline
FVisualLogLine::FVisualLogLine(const FName& InCategory, ELogVerbosity::Type InVerbosity, const FString& InLine, int64 InUserData)
: Line(InLine)
, Category(InCategory)
, Verbosity(InVerbosity)
, UserData(InUserData)
{

}

inline
void FVisualLogStatusCategory::Add(const FString& Key, const FString& Value)
{
	Data.Add(FString(Key).AppendChar(TEXT('|')) + Value);
}

inline
void FVisualLogStatusCategory::AddChild(const FVisualLogStatusCategory& Child)
{
	Children.Add(Child);
}



inline
FVisualLogShapeElement::FVisualLogShapeElement(const FString& InDescription, const FColor& InColor, uint16 InThickness, const FName& InCategory)
: Category(InCategory)
, Verbosity(ELogVerbosity::All)
, TransformationMatrix(FMatrix::Identity)
, Type(EVisualLoggerShapeElement::Invalid)
, Thicknes(InThickness)
{
	if (InDescription.IsEmpty() == false)
	{
		Description = InDescription;
	}
	SetColor(InColor);
}

inline
void FVisualLogShapeElement::SetColor(const FColor& InColor)
{
	Color = ((InColor.DWColor() >> 30) << 6)	| (((InColor.DWColor() & 0x00ff0000) >> 22) << 4)	| (((InColor.DWColor() & 0x0000ff00) >> 14) << 2)	| ((InColor.DWColor() & 0x000000ff) >> 6);
}

inline
EVisualLoggerShapeElement FVisualLogShapeElement::GetType() const
{
	return Type;
}

inline
void FVisualLogShapeElement::SetType(EVisualLoggerShapeElement InType)
{
	Type = InType;
}

inline
FColor FVisualLogShapeElement::GetFColor() const
{
	FColor RetColor(((Color & 0xc0) << 24) | ((Color & 0x30) << 18) | ((Color & 0x0c) << 12) | ((Color & 0x03) << 6));
	RetColor.A = (RetColor.A * 255) / 192; // convert alpha to 0-255 range
	return RetColor;
}


inline
int32 FVisualLogEntry::FindStatusIndex(const FString& CategoryName)
{
	for (int32 TestCategoryIndex = 0; TestCategoryIndex < Status.Num(); TestCategoryIndex++)
	{
		if (Status[TestCategoryIndex].Category == CategoryName)
		{
			return TestCategoryIndex;
		}
	}

	return INDEX_NONE;
}

#endif // ENABLE_VISUAL_LOG
