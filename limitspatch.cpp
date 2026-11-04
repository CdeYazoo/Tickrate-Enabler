#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "memutils.h"
#include "limitspatch.h"
#include "patchexceptions.h"

// The patched instruction embeds a raw 32-bit pointer.
static_assert(sizeof(void *) == 4, "limitspatch only supports 32-bit x86 (absolute-address patching)");

// Replaces the step read in SetPerformanceSettings. The patched instruction embeds this variable's address.
static double g_LimitStep = 0.015;

// cvtsd2ss xmm0, qword ptr [ebx+0xb0]   reads the step from the environment
static const BYTE kOriginal[8] = { 0xF2, 0x0F, 0x5A, 0x83, 0xB0, 0x00, 0x00, 0x00 };
// cvtsd2ss xmm0, qword ptr [abs32]      same length and register; the 4 address bytes follow
static const BYTE kReplacementHead[4] = { 0xF2, 0x0F, 0x5A, 0x05 };
// What follows must be the two multiplies: * maxAngularVelocity ([eax+0xc]) and * (pi/180) (an absolute address, which is
// relocated at load time, so only its opcode bytes are compared).
static const BYTE kExpectedAfter[9] = { 0xF3, 0x0F, 0x59, 0x40, 0x0C, 0xF3, 0x0F, 0x59, 0x05 };

static void HexBytes(char * out, size_t outLen, const BYTE * p, int n)
{
	size_t used = 0;
	out[0] = '\0';
	for(int i = 0; i < n && used + 4 <= outLen; i++)
		used += snprintf(out + used, outLen - used, "%s%02x", i ? " " : "", p[i]);
}

AngularLimitPatch::AngularLimitPatch(void * vphysicsAnchor, float solverStep)
	: m_pPatch(NULL)
{
#if defined (_LINUX)
	if(!vphysicsAnchor)
	{
		throw PatchException("Angular limit: couldn't find the loaded vphysics library (vphysics_srv.so).");
	}
	BYTE * pFunction = (BYTE *)g_MemUtils.SimpleResolve(vphysicsAnchor, LIN_PerfSettings_SYMBOL);
	const int funcOffset = LIN_PerfSettings_Offset;
#else
	BYTE * pFunction = NULL;
	const int funcOffset = 0;
	throw PatchException("The CPhysicsEnvironment::SetPerformanceSettings() patch has no Windows signature yet (see sigs.h).");
#endif
	DevMsg("SetPerformanceSettings at %p\n", pFunction);
	if(!pFunction)
	{
		throw PatchException("Angular limit: couldn't find CPhysicsEnvironment::SetPerformanceSettings() in vphysics.");
	}

	BYTE * pTarget = pFunction + funcOffset;
	if(memcmp(pTarget, kOriginal, sizeof(kOriginal)) != 0 || memcmp(pTarget + sizeof(kOriginal), kExpectedAfter, sizeof(kExpectedAfter)) != 0)
	{
		char found[(sizeof(kOriginal) + sizeof(kExpectedAfter)) * 3 + 1];
		HexBytes(found, sizeof(found), pTarget, (int)(sizeof(kOriginal) + sizeof(kExpectedAfter)));
		throw PatchException::Format(
			"Angular limit: unexpected code at SetPerformanceSettings+0x%x (function at %p, target %p). "
			"Expected 'cvtsd2ss xmm0,[ebx+0xb0]' then 'mulss [eax+0xc]' and 'mulss [pi/180]' (f2 0f 5a 83 b0 00 00 00 f3 0f 59 40 0c f3 0f 59 05) "
			"but found: %s. vphysics was probably updated; re-derive LIN_PerfSettings_Offset in sigs.h.",
			funcOffset, pFunction, pTarget, found);
	}

	g_LimitStep = (double)solverStep;

	BYTE instr_buf[sizeof(kOriginal)];
	memcpy(instr_buf, kReplacementHead, sizeof(kReplacementHead));
	const double * pNewConst = &g_LimitStep;
	memcpy(instr_buf + sizeof(kReplacementHead), &pNewConst, sizeof(pNewConst));
	m_pPatch = new BasicStaticBinPatch<8>(pTarget, instr_buf);

	Msg("Tickrate_Enabler: physics angular speed cap now computed with a %.4fs step (takes effect when a level loads)\n", solverStep);
}

AngularLimitPatch::~AngularLimitPatch()
{
	delete m_pPatch;
}

void AngularLimitPatch::Patch()
{
	if(m_pPatch) m_pPatch->Patch();
}

void AngularLimitPatch::Unpatch()
{
	if(m_pPatch) m_pPatch->Unpatch();
}
