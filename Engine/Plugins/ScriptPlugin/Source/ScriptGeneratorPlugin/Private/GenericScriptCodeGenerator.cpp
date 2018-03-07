// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericScriptCodeGenerator.h"
#include "UObject/UnrealType.h"
#include "ScriptGeneratorLog.h"

FGenericScriptCodeGenerator::FGenericScriptCodeGenerator(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& InIncludeBase)
: FScriptCodeGeneratorBase(RootLocalPath, RootBuildPath, OutputDirectory, InIncludeBase)
{

}

FString FGenericScriptCodeGenerator::GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	return GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, Function->GetName());
}

FString FGenericScriptCodeGenerator::GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, UClass* Class, const FString& FunctionName)
{
	return FString::Printf(TEXT("int32 %s_%s(void* InScriptContext)"), *Class->GetName(), *FunctionName);
}

FString FGenericScriptCodeGenerator::GenerateFunctionParamDeclaration(const FString& ClassNameCPP, UClass* Class, UFunction* Function, UProperty* Param)
{
	FString ParamDecl;
	if (Param->IsA(UObjectPropertyBase::StaticClass()) || Param->IsA(UClassProperty::StaticClass()))
	{
		ParamDecl = FString::Printf(TEXT("UObject* %s = nullptr;"), *Param->GetName());
	}
	else
	{
		ParamDecl = FString::Printf(TEXT("%s %s = %s();"), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue), *Param->GetName(), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue));
	}
	return ParamDecl;
}

FString FGenericScriptCodeGenerator::GenerateObjectDeclarationFromContext(const FString& ClassNameCPP, UClass* Class)
{
	return FString::Printf(TEXT("UObject* Obj = (UObject*)InScriptContext;"));
}

FString FGenericScriptCodeGenerator::GenerateReturnValueHandler(const FString& ClassNameCPP, UClass* Class, UFunction* Function, UProperty* ReturnValue)
{
	return TEXT("return 0;");
}

FString FGenericScriptCodeGenerator::ExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	FString GeneratedGlue = GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, Function);
	GeneratedGlue += TEXT("\r\n{\r\n");

	UProperty* ReturnValue = NULL;
	UClass* FuncSuper = NULL;
	
	if (Function->GetOwnerClass() != Class)
	{
		// Find the base definition of the function
		if (ExportedClasses.Contains(Function->GetOwnerClass()->GetFName()))
		{
			FuncSuper = Function->GetOwnerClass();
		}
	}

	FString FunctionBody;
	if (FuncSuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		FunctionBody += GenerateFunctionDispatch(Function);

		FString FunctionCallArguments;
		FString ReturnValueDeclaration;
		for (TFieldIterator<UProperty> ParamIt(Function); !ReturnValue && ParamIt; ++ParamIt)
		{
			UProperty* Param = *ParamIt;
			if (Param->GetPropertyFlags() & CPF_ReturnParm)
			{
				ReturnValue = Param;
			}
		}
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateReturnValueHandler(ClassNameCPP, Class, Function, ReturnValue));
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(InScriptContext);\r\n"), *FuncSuper->GetName(), *Function->GetName());
	}

	GeneratedGlue += FunctionBody;
	GeneratedGlue += TEXT("}\r\n\r\n");

	return GeneratedGlue;
}

FString FGenericScriptCodeGenerator::ExportProperty(const FString& ClassNameCPP, UClass* Class, UProperty* Property, int32 PropertyIndex)
{
	FString PropertyName = Property->GetName();
	UProperty* ReturnValue = NULL;
	UClass* PropertySuper = NULL;

	if (Property->GetOwnerClass() != Class)
	{
		// Find the base class where this property was defined
		if (ExportedClasses.Contains(Property->GetOwnerClass()->GetFName()))
		{
			PropertySuper = Property->GetOwnerClass();
		}
	}

	// Getter	
	FString GetterName = FString::Printf(TEXT("Get_%s"), *PropertyName);
	FString GeneratedGlue = GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, GetterName);
	GeneratedGlue += TEXT("\r\n{\r\n");
	FString FunctionBody;
	if (PropertySuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		FunctionBody += FString::Printf(TEXT("\tstatic UProperty* Property = FindScriptPropertyHelper(%s::StaticClass(), TEXT(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateFunctionParamDeclaration(ClassNameCPP, Class, NULL, Property));
		FunctionBody += FString::Printf(TEXT("\tProperty->CopyCompleteValue(&%s, Property->ContainerPtrToValuePtr<void>(Obj));\r\n"), *Property->GetName());
		FunctionBody += FString::Printf(TEXT("\t// @todo: handle property value here\r\n"));
		FunctionBody += TEXT("\treturn 0;\r\n");
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(InScriptContext);\r\n"), *PropertySuper->GetName(), *GetterName);
	}
	GeneratedGlue += FunctionBody;	
	GeneratedGlue += TEXT("}\r\n\r\n");
	FunctionBody.Empty(FunctionBody.Len());

	// Setter
	FString SetterName = FString::Printf(TEXT("Set_%s"), *PropertyName);
	GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, SetterName);
	GeneratedGlue += TEXT("\r\n{\r\n");
	if (PropertySuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		FunctionBody += FString::Printf(TEXT("\tstatic UProperty* Property = FindScriptPropertyHelper(%s::StaticClass(), TEXT(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateFunctionParamDeclaration(ClassNameCPP, Class, NULL, Property));
		FunctionBody += FString::Printf(TEXT("\tProperty->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Obj), &%s);\r\n"), *Property->GetName());
		FunctionBody += TEXT("\treturn 0;\r\n");
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(InScriptContext);\r\n"), *PropertySuper->GetName(), *SetterName);
	}
	GeneratedGlue += FunctionBody;	
	GeneratedGlue += TEXT("}\r\n\r\n");

	return GeneratedGlue;
}

FString FGenericScriptCodeGenerator::ExportAdditionalClassGlue(const FString& ClassNameCPP, UClass* Class)
{
	FString GeneratedGlue;

	if (!(Class->GetClassFlags() & CLASS_Abstract))
	{
		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, TEXT("New"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += TEXT("\tUObject* Outer = NULL;\r\n");
		GeneratedGlue += TEXT("\tFName Name = FName(\"ScriptObject\");\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tUObject* Obj = NewObject<%s>(Outer, Name);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t\tFScriptObjectReferencer::Get().AddObjectReference(Obj);\r\n");
		GeneratedGlue += TEXT("\t\t// @todo: Register the object with the script context here\r\n");
		GeneratedGlue += TEXT("\t}\r\n\treturn 0;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");

		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, TEXT("Destroy"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t\tFScriptObjectReferencer::Get().RemoveObjectReference(Obj);\r\n");
		GeneratedGlue += TEXT("\t\t// @todo: Remove the object from the script context here if required\r\n");
		GeneratedGlue += TEXT("\t}\r\n\treturn 0;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");
	}

	return GeneratedGlue;
}

bool FGenericScriptCodeGenerator::CanExportClass(UClass* Class)
{
	bool bCanExport = FScriptCodeGeneratorBase::CanExportClass(Class);
	if (bCanExport)
	{
		const FString ClassNameCPP = GetClassNameCPP(Class);
		// No members to export? Don't bother exporting the class.
		bool bHasMembersToExport = false;
		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); !bHasMembersToExport && FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (CanExportFunction(ClassNameCPP, Class, Function))
			{
				bHasMembersToExport = true;
			}
		}
		// Check properties too
		if (!bHasMembersToExport)
		{
			for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); !bHasMembersToExport && PropertyIt; ++PropertyIt)
			{
				UProperty* Property = *PropertyIt;
				if (CanExportProperty(ClassNameCPP, Class, Property))
				{
					bHasMembersToExport = true;
				}
			}
		}
		bCanExport = bHasMembersToExport;
	}
	return bCanExport;
}

void FGenericScriptCodeGenerator::ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged)
{
	if (!CanExportClass(Class))
	{
		return;
	}
	
	UE_LOG(LogScriptGenerator, Log, TEXT("Exporting class %s"), *Class->GetName());
	
	ExportedClasses.Add(Class->GetFName());
	AllSourceClassHeaders.Add(SourceHeaderFilename);

	const FString ClassGlueFilename = GetScriptHeaderForClass(Class);
	AllScriptHeaders.Add(ClassGlueFilename);	

	const FString ClassNameCPP = GetClassNameCPP(Class);
	FString GeneratedGlue(TEXT("#pragma once\r\n\r\n"));		

	for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (CanExportFunction(ClassNameCPP, Class, Function))
		{
			UE_LOG(LogScriptGenerator, Log, TEXT("  %s %s"), *Function->GetClass()->GetName(), *Function->GetName());
			GeneratedGlue += ExportFunction(ClassNameCPP, Class, Function);
		}
	}

	int32 PropertyIndex = 0;
	for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt, ++PropertyIndex)
	{
		UProperty* Property = *PropertyIt;
		if (CanExportProperty(ClassNameCPP, Class, Property))
		{
			UE_LOG(LogScriptGenerator, Log, TEXT("  %s %s"), *Property->GetClass()->GetName(), *Property->GetName());
			GeneratedGlue += ExportProperty(ClassNameCPP, Class, Property, PropertyIndex);
		}
	}

	GeneratedGlue += ExportAdditionalClassGlue(ClassNameCPP, Class);

	SaveHeaderIfChanged(ClassGlueFilename, GeneratedGlue);
}

void FGenericScriptCodeGenerator::FinishExport()
{
	GlueAllGeneratedFiles();
	RenameTempFiles();
}

void FGenericScriptCodeGenerator::GlueAllGeneratedFiles()
{
	// Generate inl library file
	FString LibGlueFilename = GeneratedCodePath / TEXT("GeneratedScriptLibraries.inl");
	FString LibGlue;

	// Include all source header files
	for (auto& HeaderFilename : AllSourceClassHeaders)
	{
		// Re-base to make sure we're including the right files on a remote machine
		FString NewFilename(RebaseToBuildPath(HeaderFilename));
		LibGlue += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
	}

	// Include all script glue headers
	for (auto& HeaderFilename : AllScriptHeaders)
	{
		// Re-base to make sure we're including the right files on a remote machine
		FString NewFilename(FPaths::GetCleanFilename(HeaderFilename));
		LibGlue += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
	}

	SaveHeaderIfChanged(LibGlueFilename, LibGlue);
}
