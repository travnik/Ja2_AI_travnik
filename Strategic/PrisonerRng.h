#ifndef PRISONER_RNG_H
#define PRISONER_RNG_H

#include "Types.h"
#include "Campaign Types.h"

// travnik: детерминированный ГПСЧ для допроса, чтобы Save/Load не менял результат
class CPrisonerRng
{
public:
	CPrisonerRng(UINT32 uiSeed, BOOLEAN fDeterministic);

	// travnik: случайное число 0..uiRange-1
	UINT32 Next(UINT32 uiRange);

	// travnik: проверка шанса в процентах
	BOOLEAN Chance(UINT32 uiChance);

	// travnik: аналог PreRandom для необязательных бросков
	UINT32 PreNext(UINT32 uiRange);

	// travnik: seed из состояния, которое сохраняется в сейве
	static UINT32 MakeInterrogationSeed(UINT32 uiSectorId, UINT32 uiWorldHour,
		UINT32 uiInterrogationPoints, UINT32 uiOldInterrogationPoints,
		const UINT8* pubPrisoners);

	// travnik: seed для проверки раскрытия снаitch-а
	static UINT32 MakeSnitchExpositionSeed(UINT32 uiSectorId, UINT32 uiWorldHour,
		UINT8 ubSoldierID, UINT16 usPrisonerCount);

private:
	void Advance();
	static UINT32 Mix(UINT32 uiHash, UINT32 uiValue);

	BOOLEAN m_fDeterministic;
	UINT32 m_uiState;
};

#endif
