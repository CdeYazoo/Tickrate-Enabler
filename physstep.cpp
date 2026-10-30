#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include "memutils.h"
#include "physstep.h"
#include "patchexceptions.h"

// The patched instruction loads a raw 32-bit pointer.
static_assert(sizeof(void *) == 4, "physstep only supports 32-bit x86 (absolute-address mov patching)");

// Only interval_per_tick (+0x1c) is read through the patched pointer.
struct physFakeGlobals
{
	float padding[7];
	float interval_per_tick;
};
static_assert(offsetof(physFakeGlobals, interval_per_tick) == 0x1c, "interval_per_tick must sit at +0x1c like in CGlobalVars");

static physFakeGlobals g_PhysFakeGlobals = { {0, 0, 0, 0, 0, 0, 0}, VANILLA_PHYSICS_TIMESTEP };
// The patched instruction is 'mov ecx, [address]', so it needs the address of a variable that holds the pointer.
static physFakeGlobals * gp_PhysFakeGlobals = &g_PhysFakeGlobals;

// The code right after the load, which makes sure this really is the SetSimulationTimestep call:
//   8b 10               mov edx, [eax]        physenv's vtable
//   ff 71 1c            push [ecx+0x1c]       gpGlobals->interval_per_tick
//   50                  push eax              this
//   ff 92 98 00 00 00   call [edx+0x98]       IPhysicsEnvironment::SetSimulationTimestep
static const BYTE kExpectedAfterLoad[] = { 0x8B, 0x10, 0xFF, 0x71, 0x1C, 0x50, 0xFF, 0x92, 0x98, 0x00, 0x00, 0x00 };

static void HexBytes(char * out, size_t outLen, const BYTE * p, int n)
{
	size_t used = 0;
	out[0] = '\0';
	for(int i = 0; i < n && used + 4 <= outLen; i++)
		used += snprintf(out + used, outLen - used, "%s%02x", i ? " " : "", p[i]);
}

PhysicsStepPatch::PhysicsStepPatch(IServerGameDLL * gamedll)
	: m_pPatch(NULL)
{
#if defined (_LINUX)
	BYTE * pFunction = (BYTE *)g_MemUtils.SimpleResolve(static_cast<void *>(gamedll), LIN_PhysicsHook_LevelInitPreEntity_SYMBOL);
	const int funcOffset = LIN_PhysStep_Offset;
#else
	BYTE * pFunction = NULL;
	const int funcOffset = 0;
	throw PatchException("The CPhysicsHook::LevelInitPreEntity() patch has no Windows signature yet (see sigs.h).");
#endif
	DevMsg("LevelInitPreEntity at %p\n", pFunction);
	if(!pFunction)
	{
		throw PatchException("Couldn't find CPhysicsHook::LevelInitPreEntity() in server memory.");
	}

	BYTE * pTarget = pFunction + funcOffset;
	const int kLoadLen = MAX_MOV_INSTR_LEN; // mov ecx, [abs32] is 6 bytes
	const int kShown = kLoadLen + (int)sizeof(kExpectedAfterLoad);

	// Must be exactly 'mov ecx, [abs32]' (8b 0d ...) followed by the SetSimulationTimestep call sequence.
	if(!(pTarget[0] == MOV_R32_RM32_OPCODE && pTarget[1] == 0x0D) || memcmp(pTarget + kLoadLen, kExpectedAfterLoad, sizeof(kExpectedAfterLoad)) != 0)
	{
		char found[kShown * 3 + 1];
		HexBytes(found, sizeof(found), pTarget, kShown);
		throw PatchException::Format(
			"Physics timestep: unexpected code at LevelInitPreEntity+0x%x (function at %p, target %p). "
			"Expected 'mov ecx,[gpGlobals]' then the SetSimulationTimestep call (8b 0d .. .. .. .. 8b 10 ff 71 1c 50 ff 92 98 00 00 00) but found: %s. "
			"The game binary was probably updated; re-derive LIN_PhysStep_Offset in sigs.h.",
			funcOffset, pFunction, pTarget, found);
	}

	BYTE instr_buf[MAX_MOV_INSTR_LEN];
	memcpy(instr_buf, pTarget, MAX_MOV_INSTR_LEN);
	const int offs = mov_src_operand_offset(instr_buf);
	if(offs == 0)
	{
		throw PatchException("Physics timestep: internal error, load instruction not recognised by mov_src_operand_offset.");
	}

	physFakeGlobals ** pNewSource = &gp_PhysFakeGlobals;
	memcpy(instr_buf + offs, &pNewSource, sizeof(pNewSource));
	m_pPatch = new BasicStaticBinPatch<MAX_MOV_INSTR_LEN>(pTarget, instr_buf);

	Msg("Tickrate_Enabler: physics timestep pinned to %.4fs (takes effect when a level loads)\n", g_PhysFakeGlobals.interval_per_tick);
}

PhysicsStepPatch::~PhysicsStepPatch()
{
	delete m_pPatch;
}

void PhysicsStepPatch::Patch()
{
	if(m_pPatch) m_pPatch->Patch();
}

void PhysicsStepPatch::Unpatch()
{
	if(m_pPatch) m_pPatch->Unpatch();
}
