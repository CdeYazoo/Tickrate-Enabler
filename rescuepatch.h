#ifndef __RESCUEPATCH_H__
#define __RESCUEPATCH_H__

#include "eiface.h"
#include "codepatch/icodepatch.h"
#include "basicbinpatch.h"
#include "sigs.h"
#include "physstep.h" // VANILLA_PHYSICS_TIMESTEP

// Returns the address of a function inside the already-loaded vphysics library (its CreateInterface), or NULL if that
// library is not loaded. It is only used to find the library for symbol lookups.
void * FindVPhysicsAnchor();

// Makes vphysics' contact "rescue" speed use the vanilla 15ms step instead of the real physics step, in the three places
// where it divides by the step: IVP_Contact_Point::get_rescue_speed_impact (penetration depth),
// IVP_Contact_Point::get_rot_speed_uncertainty (rotation) and the inlined copy of the depth formula in
// IVP_Impact_Solver_Long_Term::do_impact_of_two_objects. The solver itself keeps stepping at the tick interval.
// Opt-in (-te_rescue [seconds]).
class RescueSpeedPatch : public ICodePatch
{
public:
	RescueSpeedPatch(void * vphysicsAnchor, float timestep = VANILLA_PHYSICS_TIMESTEP);
	~RescueSpeedPatch();
	void Patch();
	void Unpatch();
private:
	enum { SITE_COUNT = 3 };
	BasicStaticBinPatch<8> * m_pPatches[SITE_COUNT];
};

#endif
