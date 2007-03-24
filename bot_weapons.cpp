//
// JK_Botti - be more human!
//
// bot_weapons.cpp
//

#define BOTWEAPONS

#include <string.h>

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>

#include "bot.h"
#include "bot_func.h"
#include "bot_weapons.h"
#include "bot_skill.h"
#include "bot_weapon_select.h"

extern bot_weapon_t weapon_defs[MAX_WEAPONS];
extern int submod_id;


// find weapon item flag
int GetWeaponItemFlag(const char * classname)
{
   bot_weapon_select_t *pSelect = &weapon_select[0];
   int itemflag = 0;
      
   while(pSelect->iId)
   {
      if(strcmp(classname, pSelect->weapon_name) == 0)
      {
         itemflag = pSelect->waypoint_flag;
         break;
      }
         
      pSelect++;
   }
   
   return(itemflag);
}

// find ammo itemflag
int GetAmmoItemFlag(const char * classname)
{
   bot_ammo_names_t *pAmmo = &ammo_names[0];
   int itemflag = 0;
   
   while(pAmmo->waypoint_flag)
   {
      if(strcmp(classname, pAmmo->ammoName) == 0)
      {
         itemflag = pAmmo->waypoint_flag;
         break;
      }
      pAmmo++;
   }
   
   return(itemflag);
}

//
qboolean BotCanUseWeapon(bot_t &pBot, const bot_weapon_select_t &select)
{
   return((pBot.bot_skill + 1) <= select.primary_skill_level || (pBot.bot_skill + 1) <= select.secondary_skill_level);
}

//
void BotSelectAttack(bot_t &pBot, const bot_weapon_select_t &select, qboolean &use_primary, qboolean &use_secondary) 
{
   use_secondary = FALSE;
   use_primary = FALSE;
   
   // if better attack is preferred
   // and bot_skill is equal or lesser than attack skill on both attacks
   if(select.prefer_higher_skill_attack && 
      (pBot.bot_skill + 1) <= select.secondary_skill_level && 
      (pBot.bot_skill + 1) <= select.primary_skill_level)
   {
      // check which one is prefered
      if(select.secondary_skill_level < select.primary_skill_level)
         use_secondary = TRUE;
      else
         use_primary = TRUE;
   }
   else
   {
      // use old method last
      if(RANDOM_LONG2(1, 100) <= select.primary_fire_percent) 
         use_primary = TRUE;
      else
         use_secondary = TRUE;
   }
}

// 
qboolean IsValidToFireAtTheMoment(bot_t &pBot, const bot_weapon_select_t &select) 
{
   // is the bot NOT carrying this weapon?
   if (!(pBot.pEdict->v.weapons & (1<<select.iId)))
      return(FALSE);

   // underwater and cannot use underwater
   if (pBot.pEdict->v.waterlevel == 3 && !select.can_use_underwater)
      return(FALSE);
   
   return(TRUE);
}

//
qboolean IsValidWeaponChoose(bot_t &pBot, const bot_weapon_select_t &select) 
{
   // Severians and Bubblemod checks, skip egon (bubblemod-egon is total conversion and severians-egon is selfkilling after time)
   if (select.iId == VALVE_WEAPON_EGON)
   {
      if(submod_id == SUBMOD_SEVS)
         return(FALSE);
      
      if(submod_id == SUBMOD_BUBBLEMOD && CVAR_GET_FLOAT("bm_gluon_mod") > 0)
         return(FALSE);
   }
   
   // is the bot NOT skilled enough to use this weapon?
   if (!BotCanUseWeapon(pBot, select))
      return(FALSE);
   
   return(TRUE);
}

//
qboolean IsValidPrimaryAttack(bot_t &pBot, const bot_weapon_select_t &select, const float distance, const qboolean always_in_range)
{
   int weapon_index = select.iId;
   qboolean primary_in_range;
   
   primary_in_range = (always_in_range) || (distance >= select.primary_min_distance && distance <= select.primary_max_distance);

   // no ammo required for this weapon OR
   // enough ammo available to fire AND
   // the bot is far enough away to use primary fire AND
   // the bot is close enough to the enemy to use primary fire
   return (primary_in_range && (weapon_defs[weapon_index].iAmmo1 == -1 || pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo1] >= select.min_primary_ammo));
}

//
qboolean IsValidSecondaryAttack(bot_t &pBot, const bot_weapon_select_t &select, const float distance, const qboolean always_in_range)
{
   int weapon_index = select.iId;
   qboolean secondary_valid = FALSE;
   qboolean secondary_in_range;
   
   // target is close enough
   secondary_in_range = (always_in_range) || (distance >= select.secondary_min_distance && distance <= select.secondary_max_distance);

   // see if there is enough secondary ammo AND
   // the bot is far enough away to use secondary fire AND
   // the bot is close enough to the enemy to use secondary fire
   if (secondary_in_range && 
       ((weapon_defs[weapon_index].iAmmo2 == -1 && !select.secondary_use_primary_ammo) ||
         (pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo2] >= select.min_secondary_ammo) ||
         (select.secondary_use_primary_ammo && 
          (weapon_defs[weapon_index].iAmmo1 == -1 || pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo1] >= select.min_primary_ammo))))
   {
      secondary_valid = TRUE;
      
      // MP5 cannot use secondary if primary is empty
      if(weapon_index == VALVE_WEAPON_MP5 &&
         (pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo1] <
          select.min_primary_ammo))
      {
         secondary_valid = FALSE;
      }
   }
   
   return(secondary_valid);
}

//
qboolean BotGetGoodWeaponCount(bot_t &pBot, const int stop_count)
{
   bot_weapon_select_t * pSelect = &weapon_select[0];
   int select_index;
   int good_count = 0;
   
   // loop through all the weapons until terminator is found...
   select_index = -1;
   while (pSelect[++select_index].iId) {
      if(!IsValidWeaponChoose(pBot, pSelect[select_index]))
         continue;
      
      if(pSelect[select_index].avoid_this_gun || pSelect[select_index].type != WEAPON_FIRE)
         continue;
      
      // don't do distance check, check if enough ammo
      if(!IsValidSecondaryAttack(pBot, pSelect[select_index], 0.0, TRUE) &&
         !IsValidPrimaryAttack(pBot, pSelect[select_index], 0.0, TRUE))
         continue;
      
      // not bad gun
      if(++good_count == stop_count)
         return(good_count);
   }
   
   return(good_count);
}

//
ammo_low_t BotPrimaryAmmoLow(bot_t &pBot, const bot_weapon_select_t &select)
{
   int weapon_index = select.iId;
   
   // this weapon doesn't use ammo
   if(weapon_defs[weapon_index].iAmmo1 == -1 || select.low_ammo_primary == -1)
      return(AMMO_NO);
   
   if(pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo1] <= select.low_ammo_primary)
      return(AMMO_LOW);
   
   return(AMMO_OK);      
}

//
ammo_low_t BotSecondaryAmmoLow(bot_t &pBot, const bot_weapon_select_t &select)
{
   int weapon_index = select.iId;
   
   if(select.low_ammo_secondary == -1)
      return(AMMO_NO);
   
   if(select.secondary_use_primary_ammo)
   {
      // this weapon doesn't use ammo
      if(weapon_defs[weapon_index].iAmmo2 == -1)
         return(AMMO_NO);
   
      if(pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo2] <= select.low_ammo_secondary)
         return(AMMO_LOW);
   }
   else
   {
      // this weapon doesn't use ammo
      if(weapon_defs[weapon_index].iAmmo1 == -1)
         return(AMMO_NO);
   
      if(pBot.m_rgAmmo[weapon_defs[weapon_index].iAmmo1] <= select.low_ammo_secondary)
         return(AMMO_LOW);
   }
   
   return(AMMO_OK);
}

//
int BotGetLowAmmoFlags(bot_t &pBot)
{
   bot_weapon_select_t * pSelect = &weapon_select[0];
   int select_index;
   int ammoflags = 0;
   
   // loop through all the weapons until terminator is found...
   select_index = -1;
   while (pSelect[++select_index].iId) {
      if(!IsValidWeaponChoose(pBot, pSelect[select_index]))
         continue;
      
      // low primary ammo?
      if(BotPrimaryAmmoLow(pBot, pSelect[select_index]) == AMMO_LOW)
      {
         ammoflags |= pSelect[select_index].ammo1_waypoint_flag;
      }
      
      // low secondary ammo?
      if(BotSecondaryAmmoLow(pBot, pSelect[select_index]) == AMMO_LOW)
      {
         ammoflags |= pSelect[select_index].ammo2_waypoint_flag;
      }
   }
   
   return(ammoflags);
}

//
qboolean BotAllWeaponsRunningOutOfAmmo(bot_t &pBot)
{
   bot_weapon_select_t * pSelect = &weapon_select[0];
   int select_index;
   
   // loop through all the weapons until terminator is found...
   select_index = -1;
   while (pSelect[++select_index].iId) {
      if(!IsValidWeaponChoose(pBot, pSelect[select_index]))
         continue;
      
      // don't do distance check, not enough ammo --> running out of ammo
      if(!IsValidSecondaryAttack(pBot, pSelect[select_index], 0.0, TRUE) &&
         !IsValidPrimaryAttack(pBot, pSelect[select_index], 0.0, TRUE))
         continue;
      
      // low primary ammo? continue
      if(BotPrimaryAmmoLow(pBot, pSelect[select_index]) != AMMO_OK)
         continue;
      
      // low secondary ammo? continue
      if(BotSecondaryAmmoLow(pBot, pSelect[select_index]) != AMMO_OK)
         continue;
      
      // this gun had enough ammo
      return(FALSE);
   }
   
   return(TRUE);
}

// Check if want to change to better weapon
int BotGetBetterWeaponChoice(bot_t &pBot, const bot_weapon_select_t &current, const bot_weapon_select_t *pSelect, const float distance, qboolean *use_primary, qboolean *use_secondary) {
   int select_index;
   *use_primary = FALSE;
   *use_secondary = FALSE;
   
   // check if we don't like current weapon.
   if(!current.avoid_this_gun)
      return -1;
        
   // loop through all the weapons until terminator is found...
   select_index = -1;
   while (pSelect[++select_index].iId) 
   {
      if(!IsValidToFireAtTheMoment(pBot, pSelect[select_index]))
         continue;

      if(!IsValidWeaponChoose(pBot, pSelect[select_index]))
         continue;
      
      if(pSelect[select_index].avoid_this_gun)
         continue;

      *use_primary = IsValidPrimaryAttack(pBot, pSelect[select_index], distance, FALSE);
      *use_secondary = IsValidSecondaryAttack(pBot, pSelect[select_index], distance, FALSE);
      
      if(*use_primary || *use_secondary)
         return select_index;
   }
   
   return -1;
}