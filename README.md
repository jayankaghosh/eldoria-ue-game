# Eldoria: The Bone King

A third-person open-world action game made with **Unreal Engine 5.8**, in which every line of code, every
system and every design decision was produced by AI.

> *Mordrak, the king's own sorcerer, feared one thing above all else: his own death. He stole the Royal Key,
> bound his soul to bone, and became the Bone King. Only one knight of the Silver Oath still stands.*
> **Rise, Sir Aldric. Eldoria is waiting.**

## About this project

Eldoria is a **personal experiment**: to see how far AI can go in building a real game, and to learn how
game development works along the way.

**Everything in this game was built by AI.** No code was written by hand and nothing was placed manually in
the Unreal editor. The game was developed through conversation with **Claude Code** (Anthropic's AI coding
agent): the human role was to play each version, describe what felt wrong or what was wanted ("the enemies
shouldn't fall in sync", "make the hit feel heavier", "show a story before the game starts"), and choose
between options. The AI then:

- wrote all of the C++ gameplay code: combat, enemy AI, ragdoll physics, the quest, saves, menus, HUD,
  loading screens and the story cinematic;
- wrote the Python and shell tools that build the world, retarget animations, make materials and sounds,
  and package the game;
- designed the levels, wrote the story and the narration script, and made the logo and loading screens;
- tested its own work by playing scripted fights, reading the engine's logs and looking at screenshots.

The narration was voiced with **ElevenLabs** (AI text-to-speech), from a script with emotion cues written by
the AI. The 3D characters, the village and the sound effects are free asset packs from Epic's **Fab**
marketplace, chosen and wired up by the AI.

## The game

- **Story-driven quest** in four chapters: break the warband in the village fields, cut down the champions at
  the ruins, climb the floating stones of the Sky Shrine for the Royal Key, and face the Bone King.
- **Weighty melee combat** inspired by God of War and Ghost of Tsushima: light and heavy combos, finishers and
  an aerial slam, block and parry, directional dodges with slow-motion on a perfect dodge, guard-breaking
  stance meters, executions on dazed enemies, hit-stop and camera shake.
- **Physical reactions**: big hits throw enemies to the ground as ragdolls; they kneel and push themselves back
  up. The hero can be knocked down too. Bodies fall where they're struck, bleed onto the ground, then fade away.
- **Three difficulty levels**, saves and loading, a narrated opening, and Low / Medium / High graphics settings.
- Realistic rendering with Lumen lighting, Nanite geometry and virtual shadow maps.

## Running it

The game runs on Macs with Apple Silicon. Third-party art isn't included in this repository (the licences
don't allow redistributing it), so setting up a copy means adding a few free packs from Fab first:
see **[SETUP.md](SETUP.md)**.

Controls: WASD or arrows to move, hold Shift to run · J/Z light attack · K/X heavy attack · F/V block (tap to
parry) · C/L or Q dodge · E interact / execute · Space jump · Esc pause · H shows all controls in game.

## Repository

- `Source/`: the game (C++) and its loading-screen module
- `Config/`: settings, including every asset the game uses
- `Scripts/`: the tools that build the world, content and app
- `Story/`: the narration script and recording guide
- `CLAUDE.md`: a detailed technical guide for AI agents working on the project

## Credits

- Built by **Claude Code** (Anthropic), directed through play-testing and feedback.
- Narration voiced with **ElevenLabs**.
- Characters: *Paragon: Greystone, Minions, Sevarog* (Epic Games, via Fab). World: *Medieval Village Megascans
  Sample* (Epic Games / Quixel). Animations: *Game Animation Sample* (Epic Games). Sound effects and blood
  effects: free packs on Fab. Font: *Cinzel* by Natanael Gama (SIL Open Font Licence).
- Made with Unreal Engine.
