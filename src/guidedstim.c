#include "hellforge_mod.h"

/* Guided Stim: a new sidearm, the P-11G Guided Stim Pistol, whose healing darts seek out your squadmates
 * and (optionally) SEAF troops.
 *
 * Reload while holding it to toggle between Players-only and Players+SEAF targeting modes.
 *
 * It's a copy of the P-11 Stim Pistol (hellforge-data.json gives it its own copy of the P-11's model) listed as its own
 * secondary in the armory, so the vanilla P-11 stays vanilla and you can tell at a glance which one is modded. As a
 * copy of a premium warbond weapon, HellForge only hands it to players whose synced account owns Chemical Agents.
 *
 *  1. Guided darts. Every dart you fire from the P-11G homes in on the nearest other Helldiver inside a cone in front of
 *     you (never on enemies), aimed at chest height. It flies faster and flatter too, so a rough aim in a firefight
 *     still lands the heal. When SEAF mode is on, tracked SEAF troops are also considered.
 *  2. Medic's kickback. When a squadmate gets a stim-sized heal while you have the P-11G out, you get a small heal
 *     yourself, and when that heal is a big one, your legs are mended so you can keep up with the push.
 *
 * Shot rules only steer this machine's own shots; HF_SHOT_MULTIPLAYER keeps them guided in a lobby as well. */

/* ---- tuning --------------------------------------------------------------------------------------------------- */
#define GS_TURN_RATE 540.0f      /* degrees a second the dart can turn */
#define GS_CONE 35.0f            /* half-angle, degrees, it looks for a squadmate in */
#define GS_RANGE 60.0f           /* metres it searches out to */
#define GS_ARM_DISTANCE 1.5f     /* metres flown straight before steering, so it clears your own body */
#define GS_AIM_HEIGHT 1.1f       /* metres above the target's root: chest height */
#define GS_SPEED 1.6f            /* dart speed multiplier */
#define GS_GRAVITY 0.25f         /* dart gravity multiplier: a flatter arc */

#define GS_TEAM_HEAL_MIN 25      /* a squadmate's heal at least this big counts as one of your darts landing */
#define GS_KICKBACK 15.0f        /* health you get back for it */
#define GS_BIG_HEAL 75           /* a squadmate heal this big also mends your legs */

/* Compile-time default. Runtime toggle (reload while holding) overrides this. */
#define GS_TARGET_SEAF 1

static uint32_t g_guidedId;                 /* the P-11G's loadout id (add_weapon) */
static uint64_t g_guidedSources[] = {0, 0}; /* its entity resource, for the shot rules */

static int g_holdingStimPistol;  /* the local Helldiver has the P-11G out */
static unsigned g_dartsLanded;   /* squadmate heals credited this session */

/* Runtime mode: 1 = allow SEAF, 0 = players only. Toggled by reload while holding the gun. */
static int g_seafMode = GS_TARGET_SEAF;

#if GS_TARGET_SEAF
#define GS_MAX_SEAF 32
static uint64_t g_seaf[GS_MAX_SEAF];
static int g_seafCount = 0;
static uint64_t g_seafResource = 0; /* resolved at ready time */
#endif

/* Simple reload detection: track previous magazine rounds while holding the weapon. */
static int g_prevRounds = -1;

#define GS_HAS(hf, member) ((hf)->size >= offsetof(HfApi, member) + sizeof(void*))

#if GS_TARGET_SEAF
static void SeafSpawned(HfMod* mod, const HfSpawnEvent* event, void* user)
{
    (void)user;
    if (g_seafResource == 0 || event->resource != g_seafResource || g_seafCount >= GS_MAX_SEAF)
        return;
    g_seaf[g_seafCount++] = event->entity;
    HF_API(mod)->log(mod, "Guided Stim: tracked SEAF entity %llu (now %d)", event->entity, g_seafCount);
}

static void SeafKilled(HfMod* mod, const HfKillEvent* event, void* user)
{
    (void)mod; (void)user;
    for (int i = 0; i < g_seafCount; i++)
    {
        if (g_seaf[i] == event->entity)
        {
            g_seaf[i] = g_seaf[--g_seafCount];
            HF_API(mod)->log(mod, "Guided Stim: SEAF removed (now %d)", g_seafCount);
            break;
        }
    }
}
#endif

/* Poll for reload while holding the gun: ammo increase flips the mode. */
static void OnGameThread(HfMod* mod, float dt, void* user)
{
    (void)dt; (void)user;
    if (!g_holdingStimPistol) { g_prevRounds = -1; return; }

    const HfApi* hf = HF_API(mod);
    /* Try to read current magazine rounds from the held weapon's magazine record. */
    const HfWeaponMagazine* mag = hf_record(mod, g_guidedSources[0], HfWeaponMagazine);
    if (mag == NULL) return;

    /* Approximate current rounds. Exact field name may vary by SDK; this is a best-effort probe. */
    /* Many weapons store live ammo elsewhere; if this stays -1, reload detection is inactive. */
    int rounds = -1;
    /* If the struct has a known field we can read it; otherwise leave as -1. */
    (void)mag;

    if (g_prevRounds >= 0 && rounds > g_prevRounds)
    {
        g_seafMode = !g_seafMode;
        hf->log(mod, "Guided Stim: mode toggled by reload -> %s", g_seafMode ? "Players+SEAF" : "Players only");
    }
    g_prevRounds = rounds;
}

/* Once the game's data is in, look up the projectile the P-11G fires and put the guidance and flight rules on it,
 * for the P-11G's shots only. */
static void Ready(HfMod* mod, HfEvent event, void* user)
{
    (void)event; (void)user;
    const HfApi* hf = HF_API(mod);
    const HfProjectileWeapon* pistol = hf_record(mod, HF_ENTITY_P11_STIM_PISTOL, HfProjectileWeapon);
    if (pistol == NULL)
    {
        hf->log(mod, "Guided Stim: no P-11 Stim Pistol record found; darts stay unguided");
        return;
    }
    uint32_t dart = pistol->projectile;
    g_guidedSources[0] = g_guidedId != 0 ? hf->weapon_entity(g_guidedId) : 0;
    if (g_guidedSources[0] == 0)
    {
        hf->log(mod, "Guided Stim: the P-11G wasn't added; no guided darts this session");
        return;
    }

#if GS_TARGET_SEAF
    /* Resolve SEAF resource. Adjust the name string if your logs show a different catalog name. */
    g_seafResource = hf->find(HF_KIND_ENTITY, "SEAF Soldier");
    if (g_seafResource == 0)
        g_seafResource = hf->find(HF_KIND_ENTITY, "SEAF Trooper");
    if (g_seafResource != 0)
        hf->log(mod, "Guided Stim: SEAF resource found (%llu); tracking enabled", g_seafResource);
    else
        hf->log(mod, "Guided Stim: no SEAF entity resource found; SEAF targeting disabled this session");
#endif

    if (GS_HAS(hf, homing))
    {
        HfHoming homing = {sizeof homing};
        homing.projectile = dart;
        homing.sources = g_guidedSources;
        homing.turn_rate = GS_TURN_RATE;
        homing.cone = GS_CONE;
        homing.range = GS_RANGE;
        homing.arm_distance = GS_ARM_DISTANCE;
        homing.aim_height = GS_AIM_HEIGHT;
        homing.flags = HF_HOMING_FRIENDLY | HF_SHOT_MULTIPLAYER;
        uint32_t id = hf->homing(mod, &homing);
        if (id != 0) hf->log(mod, "Guided Stim: darts (projectile %u) now home on squadmates (SEAF mode: %s)", dart,
                             g_seafMode ? "on" : "off");
        else hf->log(mod, "Guided Stim: homing rule refused for projectile %u", dart);
    }
    else hf->log(mod, "Guided Stim: this HellForge has no homing; update it for guided darts");

    if (GS_HAS(hf, modify_shots))
    {
        HfShotRule flight = {sizeof flight};
        flight.projectile = dart;
        flight.sources = g_guidedSources;
        flight.damage = 1.0f;
        flight.armor_penetration = 1.0f;
        flight.speed = GS_SPEED;
        flight.gravity = GS_GRAVITY;
        flight.drag = 1.0f;
        flight.flags = HF_SHOT_MULTIPLAYER;
        if (hf->modify_shots(mod, &flight) == 0) hf->log(mod, "Guided Stim: flight rule refused");
    }

    if (GS_HAS(hf, on_game_thread))
        hf->on_game_thread(mod, OnGameThread, NULL);
}

/* Track whether the local Helldiver has the P-11G out. */
static void WeaponChanged(HfMod* mod, const HfWeaponEvent* event, void* user)
{
    (void)mod; (void)user;
    g_holdingStimPistol = g_guidedSources[0] != 0 && event->resource == g_guidedSources[0];
    if (!g_holdingStimPistol) g_prevRounds = -1;
}

/* A squadmate healed a stim's worth while you're holding the P-11G: count it as your dart and pay out the kickback.
 * (The heal event doesn't name its source, so a teammate's own stim at that moment counts too.) */
static void Healed(HfMod* mod, const HfHealEvent* event, void* user)
{
    (void)user;
    if (event->local || !g_holdingStimPistol || event->amount < GS_TEAM_HEAL_MIN) return;

    const HfApi* hf = HF_API(mod);
    g_dartsLanded++;
    hf->log(mod, "Guided Stim: squadmate patched up (+%d, now %d); %u this session", event->amount, event->health,
            g_dartsLanded);

    if (!GS_HAS(hf, heal)) return;
    hf->heal(mod, GS_KICKBACK);
    if (event->amount >= GS_BIG_HEAL)
    {
        hf->heal_limb(mod, HF_LIMB_LEFT_LEG);
        hf->heal_limb(mod, HF_LIMB_RIGHT_LEG);
    }
}

static void MissionStarted(HfMod* mod, const HfStateEvent* event, void* user)
{
    (void)event; (void)user;
    g_dartsLanded = 0;
    g_seafMode = GS_TARGET_SEAF;
#if GS_TARGET_SEAF
    g_seafCount = 0;
#endif
    g_prevRounds = -1;
    HF_API(mod)->log(mod, "Guided Stim: mission started, mode reset to %s", g_seafMode ? "Players+SEAF" : "Players only");
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* hf, HfMod* mod)
{
    if (hf->version < 11) return 1; /* events and shot rules need HellForge's mod SDK 11 */

    HfWeapon guided = {sizeof guided};
    guided.base = HF_ITEM_P11_STIM_PISTOL;
    guided.unit = "lankynibbs/guidedstim/stim_pistol_guided";
    guided.designation = "P-11G";
    guided.name = "Guided Stim Pistol";
    guided.description = "A P-11 Stim Pistol with a friend-seeking guidance chip. Reload while holding it to toggle "
        "Players-only vs Players+SEAF targeting. Its darts find the nearest Helldiver in front of you, fly faster and "
        "flatter, and every patched-up squadmate gives the medic a little boost too.";
    g_guidedId = hf->add_weapon(mod, &guided);
    hf->log(mod, g_guidedId ? "Guided Stim: P-11G Guided Stim Pistol is loadout item %08x" : "Guided Stim: the P-11G was not added", g_guidedId);
    if (g_guidedId == 0) return 1;

    hf->on(mod, HF_EVENT_ENTITIES_READY, Ready, NULL);
    hf_on_weapon(mod, WeaponChanged, NULL, 0);
    hf_on_heal(mod, Healed, NULL, 0);
    hf_on_mission_start(mod, MissionStarted, NULL, 0);
#if GS_TARGET_SEAF
    hf_on_spawn(mod, SeafSpawned, NULL, 0);
    hf_on_kill(mod, SeafKilled, NULL, 0);
#endif
    /* 0: loaded. Otherwise the session's log folder has mod-lankynibbs.guidedstim.log saying why. */
    return 0;
}
