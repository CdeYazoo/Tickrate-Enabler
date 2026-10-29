/**
 * vim: set ts=4 :
 * =============================================================================
 * BoomerVomitPatch
 * Copyright (C) 2012 Michael "ProdigySim" Busby
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the author(s) give you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, the author(s) grant
 * this exception to all derivative works.  The author(s) define further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */
#include <cstdlib>
#include "memutils.h"
#include "pipebombpatch.h"
#include "patchexceptions.h"
#include <cstdio>
#include <cstring>

// FrameTimeOverride writes a raw pointer into a 6 byte instruction buffer; that is only valid on 32-bit x86.
static_assert(sizeof(void *) == 4, "pipebombpatch only supports 32-bit x86 (absolute-address mov patching)");

struct fakeGlobals {
	float padding[4];
	float frametime;
};

fakeGlobals g_FakeGlobals = { {0.0, 0.0, 0.0, 0.0}, 0.015151515 };
fakeGlobals *gp_FakeGlobals = &g_FakeGlobals;

PipeBombFrameTimePatch::PipeBombFrameTimePatch(IServerGameDLL * gamedll, bool patchSweep, bool patchPush)
	: m_pGlobalsVarAddr(NULL)
{
	InitializeBinPatches(gamedll, patchSweep, patchPush);
}

PipeBombFrameTimePatch::~PipeBombFrameTimePatch()
{
	m_patches.UnregisterAll();
}

void PipeBombFrameTimePatch::Patch()
{
	m_patches.PatchAll();
}

void PipeBombFrameTimePatch::Unpatch() 
{
	m_patches.UnpatchAll();
}

void PipeBombFrameTimePatch::InitializeBinPatches(IServerGameDLL * gamedll, bool patchSweep, bool patchPush)
{
#if defined (_LINUX)
	BYTE * pVPhysicsUpdate = FindSignature(static_cast<void *>(gamedll), LIN_VPhysicsUpdate_SYMBOL);
	DevMsg("VPhysicsUpdate at %p\n", pVPhysicsUpdate);
	BYTE *pResolveFlyCollisionCustom = FindSignature(static_cast<void *>(gamedll), LIN_ResolveFlyCollisionCustom_SYMBOL);
	DevMsg("ResolveFlyCollisionCustom at %p\n", pResolveFlyCollisionCustom);
#elif defined (_WIN32)
	BYTE * pVPhysicsUpdate = FindSignature(static_cast<void *>(gamedll), WIN_VPhysicsUpdate_SIG, WIN_VPhysicsUpdate_SIGLEN);
	DevMsg("VPhysicsUpdate at %p\n", pVPhysicsUpdate);
	BYTE *pResolveFlyCollisionCustom = FindSignature(static_cast<void *>(gamedll), WIN_ResolveFlyCollisionCustom_SIG, WIN_ResolveFlyCollisionCustom_SIGLEN);
	DevMsg("ResolveFlyCollisionCustom at %p\n", pResolveFlyCollisionCustom);
#endif
	if(!pVPhysicsUpdate)
	{
		throw PatchException("Couldn't find CTFWeaponBaseGrenadeProj::VPhysicsUpdate() in server memory.");
	}
	else if(!pResolveFlyCollisionCustom)
	{
		throw PatchException("Couldn't find CTFWeaponBaseGrenadeProj::ResolveFlyCollisionCustom() in server memory.");
	}
	if(pVPhysicsUpdate == pResolveFlyCollisionCustom)
	{
		throw PatchException::Format("VPhysicsUpdate and ResolveFlyCollisionCustom resolved to the same address (%p). "
			"Two different functions cannot share one address: the signatures in sigs.h are placeholders or too generic.",
			pVPhysicsUpdate);
	}
	// Read order must match g_FrameTimeReadOffsets in sigs.h
	if(patchSweep)
	{
		FrameTimeOverride(pVPhysicsUpdate, "VPhysicsUpdate", 0);
	}
	if(patchPush)
	{
		FrameTimeOverride(pResolveFlyCollisionCustom, "ResolveFlyCollisionCustom", 1);
		FrameTimeOverride(pResolveFlyCollisionCustom, "ResolveFlyCollisionCustom", 2);
	}
	Msg("Tickrate_Enabler: grenade frametime patch: sweep (VPhysicsUpdate) %s, push (ResolveFlyCollisionCustom) %s\n",
		patchSweep ? "ON" : "off", patchPush ? "ON" : "off");
}

static void FormatBytes(char * out, size_t outLen, const BYTE * p, int n)
{
	out[0] = '\0';
	size_t used = 0;
	for(int i = 0; i < n && used + 4 <= outLen; i++)
	{
		used += snprintf(out + used, outLen - used, "%s%02x", i ? " " : "", p[i]);
	}
}

void PipeBombFrameTimePatch::FrameTimeOverride(BYTE* pFunction, const char * funcName, int readIndex)
{
	if(readIndex < 0 || readIndex >= g_FrameTimeReadCount)
	{
		throw PatchException::Format("Internal error: frametime read #%d for %s is out of range (have %d offsets in sigs.h).",
			readIndex, funcName, g_FrameTimeReadCount);
	}
	const int funcOffset = g_FrameTimeReadOffsets[readIndex];
	DevMsg("Setting up patch for frametime read in %s (offs:0x%x).\n", funcName, funcOffset);
	BYTE instr_buf[MAX_MOV_INSTR_LEN];
	// Calculate offset target
	BYTE * pTarget = pFunction + funcOffset;
	int offs = mov_src_operand_offset(pTarget); // Find offset of disp32 in this particular mov instruction
	if(offs == 0)
	{
		char found[MAX_MOV_INSTR_LEN * 3 + 1];
		FormatBytes(found, sizeof(found), pTarget, MAX_MOV_INSTR_LEN);
		throw PatchException::Format(
			"Frametime read #%d in %s: unexpected instruction at %s+0x%x (function at %p, target %p). "
			"Expected 'mov eax, [gpGlobals]' (opcode a1) but found bytes: %s. "
			"The game binary was probably updated; re-derive g_FrameTimeReadOffsets in sigs.h.",
			readIndex, funcName, funcName, funcOffset, pFunction, pTarget, found);
	}
	memcpy(instr_buf, pTarget, MAX_MOV_INSTR_LEN);

	// All three sites should load the same gpGlobals variable. If they disagree, an offset has
	// probably drifted onto some other absolute mov. Warn only, until verified against a real server.
	void * pVarAddr;
	memcpy(&pVarAddr, pTarget + offs, sizeof(pVarAddr));
	if(m_pGlobalsVarAddr == NULL)
	{
		m_pGlobalsVarAddr = pVarAddr;
	}
	else if(pVarAddr != m_pGlobalsVarAddr)
	{
		Warning("Tickrate_Enabler: frametime read #%d in %s loads from %p but earlier sites load from %p; "
			"offset in sigs.h may be wrong.\n", readIndex, funcName, pVarAddr, m_pGlobalsVarAddr);
	}

	// make this instruction read from an immediate address
	mov_to_disp32(instr_buf);

	// Plug in our super cool immediate address.
	*(fakeGlobals ***)(instr_buf + offs) = &gp_FakeGlobals;

	// Generate BasicBinPatch
	m_patches.Register(new BasicStaticBinPatch<MAX_MOV_INSTR_LEN>(pTarget, instr_buf));

	char before[MAX_MOV_INSTR_LEN * 3 + 1], after[MAX_MOV_INSTR_LEN * 3 + 1];
	FormatBytes(before, sizeof(before), pTarget, MAX_MOV_INSTR_LEN);
	FormatBytes(after, sizeof(after), instr_buf, MAX_MOV_INSTR_LEN);
	DevMsg("  original: %s\n  patched:  %s\n", before, after);
}

BYTE * PipeBombFrameTimePatch::FindSignature(void* gamedll, const char* sig, int len)
{
#if defined (_LINUX)
	return (BYTE *)g_MemUtils.SimpleResolve(gamedll, sig);
#elif defined (_WIN32)
	if(len > 0) return (BYTE*)g_MemUtils.FindLibPattern(gamedll, sig, len);
	else return nullptr;
#endif
}
