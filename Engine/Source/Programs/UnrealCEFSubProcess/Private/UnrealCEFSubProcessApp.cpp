// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealCEFSubProcessApp.h"
#include "UnrealCEFSubProcess.h"

#if WITH_CEF3

FUnrealCEFSubProcessApp::FUnrealCEFSubProcessApp()
{
}

void FUnrealCEFSubProcessApp::OnContextCreated( CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefV8Context> Context )
{
	RemoteScripting.OnContextCreated(Browser, Frame, Context);
}

void FUnrealCEFSubProcessApp::OnContextReleased( CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefV8Context> Context )
{
	RemoteScripting.OnContextReleased(Browser, Frame, Context);
}

bool FUnrealCEFSubProcessApp::OnProcessMessageReceived( CefRefPtr<CefBrowser> Browser, CefProcessId SourceProcess, CefRefPtr<CefProcessMessage> Message )
{
	bool Result = false;
	FString MessageName = Message->GetName().ToWString().c_str();
	if (MessageName.StartsWith(TEXT("UE::")))
	{
		Result = RemoteScripting.OnProcessMessageReceived(Browser, SourceProcess, Message);
	}

	return Result;
}

void FUnrealCEFSubProcessApp::OnRenderThreadCreated( CefRefPtr<CefListValue> ExtraInfo )
{
	for(size_t I=0; I<ExtraInfo->GetSize(); I++)
	{
		if (ExtraInfo->GetType(I) == VTYPE_DICTIONARY)
		{
			CefRefPtr<CefDictionaryValue> Info = ExtraInfo->GetDictionary(I);
			if ( Info->GetType("browser") == VTYPE_INT)
			{
				int32 BrowserID = Info->GetInt("browser");
				CefRefPtr<CefDictionaryValue> Bindings = Info->GetDictionary("bindings");
				RemoteScripting.InitPermanentBindings(BrowserID, Bindings);
			}
		}
	}
}

#if !PLATFORM_LINUX
void FUnrealCEFSubProcessApp::OnFocusedNodeChanged(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefDOMNode> Node)
{
	CefRefPtr<CefProcessMessage> Message = CefProcessMessage::Create("UE::IME::FocusChanged");
	CefRefPtr<CefListValue> MessageArguments = Message->GetArgumentList();

	if (Node.get() == nullptr)
	{
		MessageArguments->SetString(0, "NONE");
	}
	else
	{
		static const TMap<uint32, CefString> DomNodeTypeStrings = []()
		{
			TMap<uint32, CefString> Result;
#define ADD_NODETYPE_STRING(NodeTypeCode) Result.Add(NodeTypeCode, #NodeTypeCode)
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_UNSUPPORTED);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_ELEMENT);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_ATTRIBUTE);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_TEXT);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_CDATA_SECTION);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_PROCESSING_INSTRUCTIONS);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_COMMENT);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_DOCUMENT);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_DOCUMENT_TYPE);
			ADD_NODETYPE_STRING(DOM_NODE_TYPE_DOCUMENT_FRAGMENT);
#undef ADD_NODETYPE_STRING
			return Result;
		}();

		MessageArguments->SetString(0, DomNodeTypeStrings[Node->GetType()]);
		MessageArguments->SetString(1, Node->GetName());
		MessageArguments->SetBool(2, Node->IsEditable());
		MessageArguments->SetString(3, Node->GetValue());
		MessageArguments->SetInt(4, Node->GetElementBounds().x);
		MessageArguments->SetInt(5, Node->GetElementBounds().y);
		MessageArguments->SetInt(6, Node->GetElementBounds().width);
		MessageArguments->SetInt(7, Node->GetElementBounds().height);
	}

	Browser->SendProcessMessage(PID_BROWSER, Message);
}
#endif

#endif