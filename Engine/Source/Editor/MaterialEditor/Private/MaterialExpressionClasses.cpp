// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialExpressionClasses.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialExpression.h"
#include "UObject/Package.h"
#include "MaterialEditor.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Preferences/MaterialEditorOptions.h"

#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionParameter.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MaterialExpressionClasses
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MaterialExpressionClasses::MaterialExpressionClasses()
	: bInitialized( false )
{

}

MaterialExpressionClasses::~MaterialExpressionClasses()
{

}

MaterialExpressionClasses* MaterialExpressionClasses::Get()
{
	static MaterialExpressionClasses Inst;
	Inst.InitMaterialExpressionClasses();
	return &Inst;
}

const UStruct* MaterialExpressionClasses::GetExpressionInputStruct()
{
	static const UStruct* ExpressionInputStruct =
		CastChecked<UStruct>( StaticFindObject( UStruct::StaticClass(), ANY_PACKAGE, TEXT("ExpressionInput")) );
	check( ExpressionInputStruct );
	return ExpressionInputStruct;
}

FCategorizedMaterialExpressionNode* MaterialExpressionClasses::GetCategoryNode(const FText& InCategoryName, bool bCreate)
{
	for (int32 CheckIndex = 0; CheckIndex < CategorizedExpressionClasses.Num(); CheckIndex++)
	{
		FCategorizedMaterialExpressionNode& CheckNode = CategorizedExpressionClasses[CheckIndex];
		if (CheckNode.CategoryName.EqualTo(InCategoryName))
		{
			return &CheckNode;
		}
	}

	if (bCreate == true)
	{
		FCategorizedMaterialExpressionNode* NewNode = new(CategorizedExpressionClasses)FCategorizedMaterialExpressionNode;
		check(NewNode);

		NewNode->CategoryName = InCategoryName;
		return NewNode;
	}

	return NULL;
}

void MaterialExpressionClasses::InitMaterialExpressionClasses()
{
	if( !bInitialized )
	{
		UMaterialEditorOptions* TempEditorOptions = NewObject<UMaterialEditorOptions>();
		UClass* BaseType = UMaterialExpression::StaticClass();
		if( BaseType )
		{
			TArray<UStructProperty*>	ExpressionInputs;
			const UStruct*				ExpressionInputStruct = GetExpressionInputStruct();

			for( TObjectIterator<UClass> It ; It ; ++It )
			{
				UClass* Class = *It;
				if( !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) )
				{
					if( Class->IsChildOf(UMaterialExpression::StaticClass()) )
					{
						ExpressionInputs.Empty();

						// Exclude comments from the expression list, as well as the base parameter expression, as it should not be used directly
						if ( Class != UMaterialExpressionComment::StaticClass() 
							&& Class != UMaterialExpressionParameter::StaticClass() )
						{
							FMaterialExpression MaterialExpression;
							// Trim the material expression name and add it to the list used for filtering.
							static const FString ExpressionPrefix = TEXT("MaterialExpression");
							FString ClassName = *Class->GetName();

							if (Class->HasMetaData("DisplayName"))
							{
								ClassName = Class->GetDisplayNameText().ToString();
							}

							if (ClassName.StartsWith(ExpressionPrefix, ESearchCase::CaseSensitive))
							{
								ClassName = ClassName.Mid(ExpressionPrefix.Len());
							}
							MaterialExpression.Name = ClassName;
							MaterialExpression.MaterialClass = Class;
							UMaterialExpression* TempObject = Cast<UMaterialExpression>(Class->GetDefaultObject());
							if (TempObject)
							{
								MaterialExpression.CreationDescription = TempObject->GetCreationDescription();
								MaterialExpression.CreationName = TempObject->GetCreationName();
							}

							AllExpressionClasses.Add(MaterialExpression);

							// Initialize the expression class input map.							
							for( TFieldIterator<UStructProperty> InputIt(Class) ; InputIt ; ++InputIt )
							{
								UStructProperty* StructProp = *InputIt;
								if( StructProp->Struct == ExpressionInputStruct )
								{
									ExpressionInputs.Add( StructProp );
								}
							}

							// See if it is in the favorites array...
							for (int32 FavoriteIndex = 0; FavoriteIndex < TempEditorOptions->FavoriteExpressions.Num(); FavoriteIndex++)
							{
								if (Class->GetName() == TempEditorOptions->FavoriteExpressions[FavoriteIndex])
								{
									FavoriteExpressionClasses.AddUnique(MaterialExpression);
								}
							}

							// Category fill...
							if (TempObject)
							{
								if (TempObject->MenuCategories.Num() == 0)
								{
									UnassignedExpressionClasses.Add(MaterialExpression);
								}
								else
								{
									for (int32 CategoryIndex = 0; CategoryIndex < TempObject->MenuCategories.Num(); CategoryIndex++)
									{
										FCategorizedMaterialExpressionNode* CategoryNode = GetCategoryNode(TempObject->MenuCategories[CategoryIndex], true);
										check(CategoryNode);

										CategoryNode->MaterialExpressions.AddUnique(MaterialExpression);
									}
								}
							}
						}
					}
				}
			}
		}

		struct FCompareFMaterialExpression
		{
			FORCEINLINE bool operator()( const FMaterialExpression& A, const FMaterialExpression& B ) const
			{
				return A.Name < B.Name;
			}
		};
		AllExpressionClasses.Sort(FCompareFMaterialExpression());
		struct FCompareFCategorizedMaterialExpressionNode
		{
			FORCEINLINE bool operator()( const FCategorizedMaterialExpressionNode& A, const FCategorizedMaterialExpressionNode& B ) const
			{
				return A.CategoryName.CompareTo(B.CategoryName) < 0;
			}
		};
		CategorizedExpressionClasses.Sort( FCompareFCategorizedMaterialExpressionNode() );

		bInitialized = true;
	}
}

bool MaterialExpressionClasses::IsMaterialExpressionInFavorites(UMaterialExpression* InExpression)
{
	for (int32 CheckIndex = 0; CheckIndex < FavoriteExpressionClasses.Num(); CheckIndex++)
	{
		if (FavoriteExpressionClasses[CheckIndex].MaterialClass == InExpression->GetClass())
		{
			return true;
		}
	}

	return false;
}

void MaterialExpressionClasses::RemoveMaterialExpressionFromFavorites(UClass* InExpression)
{
	for (int32 i = 0; i < FavoriteExpressionClasses.Num(); ++i)
	{
		if (FavoriteExpressionClasses[i].MaterialClass == InExpression)
		{
			FavoriteExpressionClasses.RemoveAt(i);
		}
	}
}

void MaterialExpressionClasses::AddMaterialExpressionToFavorites(UClass* InExpression)
{
	bool bIsUnique = true;
	for (int32 i = 0; i < FavoriteExpressionClasses.Num(); ++i)
	{
		if (FavoriteExpressionClasses[i].MaterialClass == InExpression)
		{
			bIsUnique = false;
			break;
		}
	}
	if (bIsUnique)
	{
		FMaterialExpression MaterialExpression;
		// Trim the material expression name and add it to the list used for filtering.
		MaterialExpression.Name = FString(*InExpression->GetName()).Mid(FCString::Strlen(TEXT("MaterialExpression")));
		MaterialExpression.MaterialClass = InExpression;

		FavoriteExpressionClasses.Add(MaterialExpression);
	}
}
