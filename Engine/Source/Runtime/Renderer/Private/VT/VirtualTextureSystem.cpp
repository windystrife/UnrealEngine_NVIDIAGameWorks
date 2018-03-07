// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSystem.h"

#include "TexturePagePool.h"
#include "VirtualTextureSpace.h"
#include "VirtualTextureFeedback.h"
#include "VirtualTexture.h"
#include "UniquePageList.h"
#include "Stats/Stats.h"
#include "SceneUtils.h"
#include "HAL/IConsoleManager.h"

DECLARE_STATS_GROUP( TEXT("Virtual Texturing"), STATGROUP_VirtualTexturing, STATCAT_Advanced );

DECLARE_CYCLE_STAT(			TEXT("Feedback Analysis"),				STAT_FeedbackAnalysis,				STATGROUP_VirtualTexturing );
DECLARE_CYCLE_STAT(			TEXT("VirtualTextureSystem Update"),	STAT_VirtualTextureSystem_Update,	STATGROUP_VirtualTexturing );
DECLARE_CYCLE_STAT(			TEXT("Page Table Updates"),				STAT_PageTableUpdates,				STATGROUP_VirtualTexturing );
DECLARE_CYCLE_STAT(			TEXT("UniquePageList ExpandByMips"),	STAT_UniquePageList_ExpandByMips,	STATGROUP_VirtualTexturing );
DECLARE_CYCLE_STAT(			TEXT("UniquePageList Sort"),			STAT_UniquePageList_Sort,			STATGROUP_VirtualTexturing );

DECLARE_DWORD_COUNTER_STAT( TEXT("Num pages visible"),				STAT_NumPagesVisible,				STATGROUP_VirtualTexturing );
DECLARE_DWORD_COUNTER_STAT( TEXT("Num page requests"),				STAT_NumPageRequests,				STATGROUP_VirtualTexturing );
DECLARE_DWORD_COUNTER_STAT( TEXT("Num page requests resident"),		STAT_NumPageRequestsResident,		STATGROUP_VirtualTexturing );
DECLARE_DWORD_COUNTER_STAT( TEXT("Num page requests not resident"), STAT_NumPageRequestsNotResident,	STATGROUP_VirtualTexturing );
DECLARE_DWORD_COUNTER_STAT( TEXT("Num page uploads"),				STAT_NumPageUploads,				STATGROUP_VirtualTexturing );

DECLARE_FLOAT_COUNTER_STAT( TEXT("VT"), STAT_GPU_VT, STATGROUP_GPU );


static TAutoConsoleVariable<int32> CVarVTMaxUploadsPerFrame(
	TEXT("r.VT.MaxUploadsPerFrame"),
	16,
	TEXT("Max number of page uploads per frame"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVTNumMipsToExpandRequests(
	TEXT("r.VT.NumMipsToExpandRequests"),
	3,
	TEXT("Number of mip levels to request in addition to the original request"),
	ECVF_RenderThreadSafe
	);


IVirtualTexture::~IVirtualTexture()
{}


FVirtualTextureSystem GVirtualTextureSystem;

FVirtualTextureSystem::FVirtualTextureSystem()
	: Frame(0)
{
	for( int ID = 0; ID < 16; ID++ )
	{
		Spaces[ ID ] = nullptr;
	}
}

FVirtualTextureSystem::~FVirtualTextureSystem()
{}

void FVirtualTextureSystem::RegisterSpace( FVirtualTextureSpace* Space )
{
	check( Space );

	for( int ID = 0; ID < 16; ID++ )
	{
		if( Spaces[ ID ] == nullptr )
		{
			Spaces[ ID ] = Space;
			Space->ID = ID;
			return;
		}
	}

	check(0);
}

void FVirtualTextureSystem::UnregisterSpace( FVirtualTextureSpace* Space )
{
	check( Space );
	check( Spaces[ Space->ID ] == Space );

	Spaces[ Space->ID ] = nullptr;
	Space->ID = 0xff;
}

void FVirtualTextureSystem::FeedbackAnalysis( FUniquePageList* RESTRICT RequestedPageList, const uint32* RESTRICT Buffer, uint32 Width, uint32 Height, uint32 Pitch )
{
	SCOPE_CYCLE_COUNTER( STAT_FeedbackAnalysis );

	// Combine simple runs of identical requests
	uint32 LastPixel = 0xffffffff;
	uint32 LastPage = 0xffffffff;
	uint32 LastCount = 0;

	for( uint32 y = 0; y < Height; y++ )
	{
		for( uint32 x = 0; x < Width; x++ )
		{
			const uint32 Pixel = Buffer[ x + y * Pitch ];

			if( Pixel == 0xffffffff )
			{
				continue;
			}
		
			if( Pixel == LastPixel )
			{
				LastCount++;
				continue;
			}

			// Decode pixel encoding
			const uint32 PageX	= ( Pixel >>  0 ) & 0xfff;
			const uint32 PageY	= ( Pixel >> 12 ) & 0xfff;
			const uint32 Level	= ( Pixel >> 24 ) & 0xf;
			const uint32 ID		= ( Pixel >> 28 );

			const uint32 MaxLevel = RequestedPageList->NumLevels[ ID ] - 1;

			uint32 vAddress = FMath::MortonCode2( PageX ) | ( FMath::MortonCode2( PageY ) << 1 );
			uint32 vLevel = FMath::Min( Level, MaxLevel );
			uint32 vDimensions = RequestedPageList->Dimensions[ ID ];

			// Mask out low bits
			vAddress &= 0xffffffff << ( vDimensions * vLevel );

			uint32 Page = EncodePage( ID, vLevel, vAddress );
			if( Page == LastPage )
			{
				LastCount++;
				continue;
			}

			if( LastPage != 0xffffffff )
			{
				RequestedPageList->Add( LastPage, LastCount );
			}

			LastPixel = Pixel;
			LastPage = Page;
			LastCount = 1;
		}
	}

	if( LastPage != 0xffffffff )
	{
		RequestedPageList->Add( LastPage, LastCount );
	}
}

void FVirtualTextureSystem::Update( FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel )
{
	SCOPE_CYCLE_COUNTER( STAT_VirtualTextureSystem_Update );
	SCOPED_GPU_STAT( RHICmdList, STAT_GPU_VT );

	FMemMark Mark( FMemStack::Get() );
	FUniquePageList* RESTRICT RequestedPageList = new(FMemStack::Get()) FUniquePageList;

	// Cache to avoid indirection
	for( uint32 ID = 0; ID < 16; ID++ )
	{
		FVirtualTextureSpace* RESTRICT Space = Spaces[ ID ];

		RequestedPageList->NumLevels[ ID ] = Space ? Space->PageTableLevels : 16;
		RequestedPageList->Dimensions[ ID ] = Space ? Space->Dimensions : 2;
	}
	
	int32 Pitch = 0;
	const uint32* RESTRICT Buffer = GVirtualTextureFeedback.Map( RHICmdList, Pitch );
	if( Buffer )
	{
		FeedbackAnalysis( RequestedPageList, Buffer, GVirtualTextureFeedback.Size.X, GVirtualTextureFeedback.Size.Y, Pitch );
		GVirtualTextureFeedback.Unmap( RHICmdList );
	}

	SET_DWORD_STAT( STAT_NumPagesVisible, RequestedPageList->GetNum() );

	// Can add other sources of pages to RequestedPageList here
	
	{
		SCOPE_CYCLE_COUNTER( STAT_UniquePageList_ExpandByMips );
		RequestedPageList->ExpandByMips( CVarVTNumMipsToExpandRequests.GetValueOnRenderThread() );
	}
	
	SET_DWORD_STAT( STAT_NumPageRequests, RequestedPageList->GetNum() );

	// Static to avoid malloc cost
	static FBinaryHeap< uint32, uint16 > RequestHeap;
	RequestHeap.Clear();

	{
		// Find resident

		for( uint32 i = 0, Num = RequestedPageList->GetNum(); i < Num; i++ )
		{
			const uint64 PageEncoded = RequestedPageList->GetPage(i);

			// Decode page
			uint32 ID, vLevel, vAddress;
			DecodePage( (uint32)PageEncoded, ID, vLevel, vAddress );

			FVirtualTextureSpace* RESTRICT	Space = Spaces[ ID ];	checkSlow( Space );
			FTexturePagePool* RESTRICT		Pool = Space->Pool;

			// Is this page already resident?
			uint32 pAddress = Pool->FindPage( ID, vLevel, vAddress );
			if( pAddress == ~0u )
			{
				// Page isn't resident. Start searching at parent level for nearest ancestor.
				uint32 Parent_vLevel = vLevel + 1;
				uint32 Parent_vAddress = vAddress & ( 0xffffffff << ( Space->Dimensions * Parent_vLevel ) );

				uint32 Ancestor_pAddress = Pool->FindNearestPage( ID, Parent_vLevel, Parent_vAddress );
				uint32 Ancestor_vLevel = Ancestor_pAddress != ~0u ? Pool->GetPage( Ancestor_pAddress ).vLevel : Space->PageTableLevels - 1;

				uint32 Count = RequestedPageList->GetCount(i);
				uint32 Priority = Count * ( 1 << ( Ancestor_vLevel - vLevel ) );

				RequestHeap.Add( ~Priority, i );
			}
			else
			{
				Pool->UpdateUsage( Frame, pAddress );

				//FileCache->Touch( VT->FileName, PageOffset, PageSize, priority );
			}
		}

		SET_DWORD_STAT( STAT_NumPageRequestsResident, RequestedPageList->GetNum() - RequestHeap.Num() );
		SET_DWORD_STAT( STAT_NumPageRequestsNotResident, RequestHeap.Num() );
	}
	
	// Limit the number of uploads
	// Are all pages equal? Should there be different limits on different types of pages?
	int32 NumUploadsLeft = CVarVTMaxUploadsPerFrame.GetValueOnRenderThread();

	while( RequestHeap.Num() > 0 && NumUploadsLeft > 0 )
	{
		uint32 PageIndex = RequestHeap.Top();
		RequestHeap.Pop();

		const uint64 PageEncoded = RequestedPageList->GetPage( PageIndex );

		// Decode page
		uint32 ID, vLevel, vAddress;
		DecodePage( (uint32)PageEncoded, ID, vLevel, vAddress );

		FVirtualTextureSpace* RESTRICT	Space = Spaces[ ID ];	checkSlow( Space );
		FTexturePagePool* RESTRICT		Pool = Space->Pool;

		// Find specific VT in Space
		uint64 Local_vAddress = 0;
		IVirtualTexture* RESTRICT VT = Space->Allocator.Find( vAddress, Local_vAddress );

		void* RESTRICT Location = nullptr;
		bool bPageDataAvailable = VT->LocatePageData( vLevel, Local_vAddress, Location );

		// FIXME ExpandByMips might not provide a valid page for this to fall back on

		if( bPageDataAvailable && Pool->AnyFreeAvailable( Frame ) )
		{
			uint32 pAddress = Pool->Alloc( Frame );
			check( pAddress != ~0u );

			Pool->UnmapPage( pAddress );
			
			VT->ProducePageData( RHICmdList, FeatureLevel, vLevel, Local_vAddress, pAddress, Location );
			
			Pool->MapPage( ID, vLevel, vAddress, pAddress );
			Pool->Free( Frame, pAddress );

			NumUploadsLeft--;
			INC_DWORD_STAT( STAT_NumPageUploads );
		}
	}

	SCOPE_CYCLE_COUNTER( STAT_PageTableUpdates );
	
	// Update page tables
	for( uint32 ID = 0; ID < 16; ID++ )
	{
		if( Spaces[ ID ] )
		{
			Spaces[ ID ]->ApplyUpdates( RHICmdList );
		}
	}

	Frame++;
}