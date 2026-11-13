#include "hellforge_mod.h"

/* Guided Stim: the P-11 Stim Pistol's healing darts seek out your squadmates.
 *
 *  1. Guided darts. Every dart you fire from the P-11 homes in on the nearest other Helldiver inside a cone in front of
 *     you (never on enemies), aimed at chest height. It flies faster and flatter too, so a rough aim in a firefight
 *     still lands the heal.
 *  2. Medic's kickback. When a squadmate gets a stim-sized heal while you have the P-11 out, you get a small heal
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

static const uint64_t stimPistol[] = {HF_ENTITY_P11_STIM_PISTOL, 0};

static int g_holdingStimPistol;  /* the local Helldiver has the P-11 out */
static unsigned g_dartsLanded;   /* squadmate heals credited this session */

#define GS_HAS(hf, member) ((hf)->size >= offsetof(HfApi, member) + sizeof(void*))

/* Once the game's data is in, look up the projectile the P-11 fires and put the guidance and flight rules on it. */
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

    if (GS_HAS(hf, homing))
    {
        HfHoming homing = {sizeof homing};
        homing.projectile = dart;
        homing.sources = stimPistol;
        homing.turn_rate = GS_TURN_RATE;
        homing.cone = GS_CONE;
        homing.range = GS_RANGE;
        homing.arm_distance = GS_ARM_DISTANCE;
        homing.aim_height = GS_AIM_HEIGHT;
        homing.flags = HF_HOMING_FRIENDLY | HF_SHOT_MULTIPLAYER;
        uint32_t id = hf->homing(mod, &homing);
        if (id != 0) hf->log(mod, "Guided Stim: darts (projectile %u) now home on squadmates", dart);
        else hf->log(mod, "Guided Stim: homing rule refused for projectile %u", dart);
    }
    else hf->log(mod, "Guided Stim: this HellForge has no homing; update it for guided darts");

    if (GS_HAS(hf, modify_shots))
    {
        HfShotRule flight = {sizeof flight};
        flight.projectile = dart;
        flight.sources = stimPistol;
        flight.damage = 1.0f;
        flight.armor_penetration = 1.0f;
        flight.speed = GS_SPEED;
        flight.gravity = GS_GRAVITY;
        flight.drag = 1.0f;
        flight.flags = HF_SHOT_MULTIPLAYER;
        if (hf->modify_shots(mod, &flight) == 0) hf->log(mod, "Guided Stim: flight rule refused");
    }
}

/* Track whether the local Helldiver has the P-11 out. */
static void WeaponChanged(HfMod* mod, const HfWeaponEvent* event, void* user)
{
    (void)mod; (void)user;
    g_holdingStimPistol = event->resource == HF_ENTITY_P11_STIM_PISTOL;
}

/* A squadmate healed a stim's worth while you're holding the P-11: count it as your dart and pay out the kickback.
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
    HF_API(mod)->log(mod, "Guided Stim: mission started, dart counter reset");
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* hf, HfMod* mod)
{
    if (hf->version < 11) return 1; /* events and shot rules need HellForge's mod SDK 11 */
    hf->on(mod, HF_EVENT_ENTITIES_READY, Ready, NULL);
    hf_on_weapon(mod, WeaponChanged, NULL, 0);
    hf_on_heal(mod, Healed, NULL, 0);
    hf_on_mission_start(mod, MissionStarted, NULL, 0);
    /* 0: loaded. Otherwise the session's log folder has mod-lankynibbs.guidedstim.log saying why. */
    return 0;
}
