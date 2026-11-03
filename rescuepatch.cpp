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

// Every site is a 'cvtsd2ss xmm, qword ptr [reg+0xb8]' (8 bytes) that reads 1/PSI from the environment. It is replaced by
// 'cvtsd2ss xmm, qword ptr [abs32]' (same length, same register) pointing at g_RescueInvStep.
struct PatchSite
{
	const char * name;
	const char * symbol;
	int offset;
	BYTE original[8];
	BYTE replacementHead[4];
};

static const PatchSite kSites[] =
{
	{ "get_rescue_speed_impact", LIN_RescueSpeed_SYMBOL, LIN_RescueSpeed_Offset,
	  { 0xF2, 0x0F, 0x5A, 0x92, 0xB8, 0x00, 0x00, 0x00 },   // cvtsd2ss xmm2, [edx+0xb8]
	  { 0xF2, 0x0F, 0x5A, 0x15 } },                          // cvtsd2ss xmm2, [abs32]
	{ "get_rot_speed_uncertainty", LIN_RotSpeedUncertainty_SYMBOL, LIN_RotSpeedUncertainty_Offset,
	  { 0xF2, 0x0F, 0x5A, 0x82, 0xB8, 0x00, 0x00, 0x00 },   // cvtsd2ss xmm0, [edx+0xb8]
	  { 0xF2, 0x0F, 0x5A, 0x05 } }                           // cvtsd2ss xmm0, [abs32]
};

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
{
	for(int i = 0; i < SITE_COUNT; i++) m_pPatches[i] = NULL;
#if defined (_LINUX)
	if(!vphysicsAnchor)
	{
		throw PatchException("Rescue speed: couldn't find the loaded vphysics library (vphysics_srv.so).");
	}
#else
	throw PatchException("The vphysics rescue speed patch has no Windows signatures yet (see sigs.h).");
#endif

	// First check every site, so a mismatch anywhere means nothing gets patched.
	BYTE * targets[SITE_COUNT];
	for(int i = 0; i < SITE_COUNT; i++)
	{
		const PatchSite & site = kSites[i];
		BYTE * pFunction = (BYTE *)g_MemUtils.SimpleResolve(vphysicsAnchor, site.symbol);
		DevMsg("%s at %p\n", site.name, pFunction);
		if(!pFunction)
		{
			throw PatchException::Format("Rescue speed: couldn't find IVP_Contact_Point::%s() in vphysics.", site.name);
		}
		BYTE * pTarget = pFunction + site.offset;
		if(memcmp(pTarget, site.original, sizeof(site.original)) != 0)
		{
			char found[sizeof(site.original) * 3 + 1], expected[sizeof(site.original) * 3 + 1];
			size_t usedFound = 0, usedExpected = 0;
			found[0] = expected[0] = '\0';
			for(size_t j = 0; j < sizeof(site.original); j++)
			{
				usedFound += snprintf(found + usedFound, sizeof(found) - usedFound, "%s%02x", j ? " " : "", pTarget[j]);
				usedExpected += snprintf(expected + usedExpected, sizeof(expected) - usedExpected, "%s%02x", j ? " " : "", site.original[j]);
			}
			throw PatchException::Format(
				"Rescue speed: unexpected instruction at %s+0x%x (function at %p, target %p). "
				"Expected 'cvtsd2ss xmm,[reg+0xb8]' (%s) but found bytes: %s. "
				"vphysics was probably updated; re-derive the offsets in sigs.h.",
				site.name, site.offset, pFunction, pTarget, expected, found);
		}
		targets[i] = pTarget;
	}

	g_RescueInvStep = 1.0 / (double)timestep;

	const double * pNewConst = &g_RescueInvStep;
	for(int i = 0; i < SITE_COUNT; i++)
	{
		BYTE instr_buf[sizeof(kSites[i].original)];
		memcpy(instr_buf, kSites[i].replacementHead, sizeof(kSites[i].replacementHead));
		memcpy(instr_buf + sizeof(kSites[i].replacementHead), &pNewConst, sizeof(pNewConst));
		m_pPatches[i] = new BasicStaticBinPatch<8>(targets[i], instr_buf);
	}

	Msg("Tickrate_Enabler: physics rescue speed (2 reads) now uses a %.4fs step (the solver keeps its own step)\n", timestep);
}

RescueSpeedPatch::~RescueSpeedPatch()
{
	for(int i = 0; i < SITE_COUNT; i++) delete m_pPatches[i];
}

void RescueSpeedPatch::Patch()
{
	for(int i = 0; i < SITE_COUNT; i++) if(m_pPatches[i]) m_pPatches[i]->Patch();
}

void RescueSpeedPatch::Unpatch()
{
	for(int i = 0; i < SITE_COUNT; i++) if(m_pPatches[i]) m_pPatches[i]->Unpatch();
}
