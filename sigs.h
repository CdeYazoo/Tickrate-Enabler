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

/* CObjectSentrygun::SentryThink() re-arms itself with ThinkSet(curtime + 0.05, ...).
 * Source rounds that to whole ticks, so the real think period (and with it the sentry fire rate) changes with tickrate.
 * The delay is added by one 'addsd xmm0, [abs32]' (f2 0f 58 05 <abs32>) whose operand points at the double 0.05.
 * Verified by hand against TF2 PatchVersion=11138802: SentryThink entry 00cbd5b0, addsd at 00cbd642 -> +0x92. */
#define LIN_SentryThink_SYMBOL "_ZN16CObjectSentrygun11SentryThinkEv"
#define LIN_SentryThink_DelayOffset 0x92
/* TODO: Windows signature and offset for SentryThink */

/* IVP_Contact_Point::get_rescue_speed_impact() (in vphysics_srv.so, not the game DLL) turns a penetration depth into a
 * separation speed: 2 * (rot_speed_uncertainty + (min_dist - dist) / PSI). The 1/PSI is read by one instruction,
 *   cvtsd2ss xmm2, qword ptr [edx+0xb8]   (f2 0f 5a 92 b8 00 00 00)
 * so at a 5ms step the same overlap is pushed apart 3x as hard as at vanilla's 15ms.
 * Verified by hand against TF2 PatchVersion=11138802: entry 001082c0, instruction at 001082ee -> +0x2e. */
#define LIN_RescueSpeed_SYMBOL "_ZN17IVP_Contact_Point23get_rescue_speed_impactEP15IVP_Environment"
#define LIN_RescueSpeed_Offset 0x2e
/* TODO: Windows signature and offset for IVP_Contact_Point::get_rescue_speed_impact */

/* IVP_Contact_Point::get_rot_speed_uncertainty() is added twice into the rescue speed above. For each core it computes
 *   (1/PSI) * radius * (1 - cos(|w| * 0.005))
 * with the same 1/PSI read: cvtsd2ss xmm0, qword ptr [edx+0xb8]  (f2 0f 5a 82 b8 00 00 00).
 * Verified by hand against TF2 PatchVersion=11138802: entry 001081c0, instruction at 00108239 -> +0x79. */
#define LIN_RotSpeedUncertainty_SYMBOL "_ZN17IVP_Contact_Point25get_rot_speed_uncertaintyEv"
#define LIN_RotSpeedUncertainty_Offset 0x79

/* IVP_Impact_Solver_Long_Term::do_impact_of_two_objects() is the two-object collision entry point. It contains an inlined copy
 * of the rescue speed formula above (overlap * 1/PSI), read with: cvtsd2ss xmm2, qword ptr [eax+0xb8]  (f2 0f 5a 90 b8 00 00 00).
 * Verified by hand against TF2 PatchVersion=11138802: entry 00108ca0, instruction at 00108d8c -> +0xEC.
 * (There is also a .cold clone of this function; symbol lookup is an exact match, so it is not confused with it.) */
#define LIN_DoImpactTwoObjects_SYMBOL "_ZN27IVP_Impact_Solver_Long_Term24do_impact_of_two_objectsEP11IVP_MindistP15IVP_Real_ObjectS3_"
#define LIN_DoImpactTwoObjects_Offset 0xEC

/* CPhysicsEnvironment::SetPerformanceSettings() (vphysics_srv.so) stores the angular speed cap as an angle per step:
 *   limits[+0x10] = PSI * maxAngularVelocity * (pi/180)
 * reading the step with  cvtsd2ss xmm0, qword ptr [ebx+0xb0]  (f2 0f 5a 83 b0 00 00 00). Everything that uses the cap multiplies it by
 * 1/PSI again, but CPhysicsHook::LevelInitPreEntity calls this BEFORE SetSimulationTimestep, so the PSI stored here is IVP's
 * default and not the step that is really used. At a 5ms step that makes the speed cap about 3x too loose.
 * Verified by hand against TF2 PatchVersion=11138802: entry 0004e1d0, instruction at 0004e220 -> +0x50, followed by
 *   f3 0f 59 40 0c   mulss xmm0,[eax+0xc]       maxAngularVelocity
 *   f3 0f 59 05 ..   mulss xmm0,[pi/180]        (3C8EFA35h) */
#define LIN_PerfSettings_SYMBOL "_ZN19CPhysicsEnvironment22SetPerformanceSettingsEPK27physics_performanceparams_t"
#define LIN_PerfSettings_Offset 0x50

#endif // _SIGS_H_