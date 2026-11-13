# Eldoria: guide for AI agents

Eldoria is a third-person open-world action game in **Unreal Engine 5.8** (C++), built entirely by an AI
coding agent (Claude Code) from the owner's feedback. Read this before changing anything; most of it was
learned the hard way.

## The game

The kingdom of Eldoria is held by **Mordrak, the Bone King**: once the king's sorcerer, he stole the Royal
Key and bound his soul to bone in the Sky Shrine. The hero, **Sir Aldric**, last knight of the Silver Oath,
comes home to end his reign. A narrated story plays before a new game.

Quest (4 stages, `AEldoriaGameMode` constructor/`BeginPlay`):
1. **The Village Fields**: destroy the warband camped outside the village (group `WestCamp`).
2. **The Old Ruins**: kill the champions at the eastern ruins (group `Ruins`).
3. **The Sky Shrine**: climb the floating stones and open the chest holding the Royal Key.
4. **The Bone King**: Mordrak (Sevarog mesh) in the village square; warded until the hero has the key.

Characters come from free Fab packs: hero = Paragon **Greystone**, soldiers = Paragon **Minions** (Dusk
melee "Grunt", Buff_Red "Brute"), boss = Paragon **Sevarog**. The world is Epic's **Medieval Village
Megascans Sample**.

## Controls

Move WASD/arrows · hold Shift (either) to run · Light attack J/Z/LMB · Heavy K/X/RMB-trigger · Block/parry
F/V/RMB (tap just before a hit to parry) · Dodge C/L/Q/Alt (+direction) · Interact/execute E · Jump Space
(double) · Help H · Pause Esc/P. Gamepad mapped too. All input is built in code (`AEldoriaCharacter::BuildInput`).

## Layout

| Path | What |
|---|---|
| `Source/Eldoria/EldoriaCharacter.*` | Hero: input, combo/heavy/finisher/air slam (`FHeroMove` from config), block & parry, directional dodge with i-frames and perfect-dodge slow-mo, executions, hit-stop & camera feedback, ragdoll knockdown + get-up, death, over-the-shoulder combat camera, footsteps, automated test modes |
| `Source/Eldoria/EnemyCharacter.*` | Enemy AI state machine (Idle/Chase/Attack/Stagger/Return/Down/GettingUp/Stunned/Dead), stance meter, telegraphs (blue = parryable, red = unblockable), knockdown only on "big hits", ragdoll knockdowns/deaths, get-up from ragdoll, melt-away after death, boss summons, attack tokens |
| `Source/Eldoria/EldoriaGameMode.*` | Quest, saves (USaveGame slots Autosave/Slot1-3), menus, difficulty (`FDifficultyTuning`), graphics quality, music, the new-game story cinematic, runtime foliage thinning (`ThinFoliage`), tree-trunk colliders |
| `Source/Eldoria/EldoriaAnimInstance.*` | Native anim instance used by the hero and the minions: idle/walk/run blend, falling loop, one-shot actions (full-body, upper-body only, or additive), blend out of a ragdoll pose |
| `Source/Eldoria/EldoriaSettings.*` | `UEldoriaSettings` (Config=Game): **every asset path** (meshes, anims, sounds, FX, story shots) lives in `Config/DefaultGame.ini`. Also shared helpers: `PlayAnim`, `PlaySound*`, `PlayImpact`, `SpillBlood`, `StartRagdoll`, `ClipBoneLocation` |
| `Source/Eldoria/EldoriaHUD.*` | Canvas HUD (Roboto UFont): hero panel, quest card, objective marker, enemy health/stance bars, glints, boss bar, damage numbers, blood screen overlay, controls card |
| `Source/Eldoria/EldoriaMenu.*`, `EldoriaIntro.*` | Slate menus (Main/Pause/Save/Load/Difficulty, Graphics toggle) and the story overlay (letterbox, subtitles, skip) |
| `Source/Eldoria/Pickups.*`, `Chest.*`, `HealthComponent.*` | Coins (drop from enemies only, lie flat), potions, chests, the castle gate |
| `Source/EldoriaLoadingScreen/` | Module loaded at PreLoadingScreen: studio splash ("Chandrabindu Studios presents", "in association with Claude Code"), title over a scene, loading screen with lore/tips. Reads raw files from `Content/Splash` (no UObjects that early) |
| `Config/DefaultGame.ini` | Asset paths, hero moves, enemy archetypes, story shots, packaging (DirectoriesToAlwaysCook) |
| `Config/DefaultEngine.ini` | Rendering (Lumen, Nanite, VSM, SM6 Metal), `r.AllowOcclusionQueries=0`, navigation tuning |
| `Scripts/` | Unreal Python + shell tools (see below) |
| `Story/` | Narration script and recording guide; `MyRecordings/` = the owner's ElevenLabs takes |
| `Art/` | Logo and app-icon sources (`make_logo.swift`), Cinzel fonts, loading-screen scenes |
| `archive/` | Earlier Three.js browser prototypes (not part of the game) |

## Commands

Unreal is installed at `/Users/joy/Games/UE_5.8` on the owner's Mac (`UE` below).

```bash
# compile the editor target (needed after any C++ change)
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" EldoriaEditor Mac Development -Project="$PWD/Eldoria.uproject" -WaitMutex
# run the game (add "/Game/Maps/Eldoria?newgame" to skip the menu)
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/Eldoria.uproject" -game -windowed -ResX=1600 -ResY=900 -log
# run an editor Python script headless (log with unreal.log("ELDORIA_INFO ..."); print() is not captured)
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Eldoria.uproject" -run=pythonscript -script="$PWD/Scripts/x.py" -unattended -nosplash -nopause -stdout -FullStdOutLogOutput
# package the Mac app into Build/
"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PWD/Eldoria.uproject" -platform=Mac -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -archive -archivedirectory="$PWD/Packaged" -noP4 -unattended -nodebuginfo -IgnoreCookErrors
```

Scripts that (re)generate content: `build_village_level.py` (the map; delete `Content/Maps/Eldoria.umap`
first), `make_ragdolls.py`, `retarget_hero.py`, `make_materials.py [--blood-only]`, `make_sounds.py` +
`import_sounds.py`, `export_footsteps.py`, `make_narration.sh` / `use_my_narration.sh` + `import_narration.py`,
`make_logo.swift`, `level_audio.py`. `inspect_*.py` are read-only investigations.

## Testing (development builds only)

Launch flags, all compiled out of Shipping:
- `-eldoriacombattest`: scripted 20 s fight. Logs `COMBATTEST` (state, distance, `VANISHED` if on screen but not
  drawn, pelvis height), `HEROTEST` (hero knockdown), `CAMINSIDE`/`CAMBLOCK` (camera inside/blocked by an enemy),
  `COLLAPSED` (body crushed to a point), `PERF` (frame/game/render/GPU ms), and a `stat dumpframe` at 5 s.
- `-eldoriasidecam` films that fight side-on · `-eldoriawalktest` walks, runs, then walks with the guard up
- `-eldoriascenery` plays the story shots bare (for loading-screen art) · `-eldorianointro` skips the story
- `-eldoriacontinue` presses Continue on the title screen · `-eldoriaquality=0|1|2` forces Low/Medium/High

Measure from the log; for visuals capture **only the game window** (`screencapture -l<windowID>`, find the id
with CoreGraphics by the process id). Never capture the full screen.

## Hard-won rules

- **One Unreal process at a time.** A `-game` run through the editor binary takes ~19 GB; two at once hung the
  owner's 24 GB Mac. Check `pgrep -f UnrealEditor` and ask the owner to close their copy before testing.
- **Paragon hit-reaction clips are additive.** Playing them as full poses crushes every bone to a point and the
  character vanishes (health bar still shows). `UEldoriaAnimInstance` layers additive clips with
  `AccumulateAdditivePose`; keep that.
- **Never drive Paragon materials' `HitFlashBrightness`**: in their material it also drives opacity.
- **After a ragdoll, `SetCollisionProfileName("CharacterMesh")` resets the camera response**: re-apply
  `ECC_Camera` ignore or the spring arm collapses into the body.
- Hardware occlusion queries are off (`r.AllowOcclusionQueries=0`): they hid fast-moving fighters for a frame.
- Navigation: `DataGatheringMode=Lazy` and `bUpdateNavOctreeOnComponentChange=False`. Without them the nav
  octree ate ~30 ms per frame on the village's 1.2 M foliage instances. Pickups/trunks/foliage don't affect nav.
- Death and get-up: Paragon death clips only *start* a fall; the ragdoll (`StartRagdoll`) finishes it.
  Knockdowns keep the weapon in hand (weapon bodies removed); deaths let go.
- The hero is animated by `UEldoriaAnimInstance` (not Greystone's AnimBP, which only has an `UpperBody` slot).
  Walk/run come from the Game Animation Sample, retargeted by `retarget_hero.py`; that script disables the
  copied retargeter's "Blend to Source" and "Speed Plant" ops, otherwise the feet trail behind the body.
- Foliage thinning and fight-site clearings happen **at load** (`ThinFoliage`), not in the map, because
  `build_village_level.py` regenerates the map from the village. Enemies stand in open ground, never forest.
- The title-screen menu pauses the world: use real-time tickers (`FTSTicker`), not world timers, for anything
  that must run there. Continue from the title applies the save in place (instant); mid-game loads reload the map.
- Packaging: assets referenced only by name in config must be listed in `DirectoriesToAlwaysCook`; the
  `AudioModulation` plugin must stay enabled (village MetaSounds need it or the cook asserts). Package with
  `-IgnoreCookErrors`: a few pack blueprints (Greystone/Sevarog player characters, the village soundscape trigger
  and `BP_TimeOfDay`) were made for older engines and fail to compile; the game does not depend on any of them.
- The archive step copies the app *without* its data: take `Saved/StagedBuilds/Mac/Eldoria-Mac-Shipping.app` and
  copy it to `Packaged/Eldoria.app`. The packaged game goes to `Packaged/`, never `Build/`: `Build/Mac/Resources`
  holds the app icon (`Assets.xcassets` + `AppIcon.icns`, from `Art/Logo/T_AppIcon.png`, made by `make_logo.swift`).
- Repository: third-party packs are git-ignored (licences forbid redistributing raw assets). Only Eldoria's own
  content is committed; see `SETUP.md` for how others add the packs.

## Working with the owner

The owner plays the game and gives short, direct feedback; they want God of War / Ghost of Tsushima-level
realism, not stylised or "demo" quality. Explain what you're doing in plain words, keep test launches few
and say how many you need, and verify changes in the running game before calling them done.
