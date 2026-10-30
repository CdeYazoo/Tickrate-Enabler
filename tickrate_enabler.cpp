/**
 * vim: set ts=4 :
 * =============================================================================
 * Tickrate Enabler
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
#include "tier0/icommandline.h"

#include "sourcehook.h"
#include "sourcehook_impl.h"
#include "sourcehook_impl_chookidman.h"

#include "codepatch/patchmanager.h"

#include "tickrate_enabler.h"
#include "patchexceptions.h"

#include "pipebombpatch.h"
#include "sentrypatch.h"
#include "physstep.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VERSION "1.0"

TF2TickRate g_TF2TickRatePlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(TF2TickRate, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_TF2TickRatePlugin );

//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
TF2TickRate::TF2TickRate()
{
}

TF2TickRate::~TF2TickRate()
{
}

//---------------------------------------------------------------------------------
// Purpose: called once per server frame, do recurring work here (like checking for timeouts)
//---------------------------------------------------------------------------------
void TF2TickRate::GameFrame( bool simulating )
{
}

SourceHook::Impl::CSourceHookImpl g_SourceHook;
SourceHook::ISourceHook *g_SHPtr = &g_SourceHook;
int g_PLID = 0;

SH_DECL_HOOK0(IServerGameDLL, GetTickInterval, const, 0, float);

float GetConfiguredTickInterval(bool verbose)
{
	float tickinterval = (1.0f / 66.0f);

	if ( CommandLine()->CheckParm( "-tickrate" ) )
	{
		float tickrate = CommandLine()->ParmValue( "-tickrate", 0 );
		if ( verbose )
			Msg("Tickrate_Enabler: Read TickRate %f\n", tickrate);
		if ( tickrate > 10 )
			tickinterval = 1.0f / tickrate;
	}

	return tickinterval;
}

float GetTickInterval()
{
	float tickinterval = GetConfiguredTickInterval(true);

	RETURN_META_VALUE(MRES_SUPERCEDE, tickinterval );
}

IServerGameDLL *gamedll = NULL;
IVEngineServer *engine = NULL;

//ICvar * g_pCvar = NULL;

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
bool TF2TickRate::Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	// Valve bumped the game DLL interface from 010 to 011 and 012 in 2022, so a current server no longer offers 010.
	// Between these versions IServerGameDLL only gained functions at the END of its vtable (GetTickInterval is slot 10
	// in all of them), so the GetTickInterval hook below is valid for each. Newest first. Do not add a newer version
	// here without checking that its layout still has GetTickInterval at slot 10.
	static const char * const kGameDllVersions[] = { "ServerGameDLL012", "ServerGameDLL011", "ServerGameDLL010" };
	gamedll = NULL;
	for (size_t i = 0; i < sizeof(kGameDllVersions) / sizeof(kGameDllVersions[0]); i++)
	{
		gamedll = (IServerGameDLL*)gameServerFactory(kGameDllVersions[i], NULL);
		if (gamedll)
		{
			Msg("Tickrate_Enabler: Using %s\n", kGameDllVersions[i]);
			break;
		}
	}
	if (!gamedll)
	{
		Warning("Failed to get a pointer on ServerGameDLL (tried ServerGameDLL012, 011 and 010).\n");
		return false;
	}
	engine = (IVEngineServer *)interfaceFactory("VEngineServer023", NULL);
	if (!engine)
	{
		Warning("Failed to get a pointer on VEngineServer.\n");
		return false;
	}


	Msg("Tickrate_Enabler: Found ServerGameDLL at %p\n", gamedll);
	Msg("Tickrate_Enabler: Found VEngineServer at %p\n", engine);

	SH_ADD_HOOK(IServerGameDLL, GetTickInterval, gamedll, SH_STATIC(GetTickInterval), false);

	try
	{
		// The grenade frametime patch is OFF by default. A/B testing at 200 tick showed that pinning these reads made
		// bounces worse: the two reads in ResolveFlyCollisionCustom ("push") inflated post-bounce hops about 3x, and the
		// read in VPhysicsUpdate ("sweep") gave occasional huge wall bounces. -te_push and -te_sweep turn the parts
		// back on for experiments.
		const bool patchSweep = CommandLine()->CheckParm("-te_sweep") != NULL;
		const bool patchPush = CommandLine()->CheckParm("-te_push") != NULL;
		if(patchSweep || patchPush)
		{
			m_patchManager.Register(new PipeBombFrameTimePatch(gamedll, patchSweep, patchPush));
		}
		else
		{
			Msg("Tickrate_Enabler: grenade frametime patch is off (-te_sweep and/or -te_push enable it)\n");
		}
		m_patchManager.Register(new SentryThinkDelayPatch(gamedll, GetConfiguredTickInterval(false)));
		// -te_physstep: give vphysics its vanilla 15ms step instead of the tick interval (experimental, opt-in).
		if(CommandLine()->CheckParm("-te_physstep"))
		{
			m_patchManager.Register(new PhysicsStepPatch(gamedll));
		}
		m_patchManager.PatchAll();
	}
	catch( PatchException & e)
	{
		// Load() failing means Unload() is never called, so drop the hook here or it dangles into a freed DLL.
		SH_REMOVE_HOOK(IServerGameDLL, GetTickInterval, gamedll, SH_STATIC(GetTickInterval), false);
		Error("!!!!!\nPatch Failure: %s\n!!!!!\n", e.GetDescription());
		Error("Failed to process all tickrate_enabler patches, bailing out.\n");
		return false;
	}

	//g_pCvar = reinterpret_cast<ICvar *>(interfaceFactory(CVAR_INTERFACE_VERSION,NULL));
	//if(g_pCvar == NULL)
	//{
	//	Error("RecordingHelpers: Failed to get Cvar interface.\n");
	//	return false;
	//}
	
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void TF2TickRate::Unload( void )
{
	m_patchManager.UnpatchAll();
	m_patchManager.UnregisterAll();
	SH_REMOVE_HOOK(IServerGameDLL, GetTickInterval, gamedll, SH_STATIC(GetTickInterval), false);
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is paused (i.e should stop running but isn't unloaded)
//---------------------------------------------------------------------------------
void TF2TickRate::Pause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unpaused (i.e should start executing again)
//---------------------------------------------------------------------------------
void TF2TickRate::UnPause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: the name of this plugin, returned in "plugin_print" command
//---------------------------------------------------------------------------------
const char *TF2TickRate::GetPluginDescription( void )
{
	return "Tickrate_Enabler " VERSION ", ProdigySim, rm2k";
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void TF2TickRate::LevelInit( char const *pMapName )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start, when the server is ready to accept client connections
//		edictCount is the number of entities in the level, clientMax is the max client count
//---------------------------------------------------------------------------------
void TF2TickRate::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level end (as the server is shutting down or going to a new map)
//---------------------------------------------------------------------------------
void TF2TickRate::LevelShutdown( void ) // !!!!this can get called multiple times per map change
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void TF2TickRate::ClientActive( edict_t *pEntity )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client leaves a server (or is timed out)
//---------------------------------------------------------------------------------
void TF2TickRate::ClientDisconnect( edict_t *pEntity )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on 
//---------------------------------------------------------------------------------
void TF2TickRate::ClientPutInServer( edict_t *pEntity, char const *playername )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void TF2TickRate::SetCommandClient( int index )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void TF2TickRate::ClientSettingsChanged( edict_t *pEdict )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client joins a server
//---------------------------------------------------------------------------------
PLUGIN_RESULT TF2TickRate::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client types in a command (only a subset of commands however, not CON_COMMAND's)
//---------------------------------------------------------------------------------
PLUGIN_RESULT TF2TickRate::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client is authenticated
//---------------------------------------------------------------------------------
PLUGIN_RESULT TF2TickRate::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a cvar value query is finished
//---------------------------------------------------------------------------------
void TF2TickRate::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
}
void TF2TickRate::OnEdictAllocated( edict_t *edict )
{
}
void TF2TickRate::OnEdictFreed( const edict_t *edict  )
{
}
