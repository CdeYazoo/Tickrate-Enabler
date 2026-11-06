#include "memutils.h"
#include "physicsfix.h"
#include "rescuepatch.h"
#include "limitspatch.h"
#include "patchexceptions.h"

bool RegisterPhysicsFix(PatchManager & manager, void * vphysicsAnchor, float solverStep)
{
	RescueSpeedPatch * rescue = NULL;
	AngularLimitPatch * limits = NULL;
	try
	{
		// Construct both first; each checks its own bytes and changes nothing until PatchAll() runs.
		rescue = new RescueSpeedPatch(vphysicsAnchor, VANILLA_PHYSICS_TIMESTEP);
		limits = new AngularLimitPatch(vphysicsAnchor, solverStep);
	}
	catch(PatchException & e)
	{
		delete rescue;
		delete limits;
		Warning("Tickrate_Enabler: physics fix NOT applied, the server runs with unmodified vphysics: %s\n", e.GetDescription());
		return false;
	}
	manager.Register(rescue);
	manager.Register(limits);
	Msg("Tickrate_Enabler: physics fix active (solver step %.4fs)\n", solverStep);
	return true;
}
