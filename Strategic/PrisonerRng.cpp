#include "PrisonerRng.h"
#include "random.h"

namespace
{
	// travnik: параметры хеширования seed
	const UINT32 FNV_OFFSET_BASIS = 2166136261u;
	const UINT32 FNV_PRIME = 16777619u;
	const UINT32 NON_ZERO_SEED = 0x9E3779B9u;

	// travnik: соли, чтобы потоки допроса и снаitch-а не совпадали
	const UINT32 INTERROGATION_SEED_SALT = 0x494E5452u;
	const UINT32 SNITCH_SEED_SALT = 0x534E4954u;

	// travnik: параметры xorshift32
	const UINT32 XORSHIFT_SHIFT_1 = 13;
	const UINT32 XORSHIFT_SHIFT_2 = 17;
	const UINT32 XORSHIFT_SHIFT_3 = 5;

	// travnik: разрядность результата ГПСЧ
	const UINT32 RNG_OUTPUT_BITS = 32;

	// travnik: диапазон процентного броска
	const UINT32 PERCENT_RANGE = 100;
}

CPrisonerRng::CPrisonerRng(UINT32 uiSeed, BOOLEAN fDeterministic)
	: m_fDeterministic(fDeterministic)
	, m_uiState(uiSeed ? uiSeed : NON_ZERO_SEED)
{
}

void CPrisonerRng::Advance()
{
	m_uiState ^= m_uiState << XORSHIFT_SHIFT_1;
	m_uiState ^= m_uiState >> XORSHIFT_SHIFT_2;
	m_uiState ^= m_uiState << XORSHIFT_SHIFT_3;
}

UINT32 CPrisonerRng::Next(UINT32 uiRange)
{
	// travnik: при выключенной защите сохраняем прежнее поведение
	if ( !m_fDeterministic )
		return Random( uiRange );

	if ( uiRange == 0 )
		return 0;

	Advance();

	return (UINT32)( ( (UINT64)m_uiState * uiRange ) >> RNG_OUTPUT_BITS );
}

BOOLEAN CPrisonerRng::Chance(UINT32 uiChance)
{
	return ( Next( PERCENT_RANGE ) < uiChance ) ? TRUE : FALSE;
}

UINT32 CPrisonerRng::PreNext(UINT32 uiRange)
{
	// travnik: при выключенной защите используем прежний PreRandom
	if ( !m_fDeterministic )
		return PreRandom( uiRange );

	return Next( uiRange );
}

UINT32 CPrisonerRng::Mix(UINT32 uiHash, UINT32 uiValue)
{
	uiHash ^= uiValue;
	uiHash *= FNV_PRIME;
	return uiHash;
}

UINT32 CPrisonerRng::MakeInterrogationSeed(UINT32 uiSectorId, UINT32 uiWorldHour,
	UINT32 uiInterrogationPoints, UINT32 uiOldInterrogationPoints,
	const UINT8* pubPrisoners)
{
	UINT32 uiHash = FNV_OFFSET_BASIS;

	uiHash = Mix( uiHash, INTERROGATION_SEED_SALT );
	uiHash = Mix( uiHash, uiSectorId );
	uiHash = Mix( uiHash, uiWorldHour );
	uiHash = Mix( uiHash, uiInterrogationPoints );
	uiHash = Mix( uiHash, uiOldInterrogationPoints );

	for ( UINT8 ubPrisonerType = PRISONER_ADMIN; ubPrisonerType < PRISONER_MAX; ++ubPrisonerType )
		uiHash = Mix( uiHash, pubPrisoners[ubPrisonerType] );

	return uiHash ? uiHash : NON_ZERO_SEED;
}

UINT32 CPrisonerRng::MakeSnitchExpositionSeed(UINT32 uiSectorId, UINT32 uiWorldHour,
	UINT8 ubSoldierID, UINT16 usPrisonerCount)
{
	UINT32 uiHash = FNV_OFFSET_BASIS;

	uiHash = Mix( uiHash, SNITCH_SEED_SALT );
	uiHash = Mix( uiHash, uiSectorId );
	uiHash = Mix( uiHash, uiWorldHour );
	uiHash = Mix( uiHash, ubSoldierID );
	uiHash = Mix( uiHash, usPrisonerCount );

	return uiHash ? uiHash : NON_ZERO_SEED;
}
