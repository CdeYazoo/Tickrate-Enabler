#ifndef __PHYSSTEP_H__
#define __PHYSSTEP_H__

#include "eiface.h"
#include "codepatch/icodepatch.h"
#include "basicbinpatch.h"
#include "misc_asm.h"
#include "sigs.h"

// The vphysics simulation step vanilla TF2 uses at its default tickrate (200/3 Hz), in seconds.
#define VANILLA_PHYSICS_TIMESTEP 0.015f

// Makes CPhysicsHook::LevelInitPreEntity() give the physics solver the vanilla 15ms step instead of the tick interval.
// Opt-in (-te_physstep). Takes effect the next time a level loads.
class PhysicsStepPatch : public ICodePatch
{
public:
	PhysicsStepPatch(IServerGameDLL * gamedll);
	~PhysicsStepPatch();
	void Patch();
	void Unpatch();
private:
	BasicStaticBinPatch<MAX_MOV_INSTR_LEN> * m_pPatch;
};

#endif
