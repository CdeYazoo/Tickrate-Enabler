## Tickrate Enabler for TF2

### Description

Enables the "-tickrate" switch to be used from the command line to set the game's tickrate.

Status (Linux only; checked against TF2 build 11138802):

- **Use `-tickrate N`**, ideally a multiple of 200/3 (for example 200). Two fixes are on by default.
- **Sentry fire rate.** `SentryThink` re-arms itself every 0.05s, which Source rounds to a number of ticks that changes with tickrate (and wrangled sentries sit right on a rounding edge). The patch makes the delay a whole number of ticks equal to vanilla's 45ms, so sentries and the Wrangler fire at the vanilla rate at any multiple of 200/3.
- **Physics fix.** vphysics divides by its step in a few places that were tuned for vanilla's 15ms step, which makes them too strong or too loose at 5ms: the penetration and rotation "rescue speed" (three reads) and the angular speed cap. The fix patches `vphysics_srv.so` so they behave as at 15ms while the solver keeps stepping at the tick interval. Grenade bounces were measured with 100 standardized throws per setting (one tester, one rig): vanilla 66.67 tick gave 9 defective grenades and 0 superelastic bounces; 200 tick with only the rescue-speed part gave 16 and 2; 200 tick with both parts gave 7 and 0. That last result is indistinguishable from vanilla at this sample size. `-te_nophysfix` turns the physics fix off.
- **The physics fix is best effort.** Both parts are applied together or not at all. If vphysics does not match the expected bytes (for example after a game update), the plugin logs a warning and the server runs with unmodified vphysics. The sentry patch is not best effort: a mismatch there stops the server with a `Patch Failure` message naming the instruction.
- **Demoman charge turning is not part of this plugin.** The turn limit is applied per command or frame, so it loosens with tickrate, and the cap also depends on `frametime / tick interval`, so no single multiplier is exact at every frame rate. The cap scales with the `charge_turn_control` attribute (item attribute 246, a percentage multiplier), so scaling it down on the three shields (Chargin' Targe 131, Splendid Screen 406, Tide Turner 1099) through a TF2Items config or a SourceMod plugin is the way to tune it. Tune by testing.
- Not done: Windows (no signatures yet).
- Removed along the way: a patch that pinned `gpGlobals->frametime` in the grenade functions (by the original source those functions already scale correctly, and pinning made bounces worse), and a `-te_physstep` workaround that pinned the physics step (it slowed doors).

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
