# Guided Stim

A private HellForge mod for HELLDIVERS 2.

HellForge lets you run your own local copy of the game with mods. This repo holds the sources for the **Guided Stim** code mod.

## Layout

```
hellforge-mod.json   manifest (id, name, version, dll, sdk)
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
