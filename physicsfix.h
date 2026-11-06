#ifndef __PHYSICSFIX_H__
#define __PHYSICSFIX_H__

#include "codepatch/patchmanager.h"

// The vphysics fix: makes the contact "rescue" speed (3 reads, see rescuepatch.h) and the angular speed cap (see limitspatch.h)
// behave as they do at vanilla's 15ms physics step, while the solver keeps stepping at the tick interval.
//
// Best effort: both patches are applied together or not at all. If anything does not match (for example after a game or
// vphysics update), a warning is logged and the server simply runs with unmodified vphysics. It never throws.
//
// vphysicsAnchor: an address inside the loaded vphysics library (see FindVPhysicsAnchor), or NULL.
// solverStep: the step the physics solver will really run at, in seconds.
// Returns true if the fix was registered with the manager.
bool RegisterPhysicsFix(PatchManager & manager, void * vphysicsAnchor, float solverStep);

#endif
