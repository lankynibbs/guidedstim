# Guided Stim

A private HellForge mod for HELLDIVERS 2.

HellForge lets you run your own local copy of the game with mods. This repo holds the sources for the **Guided Stim** code mod.

## What it does

Adds a new secondary, the **P-11G Guided Stim Pistol**, next to the vanilla P-11 in the armory (the vanilla one is left alone). It's a copy of the P-11, so HellForge only gives it to players whose synced account owns the Chemical Agents warbond.

- **Guided darts.** P-11G darts home in on the nearest other Helldiver in a 35-degree cone within 60 m (never on enemies), aimed at chest height. They also fly 1.6x faster with a quarter of the gravity.
- **Medic's kickback.** When a squadmate gets a stim-sized heal (25+) while you have the P-11G out, you heal 15 yourself; a big heal (75+) also mends both your legs. Each one is logged with a per-mission counter.

Tuning values are `#define`s at the top of `src/guidedstim.c`. `hellforge-data.json` gives the P-11G its own copy of the P-11's model. Needs HellForge mod SDK 11 or later; guidance works in multiplayer lobbies too.

## Layout

```
hellforge-mod.json   manifest (id, name, version, dll, sdk)
hellforge-data.json  data: the P-11G's model (a copy of the P-11's)
src/guidedstim.c     C source
CMakeLists.txt       for IDEs
.gitignore
```

## Using it

1. Clone or copy this folder into your HellForge installation's `mods/` directory (or zip it and add via the Mods page).
2. Ensure the mod tools (Zig) are installed in HellForge.
3. Build from the Mods page (**Build now**) or let a session build it automatically.
4. Enable the mod and play.

Id: `lankynibbs.guidedstim`  
SDK: 16 (adjust if needed for your HellForge version).

See the [HellForge modding guide](https://github.com/SkyeShade/HellForge/blob/main/docs/modding/README.md) and [first-mod walkthrough](https://github.com/SkyeShade/HellForge/blob/main/docs/modding/first-mod.md) for details.

---

*Private repository.*
