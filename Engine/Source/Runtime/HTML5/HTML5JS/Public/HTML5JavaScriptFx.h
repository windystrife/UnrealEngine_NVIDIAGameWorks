// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma  once

	extern "C" {
		bool UE_SendAndRecievePayLoad( char *URL, char* indata, int insize, char** outdata, int* outsize );

		// SaveGame
		bool UE_SaveGame(const char* Name, const char* indata, int insize);
		bool UE_LoadGame(const char* Name, char **outdata, int* outsize);
		bool UE_DeleteSavedGame(const char* Name);
		bool UE_DoesSaveGameExist(const char* Name);

		// MessageBox
		int UE_MessageBox( int MsgType, const char* Text, const char* Caption);

		// CultureName
		int UE_GetCurrentCultureName(const char* OutName, int outsize);

		// MakeHTTPDataRequest
		void UE_MakeHTTPDataRequest(void *ctx,
					const char* url, const char* verb,
					const char* payload, int payloadsize, // POST
					const char* headers,
					int async, // e.g. onBeforeUnload
					int freeBuffer,
					void(*onload)(void*, void*, unsigned, void*),
					void(*onerror)(void*, int, const char*),
					void(*onprogress)(void*, int, int)
				);

		// onBeforeUnload
		void UE_Reset_OnBeforeUnload();
		void UE_Register_OnBeforeUnload(void *ctx, void(*callback)(void*));
		void UE_UnRegister_OnBeforeUnload(void *ctx, void(*callback)(void*));

		// GSystemResolution
		void UE_GSystemResolution( int(*resX)(), int(*rexY)() );

		void UE_EngineRegisterCanvasResizeListener(void(*listener)());

		// Returns the WebGL major version number that the browser supports (e.g. 2, 1 or 0)
		// This function can be called even before creating any GL contexts on C/C++ side.
		int UE_BrowserWebGLVersion();
	}

