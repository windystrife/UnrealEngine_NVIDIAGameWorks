// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Evaluation/Blending/MovieSceneBlendType.h"
#include "InlineValue.h"
#include "MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationScope.h"


template<typename DataType> struct TMovieSceneInitialValueStore;

/**
 * Implementation of custom blend logic should be as follows (using doubles as an example).
 * Specializing TBlendableTokenTraits for a particular input data type causes WorkingDataType to be used during the blending operation.
 * Where WorkingDataType belongs to a namespace, ADL will be employed to discover any relevant overloads for BlendValue that match the necessary types.
 * This allows blending of any arbitrary type into the WorkingDataType.

	namespace MovieScene
	{
		// Define a custom namespaced type that will be used to calculate blends between doubles
		struct FBlendableDouble
		{
			FBlendableDouble()
				: AbsoluteTotal(0.0), AdditiveTotal(0.0)
			{}

			double AbsoluteTotal;
			double AdditiveTotal;

			TOptional<float> TotalWeight;

			double Resolve(TMovieSceneInitialValueStore<int32>& InitialValueStore)
			{
				if (TotalWeight.IsSet())
				{
					if (TotalWeight.GetValue() == 0.f)
					{
						AbsoluteTotal = InitialValueStore.GetInitialValue();
					}
					else
					{
						AbsoluteTotal /= TotalWeight.GetValue();
					}
				}

				return AbsoluteTotal + AdditiveTotal;
			}
		};

		void BlendValue(FBlendableDouble& OutBlend, double InValue, float Weight, EMovieSceneBlendType BlendType, TMovieSceneInitialValueStore<double>& InitialValueStore)
		{
			if (BlendType == EMovieSceneBlendType::Absolute || BlendType == EMovieSceneBlendType::Relative)
			{
				if (BlendType == EMovieSceneBlendType::Relative)
				{
					OutBlend.AbsoluteTotal += (InitialValueStore.GetInitialValue() + InValue) * Weight;
				}
				else
				{
					OutBlend.AbsoluteTotal += InValue * Weight;
				}

				OutBlend.TotalWeight = OutBlend.TotalWeight.Get(0.f) + Weight;
			}
			else if (BlendType == EMovieSceneBlendType::Additive)
			{
				OutBlend.AdditiveTotal += InValue * Weight;
			}
		}
	}
	template<> struct TBlendableTokenTraits<double> { typedef MovieScene::FBlendableDouble WorkingDataType; };
 */

namespace MovieScene
{
	template<typename InType, typename WorkingDataType, typename SourceDataType>
	void BlendValue(WorkingDataType& OutBlend, InType InValue, float Weight, EMovieSceneBlendType BlendType, TMovieSceneInitialValueStore<SourceDataType>& InitialValueStore)
	{
		// Always assert on instantiation
		static_assert(TIsSame<WorkingDataType, void>::Value, "BlendValue must be implemented for the specified types in order to blend them with Sequencer.");
	}
}

template<typename DataType> struct TBlendableTokenTraits { typedef DataType WorkingDataType; };

/**
 * Templated structure that encapsulates any blendable data type, and the information required to blend it
 */
template<typename DataType>
struct TBlendableToken
{
	typedef typename TBlendableTokenTraits<DataType>::WorkingDataType WorkingDataType;

	/** Default construction */
	TBlendableToken() = default;

	/** Construction from a value, blend method, and a weight. Scope and bias to be populated later */
	template<typename T>
	TBlendableToken(T&& InValue, EMovieSceneBlendType InBlendType, float InWeight = 1.f)
		: Value(TData<typename TDecay<T>::Type>(Forward<T>(InValue)))
		, HierarchicalBias(0)
		, Weight(InWeight)
		, BlendType(InBlendType)
	{}

	/** Construction from a value, scope, context, blend method, and a weight */
	template<typename T>
	TBlendableToken(T&& InValue, const FMovieSceneEvaluationScope& InCurrentScope, const FMovieSceneContext& InContext, EMovieSceneBlendType InBlendType, float InWeight = 1.f)
		: Value(TData<typename TDecay<T>::Type>(Forward<T>(InValue)))
		, AnimatingScope(InCurrentScope)
		, HierarchicalBias(InContext.GetHierarchicalBias())
		, Weight(InWeight)
		, BlendType(InBlendType)
	{}

	/** Copying is disabled */
	TBlendableToken(const TBlendableToken&) = delete;
	TBlendableToken& operator=(const TBlendableToken&) = delete;

	/** Move construction/assignment */
	TBlendableToken(TBlendableToken&&) = default;
	TBlendableToken& operator=(TBlendableToken&&) = default;

	/**
	 * Add this value into the specified cumulative blend
	 *
	 * @param CumulativeBlend		The working structure to add our value onto
	 */
	void AddTo(WorkingDataType& CumulativeBlend, TMovieSceneInitialValueStore<DataType>& InitialValueStore) const
	{
		check(Value.IsValid());
		Value->AddTo(CumulativeBlend, Weight, BlendType, InitialValueStore);
	}

private:

	/** Base class for all value types */
	struct IData
	{
		virtual ~IData() {}
		virtual void AddTo(WorkingDataType& CumulativeBlend, float Weight, EMovieSceneBlendType BlendType, TMovieSceneInitialValueStore<DataType>& InitialValueStore) const = 0;
	};

	/** Templated value data for any other type */
	template<typename T>
	struct TData : IData
	{
		TData(T In) : Data(MoveTemp(In)) {}

		virtual void AddTo(WorkingDataType& CumulativeBlend, float InWeight, EMovieSceneBlendType InBlendType, TMovieSceneInitialValueStore<DataType>& InitialValueStore) const
		{
			// Use the default BlendValue function, or any other BlendValue function found through ADL on WorkingDataType
			using MovieScene::BlendValue;
			BlendValue(CumulativeBlend, Data, InWeight, InBlendType, InitialValueStore);
		}

		/** The actual value to blend */
		T Data;
	};

	/** The actual user provided value data, stored as inline bytes (unless it's > sizeof(DataType)) */
	TInlineValue<IData, sizeof(DataType)> Value;

public:

	/** The scope from which this token was generated. Used for restoring pre-animated state where necessary. */
	FMovieSceneEvaluationScope AnimatingScope;

	/** The hierarchical bias for this template instance */
	int32 HierarchicalBias;

	/** Weight to apply to the value */
	float Weight;

	/** Enumeration specifying how this token should be blended */
	EMovieSceneBlendType BlendType;
};
