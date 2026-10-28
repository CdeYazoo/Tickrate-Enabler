#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "memutils.h"
#include "sentrypatch.h"
#include "patchexceptions.h"

/* Why a flat 0.05s think delay makes sentry timing depend on tickrate (checked by decompiling TF2 PatchVersion=11138802):
 *  - CBaseEntity::ThinkSet / SetNextThink store (int)(time / gpGlobals->interval_per_tick + 0.5) as the next think tick.
 *  - CBaseEntity::PhysicsRunSpecificThink runs a think at most once per tick, once tickcount >= that tick. It clears the
 *    next-think tick first, then dispatches; SentryThink re-arms itself with ThinkSet(curtime + 0.05).
 *  - So the think period is round(0.05 / interval) ticks, which is 3 ticks (45ms) at 200/3 tick but 10 ticks (50ms) at 200 tick.
 *    Attack() then gates shots on curtime >= nextAttack, and MoveTurret() assumes 0.05s per call, so both follow the think period.
 * Not seen in the binary: the engine setting curtime = tickcount * interval each tick (standard Source behaviour). */

// The patched instruction embeds this variable's address, so it must outlive the patch.
static double g_SentryThinkDelay = 0.045;

// The addsd operand is written as a raw 32-bit pointer.
static_assert(sizeof(void *) == 4, "sentrypatch only supports 32-bit x86 (absolute-address addsd patching)");

double ComputeSentryThinkDelay(float tickInterval)
{
	const double kVanillaDelay = 0.045; // 3 ticks at 200/3 tick
	if(!(tickInterval > 0.0f)) return kVanillaDelay;
	// Whole ticks only. Subtracting a hair makes an exact .5 round down rather than depending on float noise.
	int ticks = (int)floor(kVanillaDelay / tickInterval + 0.5 - 1e-3);
	if(ticks < 1) ticks = 1;
	return ticks * (double)tickInterval;
}

SentryThinkDelayPatch::SentryThinkDelayPatch(IServerGameDLL * gamedll, float tickInterval)
	: m_pPatch(NULL)
{
#if defined (_LINUX)
	BYTE * pFunction = (BYTE *)g_MemUtils.SimpleResolve(static_cast<void *>(gamedll), LIN_SentryThink_SYMBOL);
	const int funcOffset = LIN_SentryThink_DelayOffset;
#else
	BYTE * pFunction = NULL;
	const int funcOffset = 0;
	throw PatchException("The CObjectSentrygun::SentryThink() patch has no Windows signature yet (see sigs.h).");
#endif
	DevMsg("SentryThink at %p\n", pFunction);
	if(!pFunction)
	{
		throw PatchException("Couldn't find CObjectSentrygun::SentryThink() in server memory.");
	}

	BYTE * pTarget = pFunction + funcOffset;
	if(!is_addsd_abs32(pTarget))
	{
		char found[ADDSD_ABS32_INSTR_LEN * 3 + 1];
		size_t used = 0;
		found[0] = '\0';
		for(int i = 0; i < ADDSD_ABS32_INSTR_LEN; i++)
			used += snprintf(found + used, sizeof(found) - used, "%s%02x", i ? " " : "", pTarget[i]);
		throw PatchException::Format(
			"SentryThink delay: unexpected instruction at SentryThink+0x%x (function at %p, target %p). "
			"Expected 'addsd xmm, [abs32]' (f2 0f 58 05 ...) but found bytes: %s. "
			"The game binary was probably updated; re-derive LIN_SentryThink_DelayOffset in sigs.h.",
			funcOffset, pFunction, pTarget, found);
	}

	// The instruction must be adding the 0.05s think delay. If the constant is anything else, this is the wrong instruction.
	const BYTE * pOperand = pTarget + ADDSD_ABS32_OPERAND_OFFSET;
	const double * pOrigConst;
	memcpy(&pOrigConst, pOperand, sizeof(pOrigConst));
	double origConst;
	memcpy(&origConst, pOrigConst, sizeof(origConst));
	if(fabs(origConst - 0.05) > 1e-9)
	{
		throw PatchException::Format(
			"SentryThink delay: addsd at SentryThink+0x%x reads %.9g from %p, expected 0.05. "
			"Wrong instruction or changed game code; re-derive LIN_SentryThink_DelayOffset in sigs.h.",
			funcOffset, origConst, pOrigConst);
	}

	g_SentryThinkDelay = ComputeSentryThinkDelay(tickInterval);
	DevMsg("SentryThink think delay: 0.05s -> %.6fs (tick interval %.6f)\n", g_SentryThinkDelay, tickInterval);

	BYTE instr_buf[ADDSD_ABS32_INSTR_LEN];
	memcpy(instr_buf, pTarget, ADDSD_ABS32_INSTR_LEN);
	const double * pNewConst = &g_SentryThinkDelay;
	memcpy(instr_buf + ADDSD_ABS32_OPERAND_OFFSET, &pNewConst, sizeof(pNewConst));
	m_pPatch = new BasicStaticBinPatch<ADDSD_ABS32_INSTR_LEN>(pTarget, instr_buf);
}

SentryThinkDelayPatch::~SentryThinkDelayPatch()
{
	delete m_pPatch;
}

void SentryThinkDelayPatch::Patch()
{
	if(m_pPatch) m_pPatch->Patch();
}

void SentryThinkDelayPatch::Unpatch()
{
	if(m_pPatch) m_pPatch->Unpatch();
}
