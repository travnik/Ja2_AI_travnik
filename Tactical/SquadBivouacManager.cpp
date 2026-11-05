#ifdef PRECOMPILEDHEADERS
	#include "Tactical All.h"
#else
	#include "Types.h"
	#include "SquadBivouacManager.h"
	#include "Squads.h"
	#include "Items.h"
	#include "Handle Items.h"			// AddItemToPool()
	#include "Tactical Save.h"			// AddItemsToUnLoadedSector()
	#include "Overhead.h"				// MercPtrs, Menptr, gTacticalStatus
	#include "Overhead Types.h"
	#include "Strategic Movement.h"		// GROUP, GetGroup()
	#include "Queen Command.h"			// NumHostilesInSector()
	#include "Strategicmap.h"			// gWorldSectorX/Y/Z
	#include "GameSettings.h"			// gGameExternalOptions (BIVOUAC_* values)
	#include "message.h"				// ScreenMsg
	#include "MessageBoxScreen.h"		// DoMessageBox
	#include "Map Screen Interface.h"	// DoMapMessageBox, MAP_SCREEN messagebox style
	#include "jascreens.h"				// screen ids (MAP_SCREEN)
	#include "screenids.h"
	#include "World Items.h"			// WORLD_ITEM_REACHABLE etc.
	#include "Isometric Utils.h"		// RandomGridNo(), TileIsOutOfBounds()
	#include "Debug.h"
	#include "Text.h"					// gzLateLocalizedString
	#include "Soldier macros.h"
	#include "Assignments.h"			// ON_DUTY, NUMBER_OF_SQUADS related enums
	#include "Game Clock.h"				// PauseGame(), LockPauseState()
	#include "Map Screen Interface Map.h"	// bSelectedInfoChar, gCharactersList
#endif

#include "SquadBivouacManager.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Squad Bivouac Manager - implementation
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / singleton
////////////////////////////////////////////////////////////////////////////////////////////////////

SquadBivouacManager::SquadBivouacManager()
	: m_fBivouacUIMode( FALSE )
	, m_iPendingDisbandSquad( -1 )
	, m_usPendingDisbandSoldierID( 0 )
	, m_iActiveUISquad( -1 )
{
	for ( INT32 i = 0; i < NUMBER_OF_SQUADS; ++i )
	{
		m_Bivouac[i].initialize();
	}
}

SquadBivouacManager::~SquadBivouacManager()
{
}

SquadBivouacManager& SquadBivouacManager::Instance()
{
	static SquadBivouacManager s_Instance;
	return s_Instance;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal helpers
////////////////////////////////////////////////////////////////////////////////////////////////////

// validate squad id and return array index, or -1
INT32 SquadBivouacManager::SquadToDataIndex( INT32 iSquadID ) const
{
	if ( iSquadID < 0 || iSquadID >= NUMBER_OF_SQUADS )
	{
		return( -1 );
	}
	return( iSquadID );
}

// count living mercs currently assigned to the squad
UINT16 SquadBivouacManager::CountLiveMercsInSquad( INT32 iSquadID ) const
{
	UINT16 usCount = 0;

	if ( iSquadID < 0 || iSquadID >= NUMBER_OF_SQUADS )
	{
		return( 0 );
	}

	for ( INT32 iSlot = 0; iSlot < NUMBER_OF_SOLDIERS_PER_SQUAD; ++iSlot )
	{
		SOLDIERTYPE* pSoldier = Squad[iSquadID][iSlot];
		if ( pSoldier != NULL && pSoldier->bActive && pSoldier->stats.bLife > 0 )
		{
			++usCount;
		}
	}

	return( usCount );
}

// TRUE if the squad's sector is ours and has no hostiles
BOOLEAN SquadBivouacManager::IsSectorPeacefulAndOurs( INT32 iSquadID ) const
{
	INT16 sX = -1, sY = -1;
	INT16 bZ = 0;

	if ( iSquadID < 0 || iSquadID >= NUMBER_OF_SQUADS )
	{
		return( FALSE );
	}

	// squad location
	if ( !SectorSquadIsIn( (INT8)iSquadID, &sX, &sY, &bZ ) )
	{
		return( FALSE );
	}

	// no enemies / bloodcats / hostile civilians in that sector
	if ( NumHostilesInSector( sX, sY, bZ ) > 0 )
	{
		return( FALSE );
	}

	// if the loaded sector is this one and combat is going on, deny as well
	if ( ( gWorldSectorX == sX ) && ( gWorldSectorY == sY ) && ( gbWorldSectorZ == bZ ) )
	{
		if ( ( gTacticalStatus.uiFlags & INCOMBAT ) || gTacticalStatus.fEnemyInSector )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Lifecycle
////////////////////////////////////////////////////////////////////////////////////////////////////

void SquadBivouacManager::Initialize()
{
	// wipe all slots
	for ( INT32 i = 0; i < NUMBER_OF_SQUADS; ++i )
	{
		m_Bivouac[i].initialize();
	}

	// reset UI state
	m_fBivouacUIMode = FALSE;
	m_iPendingDisbandSquad = -1;
	m_usPendingDisbandSoldierID = 0;
	m_iActiveUISquad = -1;

	// config values are NOT read here.
	// They are read once in LoadGameExternalOptions() (GameSettings.cpp) into
	// gGameExternalOptions (BIVOUAC_MIN_MERCS_REQUIRED / BIVOUAC_WEIGHT_CAPACITY /
	// BIVOUAC_MOVEMENT_PENALTY_MULTIPLIER), so this feature has no hardcoded numbers.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Inventory access
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN SquadBivouacManager::IsEmpty( INT32 iSquadID )
{
	INT32 iIndex = SquadToDataIndex( iSquadID );
	if ( iIndex == -1 )
	{
		return( TRUE );
	}

	for ( UINT32 i = 0; i < BIVOUAC_NUM_SLOTS; ++i )
	{
		if ( m_Bivouac[iIndex].Slots[i].exists() )
		{
			return( FALSE );
		}
	}

	return( TRUE );
}

BOOLEAN SquadBivouacManager::IsAccessAllowed( INT32 iSquadID )
{
	// valid squad?
	if ( SquadToDataIndex( iSquadID ) == -1 )
	{
		return( FALSE );
	}

	// squad must have enough LIVE mercs to carry the bivouac
	UINT16 usMinMercs = (UINT16)__max( 1, gGameExternalOptions.usBivouacMinMercsRequired );
	if ( CountLiveMercsInSquad( iSquadID ) < usMinMercs )
	{
		return( FALSE );
	}

	// squad must be stationary (not between sectors)
	if ( IsThisSquadOnTheMove( (INT8)iSquadID ) )
	{
		return( FALSE );
	}

	// sector must be ours and peaceful
	if ( !IsSectorPeacefulAndOurs( iSquadID ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

OBJECTTYPE* SquadBivouacManager::GetInventoryPointer( INT32 iSquadID )
{
	INT32 iIndex = SquadToDataIndex( iSquadID );
	if ( iIndex == -1 )
	{
		return( NULL );
	}

	// pointer to the whole 30-slot array; UI and drag&drop operate on it natively
	return( &(m_Bivouac[iIndex].Slots[0]) );
}

void SquadBivouacManager::DumpToFloor( INT32 iSquadID, INT16 sMapX, INT16 sMapY, INT8 bMapZ, INT32 sGridNo )
{
	INT32 iIndex = SquadToDataIndex( iSquadID );
	if ( iIndex == -1 )
	{
		return;
	}

	// gridno for the drop. If invalid, use a random spot (loaded sector) or entry point flag (unloaded).
	INT32 sDropGridNo = sGridNo;
	if ( TileIsOutOfBounds( sDropGridNo ) )
	{
		// will be resolved per-item below
	}

	BOOLEAN fLoadedSector = ( ( gWorldSectorX == sMapX ) && ( gWorldSectorY == sMapY ) && ( gbWorldSectorZ == bMapZ ) );

	if ( fLoadedSector )
	{
		// loaded sector: add each stack to the item pool individually
		if ( TileIsOutOfBounds( sDropGridNo ) )
		{
			sDropGridNo = RandomGridNo();
		}

		for ( UINT32 i = 0; i < BIVOUAC_NUM_SLOTS; ++i )
		{
			OBJECTTYPE* pObj = &(m_Bivouac[iIndex].Slots[i]);
			if ( !pObj->exists() )
			{
				continue;
			}

			// world flags: reachable so the player can pick it up again
			AddItemToPool( sDropGridNo, pObj, 1, 0, WORLD_ITEM_REACHABLE, 0 );

			// clear the slot
			DeleteObj( pObj );
		}
	}
	else
	{
		// unloaded sector: bulk-add through the temp-file mechanism.
		// Copy stacks to a temporary array because AddItemsToUnLoadedSector consumes them.
		OBJECTTYPE pTempObjects[BIVOUAC_NUM_SLOTS];
		UINT32 uiCount = 0;

		for ( UINT32 i = 0; i < BIVOUAC_NUM_SLOTS; ++i )
		{
			OBJECTTYPE* pObj = &(m_Bivouac[iIndex].Slots[i]);
			if ( !pObj->exists() )
			{
				continue;
			}

			pTempObjects[uiCount] = *pObj;
			++uiCount;

			// clear the slot after copying
			DeleteObj( pObj );
		}

		if ( uiCount > 0 )
		{
			if ( TileIsOutOfBounds( sDropGridNo ) )
			{
				// -1 gridno => items get the "use entry point" flag inside AddItemsToUnLoadedSector
				AddItemsToUnLoadedSector( sMapX, sMapY, bMapZ, -1, uiCount, &(pTempObjects[0]),
											0, WOLRD_ITEM_FIND_SWEETSPOT_FROM_GRIDNO | WORLD_ITEM_REACHABLE, 0, 1, FALSE );
			}
			else
			{
				AddItemsToUnLoadedSector( sMapX, sMapY, bMapZ, sDropGridNo, uiCount, &(pTempObjects[0]),
											0, WORLD_ITEM_REACHABLE, 0, 1, FALSE );
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Balance / penalties
////////////////////////////////////////////////////////////////////////////////////////////////////

UINT32 SquadBivouacManager::GetTotalWeight( INT32 iSquadID )
{
	INT32 iIndex = SquadToDataIndex( iSquadID );
	if ( iIndex == -1 )
	{
		return( 0 );
	}

	UINT32 uiTotalWeight = 0;
	for ( UINT32 i = 0; i < BIVOUAC_NUM_SLOTS; ++i )
	{
		// CalculateObjectWeight() accounts for stack size, attachments and dynamic ammo weight
		uiTotalWeight += CalculateObjectWeight( &(m_Bivouac[iIndex].Slots[i]) );
	}

	return( uiTotalWeight );
}

FLOAT SquadBivouacManager::GetOverloadRatio( INT32 iSquadID )
{
	UINT32 uiCapacity = (UINT32)__max( 1, gGameExternalOptions.uiBivouacWeightCapacity );

	UINT32 uiWeight = GetTotalWeight( iSquadID );

	// ratio >= 1.0: below capacity it stays at 1.0 (no penalty), above capacity it grows
	FLOAT fRatio = (FLOAT)uiWeight / (FLOAT)uiCapacity;
	if ( fRatio < 1.0f )
	{
		fRatio = 1.0f;
	}

	return( fRatio );
}

BOOLEAN SquadBivouacManager::IsOverloaded( INT32 iSquadID )
{
	return( GetTotalWeight( iSquadID ) > (UINT32)__max( 1, gGameExternalOptions.uiBivouacWeightCapacity ) );
}

INT32 SquadBivouacManager::GetOverloadBreathDrain( INT32 iSquadID )
{
	// no drain when not overloaded
	if ( !IsOverloaded( iSquadID ) )
	{
		return( 0 );
	}

	UINT32 uiCapacity = (UINT32)__max( 1, gGameExternalOptions.uiBivouacWeightCapacity );
	UINT32 uiWeight = GetTotalWeight( iSquadID );

	// overload percent above capacity: 100% = exactly at capacity, 200% = twice the capacity
	INT32 iOverloadPercent = (INT32)( ( 100 * (INT64)uiWeight ) / (INT64)uiCapacity );	// 100..N

	// dynamic drain: overload% * multiplier / 100.
	// E.g. 200% overload with multiplier 15 => 30 breath points per minute (scaled by caller).
	INT32 iDrain = ( iOverloadPercent * gGameExternalOptions.usBivouacMovementPenaltyMultiplier ) / 100;

	return( __max( 0, iDrain ) );
}

UINT32 SquadBivouacManager::ApplyMovementTimePenalty( UINT32 uiBaseTime, INT32 iSquadID )
{
	// safety: never touch illegal travel times
	if ( uiBaseTime == 0 || uiBaseTime == 0xffffffff )
	{
		return( uiBaseTime );
	}

	if ( !IsOverloaded( iSquadID ) )
	{
		return( uiBaseTime );
	}

	FLOAT fRatio = GetOverloadRatio( iSquadID );	// > 1.0 here

	// penalty factor: (ratio - 1) * multiplier / 100, e.g.
	//   ratio 1.5, multiplier 15 => +7.5% travel time
	//   ratio 2.0, multiplier 15 => +15% travel time
	FLOAT fPenalty = ( fRatio - 1.0f ) * ( (FLOAT)gGameExternalOptions.usBivouacMovementPenaltyMultiplier / 100.0f );

	FLOAT fResult = (FLOAT)uiBaseTime * ( 1.0f + fPenalty );

	// saturate instead of overflowing
	if ( fResult >= (FLOAT)0xfffffffe )
	{
		return( 0xfffffffe );
	}

	return( (UINT32)( fResult + 0.5f ) );	// round up half a minute
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Disband interception
////////////////////////////////////////////////////////////////////////////////////////////////////

void SquadBivouacManager::SetPendingDisbandSquad( INT32 iSquadID, UINT16 usSoldierID )
{
	m_iPendingDisbandSquad = iSquadID;
	m_usPendingDisbandSoldierID = usSoldierID;
}

BOOLEAN SquadBivouacManager::HandleLastMercLeavingSquad( INT32 iSquadID, SOLDIERTYPE* pLastSoldier )
{
	// empty bivouac -> nothing to protect, allow the removal
	if ( IsEmpty( iSquadID ) )
	{
		return( TRUE );
	}

	// remember who/what we are deciding about
	SetPendingDisbandSquad( iSquadID, ( pLastSoldier != NULL ? pLastSoldier->ubID : 0 ) );

	// pause the game while the player decides (mirrors dismissal flow in Assignments.cpp)
	PauseGame();
	LockPauseState( 9 );

	// ask the player: dump the loot to the floor, or cancel?
	// gzLateLocalizedString[67] = "There are items left in the squad's bivouac. Drop them in the sector before disbanding the squad?"
	DoMapMessageBox( MSG_BOX_BASIC_STYLE, gzLateLocalizedString[67], MAP_SCREEN, MSG_BOX_FLAG_YESNO,
						SquadBivouacManager::BivouacDisbandMessageBoxCallBack );

	// blocked until the callback decides
	return( FALSE );
}

void SquadBivouacManager::BivouacDisbandMessageBoxCallBack( UINT8 bExitValue )
{
	SquadBivouacManager& m = SQBM();

	INT32 iSquadID = m.GetPendingDisbandSquad();

	// resume time regardless of the answer (we paused it when showing the box)
	UnLockPauseState();
	UnPauseGame();

	if ( iSquadID == -1 )
	{
		return;
	}

	if ( bExitValue == MSG_BOX_RETURN_YES )
	{
		// find out where the squad is and dump everything onto the ground there
		INT16 sX = -1, sY = -1;
		INT16 bZ = 0;

		if ( SectorSquadIsIn( (INT8)iSquadID, &sX, &sY, &bZ ) )
		{
			// gridno -1 => random/entry point, resolved inside DumpToFloor
			m.DumpToFloor( iSquadID, sX, sY, bZ, -1 );

			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[68] );	// "The squad's bivouac has been emptied into the sector."
		}

		// clear the pending marker: the caller of HandleLastMercLeavingSquad() may now retry the removal
		m.SetPendingDisbandSquad( -1, 0 );

		// notify the squad system that the bivouac is now empty so the pending removal can proceed;
		// the actual removal retry is performed by the hook in Squads.cpp (see gBivouacAllowRemovalOnce).
	}
	else // MSG_BOX_RETURN_NO (or ESC)
	{
		// cancel: nothing is dumped. The removal call that triggered the check has already been
		// aborted by the hook in Squads.cpp (it checks the return value of HandleLastMercLeavingSquad),
		// so the last merc simply stays in the squad.
		m.SetPendingDisbandSquad( -1, 0 );
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Map screen UI state
////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 SquadBivouacManager::GetBivouacUISquadID()
{
	if ( !m_fBivouacUIMode )
	{
		return( -1 );
	}

	SOLDIERTYPE* pSoldier = NULL;

	if ( bSelectedInfoChar == -1 || !gCharactersList[bSelectedInfoChar].fValid )
	{
		return( -1 );
	}

	pSoldier = MercPtrs[ gCharactersList[bSelectedInfoChar].usSolID ];

	if ( pSoldier == NULL || !pSoldier->bActive )
	{
		return( -1 );
	}

	// only mercs assigned TO a squad expose its bivouac
	if ( pSoldier->bAssignment < ON_DUTY )
	{
		return( (INT32)pSoldier->bAssignment );
	}

	return( -1 );
}

BOOLEAN SquadBivouacManager::UpdateBivouacUIModeForSelectedChar( INT8 bSelectedInfoChar )
{
	// leaving inventory or no character -> leave bivouac mode too
	if ( bSelectedInfoChar == -1 || !gCharactersList[bSelectedInfoChar].fValid )
	{
		m_fBivouacUIMode = FALSE;
		return( FALSE );
	}

	SOLDIERTYPE* pSoldier = MercPtrs[ gCharactersList[bSelectedInfoChar].usSolID ];

	// character not on a squad -> no bivouac
	if ( pSoldier == NULL || pSoldier->bAssignment < 0 || pSoldier->bAssignment >= ON_DUTY )
	{
		m_fBivouacUIMode = FALSE;
		return( FALSE );
	}

	// access rules (min mercs, peaceful, not moving)
	if ( !IsAccessAllowed( (INT32)pSoldier->bAssignment ) )
	{
		m_fBivouacUIMode = FALSE;
		return( FALSE );
	}

	return( TRUE );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Map screen sector-inventory <-> bivouac bridge (SQUAD_BIVOUAC UI mode)
////////////////////////////////////////////////////////////////////////////////////////////////////

void SquadBivouacManager::BuildBivouacStash( std::vector< WORLDITEM >& vec )
{
	// one WORLDITEM per bivouac slot; the UI renders and drag&drops against these
	vec.resize( BIVOUAC_NUM_SLOTS );

	INT32 iIndex = SquadToDataIndex( m_iActiveUISquad );

	for( UINT32 i = 0; i < BIVOUAC_NUM_SLOTS; ++i )
	{
		WORLDITEM& wi = vec[i];
		wi.initialize();

		if( iIndex == -1 )
		{
			continue;
		}

		OBJECTTYPE* pObj = &(m_Bivouac[iIndex].Slots[i]);
		if( pObj->exists() )
		{
			wi.fExists = TRUE;
			wi.bVisible = 1;
			wi.sGridNo = -1;		// no real position, UI-only
			wi.usFlags = WORLD_ITEM_REACHABLE;
			wi.object = *pObj;
		}
	}
}

void SquadBivouacManager::StoreBivouacStash( std::vector< WORLDITEM >& vec )
{
	INT32 iIndex = SquadToDataIndex( m_iActiveUISquad );
	if( iIndex == -1 )
	{
		return;
	}

	// wipe squad slots first
	m_Bivouac[iIndex].initialize();

	UINT32 uiCount = (UINT32)min( (INT32)vec.size(), (INT32)BIVOUAC_NUM_SLOTS );
	for( UINT32 i = 0; i < uiCount; ++i )
	{
		if( vec[i].fExists && vec[i].object.exists() )
		{
			m_Bivouac[iIndex].Slots[i] = vec[i].object;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Save / Load
//
// Layout (fixed-size, forward compatible):
//   UINT32  uiVersionMarker	- format marker (BIVOUAC_SAVE_FORMAT_VERSION)
//   UINT32  uiNumSquads		- NUMBER_OF_SQUADS, sanity checked on load
//   for each squad:
//     UINT32 uiSlotCount		- BIVOUAC_NUM_SLOTS
//     BIVOUAC_NUM_SLOTS x OBJECTTYPE::Save() (self-describing size per stack)
////////////////////////////////////////////////////////////////////////////////////////////////////

#define BIVOUAC_SAVE_FORMAT_VERSION		1

BOOLEAN SquadBivouacManager::Save( HWFILE hFile )
{
	UINT32	uiNumBytesWritten = 0;
	UINT32	uiVersionMarker = BIVOUAC_SAVE_FORMAT_VERSION;
	UINT32	uiNumSquads = (UINT32)NUMBER_OF_SQUADS;
	UINT32	uiSlotCount = (UINT32)BIVOUAC_NUM_SLOTS;

	// format marker
	FileWrite( hFile, &uiVersionMarker, sizeof( UINT32 ), &uiNumBytesWritten );
	if ( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return( FALSE );
	}

	// squad count (sanity check on load)
	FileWrite( hFile, &uiNumSquads, sizeof( UINT32 ), &uiNumBytesWritten );
	if ( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return( FALSE );
	}

	for ( UINT32 iSquad = 0; iSquad < uiNumSquads; ++iSquad )
	{
		// slot count per squad
		FileWrite( hFile, &uiSlotCount, sizeof( UINT32 ), &uiNumBytesWritten );
		if ( uiNumBytesWritten != sizeof( UINT32 ) )
		{
			return( FALSE );
		}

		// each slot saves itself (handles stacks, attachments, LBE internally)
		for ( UINT32 iSlot = 0; iSlot < uiSlotCount; ++iSlot )
		{
			if ( !m_Bivouac[iSquad].Slots[iSlot].Save( hFile, false ) )
			{
				return( FALSE );
			}
		}
	}

	return( TRUE );
}

BOOLEAN SquadBivouacManager::Load( HWFILE hFile )
{
	UINT32	uiNumBytesRead = 0;
	UINT32	uiVersionMarker = 0;
	UINT32	uiNumSquads = 0;
	UINT32	uiSlotCount = 0;

	// wipe first, so a partially failed load cannot leave stale items behind
	for ( INT32 i = 0; i < NUMBER_OF_SQUADS; ++i )
	{
		m_Bivouac[i].initialize();
	}

	// format marker
	FileRead( hFile, &uiVersionMarker, sizeof( UINT32 ), &uiNumBytesRead );
	if ( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return( FALSE );
	}
	if ( uiVersionMarker != BIVOUAC_SAVE_FORMAT_VERSION )
	{
		// unknown future/older format: refuse instead of corrupting inventory
		return( FALSE );
	}

	// squad count
	FileRead( hFile, &uiNumSquads, sizeof( UINT32 ), &uiNumBytesRead );
	if ( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return( FALSE );
	}
	if ( uiNumSquads != (UINT32)NUMBER_OF_SQUADS )
	{
		// savegame from a build with a different squad count: cannot map data safely
		return( FALSE );
	}

	for ( UINT32 iSquad = 0; iSquad < uiNumSquads; ++iSquad )
	{
		// slot count per squad
		FileRead( hFile, &uiSlotCount, sizeof( UINT32 ), &uiNumBytesRead );
		if ( uiNumBytesRead != sizeof( UINT32 ) )
		{
			return( FALSE );
		}
		if ( uiSlotCount != (UINT32)BIVOUAC_NUM_SLOTS )
		{
			return( FALSE );
		}

		for ( UINT32 iSlot = 0; iSlot < uiSlotCount; ++iSlot )
		{
			OBJECTTYPE& obj = m_Bivouac[iSquad].Slots[iSlot];
			obj.initialize();

			if ( !obj.Load( hFile ) )
			{
				return( FALSE );
			}
		}
	}

	return( TRUE );
}
