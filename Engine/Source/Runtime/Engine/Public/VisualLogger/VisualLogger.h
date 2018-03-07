// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "EngineStats.h"

#if ENABLE_VISUAL_LOG

#define REDIRECT_TO_VLOG(Dest) FVisualLogger::Get().Redirect(this, Dest)
#define REDIRECT_OBJECT_TO_VLOG(Src, Dest) FVisualLogger::Get().Redirect(Src, Dest)

#define CONNECT_WITH_VLOG(Dest)
#define CONNECT_OBJECT_WITH_VLOG(Src, Dest)

// Text, regular log
#define UE_VLOG(LogOwner, CategoryName, Verbosity, Format, ...) if( FVisualLogger::IsRecording() ) FVisualLogger::CategorizedLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Format, ##__VA_ARGS__)
#define UE_CVLOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG(LogOwner, CategoryName, Verbosity, Format, ##__VA_ARGS__);} 
// Text, log with output to regular unreal logs too
#define UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ...) { if(FVisualLogger::IsRecording()) FVisualLogger::CategorizedLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Format, ##__VA_ARGS__); UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); }
#define UE_CVLOG_UELOG(Condition, LogOwner, CategoryName, Verbosity, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ##__VA_ARGS__);} 
// Segment shape
#define UE_VLOG_SEGMENT(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, SegmentStart, SegmentEnd, Color, 0, Format, ##__VA_ARGS__)
#define UE_CVLOG_SEGMENT(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_SEGMENT(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Format, ##__VA_ARGS__);}
// Segment shape
#define UE_VLOG_SEGMENT_THICK(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ##__VA_ARGS__)
#define UE_CVLOG_SEGMENT_THICK(Condition, LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_SEGMENT_THICK(LogOwner, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, Format, ##__VA_ARGS__);} 
// Localization as sphere shape
#define UE_VLOG_LOCATION(LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Location, Radius, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_LOCATION(Condition, LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_LOCATION(LogOwner, CategoryName, Verbosity, Location, Radius, Color, Format, ##__VA_ARGS__);} 
// Box shape
#define UE_VLOG_BOX(LogOwner, CategoryName, Verbosity, Box, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Box, FMatrix::Identity, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_BOX(Condition, LogOwner, CategoryName, Verbosity, Box, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_BOX(Actor, CategoryName, Verbosity, Box, FMatrix::Identity, Color, Format, ##__VA_ARGS__);} 
// Oriented box shape
#define UE_VLOG_OBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Box, Matrix, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_OBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_BOX(Actor, CategoryName, Verbosity, Box, Matrix, Color, Format, ##__VA_ARGS__);} 
// Cone shape
#define UE_VLOG_CONE(LogOwner, CategoryName, Verbosity, Orgin, Direction, Length, Angle, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Orgin, Direction, Length, Angle, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_CONE(Condition, LogOwner, CategoryName, Verbosity, Orgin, Direction, Length, Angle, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CONE(Actor, CategoryName, Verbosity, Orgin, Direction, Length, Angle, Color, Format, ##__VA_ARGS__);} 
// Cylinder shape
#define UE_VLOG_CYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Start, End, Radius, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_CYLINDER(Condition, LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CYLINDER(LogOwner, CategoryName, Verbosity, Start, End, Radius, Color, Format, ##__VA_ARGS__);} 
// Capsule shape
#define UE_VLOG_CAPSULE(LogOwner, CategoryName, Verbosity, Center, HalfHeight, Radius, Rotation, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Center, HalfHeight, Radius, Rotation, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_CAPSULE(Condition, LogOwner, CategoryName, Verbosity, Center, HalfHeight, Radius, Rotation, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CAPSULE(LogOwner, CategoryName, Verbosity, Center, HalfHeight, Radius, Rotation, Color, Format, ##__VA_ARGS__);} 
// Histogram data for 2d graphs 
#define UE_VLOG_HISTOGRAM(LogOwner, CategoryName, Verbosity, GraphName, DataName, Data) if(FVisualLogger::IsRecording()) FVisualLogger::HistogramDataLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, GraphName, DataName, Data, FColor::White, TEXT(""))
#define UE_CVLOG_HISTOGRAM(Condition, LogOwner, CategoryName, Verbosity, GraphName, DataName, Data) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_HISTOGRAM(LogOwner, CategoryName, Verbosity, GraphName, DataName, Data);} 
// NavArea or vertically pulled convex shape
#define UE_VLOG_PULLEDCONVEX(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::NavAreaShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_PULLEDCONVEX(Condition, LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_PULLEDCONVEX(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ##__VA_ARGS__);}
// regular 3d mesh shape to log
#define UE_VLOG_MESH(LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, Format, ...) if (FVisualLogger::IsRecording()) FVisualLogger::GeometryShapeLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Vertices, Indices, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_MESH(Condition, LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, Format, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_MESH(LogOwner, CategoryName, Verbosity, Vertices, Indices, Color, Format, ##__VA_ARGS__);}
// 2d convex poly shape
#define UE_VLOG_CONVEXPOLY(LogOwner, CategoryName, Verbosity, Points, Color, Format, ...) if(FVisualLogger::IsRecording()) FVisualLogger::GeometryConvexLogf(LogOwner, CategoryName, ELogVerbosity::Verbosity, Points, Color, Format, ##__VA_ARGS__)
#define UE_CVLOG_CONVEXPOLY(Condition, LogOwner, CategoryName, Verbosity, Points, Color, Format, ...)  if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_CONVEXPOLY(LogOwner, CategoryName, Verbosity, Points, Color, Format, ##__VA_ARGS__);}


#define DECLARE_VLOG_EVENT(EventName) extern FVisualLogEventBase EventName;
#define DEFINE_VLOG_EVENT(EventName, Verbosity, UserFriendlyDesc) FVisualLogEventBase EventName(TEXT(#EventName), TEXT(UserFriendlyDesc), ELogVerbosity::Verbosity); 

#define UE_VLOG_EVENTS(LogOwner, TagNameToLog, ...) if(FVisualLogger::IsRecording()) FVisualLogger::EventLog(LogOwner, TagNameToLog, ##__VA_ARGS__)
#define UE_CVLOG_EVENTS(Condition, LogOwner, TagNameToLog, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_EVENTS(LogOwner, TagNameToLog, ##__VA_ARGS__);}
#define UE_VLOG_EVENT_WITH_DATA(LogOwner, LogEvent, ...) if(FVisualLogger::IsRecording()) FVisualLogger::EventLog(LogOwner, LogEvent, ##__VA_ARGS__)
#define UE_CVLOG_EVENT_WITH_DATA(Condition, LogOwner, LogEvent, ...) if(FVisualLogger::IsRecording() && Condition) {UE_VLOG_EVENT_WITH_DATA(LogOwner, LogEvent, ##__VA_ARGS__);}

#define UE_IFVLOG(__code_block__) if( FVisualLogger::IsRecording() ) { __code_block__; }

#else
#define REDIRECT_TO_VLOG(Dest)
#define REDIRECT_OBJECT_TO_VLOG(Src, Dest)
#define CONNECT_WITH_VLOG(Dest)
#define CONNECT_OBJECT_WITH_VLOG(Src, Dest)

#define UE_VLOG(Actor, CategoryName, Verbosity, Format, ...)
#define UE_CVLOG(Condition, Actor, CategoryName, Verbosity, Format, ...)
#define UE_VLOG_UELOG(LogOwner, CategoryName, Verbosity, Format, ...)
#define UE_CVLOG_UELOG(Condition, Actor, CategoryName, Verbosity, Format, ...)
#define UE_VLOG_SEGMENT(Actor, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, DescriptionFormat, ...)
#define UE_CVLOG_SEGMENT(Condition, Actor, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, DescriptionFormat, ...)
#define UE_VLOG_SEGMENT_THICK(Actor, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, DescriptionFormat, ...)
#define UE_CVLOG_SEGMENT_THICK(Condition, Actor, CategoryName, Verbosity, SegmentStart, SegmentEnd, Color, Thickness, DescriptionFormat, ...)
#define UE_VLOG_LOCATION(Actor, CategoryName, Verbosity, Location, Radius, Color, DescriptionFormat, ...)
#define UE_CVLOG_LOCATION(Condition, Actor, CategoryName, Verbosity, Location, Radius, Color, DescriptionFormat, ...)
#define UE_VLOG_BOX(Actor, CategoryName, Verbosity, Box, Color, DescriptionFormat, ...) 
#define UE_CVLOG_BOX(Condition, Actor, CategoryName, Verbosity, Box, Color, DescriptionFormat, ...) 
#define UE_VLOG_OBOX(LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...)  
#define UE_CVLOG_OBOX(Condition, LogOwner, CategoryName, Verbosity, Box, Matrix, Color, Format, ...) 
#define UE_VLOG_CONE(Object, CategoryName, Verbosity, Orgin, Direction, Length, Angle, Color, DescriptionFormat, ...)
#define UE_CVLOG_CONE(Condition, Object, CategoryName, Verbosity, Orgin, Direction, Length, Angle, Color, DescriptionFormat, ...)
#define UE_VLOG_CYLINDER(Object, CategoryName, Verbosity, Start, End, Radius, Color, DescriptionFormat, ...)
#define UE_CVLOG_CYLINDER(Condition, Object, CategoryName, Verbosity, Start, End, Radius, Color, DescriptionFormat, ...)
#define UE_VLOG_CAPSULE(Object, CategoryName, Verbosity, Center, HalfHeight, Radius, Rotation, Color, DescriptionFormat, ...)
#define UE_CVLOG_CAPSULE(Condition, Object, CategoryName, Verbosity, Center, HalfHeight, Radius, Rotation, Color, DescriptionFormat, ...)
#define UE_VLOG_HISTOGRAM(Actor, CategoryName, Verbosity, GraphName, DataName, Data)
#define UE_CVLOG_HISTOGRAM(Condition, Actor, CategoryName, Verbosity, GraphName, DataName, Data)
#define UE_VLOG_PULLEDCONVEX(LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...)
#define UE_CVLOG_PULLEDCONVEX(Condition, LogOwner, CategoryName, Verbosity, ConvexPoints, MinZ, MaxZ, Color, Format, ...)
#define UE_VLOG_MESH(LogOwner, CategoryName, Verbosity, Vertices, Indexes, Color, Format, ...) 
#define UE_CVLOG_MESH(Condition, LogOwner, CategoryName, Verbosity, Vertices, Indexes, Color, Format, ...) 
#define UE_VLOG_CONVEXPOLY(LogOwner, CategoryName, Verbosity, Points, Color, Format, ...) 
#define UE_CVLOG_CONVEXPOLY(Condition, LogOwner, CategoryName, Verbosity, Points, Color, Format, ...) 

#define DECLARE_VLOG_EVENT(EventName)
#define DEFINE_VLOG_EVENT(EventName, Verbosity, UserFriendlyDesc)
#define UE_VLOG_EVENTS(LogOwner, TagNameToLog, ...) 
#define UE_CVLOG_EVENTS(Condition, LogOwner, TagNameToLog, ...) 
#define UE_VLOG_EVENT_WITH_DATA(LogOwner, LogEvent, ...)
#define UE_CVLOG_EVENT_WITH_DATA(Condition, LogOwner, LogEvent, ...)

#define UE_IFVLOG(__code_block__)

#endif //ENABLE_VISUAL_LOG

// helper macros
#define TEXT_EMPTY TEXT("")
#define TEXT_NULL TEXT("NULL")
#define TEXT_TRUE TEXT("TRUE")
#define TEXT_FALSE TEXT("FALSE")
#define TEXT_CONDITION(Condition) ((Condition) ? TEXT_TRUE : TEXT_FALSE)

class FVisualLogger;

DECLARE_LOG_CATEGORY_EXTERN(LogVisual, Display, All);


#if ENABLE_VISUAL_LOG

class FVisualLogDevice;
class FVisualLogExtensionInterface;
class UObject;
class UWorld;
struct FLogCategoryBase;

DECLARE_DELEGATE_RetVal(FString, FVisualLogFilenameGetterDelegate);

class ENGINE_API FVisualLogger : public FOutputDevice
{
public:
	// Regular text log
	VARARG_DECL(static void, static void, return, CategorizedLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity));
	// Segment log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Start) VARARG_EXTRA(const FVector& End) VARARG_EXTRA(const FColor& Color) VARARG_EXTRA(const uint16 Thickness), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Start) VARARG_EXTRA(End) VARARG_EXTRA(Color) VARARG_EXTRA(Thickness));
	// Location/Shpere log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Location) VARARG_EXTRA(float Radius) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Location) VARARG_EXTRA(Radius) VARARG_EXTRA(Color));
	// Box log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FBox& Box) VARARG_EXTRA(const FMatrix& Matrix) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Box) VARARG_EXTRA(Matrix) VARARG_EXTRA(Color));
	// Cone log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Orgin) VARARG_EXTRA(const FVector& Direction) VARARG_EXTRA(const float Length) VARARG_EXTRA(const float Angle) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Orgin) VARARG_EXTRA(Direction) VARARG_EXTRA(Length) VARARG_EXTRA(Angle) VARARG_EXTRA(Color));
	// Cylinder log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Start) VARARG_EXTRA(const FVector& End) VARARG_EXTRA(const float Radius) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Start) VARARG_EXTRA(End) VARARG_EXTRA(Radius) VARARG_EXTRA(Color));
	// Capsule log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Center) VARARG_EXTRA(float HalfHeight) VARARG_EXTRA(float Radius) VARARG_EXTRA(const FQuat & Rotation) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Center) VARARG_EXTRA(HalfHeight) VARARG_EXTRA(Radius) VARARG_EXTRA(Rotation) VARARG_EXTRA(Color));
	// NavArea/Extruded convex log
	VARARG_DECL(static void, static void, return, NavAreaShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const TArray<FVector> &ConvexPoints) VARARG_EXTRA(float MinZ) VARARG_EXTRA(float MaxZ) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(ConvexPoints) VARARG_EXTRA(MinZ) VARARG_EXTRA(MaxZ) VARARG_EXTRA(Color));
	// 3d Mesh log
	VARARG_DECL(static void, static void, return, GeometryShapeLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const TArray<FVector> &Vertices) VARARG_EXTRA(const TArray<int32> &Indices) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Vertices) VARARG_EXTRA(Indices) VARARG_EXTRA(Color));
	// 2d Convex shape
	VARARG_DECL(static void, static void, return, GeometryConvexLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const TArray<FVector> &Points) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(Points) VARARG_EXTRA(Color));
	//Histogram data
	VARARG_DECL(static void, static void, return, HistogramDataLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const UObject* LogOwner) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(FName GraphName) VARARG_EXTRA(FName DataName) VARARG_EXTRA(const FVector2D& Data) VARARG_EXTRA(const FColor& Color), VARARG_EXTRA(LogOwner) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) VARARG_EXTRA(GraphName) VARARG_EXTRA(DataName) VARARG_EXTRA(Data) VARARG_EXTRA(Color));
	// Navigation data debug snapshot
	static void NavigationDataDump(const UObject* LogOwner, const FLogCategoryBase& Category, const ELogVerbosity::Type Verbosity, const FBox& Box);

	/** Log events */
	static void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FName EventTag2 = NAME_None, const FName EventTag3 = NAME_None, const FName EventTag4 = NAME_None, const FName EventTag5 = NAME_None, const FName EventTag6 = NAME_None);
	static void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2);
	static void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3);
	static void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4);
	static void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5);
	static void EventLog(const UObject* LogOwner, const FName EventTag1, const FVisualLogEventBase& Event1, const FVisualLogEventBase& Event2, const FVisualLogEventBase& Event3, const FVisualLogEventBase& Event4, const FVisualLogEventBase& Event5, const FVisualLogEventBase& Event6);
	
	static void EventLog(const UObject* LogOwner, const FVisualLogEventBase& Event1, const FName EventTag1 = NAME_None, const FName EventTag2 = NAME_None, const FName EventTag3 = NAME_None, const FName EventTag4 = NAME_None, const FName EventTag5 = NAME_None, const FName EventTag6 = NAME_None);

	// static getter
	static FVisualLogger& Get()
	{
		static FVisualLogger GVisLog;
		return GVisLog;
	}

	virtual ~FVisualLogger() {}

	// called on engine shutdown to flush all, etc.
	virtual void Shutdown();

	// Removes all logged data 
	void Cleanup(UWorld* OldWorld, bool bReleaseMemory = false);

	/** Set log owner redirection from one object to another, to combine logs */
	static void Redirect(UObject* FromObject, UObject* ToObject);

	/** find and return redirection object for given object*/
	static UObject* FindRedirection(const UObject* Object);

	/** blocks all categories from logging. It can be bypassed with white list for categories */
	void BlockAllCategories(bool InBlock) { bBlockedAllCategories = InBlock; }

	/** checks if all categories are blocked */
	bool IsBlockedForAllCategories() const { return !!bBlockedAllCategories; }

	/** Returns white list for modifications */
	const TArray<FName>& GetWhiteList() const { return CategoriesWhitelist; }

	bool IsWhiteListed(const FName& Name) const { return CategoriesWhitelist.Find(Name) != INDEX_NONE; }

	void AddCategoryToWhitelist(FName Category) { CategoriesWhitelist.AddUnique(Category); }

	DEPRECATED(4.12, "Please use the AddCategoryToWhitelist instead")
	void AddCategortyToWhiteList(FName Category) { AddCategoryToWhitelist(Category); }

	void ClearWhiteList() { CategoriesWhitelist.Reset(); }

	/** Generates and returns Id unique for given timestamp - used to connect different logs between (ex. text log with geometry shape) */
	int32 GetUniqueId(float Timestamp);

	/** Starts visual log collecting and recording */
	void SetIsRecording(bool InIsRecording);
	/** return information is vlog recording is enabled or not */
	FORCEINLINE static bool IsRecording() { return !!bIsRecording; }

	/** Starts visual log collecting and recording */
	void SetIsRecordingToFile(bool InIsRecording);
	/** return information is vlog recording is enabled or not */
	bool IsRecordingToFile() const { return !!bIsRecordingToFile; }
	/** disables recording to file and discards all data without saving it to file */
	void DiscardRecordingToFile();

	void SetIsRecordingOnServer(bool IsRecording) { bIsRecordingOnServer = IsRecording; }
	bool IsRecordingOnServer() const { return !!bIsRecordingOnServer; }

	/** Add visual logger output device */
	void AddDevice(FVisualLogDevice* InDevice) { OutputDevices.AddUnique(InDevice); }
	/** Remove visual logger output device */
	void RemoveDevice(FVisualLogDevice* InDevice) { OutputDevices.RemoveSwap(InDevice); }
	/** Remove visual logger output device */
	const TArray<FVisualLogDevice*>& GetDevices() const { return OutputDevices; }
	/** Check if log category can be recorded, verify before using GetEntryToWrite! */
	bool IsCategoryLogged(const FLogCategoryBase& Category) const;
	/** Returns  current entry for given TimeStap or creates another one  but first it serialize previous 
	 *	entry as completed to vislog devices. Use VisualLogger::DontCreate to get current entry without serialization
	 *	@note this function can return null */
	FVisualLogEntry* GetEntryToWrite(const UObject* Object, float TimeStamp, ECreateIfNeeded ShouldCreate = ECreateIfNeeded::Create);
	/** Retrieves last used entry for given UObject
	 *	@note this function can return null */
	FVisualLogEntry* GetLastEntryForObject(const UObject* Object);
	/** flush and serialize data if timestamp allows it */
	virtual void Flush() override;

	/** FileName getter to set project specific file name for vlogs - highly encouradged to use FVisualLogFilenameGetterDelegate::CreateUObject with this */
	void SetLogFileNameGetter(const FVisualLogFilenameGetterDelegate& InLogFileNameGetter) { LogFileNameGetter = InLogFileNameGetter; }

	/** Register extension to use by LogVisualizer  */
	void RegisterExtension(FName TagName, FVisualLogExtensionInterface* ExtensionInterface) { check(AllExtensions.Contains(TagName) == false); AllExtensions.Add(TagName, ExtensionInterface); }
	/**  Removes previously registered extension */
	void UnregisterExtension(FName TagName, FVisualLogExtensionInterface* ExtensionInterface) { AllExtensions.Remove(TagName); }
	/** returns extension identified by given tag */
	FVisualLogExtensionInterface* GetExtensionForTag(const FName TagName) const { return AllExtensions.Contains(TagName) ? AllExtensions[TagName] : nullptr; }
	/** Returns reference to map with all registered extension */
	const TMap<FName, FVisualLogExtensionInterface*>& GetAllExtensions() const { return AllExtensions; }

	/** internal check for each usage of visual logger */
	static bool CheckVisualLogInputInternal(const UObject* Object, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, UWorld **World, FVisualLogEntry **CurrentEntry);

	typedef TMap<UObject*, TArray<TWeakObjectPtr<const UObject> > > RedirectionMapType;
	static RedirectionMapType& GetRedirectionMap(const UObject* InObject);

	typedef TMap<const UObject*, TWeakObjectPtr<const UWorld> > FObjectToWorldMapType;
	FObjectToWorldMapType& GetObjectToWorldMap() { return ObjectToWorldMap; }

	void AddWhitelistedClass(UClass& InClass);
	bool IsClassWhitelisted(const UClass& InClass) const;

	void AddWhitelistedObject(const UObject& InObject);
	void ClearObjectWhitelist();
	bool IsObjectWhitelisted(const UObject* InObject) const;

private:
	FVisualLogger();
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override { ensureMsgf(0, TEXT("Regular serialize is forbiden for visual logs")); }

protected:
	/** Array of output devices to redirect to */
	TArray<FVisualLogDevice*> OutputDevices;
	// Map for inter-objects redirections
	static TMap<const UWorld*, RedirectionMapType> WorldToRedirectionMap;

	// white-listed classes - only instances of these classes will be logged. 
	// if ClassWhitelist is empty (default) everything will log
	TArray<UClass*> ClassWhitelist;

	// white-listed objects - takes priority over class whitelist and should be used to create exceptions in it
	// if ObjectWhitelist is empty (default) everything will log
	// do NOT read from those pointers, they can be invalid!
	TSet<const UObject*> ObjectWhitelist;

	// white list of categories to bypass blocking
	TArray<FName>	CategoriesWhitelist;
	// Visual Logger extensions map
	TMap<FName, FVisualLogExtensionInterface*> AllExtensions;
	// last generated unique id for given times tamp
	TMap<float, int32> LastUniqueIds;
	// Current entry with all data
	TMap<const UObject*, FVisualLogEntry>	CurrentEntryPerObject;
	// Map to contain names for Objects (they can be destroyed after while)
	TMap<const UObject*, FName> ObjectToNameMap;
	// Map to contain class names for Objects (they can be destroyed after while)
	TMap<const UObject*, FName> ObjectToClassNameMap;
	// Map to contain information about pointers in game
	TMap<const UObject*, TWeakObjectPtr<const UObject> > ObjectToPointerMap;
	// Cached map to world information because it's just raw pointer and not real object
	FObjectToWorldMapType ObjectToWorldMap;
	// if set all categories are blocked from logging
	int32 bBlockedAllCategories : 1;
	// if set we are recording to file
	int32 bIsRecordingToFile : 1;
	// variable set (from cheat manager) when logging is active on server
	int32 bIsRecordingOnServer : 1;
	// start recording time
	float StartRecordingToFileTime;
	/** Delegate to set project specific file name for vlogs */
	FVisualLogFilenameGetterDelegate LogFileNameGetter;
	/** Specifies if the Binary Device is being used */
	bool UseBinaryFileDevice;
	/** Handle for timer used to serialize all waiting logs */
	FTimerHandle VisualLoggerCleanupTimerHandle;

	// if set we are recording and collecting all vlog data
	static int32 bIsRecording;
};

// Unfortunately needs to be a #define since it uses GET_VARARGS_RESULT which uses the va_list stuff which operates on the
// current function, so we can't easily call a function
#define COLLAPSED_LOGF(SerializeFunc) \
	int32	BufferSize	= 1024; \
	TCHAR*	Buffer		= nullptr; \
	int32	Result		= -1; \
	/* allocate some stack space to use on the first pass, which matches most strings */ \
	TCHAR	StackBuffer[512]; \
	TCHAR*	AllocatedBuffer = nullptr; \
	\
	/* first, try using the stack buffer */ \
	Buffer = StackBuffer; \
	GET_VARARGS_RESULT( Buffer, ARRAY_COUNT(StackBuffer), ARRAY_COUNT(StackBuffer) - 1, Fmt, Fmt, Result ); \
	\
	/* if that fails, then use heap allocation to make enough space */ \
			while(Result == -1) \
						{ \
		FMemory::SystemFree(AllocatedBuffer); \
		/* We need to use malloc here directly as GMalloc might not be safe. */ \
		Buffer = AllocatedBuffer = (TCHAR*) FMemory::SystemMalloc( BufferSize * sizeof(TCHAR) ); \
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result ); \
		BufferSize *= 2; \
						}; \
	Buffer[Result] = 0; \
	; \
	\
	SerializeFunc; \
	FMemory::SystemFree(AllocatedBuffer);

inline
VARARG_BODY(void, FVisualLogger::CategorizedLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddText(Buffer, Category.GetCategoryName(), Verbosity);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Start) VARARG_EXTRA(const FVector& End) VARARG_EXTRA(const FColor& Color) VARARG_EXTRA(const uint16 Thickness))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Start, End, Category.GetCategoryName(), Verbosity, Color, Buffer, Thickness);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Location) VARARG_EXTRA(float Radius) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Location, Category.GetCategoryName(), Verbosity, Color, Buffer, Radius);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FBox& Box) VARARG_EXTRA(const FMatrix& Matrix) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Box, Matrix, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Orgin) VARARG_EXTRA(const FVector& Direction) VARARG_EXTRA(const float Length) VARARG_EXTRA(const float Angle)  VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Orgin, Direction, Length, Angle, Angle, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Start) VARARG_EXTRA(const FVector& End) VARARG_EXTRA(const float Radius) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Start, End, Radius, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const FVector& Center) VARARG_EXTRA(float HalfHeight) VARARG_EXTRA(float Radius) VARARG_EXTRA(const FQuat& Rotation) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Center, HalfHeight, Radius, Rotation, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

inline
VARARG_BODY(void, FVisualLogger::NavAreaShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const TArray<FVector> &ConvexPoints) VARARG_EXTRA(float MinZ) VARARG_EXTRA(float MaxZ) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(ConvexPoints, MinZ, MaxZ, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryShapeLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const TArray<FVector> &Vertices) VARARG_EXTRA(const TArray<int32> &Indices) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddElement(Vertices, Indices, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}

inline
VARARG_BODY(void, FVisualLogger::GeometryConvexLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(const TArray<FVector> &Points) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddConvexElement(Points, Category.GetCategoryName(), Verbosity, Color, Buffer);
	);
}


inline
VARARG_BODY(void, FVisualLogger::HistogramDataLogf, const TCHAR*, VARARG_EXTRA(const UObject* Object) VARARG_EXTRA(const FLogCategoryBase& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity) VARARG_EXTRA(FName GraphName) VARARG_EXTRA(FName DataName) VARARG_EXTRA(const FVector2D& Data) VARARG_EXTRA(const FColor& Color))
{
	SCOPE_CYCLE_COUNTER(STAT_VisualLog);
	UWorld *World = nullptr;
	FVisualLogEntry *CurrentEntry = nullptr;
	if (CheckVisualLogInputInternal(Object, Category, Verbosity, &World, &CurrentEntry) == false)
	{
		return;
	}

	COLLAPSED_LOGF(
		CurrentEntry->AddHistogramData(Data, Category.GetCategoryName(), Verbosity, GraphName, DataName);
	);
}

#undef COLLAPSED_LOGF

#endif //ENABLE_VISUAL_LOG
