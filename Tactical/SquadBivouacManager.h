#ifndef _SQUAD_BIVOUAC_MANAGER_H_
#define _SQUAD_BIVOUAC_MANAGER_H_

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Squad Bivouac Manager (SQUAD_BIVOUAC feature)
//
//	Encapsulated singleton managing a shared 30-slot "bivouac" inventory per squad,
//	accessible on the Map Screen through the Sector Inventory UI.
//
//	Features:
//	 - 30 OBJECTTYPE slots per squad, serializable (savegame-safe, fixed-size blocks)
//	 - Access control: squad not between sectors, sector peaceful, enough live mercs
//	 - Overload penalties: travel time multiplier + per-minute breath (bBreathMax) drain
//	 - Guard against disbanding a squad with items left inside (YES/NO messagebox -> dump to floor)
//	 - All balance values are read from Ja2_Options.ini via gGameExternalOptions (no hardcode)
//
//	Integration: only one-line hooks in procedural engine files (see SQBM() macro).
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Types.h"
#include "Item Types.h"
#include "FileMan.h"
#include "World Items.h"
#include "Squads.h"			// NUMBER_OF_SQUADS
#include <vector>

// number of slots in one squad's bivouac
#define BIVOUAC_NUM_SLOTS				30

// forward declarations to keep the header light
class	SOLDIERTYPE;
struct	GROUP;
class	SquadBivouacManager;

// hook helper: SquadBivouacManager::Instance().Method()  ->  SQBM().Method()
#define SQBM()	SquadBivouacManager::Instance()

////////////////////////////////////////////////////////////////////////////////////////////////////
// BivouacData
//
// Plain, serializable POD-ish storage block for ONE squad.
// Kept deliberately simple so Save()/Load() can stream it with fixed sizes.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct BivouacData
{
	// 30 fixed item slots. OBJECTTYPE supports Load()/Save() and initialize().
	OBJECTTYPE	Slots[ BIVOUAC_NUM_SLOTS ];

	void initialize()
	{
		for ( UINT32 i = 0; i < BIVOUAC_NUM_SLOTS; ++i )
		{
			Slots[i].initialize();
		}
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SquadBivouacManager
////////////////////////////////////////////////////////////////////////////////////////////////////
class SquadBivouacManager
{
public:
	// singleton access
	static SquadBivouacManager& Instance();

	////////////////////////////////////////////////////////////////////////////////////////////
	// Lifecycle
	////////////////////////////////////////////////////////////////////////////////////////////

	// Clear all bivouac arrays (New Game start) and read BIVOUAC_* config values.
	// Hooked from InitSquads() in Squads.cpp.
	void Initialize();

	////////////////////////////////////////////////////////////////////////////////////////////
	// Inventory access
	////////////////////////////////////////////////////////////////////////////////////////////

	// TRUE if all 30 slots of this squad are empty.
	BOOLEAN	IsEmpty( INT32 iSquadID );

	// TRUE if the player may open/manipulate this squad's bivouac right now:
	//  - squad id valid
	//  - squad not between sectors (not moving)
	//  - no hostiles in the squad's sector (peaceful & ours)
	//  - number of live mercs in squad >= BIVOUAC_MIN_MERCS_REQUIRED
	//  - no item currently on the map screen cursor
	BOOLEAN	IsAccessAllowed( INT32 iSquadID );

	// Returns pointer to the 30-slot array so the map screen UI can render
	// and drag&drop natively against this storage.
	// Returns NULL on invalid squad id.
	OBJECTTYPE*	GetInventoryPointer( INT32 iSquadID );

	// Dump every item of the squad's bivouac onto the ground at sGridNo.
	// For the LOADED sector: items go through AddItemToPool() and slots are cleared with DeleteObj().
	// For an UNLOADED sector: items go through AddItemsToUnLoadedSector() (single bulk call).
	// sGridNo == -1 means "use random gridno / entry point" (unloaded sector case).
	void	DumpToFloor( INT32 iSquadID, INT16 sMapX, INT16 sMapY, INT8 bMapZ, INT32 sGridNo );

	////////////////////////////////////////////////////////////////////////////////////////////
	// Balance / penalties
	////////////////////////////////////////////////////////////////////////////////////////////

	// Total weight of all items inside the squad's bivouac (internal JA2 weight units).
	UINT32	GetTotalWeight( INT32 iSquadID );

	// Total weight capacity of this squad's bivouac:
	//   BIVOUAC_WEIGHT_CAPACITY (config base) + sum of the free (spare) carrying
	//   capacity of every live merc in the squad.
	// Free capacity of one merc = strength-based max carry weight - currently carried weight.
	// Calculated on demand, so it always reflects the current squad composition and equipment.
	UINT32	GetWeightCapacity( INT32 iSquadID );

	// Sum of the strength-based maximum carry weight over all live mercs of the squad.
	// (Informational: free capacity = this - currently carried weight.)
	UINT32	GetMercsMaxCarryWeight( INT32 iSquadID );

	// Sum of the weight currently carried by all live mercs of the squad.
	UINT32	GetMercsCarriedWeight( INT32 iSquadID );

	// Sum of the free (spare) carrying capacity of all live mercs of the squad
	// (never negative for a single merc: an overloaded merc just contributes 0).
	UINT32	GetMercsFreeWeight( INT32 iSquadID );

	// Overload ratio: GetTotalWeight / GetWeightCapacity, clamped to >= 1.0.
	// 1.0 = within capacity, >1.0 = overloaded by that factor.
	FLOAT	GetOverloadRatio( INT32 iSquadID );

	// TRUE if the bivouac is overloaded (ratio > 1.0).
	BOOLEAN	IsOverloaded( INT32 iSquadID );

	// Additional per-minute breath drain for a marching merc of this squad.
	// Scales dynamically with the overload percentage:
	//   drain = overloadPercent * BIVOUAC_MOVEMENT_PENALTY_MULTIPLIER / 100
	// Returns 0 for non-overloaded squads / invalid params.
	INT32	GetOverloadBreathDrain( INT32 iSquadID );

	// Movement time multiplier: returns travel time scaled by overload penalty.
	//   uiTime * (1 + (ratio-1) * BIVOUAC_MOVEMENT_PENALTY_MULTIPLIER / 100)
	// Used as a one-line hook inside GetSectorMovementTime()/GetSectorMvtTimeForGroup().
	UINT32	ApplyMovementTimePenalty( UINT32 uiBaseTime, INT32 iSquadID );

	////////////////////////////////////////////////////////////////////////////////////////////
	// Disband interception
	////////////////////////////////////////////////////////////////////////////////////////////

	// Called when the LAST merc is about to leave squad iSquadID.
	// If bivouac is not empty: shows DoMessageBox(YES/NO) and returns FALSE (block removal).
	//   YES -> dump items to sector floor, allow removal.
	//   NO  -> abort, merc stays in the squad.
	// If bivouac is empty: returns TRUE immediately (allow removal).
	BOOLEAN	HandleLastMercLeavingSquad( INT32 iSquadID, SOLDIERTYPE* pLastSoldier );

	// Messagebox callback. Installed as extern "C"-style free function is NOT needed:
	// the static member below is registered directly with DoMessageBox/DoMapMessageBox.
	static void	BivouacDisbandMessageBoxCallBack( UINT8 bExitValue );

	// SQUAD_BIVOUAC: pending assignment change deferred to the disband YES-callback.
	// When the last merc of a squad with a non-empty bivouac is being reassigned, the
	// caller stores the requested change here INSTEAD of performing it; if the player
	// answers YES, items are dumped and the change is replayed; on NO it is discarded.
	struct PendingAssignChange
	{
		UINT16	usSoldierID;
		INT8	bAssignment;
		INT32	iParam1;
		INT32	iParam2;
		INT32	iParam3;
		BOOLEAN	fValid;
	};

	// Queue a SetSoldierAssignment()-call to be executed after the player confirms the dump.
	void	SetPendingAssignmentChange( SOLDIERTYPE* pSoldier, INT8 bAssignment, INT32 iParam1, INT32 iParam2, INT32 iParam3 );

	// Fetch the deferred assignment change (if any) and consume it. Returns FALSE if none pending.
	BOOLEAN	TakePendingAssignmentChange( UINT16& usSoldierID, INT8& bAssignment, INT32& iParam1, INT32& iParam2, INT32& iParam3 );

	////////////////////////////////////////////////////////////////////////////////////////////
	// Save / Load (savegame)
	////////////////////////////////////////////////////////////////////////////////////////////

	// Writes all squad bivouac data as a fixed-size block. Returns FALSE on file error.
	BOOLEAN	Save( HWFILE hFile );
	// Reads all squad bivouac data. Returns FALSE on file error or corrupted size fields.
	BOOLEAN	Load( HWFILE hFile );

	////////////////////////////////////////////////////////////////////////////////////////////
	// Map screen UI state
	////////////////////////////////////////////////////////////////////////////////////////////

	// Bivouac overlay mode: when TRUE, the map screen "team inventory" panel shows
	// the bivouac slots of the selected character's squad instead of the vehicle trunk.
	BOOLEAN	IsBivouacUIModeActive()				{ return m_fBivouacUIMode; }
	void	SetBivouacUIMode( BOOLEAN fActive )	{ m_fBivouacUIMode = fActive; }
	void	ToggleBivouacUIMode()				{ m_fBivouacUIMode = !m_fBivouacUIMode; }

	// Squad id currently shown in bivouac UI mode (-1 = none). Derived from selected char.
	INT32	GetBivouacUISquadID();
	// Enable/disable UI mode based on the currently selected map screen character.
	// Returns FALSE (and disables mode) if access is not allowed for that squad.
	BOOLEAN	UpdateBivouacUIModeForSelectedChar( INT8 bSelectedInfoChar );

	// Used by the messagebox callback: squad id pending disband decision.
	void	SetPendingDisbandSquad( INT32 iSquadID, UINT16 usSoldierID );
	INT32	GetPendingDisbandSquad()			{ return m_iPendingDisbandSquad; }

	////////////////////////////////////////////////////////////////////////////////////////////
	// Map screen sector-inventory <-> bivouac bridge (SQUAD_BIVOUAC UI mode)
	////////////////////////////////////////////////////////////////////////////////////////////

	// Build pInventoryPoolList-compatible WORLDITEM array from the squad's bivouac slots.
	// Output vector is resized to BIVOUAC_NUM_SLOTS entries (fExists = slot used).
	void	BuildBivouacStash( std::vector< WORLDITEM >& vec );

	// Read items back from the WORLDITEM array into the squad's bivouac slots (only first
	// BIVOUAC_NUM_SLOTS entries), clearing the squad slots first. Used on closing the UI.
	void	StoreBivouacStash( std::vector< WORLDITEM >& vec );

	// squad whose bivouac is currently displayed in the sector inventory UI, -1 = sector inventory
	INT32	GetActiveUISquadID()				{ return m_iActiveUISquad; }
	void	SetActiveUISquadID( INT32 iSquadID ){ m_iActiveUISquad = iSquadID; }

private:
	SquadBivouacManager();
	~SquadBivouacManager();
	SquadBivouacManager( const SquadBivouacManager& );				// no copy
	SquadBivouacManager& operator=( const SquadBivouacManager& );	// no assign

	// helpers
	INT32	SquadToDataIndex( INT32 iSquadID ) const;		// validate & map id -> array index
	UINT16	CountLiveMercsInSquad( INT32 iSquadID ) const;
	BOOLEAN	IsSectorPeacefulAndOurs( INT32 iSquadID ) const;
	UINT32	GetMercMaxCarryWeight( SOLDIERTYPE* pSoldier ) const;	// strength-based max carry weight of one merc

	// storage: one data block per squad (NUMBER_OF_SQUADS defined in Squads.h)
	BivouacData		m_Bivouac[ NUMBER_OF_SQUADS ];

	// UI state
	BOOLEAN			m_fBivouacUIMode;

	// disband interception state (squad whose bivouac triggered the YES/NO box)
	INT32			m_iPendingDisbandSquad;
	UINT16			m_usPendingDisbandSoldierID;

	// deferred assignment change executed when the player confirms the dump
	PendingAssignChange	m_PendingAssignChange;

	// squad whose bivouac is currently displayed in the map screen sector inventory UI (-1 = none)
	INT32			m_iActiveUISquad;
};

#endif // _SQUAD_BIVOUAC_MANAGER_H_
