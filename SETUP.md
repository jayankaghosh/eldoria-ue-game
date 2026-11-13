# Eldoria: setting up the project

Eldoria is an open-world action game for **Unreal Engine 5.8** on Mac (Apple Silicon). This repository holds
the game's code, settings, scripts, story and its own assets: logos, loading screens, blood materials,
sound effects and the narration. The characters, the village and the other third-party art come from free
packs on Fab, which their licences don't allow us to redistribute, so each person adds them to their own copy.
The steps below take an hour or two, mostly downloading.

## 1. What you need

- A Mac with Apple Silicon (M1 or newer), about **70 GB** free disk space, and **Xcode** (from the App Store).
- **Git LFS** before cloning: `brew install git-lfs`, then `git lfs install`.
- The **Epic Games Launcher** (https://store.epicgames.com/download), signed in, with **Unreal Engine 5.8**
  installed from its Unreal Engine tab. Unreal is free; Epic takes 5% only of a game's revenue above $1 million.

## 2. Add the free packs

Sign in at https://www.fab.com with the same Epic account and click **Add to My Library** on each pack.
Then, in the Epic Games Launcher → Unreal Engine → **Fab Library**, add each one to the Eldoria project.

**Add to Project → Eldoria** (if Eldoria isn't listed, open `Eldoria.uproject` once first):

| Pack (search on Fab) | Used for |
|---|---|
| Paragon: Greystone | Sir Aldric, the hero |
| Paragon: Minions | the Bone King's soldiers and brutes |
| Paragon: Sevarog | Mordrak, the Bone King |
| Free Realistic Sword Sound Effects Pack | sword strikes |
| Whoosh Sound Effects Pack Lite Edition | sword swings |
| Health & Damage System Sound Effects Pack | heartbeat, healing |
| Interface & Item Sounds Pack | coins |
| Free Blood VFX (by Vefects) | blood splashes |

**Full projects:** these two are whole sample projects. Choose **Create Project**, then copy files from them
into Eldoria:

- **Medieval Village Megascans Sample**: copy everything in its `Content` folder into `Eldoria/Content`,
  merging folders and keeping any Eldoria files. This is the world: terrain, houses, forest, lighting and audio.
- **Game Animation Sample** (Epic): copy only these, keeping the same folder paths, from its `Content` into
  `Eldoria/Content`:
  - `Characters/UEFN_Mannequin/Meshes`, `Materials` and `Textures` (whole folders)
  - `Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin.uasset` and `PA_UEFN_Mannequin.uasset`
  - `Characters/Paragon/Heroes/TwinBlast/Rigs/IK_TwinBlast_Retarget.uasset` and `RTG_UEFN_to_TwinBlast.uasset`
  - `Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.uasset`
  - from `Characters/UEFN_Mannequin/Animations/Walk` and `.../Run`: `M_Neutral_Walk_Loop_*` and
    `M_Neutral_Run_Loop_*` for the directions F, B, FL, FR, BL, BR, LL, RR

## 3. Compile and build the world

Run these from the project folder in Terminal. Each `pythonscript` step runs Unreal without a window; let it
finish before starting the next.

```bash
UE=/Users/Shared/Epic\ Games/UE_5.8      # wherever Unreal 5.8 is installed
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" EldoriaEditor Mac Development -Project="$PWD/Eldoria.uproject" -WaitMutex
run() { "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/Eldoria.uproject" -run=pythonscript -script="$PWD/Scripts/$1" -unattended -nosplash -nopause; }
run make_ragdolls.py          # ragdoll physics for the brute and the Bone King
run retarget_hero.py          # walk and run animations for Sir Aldric
run export_footsteps.py       # the village's footstep recordings, as Eldoria's own sounds
run import_sounds.py
run build_village_level.py    # builds the Eldoria map from the village, with every camp, chest and enemy
```

Then open `Eldoria.uproject`, or start the game directly:

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/Eldoria.uproject" -game -windowed -ResX=1600 -ResY=900
```

## 4. Make the standalone app (optional)

```bash
"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PWD/Eldoria.uproject" -platform=Mac \
  -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -archive -archivedirectory="$PWD/Packaged" \
  -noP4 -unattended -nodebuginfo -IgnoreCookErrors
```

Expect an hour or more the first time. Then take the finished app from the staging folder (Unreal's archive
step copies a version without the game data) and give it a proper name:

```bash
rm -rf Packaged/Eldoria.app && ditto Saved/StagedBuilds/Mac/Eldoria-Mac-Shipping.app Packaged/Eldoria.app
```

The app icon comes from `Build/Mac/Resources` (made by `Scripts/make_logo.swift`).

## What's in the repository

- `Source/Eldoria`: the game in C++: combat (combos, parry, dodge, stance and executions, ragdoll knockdowns),
  enemy AI, the quest, saves, menus, HUD, the new-game story and difficulty levels.
- `Source/EldoriaLoadingScreen`: the studio splash, title card and loading screens.
- `Config`: game settings, including every asset path the game uses (`DefaultGame.ini`).
- `Scripts`: the Python and shell tools that build the map, sounds, materials, animations and narration.
- `Content`: Eldoria's own assets only (see `.gitignore`).
- `Story`: the narration script and the recording guide. `Art`: logo sources and fonts (Cinzel, SIL Open Font
  Licence).
- `archive/`: the early browser prototypes (`cd archive && npm install && npx vite eldoria-web`).
