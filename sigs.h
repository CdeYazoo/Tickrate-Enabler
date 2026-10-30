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
#ifndef _SIGS_H_
#define _SIGS_H_

/* Platform specific offset+sig data */

/* CTFWeaponBaseGrenadeProj::VPhysicsUpdate() function finding */
#define LIN_VPhysicsUpdate_SYMBOL "_ZN24CTFWeaponBaseGrenadeProj14VPhysicsUpdateEP14IPhysicsObject"
/* CTFWeaponBaseGrenadeProj::ResolveFlyCollisionCustom() function finding */
#define LIN_ResolveFlyCollisionCustom_SYMBOL "_ZN24CTFWeaponBaseGrenadeProj25ResolveFlyCollisionCustomER10CGameTraceR6Vector"

/* TODO: Find sigs for Windows.
 * The two signatures below are currently byte-identical placeholders, so they resolve to the
 * same function (FindPattern returns the first match). Each needs its own real signature. */
#define WIN_VPhysicsUpdate_SIG "\x55\x8B\xEC\x81\xEC\x2A\x2A\x2A\x2A\xA1\x2A\x2A\x2A\x2A\x33\xC5\x89\x45\xFC\x53\x56\x57\x8B\xF9\x8B\x87\x2A\x2A\x2A\x2A\x83"
#define WIN_VPhysicsUpdate_SIGLEN 31

#define WIN_ResolveFlyCollisionCustom_SIG "\x55\x8B\xEC\x81\xEC\x2A\x2A\x2A\x2A\xA1\x2A\x2A\x2A\x2A\x33\xC5\x89\x45\xFC\x53\x56\x57\x8B\xF9\x8B\x87\x2A\x2A\x2A\x2A\x83"
#define WIN_ResolveFlyCollisionCustom_SIGLEN 31

/* gpGlobals read offsets into CTFWeaponBaseGrenadeProj::VPhysicsUpdate() and CTFWeaponBaseGrenadeProj::ResolveFlyCollisionCustom()*/
const int g_FrameTimeReadOffsets[] =
#if defined (_LINUX)
	/* Verified by hand against a production server_srv.so, TF2 PatchVersion=11138802:
	 *   VPhysicsUpdate            entry 00e13c20, mov eax,[gpGlobals] at 00e13d29 -> +0x109
	 *   ResolveFlyCollisionCustom entry 00e10b00, loads at 00e10d87 -> +0x287 and 00e10e20 -> +0x320
	 * All three are 'a1 64 a2 80 01' (mov eax,[0x0180a264]) followed by a read of gpGlobals+0x10 (frametime).
	 * The previous values {0x11E, 0x290, 0x339} no longer land on those instructions. */
	{0x109, 0x287, 0x320};
#elif defined (_WIN32)
	{0x11E, 0x290, 0x339}; // TODO: Change offset (copied from Linux; Windows codegen differs)
#endif

/* Order: [0] VPhysicsUpdate, [1] and [2] ResolveFlyCollisionCustom */
const int g_FrameTimeReadCount = sizeof(g_FrameTimeReadOffsets) / sizeof(g_FrameTimeReadOffsets[0]);
/* CObjectSentrygun::SentryThink() re-arms itself with ThinkSet(curtime + 0.05, ...).
 * Source rounds that to whole ticks, so the real think period (and with it the sentry fire rate) changes with tickrate.
 * The delay is added by one 'addsd xmm0, [abs32]' (f2 0f 58 05 <abs32>) whose operand points at the double 0.05.
 * Verified by hand against TF2 PatchVersion=11138802: SentryThink entry 00cbd5b0, addsd at 00cbd642 -> +0x92. */
#define LIN_SentryThink_SYMBOL "_ZN16CObjectSentrygun11SentryThinkEv"
#define LIN_SentryThink_DelayOffset 0x92
/* TODO: Windows signature and offset for SentryThink */

/* CPhysicsHook::LevelInitPreEntity() creates the vphysics environment and calls
 *   physenv->SetSimulationTimestep( gpGlobals->interval_per_tick )
 * so the physics solver steps in tick-sized increments (5ms at 200 tick, 15ms in vanilla).
 * Verified by hand against TF2 PatchVersion=11138802: entry 0077b010, 'mov ecx,[gpGlobals]' (8b 0d <abs32>) at 0077b111 -> +0x101,
 * followed by: 8b 10 / ff 71 1c / 50 / ff 92 98 00 00 00  (mov edx,[eax]; push [ecx+0x1c]; push eax; call [edx+0x98]) */
#define LIN_PhysicsHook_LevelInitPreEntity_SYMBOL "_ZN12CPhysicsHook18LevelInitPreEntityEv"
#define LIN_PhysStep_Offset 0x101
/* TODO: Windows signature and offset for CPhysicsHook::LevelInitPreEntity */

#endif // _SIGS_H_