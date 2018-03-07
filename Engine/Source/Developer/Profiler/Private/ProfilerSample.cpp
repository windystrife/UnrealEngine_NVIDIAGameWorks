// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerSample.h"


#define LOCTEXT_NAMESPACE "ProfilerSample"


FString EProfilerSampleTypes::ToName(Type ProfilerSampleType)
{
	switch(ProfilerSampleType)
	{
		case HierarchicalTime:
			return LOCTEXT("StatType_Name_Hierarchical","Hier").ToString();
		case NumberInt:
			return LOCTEXT("StatType_Name_NumberInt","Int").ToString();
		case NumberFloat:
			return LOCTEXT("StatType_Name_NumberFloat","Float").ToString();
		case Memory:
			return LOCTEXT("StatType_Name_Memory","Mem").ToString();
		default:
			return LOCTEXT("StatType_InvalidOrMax","Unknown profiler sample type").ToString();
	}
}


FString EProfilerSampleTypes::ToDescription(Type ProfilerSampleType)
{
	switch(ProfilerSampleType)
	{
		case HierarchicalTime:
			return LOCTEXT("StatType_Desc_Hierarchical","Hierarchical - Displayed as a time and call count").ToString();
		case NumberInt:
			return LOCTEXT("StatType_Desc_NumberInt","Numerical - Displayed as a integer number").ToString();
		case NumberFloat:
			return LOCTEXT("StatType_Desc_NumberFloat","Numerical - Displayed as a floating number").ToString();
		case Memory:
			return LOCTEXT("StatType_Desc_Memory","Memory - Displayed as a human readable data counter").ToString();
		default:
			return LOCTEXT("StatDesc_InvalidOrMax","Unknown profiler sample type").ToString();
	}
}


const FProfilerSample FProfilerSample::Invalid;


void FProfilerSample::FixupChildrenOrdering(const TMap<uint32,uint32>& ChildrenOrderingIndices)
{
	FIndicesArray ChildrenIndices;
	for(auto It = ChildrenOrderingIndices.CreateConstIterator(); It; ++It)
	{
		ChildrenIndices.Add(_ChildrenIndices[It.Key()]);
	}

	_ChildrenIndices = ChildrenIndices;
}


#undef LOCTEXT_NAMESPACE
