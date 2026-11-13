#include "hellforge_mod.h"

/* Starter HellForge code mod: tweak the AR-23 Liberator (from the official template).
   Customize as needed for Guided Stim. */

static const HfSet liberatorSet[] = {
    {HF_FIELD_AT(ProjectileWeapon, fire_rate, 1), HF_F32(1200)}, /* the rate slot the weapon fires at */
    HF_SET_GROUP(WeaponData, recoil, HF_F32(0)),                  /* both recoil blocks, sideways and up */
    {0}
};
static const HfEditDef liberator = {.entity = HF_ENTITY_AR23_LIBERATOR, .set = liberatorSet};

/* Events are for logic, not for timing edits: here, a line in the log once the game's data has loaded. */
static void Ready(HfMod* mod, HfEvent event, void* user)
{
    const HfProjectileWeapon* weapon = hf_record(mod, HF_ENTITY_AR23_LIBERATOR, HfProjectileWeapon);
    if (weapon != NULL) HF_API(mod)->log(mod, "AR-23 Liberator: %.0f rounds per minute", (double)weapon->fire_rate[1]);
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* hf, HfMod* mod)
{
    if (hf->version < 6) return 1; /* needs HellForge's mod SDK 6 */
    hf->on(mod, HF_EVENT_ENTITIES_READY, Ready, NULL);
    /* 0: loaded. Otherwise the session's log folder has mod-lankynibbs.guidedstim.log saying why. */
    return hf->edit_entity(mod, &liberator);
}
