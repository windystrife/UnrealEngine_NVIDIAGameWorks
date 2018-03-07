// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidJSScripting.h"
#include "AndroidApplication.h"
#include "AndroidWebBrowserWindow.h"
#include "AndroidWebBrowserDialog.h"
#include "AndroidWebBrowserWidget.h"
#include "AndroidJava.h"
#include "AndroidJSStructSerializerBackend.h"
#include "AndroidJSStructDeserializerBackend.h"
#include "StructSerializer.h"
#include "StructDeserializer.h"
#include "UObject/UnrealType.h"

// For UrlDecode/Encode
#include "Http.h"

// Inseterted as a part of an URL to send a message to the front end.
// Note, we can't use a custom protocol due to cross-domain issues.
const FString FAndroidJSScripting::JSMessageTag = TEXT("/!!com.epicgames.ue4.message/");


namespace
{
	const FString ExecuteMethodCommand = TEXT("ExecuteUObjectMethod");
	const FString ScriptingInit =
		TEXT("(function() {")
			TEXT("var util = Object.create({")

			// Simple random-based (RFC-4122 version 4) UUID generator.
			// Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx where x is any hexadecimal digit and y is one of 8, 9, a, or b
			// This function returns the UUID as a hex string without the dashes
			TEXT("uuid: function()")
			TEXT("{")
			TEXT("	var b = new Uint8Array(16); window.crypto.getRandomValues(b);")
			TEXT("	b[6] = b[6]&0xf|0x40; b[8]=b[8]&0x3f|0x80;") // Set the reserved bits to the correct values
			TEXT("	return Array.prototype.reduce.call(b, function(a,i){return a+((0x100|i).toString(16).substring(1))},'').toUpperCase();")
			TEXT("}, ")

			// save a callback function in the callback registry
			// returns the uuid of the callback for passing to the host application
			// ensures that each function object is only stored once.
			// (Closures executed multiple times are considered separate objects.)
			TEXT("registerCallback: function(callback)")
			TEXT("{")
			TEXT("	var key;")
			TEXT("	for(key in this.callbacks)")
			TEXT("	{")
			TEXT("		if (!this.callbacks[key].isOneShot && this.callbacks[key].accept === callback)")
			TEXT("		{")
			TEXT("			return key;")
			TEXT("		}")
			TEXT("	}")
			TEXT("	key = this.uuid();")
			TEXT("	this.callbacks[key] = {accept:callback, reject:callback, bIsOneShot:false};")
			TEXT("	return key;")
			TEXT("}, ")

			TEXT("registerPromise: function(accept, reject, name)")
			TEXT("{")
			TEXT("	var key = this.uuid();")
			TEXT("	this.callbacks[key] = {accept:accept, reject:reject, bIsOneShot:true, name:name};")
			TEXT("	return key;")
			TEXT("}, ")

			// invoke a callback method or promise by uuid
			TEXT("invokeCallback: function(key, bIsError, args)")
			TEXT("{")
			TEXT("	var callback = this.callbacks[key];")
			TEXT("	if (typeof callback === 'undefined')")
			TEXT("	{")
			TEXT("		console.error('Unknown callback id', key);")
			TEXT("		return;")
			TEXT("	}")
			TEXT("	if (callback.bIsOneShot)")
			TEXT("	{")
			TEXT("		callback.iwanttodeletethis=true;")
			TEXT("		delete this.callbacks[key];")
			TEXT("	}")
			TEXT("	callback[bIsError?'reject':'accept'].apply(window, args);")
			TEXT("}, ")

			// convert an argument list to a dictionary of arguments.
			// The args argument must be an argument object as it uses the callee member to deduce the argument names
			TEXT("argsToDict: function(args)")
			TEXT("{")
			TEXT("	var res = {};")
			TEXT("	args.callee.toString().match(/\\((.+?)\\)/)[1].split(/\\s*,\\s*/).forEach(function(name, idx){res[name]=args[idx]});")
			TEXT("	return res;")
			TEXT("}, ")

			// encodes and sends a message to the host application
			TEXT("sendMessage: function()")
			TEXT("{")
			TEXT("	var req=new XMLHttpRequest();")
			TEXT("	req.open('GET', '") + FAndroidJSScripting::JSMessageTag + TEXT("' + Array.prototype.map.call(arguments,function(e){return encodeURIComponent(e)}).join('/'), true);")
			TEXT("	req.send(null);")
			TEXT("}, ")

			// uses the above helper methods to execute a method on a uobject instance.
			// the method set as callee on args needs to be a named function, as the name of the method to invoke is taken from it
			TEXT("executeMethod: function(id, args)")
			TEXT("{")
			TEXT("	var self = this;") // the closures need access to the outer this object

			// In case there are function objects in the argument list, temporarily override Function.toJSON to be able to pass them as callbacks
			TEXT("	var functionJSON = Function.prototype.toJSON;")
			TEXT("	Function.prototype.toJSON = function(){ return self.registerCallback(this) };")

			// Create a promise object to return back to the caller and create a callback function to handle the response
			TEXT("	var promiseID;")
			TEXT("	var promise = new Promise(function (accept, reject) ")
			TEXT("	{")
			TEXT("		promiseID = self.registerPromise(accept, reject, args.callee.name)")
			TEXT("	});")

			// Actually invoke the method by sending a message to the host app
			TEXT("	this.sendMessage('") + ExecuteMethodCommand + TEXT("', id, promiseID, args.callee.name, JSON.stringify(this.argsToDict(args)));")

			// Restore Function.toJSON back to its old value (usually undefined) and return the promise object to the caller
			TEXT("	Function.prototype.toJSON = functionJSON;")
			TEXT("	return promise;")
			TEXT("}")
			TEXT("},{callbacks: {value:{}}});")

			// Create the global window.ue variable
			TEXT("window.ue = Object.create({}, {'$': {writable: false, configurable:false, enumerable: false, value:util}});")
		TEXT("})();")
	;
	const FString ScriptingPostInit =
		TEXT("(function() {")
		TEXT("	document.dispatchEvent(new CustomEvent('ue:ready', {details: window.ue}));")
		TEXT("})();")
	;


	typedef TSharedRef<TJsonWriter<>> FJsonWriterRef;

	template<typename ValueType> void WriteValue(FJsonWriterRef Writer, const FString& Key, const ValueType& Value)
	{
		Writer->WriteValue(Key, Value);
	}
	void WriteNull(FJsonWriterRef Writer, const FString& Key)
	{
		Writer->WriteNull(Key);
	}
	void WriteArrayStart(FJsonWriterRef Writer, const FString& Key)
	{
		Writer->WriteArrayStart(Key);
	}
	void WriteObjectStart(FJsonWriterRef Writer, const FString& Key)
	{
		Writer->WriteObjectStart(Key);
	}
	void WriteRaw(FJsonWriterRef Writer, const FString& Key, const FString& Value)
	{
		Writer->WriteRawJSONValue(Key, Value);
	}
	template<typename ValueType> void WriteValue(FJsonWriterRef Writer, const int, const ValueType& Value)
	{
		Writer->WriteValue(Value);
	}
	void WriteNull(FJsonWriterRef Writer, int)
	{
		Writer->WriteNull();
	}
	void WriteArrayStart(FJsonWriterRef Writer, int)
	{
		Writer->WriteArrayStart();
	}
	void WriteObjectStart(FJsonWriterRef Writer, int)
	{
		Writer->WriteObjectStart();
	}
	void WriteRaw(FJsonWriterRef Writer, int, const FString& Value)
	{
		Writer->WriteRawJSONValue(Value);
	}

	template<typename KeyType>
	bool WriteJsParam(FAndroidJSScriptingRef Scripting, FJsonWriterRef Writer, const KeyType& Key, FWebJSParam& Param)
	{
		switch (Param.Tag)
		{
			case FWebJSParam::PTYPE_NULL:
				WriteNull(Writer, Key);
				break;
			case FWebJSParam::PTYPE_BOOL:
				WriteValue(Writer, Key, Param.BoolValue);
				break;
			case FWebJSParam::PTYPE_DOUBLE:
				WriteValue(Writer, Key, Param.DoubleValue);
				break;
			case FWebJSParam::PTYPE_INT:
				WriteValue(Writer, Key, Param.IntValue);
				break;
			case FWebJSParam::PTYPE_STRING:
				WriteValue(Writer, Key, Param.StringValue);
				break;
			case FWebJSParam::PTYPE_OBJECT:
			{
				if (Param.ObjectValue == nullptr)
				{
					WriteNull(Writer, Key);
				}
				else
				{
					FString ConvertedObject = Scripting->ConvertObject(Param.ObjectValue);
					WriteRaw(Writer, Key, ConvertedObject);
				}
				break;
			}
			case FWebJSParam::PTYPE_STRUCT:
			{
				FString ConvertedStruct = Scripting->ConvertStruct(Param.StructValue->GetTypeInfo(), Param.StructValue->GetData());
				WriteRaw(Writer, Key, ConvertedStruct);
				break;
			}
			case FWebJSParam::PTYPE_ARRAY:
			{
				WriteArrayStart(Writer, Key);
				for(int i=0; i < Param.ArrayValue->Num(); ++i)
				{
					WriteJsParam(Scripting, Writer, i, (*Param.ArrayValue)[i]);
				}
				Writer->WriteArrayEnd();
				break;
			}
			case FWebJSParam::PTYPE_MAP:
			{
				WriteObjectStart(Writer, Key);
				for(auto& Pair : *Param.MapValue)
				{
					WriteJsParam(Scripting, Writer, *Pair.Key, Pair.Value);
				}
				Writer->WriteObjectEnd();
				break;
			}
			default:
				return false;
		}
		return true;
	}
}

void FAndroidJSScripting::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent )
{
	const FString ExposedName = GetBindingName(Name, Object);
	FString Converted = ConvertObject(Object);
	if (bIsPermanent)
	{
		// Each object can only have one permanent binding
		if (BoundObjects[Object].bIsPermanent)
		{
			return;
		}
		// Existing permanent objects must be removed first
		if (PermanentUObjectsByName.Contains(ExposedName))
		{
			return;
		}
		BoundObjects[Object]={true, -1};
		PermanentUObjectsByName.Add(ExposedName, Object);
	}

	TSharedPtr<FAndroidWebBrowserWindow> Window = WindowPtr.Pin();
	if (Window.IsValid())
	{
		FString SetValueScript = FString::Printf(TEXT("window.ue['%s'] = %s;"), *ExposedName.ReplaceCharWithEscapedChar(), *Converted);
		Window->ExecuteJavascript(SetValueScript);
	}
}

void FAndroidJSScripting::UnbindUObject(const FString& Name, UObject* Object, bool bIsPermanent)
{
	const FString ExposedName = GetBindingName(Name, Object);
	if (bIsPermanent)
	{
		// If overriding an existing permanent object, make it non-permanent
		if (PermanentUObjectsByName.Contains(ExposedName) && (Object == nullptr || PermanentUObjectsByName[ExposedName] == Object))
		{
			Object = PermanentUObjectsByName.FindAndRemoveChecked(ExposedName);
			BoundObjects.Remove(Object);
			return;
		}
		else
		{
			return;
		}
	}

	TSharedPtr<FAndroidWebBrowserWindow> Window = WindowPtr.Pin();
	if (Window.IsValid())
	{
		FString DeleteValueScript = FString::Printf(TEXT("delete window.ue['%s'];"), *ExposedName.ReplaceCharWithEscapedChar());
		Window->ExecuteJavascript(DeleteValueScript);
	}
}

bool FAndroidJSScripting::OnJsMessageReceived(const FString& Command, const TArray<FString>& Params, const FString& Origin)
{
	bool Result = false;
	if (Command == ExecuteMethodCommand)
	{
		Result = HandleExecuteUObjectMethodMessage(Params);
	}

	return Result;
}

FString FAndroidJSScripting::ConvertStruct(UStruct* TypeInfo, const void* StructPtr)
{
	return TEXT("undefined");
}

FString FAndroidJSScripting::ConvertObject(UObject* Object)
{
	RetainBinding(Object);
	UClass* Class = Object->GetClass();

	bool first = true;
	FString Result = TEXT("(function(){ return Object.create({");
	for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if(!first)
		{
			Result.Append(TEXT(","));
		}
		else
		{
			first = false;
		}
		Result.Append(*GetBindingName(Function));
		Result.Append(TEXT(": function "));
		Result.Append(*GetBindingName(Function));
		Result.Append(TEXT(" ("));

		bool firstArg = true;
		for ( TFieldIterator<UProperty> It(Function); It; ++It )
		{
			UProperty* Param = *It;
			if (Param->PropertyFlags & CPF_Parm && ! (Param->PropertyFlags & CPF_ReturnParm) )
			{
				UStructProperty *StructProperty = Cast<UStructProperty>(Param);
				if (!StructProperty || !StructProperty->Struct->IsChildOf(FWebJSResponse::StaticStruct()))
				{
					if(!firstArg)
					{
						Result.Append(TEXT(", "));
					}
					else
					{
						firstArg = false;
					}
					Result.Append(*GetBindingName(Param));
				}
			}
		}

		Result.Append(TEXT(")"));
		Result.Append(TEXT(" {return window.ue.$.executeMethod(this.$id, arguments)}"));
	}
	Result.Append(TEXT("},{"));
	Result.Append(TEXT("$id: {writable: false, configurable:false, enumerable: false, value: '"));
 	Result.Append(*PtrToGuid(Object).ToString(EGuidFormats::Digits));
	Result.Append(TEXT("'}})})()"));
	return Result;
}

void FAndroidJSScripting::InvokeJSFunction(FGuid FunctionId, int32 ArgCount, FWebJSParam Arguments[], bool bIsError)
{
	TSharedPtr<FAndroidWebBrowserWindow> Window = WindowPtr.Pin();
	if (Window.IsValid())
	{
		FString CallbackScript = FString::Printf(TEXT("window.ue.$.invokeCallback('%s', %s, "), *FunctionId.ToString(EGuidFormats::Digits), (bIsError)?TEXT("true"):TEXT("false"));
		{
			TArray<uint8> Buffer;
			FMemoryWriter MemoryWriter(Buffer);
			FJsonWriterRef JsonWriter = TJsonWriter<>::Create(&MemoryWriter);
			JsonWriter->WriteArrayStart();
			for(int i=0; i<ArgCount; i++)
			{
				WriteJsParam(SharedThis(this), JsonWriter, i, Arguments[i]);
			}
			JsonWriter->WriteArrayEnd();
			CallbackScript.Append((TCHAR*)Buffer.GetData(), Buffer.Num()/sizeof(TCHAR));
		}
		CallbackScript.Append(TEXT(")"));
		Window->ExecuteJavascript(CallbackScript);
	}
}

void FAndroidJSScripting::InvokeJSFunctionRaw(FGuid FunctionId, const FString& RawJSValue, bool bIsError)
{
	TSharedPtr<FAndroidWebBrowserWindow> Window = WindowPtr.Pin();
	if (Window.IsValid())
	{
		FString CallbackScript = FString::Printf(TEXT("window.ue.$.invokeCallback('%s', %s, [%s])"),
			*FunctionId.ToString(EGuidFormats::Digits), (bIsError)?TEXT("true"):TEXT("false"), *RawJSValue);
		Window->ExecuteJavascript(CallbackScript);
	}
}

void FAndroidJSScripting::InvokeJSErrorResult(FGuid FunctionId, const FString& Error)
{
	FWebJSParam Args[1] = {FWebJSParam(Error)};
	InvokeJSFunction(FunctionId, 1, Args, true);
}

bool FAndroidJSScripting::HandleExecuteUObjectMethodMessage(const TArray<FString>& MessageArgs)
{
	if (MessageArgs.Num() != 4)
	{
		return false;
	}

	FGuid ObjectKey;
	if (!FGuid::Parse(MessageArgs[0], ObjectKey))
	{
		// Invalid GUID
		return false;
	}
	// Get the promise callback and use that to report any results from executing this function.
	FGuid ResultCallbackId;
	if (!FGuid::Parse(MessageArgs[1], ResultCallbackId))
	{
		// Invalid GUID
		return false;
	}

	UObject* Object = GuidToPtr(ObjectKey);
	if (Object == nullptr)
	{
		// Unknown uobject id
		InvokeJSErrorResult(ResultCallbackId, TEXT("Unknown UObject ID"));
		return true;
	}

	FName MethodName = FName(*MessageArgs[2]);
	UFunction* Function = Object->FindFunction(MethodName);
	if (!Function)
	{
		InvokeJSErrorResult(ResultCallbackId, TEXT("Unknown UObject Function"));
		return true;
	}

	// Coerce arguments to function arguments.
	uint16 ParamsSize = Function->ParmsSize;
	TArray<uint8> Params;
	UProperty* ReturnParam = nullptr;
	UProperty* PromiseParam = nullptr;

	if (ParamsSize > 0)
	{
		// Find return parameter and a promise argument if present, as we need to handle them differently
		for ( TFieldIterator<UProperty> It(Function); It; ++It )
		{
			UProperty* Param = *It;
			if (Param->PropertyFlags & CPF_Parm)
			{
				if (Param->PropertyFlags & CPF_ReturnParm)
				{
					ReturnParam = Param;
				}
				else
				{
					UStructProperty *StructProperty = Cast<UStructProperty>(Param);
					if (StructProperty && StructProperty->Struct->IsChildOf(FWebJSResponse::StaticStruct()))
					{
						PromiseParam = Param;
					}
				}
				if (ReturnParam && PromiseParam)
				{
					break;
				}
			}
		}

		// UFunction is a subclass of UStruct, so we can treat the arguments as a struct for deserialization
		Params.AddUninitialized(ParamsSize);
		Function->InitializeStruct(Params.GetData());

		FAndroidJSStructDeserializerBackend Backend = FAndroidJSStructDeserializerBackend(SharedThis(this), MessageArgs[3]);
		FStructDeserializer::Deserialize(Params.GetData(), *Function, Backend);
	}

	if (PromiseParam)
	{
		FWebJSResponse* PromisePtr = PromiseParam->ContainerPtrToValuePtr<FWebJSResponse>(Params.GetData());
		if (PromisePtr)
		{
			*PromisePtr = FWebJSResponse(SharedThis(this), ResultCallbackId);
		}
	}

	Object->ProcessEvent(Function, Params.GetData());
	if ( ! PromiseParam ) // If PromiseParam is set, we assume that the UFunction will ensure it is called with the result
	{
		if ( ReturnParam )
		{
			FStructSerializerPolicies ReturnPolicies;
			ReturnPolicies.PropertyFilter = [&](const UProperty* CandidateProperty, const UProperty* ParentProperty)
			{
				return ParentProperty != nullptr || CandidateProperty == ReturnParam;
			};
			FAndroidJSStructSerializerBackend ReturnBackend = FAndroidJSStructSerializerBackend(SharedThis(this));
			FStructSerializer::Serialize(Params.GetData(), *Function, ReturnBackend, ReturnPolicies);

			// Extract the result value from the serialized JSON object:
			FString ResultJS = ReturnBackend.ToString();

			ResultJS.Append(TEXT("['"));
			ResultJS.Append(GetBindingName(ReturnParam).ReplaceCharWithEscapedChar());
			ResultJS.Append(TEXT("']"));

			InvokeJSFunctionRaw(ResultCallbackId, ResultJS, false);
		}
		else
		{
			InvokeJSFunction(ResultCallbackId, 0, nullptr, false);
		}
	}
	return true;
}

void FAndroidJSScripting::PageLoaded(TSharedRef<class FAndroidWebBrowserWindow> InWindow)
{
	WindowPtr = InWindow;

	// Expunge temporary objects.
	for (TMap<UObject*, ObjectBinding>::TIterator It(BoundObjects); It; ++It)
	{
		if (!It->Value.bIsPermanent)
		{
			It.RemoveCurrent();
		}
	}

	FString Script = ScriptingInit;
	for(auto& Item : PermanentUObjectsByName)
	{
		Script.Append(*FString::Printf(TEXT("window.ue['%s'] = %s;"), *Item.Key.ReplaceCharWithEscapedChar(), *ConvertObject(Item.Value)));
	}
	Script.Append(ScriptingPostInit);
	InWindow->ExecuteJavascript(Script);
}

FAndroidJSScripting::FAndroidJSScripting(bool bJSBindingToLoweringEnabled)
	: FWebJSScripting(bJSBindingToLoweringEnabled)
{
}
