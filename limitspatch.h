#ifndef __LIMITSPATCH_H__
#define __LIMITSPATCH_H__

#include "eiface.h"
#include "codepatch/icodepatch.h"
#include "basicbinpatch.h"
#include "sigs.h"

// vphysics stores its angular speed cap as (step * maxAngularVelocity), but SetPerformanceSettings runs before the step is set,
// so the cap ends up scaled for IVP's default step and not the real one. This patch makes that one calculation use the step the
// solver really runs at, so the cap comes out as exactly maxAngularVelocity at any tickrate.
// Opt-in (-te_limits [seconds]); by default the step is the tick interval, or the -te_physstep value when that is on.
class AngularLimitPatch : public ICodePatch
{
public:
	AngularLimitPatch(void * vphysicsAnchor, float solverStep);
	~AngularLimitPatch();
	void Patch();
	void Unpatch();
private:
	BasicStaticBinPatch<8> * m_pPatch;
};

#endif
