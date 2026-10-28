#ifndef __SENTRYPATCH_H__
#define __SENTRYPATCH_H__

#include "eiface.h"
#include "codepatch/icodepatch.h"
#include "basicbinpatch.h"
#include "misc_asm.h"
#include "sigs.h"

// Defined in tickrate_enabler.cpp: the tick interval the plugin gives the engine.
float GetConfiguredTickInterval(bool verbose);

// Think delay that gives the same sentry think period, in seconds, as vanilla TF2 (200/3 tick, 3 ticks = 45ms).
// Always a whole number of ticks, so Source's tick rounding cannot flip it. Exposed for testing.
double ComputeSentryThinkDelay(float tickInterval);

// Makes CObjectSentrygun::SentryThink() re-arm after a tick-aligned delay instead of a flat 0.05s, so the fire rate of
// sentries (wrangled ones especially) no longer depends on how the 0.05s happens to round at the chosen tickrate.
class SentryThinkDelayPatch : public ICodePatch
{
public:
	SentryThinkDelayPatch(IServerGameDLL * gamedll, float tickInterval);
	~SentryThinkDelayPatch();
	void Patch();
	void Unpatch();
private:
	BasicStaticBinPatch<ADDSD_ABS32_INSTR_LEN> * m_pPatch;
};

#endif
