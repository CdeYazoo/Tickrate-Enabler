## Tickrate Enabler for TF2

### Description

Enables the "-tickrate" switch to be used from the command line to set the game's tickrate.

Status / TODO:

- Game DLL interface: the plugin accepts ServerGameDLL012, 011 and 010 (newest first) and logs the one it uses. `GetTickInterval` is vtable slot 10 in all three. A newer version needs its layout checked before it is added to `Load()`.
- Testing switches: start the server with `-te_nosweep` and/or `-te_nopush` to turn off parts of the grenade patch (the frametime read in `VPhysicsUpdate`, and the two reads in `ResolveFlyCollisionCustom`). With both, the grenade patch is off entirely. The console prints which parts are on.
- Grenades/Stickybombs: projectile bounce used to scale inversely with tickrate. This is patched on Linux (`pipebombpatch.cpp` pins the frametime read in `CTFWeaponBaseGrenadeProj::VPhysicsUpdate()` and `ResolveFlyCollisionCustom()` to 1/66s). **Windows is not done yet**: `sigs.h` still has placeholder signatures and copied-from-Linux offsets, so the plugin will refuse to load there.
- Sentries/Wrangler: sentry fire rate depended on how the 0.05s `SentryThink` delay rounds to ticks. On Linux the delay is now a whole number of ticks that matches vanilla (200/3 tick) timing. Written from decompilation, not yet verified in game; Windows needs a signature.

### Building (Linux, GitHub Actions)

The workflow in `.github/workflows/build.yml` builds the plugin with the project's own AMBuild scripts. Push to GitHub, open the repository's **Actions** tab, pick the latest "Build Linux plugin (TF2)" run, and download the `tickrate_enabler-tf2-linux` artifact. It contains the `addons` folder, laid out for the server. A second artifact holds an unstripped copy for crash analysis. You can also start a build by hand with **Run workflow**.

Releases are created automatically. Pushing to the default branch refreshes a rolling prerelease called **latest** under the repository's **Releases** page. Pushing a tag that starts with `v` (for example `git tag v1.0 && git push origin v1.0`) publishes a versioned release. Each release has a zip of the `addons` folder plus a SHA-256 checksum. Pull requests only build, they never release.

The build needs old dependencies (a 2019 HL2SDK snapshot, SourceMod and Metamod:Source 1.11-dev), so the workflow pins exact commits. It is built on Ubuntu 22.04 (glibc 2.35) so it loads on servers with glibc 2.36 or older, and the build fails if that stops being true. It adds `-Wno-error` through small compiler wrappers because modern gcc warns about code in that old SDK. Linux only; Windows needs signatures first (see `sigs.h`).

### Instructions

1. Place tickrate_enabler.dll (Windows) or tickrate_enabler.so (Linux) in your server's addons folder.
2. Place tickrate_enabler.vdf in your server's addons folder.
3. Add "-tickrate <desired_tickrate>" to your server's launch parameters. e.g. -tickrate 100
4. Make sure the following convar settings are properly set in server.cfg or otherwise:

    sv_maxupdaterate 100  
    sv_maxcmdrate 100

5. PROFIT!!!

### Credits

Parts of this project contain GPLv3 code adapted from SourceMod (Allied Modders L.L.C.), Left4Downtown2 (Igor Smirnov et. al.) and Tickrate Enabler for L4D/L4D2 (Michael "ProdigySim" Busby).
