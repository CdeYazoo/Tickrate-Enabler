#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "memutils.h"
#include "rescuepatch.h"
#include "patchexceptions.h"
#if defined (_LINUX)
#include <link.h>
#include <dlfcn.h>
#endif

// The patched instruction embeds a raw 32-bit pointer.
static_assert(sizeof(void *) == 4, "rescuepatch only supports 32-bit x86 (absolute-address patching)");

// Replaces 1/PSI in get_rescue_speed_impact. The patched instruction embeds this variable's address.
static double g_RescueInvStep = 1.0 / VANILLA_PHYSICS_TIMESTEP;

// cvtsd2ss xmm2, qword ptr [edx+0xb8]   reads 1/PSI from the environment
static const BYTE kOriginal[8] = { 0xF2, 0x0F, 0x5A, 0x92, 0xB8, 0x00, 0x00, 0x00 };
// cvtsd2ss xmm2, qword ptr [abs32]      same length and register; the 4 address bytes follow
static const BYTE kReplacementHead[4] = { 0xF2, 0x0F, 0x5A, 0x15 };

#if defined (_LINUX)
struct ModuleSearch
{
	const char * needle;
	void * anchor;
};

static int FindModuleCallback(struct dl_phdr_info * info, size_t, void * data)
{
	ModuleSearch * s = (ModuleSearch *)data;
	if(info->dlpi_name && *info->dlpi_name && strstr(info->dlpi_name, s->needle))
	{
		void * handle = dlopen(info->dlpi_name, RTLD_NOW | RTLD_NOLOAD);
		if(handle)
		{
			s->anchor = dlsym(handle, "CreateInterface");
			dlclose(handle);
			if(s->anchor) return 1;
		}
	}
	return 0;
}
#endif

void * FindVPhysicsAnchor()
{
#if defined (_LINUX)
	ModuleSearch s = { "vphysics", NULL };
	dl_iterate_phdr(FindModuleCallback, &s);
	return s.anchor;
#else
	return NULL;
#endif
}

RescueSpeedPatch::RescueSpeedPatch(void * vphysicsAnchor, float timestep)
	: m_pPatch(NULL)
{
#if defined (_LINUX)
	if(!vphysicsAnchor)
	{
		throw PatchException("Rescue speed: couldn't find the loaded vphysics library (vphysics_srv.so).");
	}
	BYTE * pFunction = (BYTE *)g_MemUtils.SimpleResolve(vphysicsAnchor, LIN_RescueSpeed_SYMBOL);
	const int funcOffset = LIN_RescueSpeed_Offset;
#else
	BYTE * pFunction = NULL;
	const int funcOffset = 0;
	throw PatchException("The IVP_Contact_Point::get_rescue_speed_impact() patch has no Windows signature yet (see sigs.h).");
#endif
	DevMsg("get_rescue_speed_impact at %p\n", pFunction);
	if(!pFunction)
	{
		throw PatchException("Rescue speed: couldn't find IVP_Contact_Point::get_rescue_speed_impact() in vphysics.");
	}

	BYTE * pTarget = pFunction + funcOffset;
	if(memcmp(pTarget, kOriginal, sizeof(kOriginal)) != 0)
	{
		char found[sizeof(kOriginal) * 3 + 1];
		size_t used = 0;
		found[0] = '\0';
		for(size_t i = 0; i < sizeof(kOriginal); i++)
			used += snprintf(found + used, sizeof(found) - used, "%s%02x", i ? " " : "", pTarget[i]);
		throw PatchException::Format(
			"Rescue speed: unexpected instruction at get_rescue_speed_impact+0x%x (function at %p, target %p). "
			"Expected 'cvtsd2ss xmm2,[edx+0xb8]' (f2 0f 5a 92 b8 00 00 00) but found bytes: %s. "
			"vphysics was probably updated; re-derive LIN_RescueSpeed_Offset in sigs.h.",
			funcOffset, pFunction, pTarget, found);
	}

	g_RescueInvStep = 1.0 / (double)timestep;

	BYTE instr_buf[sizeof(kOriginal)];
	memcpy(instr_buf, kReplacementHead, sizeof(kReplacementHead));
	const double * pNewConst = &g_RescueInvStep;
	memcpy(instr_buf + sizeof(kReplacementHead), &pNewConst, sizeof(pNewConst));
	m_pPatch = new BasicStaticBinPatch<8>(pTarget, instr_buf);

	Msg("Tickrate_Enabler: physics rescue speed now uses a %.4fs step (the solver keeps its own step)\n", timestep);
}

RescueSpeedPatch::~RescueSpeedPatch()
{
	delete m_pPatch;
}

void RescueSpeedPatch::Patch()
{
	if(m_pPatch) m_pPatch->Patch();
}

void RescueSpeedPatch::Unpatch()
{
	if(m_pPatch) m_pPatch->Unpatch();
}
