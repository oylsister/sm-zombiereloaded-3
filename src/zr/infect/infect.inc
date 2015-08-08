/*
 * ============================================================================
 *
 *  Zombie:Reloaded
 *
 *  File:          infect.inc
 *  Type:          Core
 *  Description:   Client infection functions.
 *
 *  Copyright (C) 2009-2013  Greyscale, Richard Helgeby
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ============================================================================
 */

#pragma newdecls required

/**
 * @section Explosion flags.
 */
#define EXP_NODAMAGE               1
#define EXP_REPEATABLE             2
#define EXP_NOFIREBALL             4
#define EXP_NOSMOKE                8
#define EXP_NODECAL               16
#define EXP_NOSPARKS              32
#define EXP_NOSOUND               64
#define EXP_RANDOMORIENTATION    128
#define EXP_NOFIREBALLSMOKE      256
#define EXP_NOPARTICLES          512
#define EXP_NODLIGHTS           1024
#define EXP_NOCLAMPMIN          2048
#define EXP_NOCLAMPMAX          4096
/**
 * @endsection
 */

/**
 * @section Global variables to store infect timer handles.
 */
Handle tInfect = null;
Handle tInfectCountdown = null;
/**
 * @endsection
 */

/**
 * Infection countdown data pack.
 */
Handle hInfectCountdownData = null;

/**
 * Array for flagging client as zombie.
 */
bool bZombie[MAXPLAYERS + 1];

/**
 * @section bInfectImmune indexes
 */
#define INFECT_TYPE_MOTHER 0
#define INFECT_TYPE_NORMAL 1
/**
 * @endsection
 */

/**
 * Array for flagging client to be protected. (See defines above)
 */
bool bInfectImmune[MAXPLAYERS + 1][2];

/**
 * Available mother zombie infection modes.
 */
enum InfectMode
{
    InfectMode_Invalid = -1,    /** Invalid mode, used by validators. */
    InfectMode_Dynamic,         /** Every n-th player is infected. */
    InfectMode_Absolute,        /** Keep n humans (negative n) or infect n zombies. */
    InfectMode_Range            /** An absolute number of zombies infected (min to max). */
}

#include "zr/infect/events"

/**
 * Timer callback, chooses mother zombies.
 * 
 * @param timer     The timer handle.  
 */ 
public Action InfectMotherZombie(Handle timer)
{
    // Reset timer handle.
    tInfect = null;
    
    // Create eligible player list.
    Handle arrayEligibleClients = null;
    int eligibleclients = ZRCreateEligibleClientList(arrayEligibleClients, true, true, true);
    
    // If there are no eligible client's then stop.
    if (!eligibleclients)
    {
        // Destroy handle.
        CloseHandle(arrayEligibleClients);
        return Plugin_Stop;
    }
    
    // Prune list of immune clients.
    eligibleclients = InfectRemoveImmuneClients(arrayEligibleClients);
    
    // Move all clients to CT.
    InfectMoveAllToCT();
    
    int mothercount;
    int ratio = GetConVarInt(g_hCvarsList[CVAR_INFECT_MZOMBIE_RATIO]);
    int min = GetConVarInt(g_hCvarsList[CVAR_INFECT_MZOMBIE_MIN]);
    int max = GetConVarInt(g_hCvarsList[CVAR_INFECT_MZOMBIE_MAX]);
    
    // Count valid human clients.
    int humancount;
    ZRCountValidClients(_, humancount, _, true);
    
    // Get and validate infection mode. This will also log a warning on error.
    InfectMode mode = InfectGetModeOrFail();
    
    // Apply infection mode.
    switch (mode)
    {
        case InfectMode_Invalid:
        {
            // Validation failed. Fall back to one mother zombie.
            mothercount = 1;
        }
        case InfectMode_Dynamic:
        {
            // Dynamic mode. Every n-th player is infected.
            
            // A ratio of 0 will infect one zombie (to keep backwards compatibility).
            if (ratio == 0)
            {
                mothercount = 1;
            }
            
            // Calculate number of zombies to infect.
            mothercount = RoundToNearest(float(humancount) / ratio);
            
            // Require at least one mother zombie.
            if (mothercount == 0)
            {
                mothercount = 1;
            }
        }
        case InfectMode_Absolute:
        {
            if (ratio > 0)
            {
                // Infect n humans.
                mothercount = ratio;
            }
            else
            {
                // Infect all but n humans. Since ratio already is negative
                // just add the numbers. (Zero ratio is catched by validator.)
                mothercount = humancount + ratio;
                
                // Force at least one mother zombie.
                if (mothercount == 0)
                {
                    mothercount = 1;
                }
            }
        }
        case InfectMode_Range:
        {
            // Get a random number between the range.
            mothercount = Math_GetRandomInt(min, max);
        }
    }
    
    // Infect players.
    for (int n = 0; n < mothercount; n++)
    {
        // Recount eligible clients.
        eligibleclients = GetArraySize(arrayEligibleClients);
        
        // Stop if there are no more eligible clients.
        if (eligibleclients <= 0)
        {
            break;
        }
        
        // Get a random array index.
        int i = Math_GetRandomInt(0, eligibleclients - 1);
        
        // Get the client stored in the random array index.
        int client = GetArrayCell(arrayEligibleClients, i);
        
        // Infect player.
        InfectHumanToZombie(client, _, true);
        
        // Remove player from eligible client list.
        RemoveFromArray(arrayEligibleClients, i);
    }
    
    // Mother zombies have been infected.
    g_bZombieSpawned = true;
    
    // Destroy client list.
    CloseHandle(arrayEligibleClients);
    
    return Plugin_Stop;
}

/**
 * Moves all alive clients to the CT team.
 */
void InfectMoveAllToCT()
{
    // Move all clients to CT
    for (int client = 1; client <= MaxClients; client++)
    {
        // If client isn't in-game, then stop.
        if (!IsClientInGame(client))
        {
            continue;
        }
        
        // If client is dead, then stop.
        if (!IsPlayerAlive(client))
        {
            continue;
        }
        
        // Switch client to CT team.
        CS_SwitchTeam(client, CS_TEAM_CT);
    }
}

/**
 * Removes immune clients from a client list. If a client is removed, their
 * immunity is also removed.
 *
 * @param clientList        List of clients.
 * @param keepLastPlayer    Don't remove if there's only one player left.
 *
 * @return                  Number of clients remaining.
 */
int InfectRemoveImmuneClients(Handle clientList, bool keepLastPlayer = true)
{
    int len = GetArraySize(clientList);
    
    // Loop though client list.
    for (int i = 0; i < len; i++)
    {
        // Stop pruning if there is only one player left.
        if (keepLastPlayer && len <= 1)
        {
            break;
        }
        
        // Get client.
        int client = GetArrayCell(clientList, i);
        
        // Check if client is immune from mother zombie infection.
        if (bInfectImmune[client][INFECT_TYPE_MOTHER])
        {
            // Take away immunity.
            bInfectImmune[client][INFECT_TYPE_MOTHER] = false;
            
            // Remove client from array.
            RemoveFromArray(clientList, i);
            
            // Update list size.
            len--;
            
            // Backtrack one index, because we deleted it out from under the loop.
            i--;
        }
    }
    
    return len;
}

/**
 * Timer callback, displays countdown to clients.
 * 
 * @param timer     The timer handle.  
 */ 
public Action InfectCountdown(Handle timer)
{
    bool countdown = GetConVarBool(g_hCvarsList[CVAR_INFECT_MZOMBIE_COUNTDOWN]);
    if (!countdown)
    {
        InfectStopCountdown();
        return Plugin_Stop;
    }
    
    // Read the info from the datapack.
    ResetPack(hInfectCountdownData);
    float length = ReadPackFloat(hInfectCountdownData);
    float counter = ReadPackFloat(hInfectCountdownData);
    
    // Check if the countdown has finished.
    if (counter >= length)
    {
        InfectStopCountdown();
        return Plugin_Stop;
    }
    
    // Print the countdown text to the clients.
    TranslationPrintCenterTextAll(false, "Infect countdown", RoundToNearest(length - counter));
    
    counter++;
    
    // Write the new counter value to the datapack.
    ResetPack(hInfectCountdownData);
    WritePackFloat(hInfectCountdownData, length);
    WritePackFloat(hInfectCountdownData, counter);
    
    return Plugin_Continue;
}

/**
 * Stops the infection countdown timer.
 */
void InfectStopCountdown()
{
    // Kill the timer.
    ZREndTimer(tInfectCountdown);
    
    // Destroy data pack.
    if (hInfectCountdownData != null)
    {
        CloseHandle(hInfectCountdownData);
        hInfectCountdownData = null;
    }
}

/**
 * Infects a client. Execute events, sets attributes and flags that indicate
 * that the client is a zombie.
 *
 * @param client            The client to infect.
 * @param attacker          (Optional) The attacker who did the infect.
 * @param motherinfect      (Optional) Indicates a mother zombie infect.
 * @param respawnoverride   (Optional) Set to true to override respawn cvar.
 * @param respawn           (Optional) Value to override with.
 */
void InfectHumanToZombie(int client, int attacker = -1, bool motherinfect = false, bool respawnoverride = false, bool respawn = false)
{
    // Forward pre-event to modules.
    Action result = APIOnClientInfect(client, attacker, motherinfect, respawnoverride, respawn);
    
    // Check if infection should be blocked.
    if (result == Plugin_Handled)
    {
        return;
    }
    
    // Mark player as zombie.
    bZombie[client] = true;
    
    // Check if consecutive infection protection is enabled.
    bool infectconsecutiveblock = GetConVarBool(g_hCvarsList[CVAR_INFECT_CONSECUTIVE_BLOCK]);
    if (infectconsecutiveblock)
    {
        // If this is a mother infect, flag the player as immune for next mother
        // infection. Otherwise do nothing and keep the current flag.
        if (motherinfect)
        {
            bInfectImmune[client][INFECT_TYPE_MOTHER] = true;
        }
    }
    else
    {
        // Consecutive infection protection is disabled. No immunity.
        bInfectImmune[client][INFECT_TYPE_MOTHER] = false;
    }
    
    // Apply effects.
    InfectFireEffects(client);
    
    // Stop coundown, if running.
    InfectStopCountdown();
    
    // If attacker is valid, then continue.
    if (ZRIsClientValid(attacker))
    {
        // Create and send custom player_death event.
        Handle event = CreateEvent("player_death");
        if (event != null)
        {
            SetEventInt(event, "userid", GetClientUserId(client));
            SetEventInt(event, "attacker", GetClientUserId(attacker));
            SetEventString(event, "weapon", "zombie_claws_of_death");
            FireEvent(event, false);
        }
        
        // Apply score and health gain.
        InfectUpdateScore(attacker, client);
    }
    
    // Get a list of all client's weapon indexes.
    int weapons[WeaponsSlot];
    WeaponsGetClientWeapons(client, weapons);
    
    // Check if weapons drop is enabled.
    bool weaponsdrop = GetConVarBool(g_hCvarsList[CVAR_INFECT_WEAPONS_DROP]);
    
    // This must be after the event forwarding because it fixes a problem caused by changing models in ClassOnClientInfected.
    // Remove all weapons but knife.
    WeaponsRemoveAllClientWeapons(client, weaponsdrop);
    
    // Switch the player to terrorists.
    // TODO: A solution to stop confusing bots? Respawn and teleport?
    CS_SwitchTeam(client, CS_TEAM_T);
    
    // If respawn is enabled, then teleport mother zombie back to spawnpoint.
    if (motherinfect)
    {
        bool zombierespawn = GetConVarBool(g_hCvarsList[CVAR_INFECT_MZOMBIE_RESPAWN]);
        if(zombierespawn)
        {
            ZTele_TeleportClient(client);
        }
    }
    // Check override.
    else
    {
        if (respawnoverride && respawn)
        {
            ZTele_TeleportClient(client);
        }
    }
    
    // Print message to client.
    TranslationPrintToChat(client, "Infect infected");
    
    // Forward event to modules.
    ClassOnClientInfected(client, motherinfect);
    RoundEndOnClientInfected();
    DamageOnClientInfected(client, motherinfect);
    SEffectsOnClientInfected(client);
    ZTele_OnClientInfected(client);
    ZHPOnClientInfected(client);
    APIOnClientInfected(client, attacker, motherinfect, respawnoverride, respawn);
    ImmunityOnClientInfected(client);
}

/**
 * Turns a zombie back into a human. Execute events, sets attributes and flags that indicate
 * that the client is a human.
 *
 * @param client        The client to make human.
 * @param respawn       Teleport client back to spawn if true.
 * @param protect       Start spawn protection on new human.
 */
void InfectZombieToHuman(int client, bool respawn = false, bool protect = false)
{
    // Forward pre-event to modules.
    Action result = APIOnClientHuman(client, respawn, protect);
    
    // Check if action should be blocked.
    if (result == Plugin_Handled)
    {
        return;
    }
    
    // Mark player as human.
    bZombie[client] = false;
    
    // Switch the player to counter-terrorists.
    CS_SwitchTeam(client, CS_TEAM_CT);
    
    // Set client as translation target.
    SetGlobalTransTarget(client);
    
    // Print message to client.
    TranslationPrintToChat(client, "Infect human");
    
    // Forward event to modules.
    ClassReloadPlayer(client);
    RoundEndOnClientInfected();
    ZTele_OnClientInfected(client);
    
    // Give human a new knife. (If you leave the old one there will be glitches with the knife positioning)
    int knife = GetPlayerWeaponSlot(client, view_as<int>(Slot_Melee));
    if (knife != -1)
    {
        RemovePlayerItem(client, knife);
        AcceptEntityInput(knife, "Kill");
        GivePlayerItem(client, "weapon_knife");
    }
    
    // Check if we should respawn the client.
    if (respawn)
    {
        ZTele_TeleportClient(client);
    }
    
    // Check if we should spawn protect the client.
    if (protect)
    {
        SpawnProtectStart(client);
    }
    
    // Forward event to modules.
    SEffectsOnClientHuman(client);
    APIOnClientHumanPost(client, respawn, protect);
    ImmunityOnClientHuman(client);
}

/**
 * Updates score for attacker and victim. Applies health gain for attacker.
 */
void InfectUpdateScore(int attacker, int victim)
{
    // Give client's infector a point.
    int score = ToolsClientScore(attacker, true, false);
    ToolsClientScore(attacker, true, true, ++score);
    
    // Add a death to the zombie's score.
    int deaths = ToolsClientScore(victim, false, false);
    ToolsClientScore(victim, false, true, ++deaths);
    
    // Apply infect HP gain.
    int healthgain = ClassGetHealthInfectGain(attacker);
    int health = GetClientHealth(attacker);
    
    // Set attacker's new health.
    SetEntityHealth(attacker, health + healthgain);
    
    // Forward event to modules.
    ZHPOnHealthInfectGain(attacker);
}

/**
 * Creates effects on a newly infected client.
 * 
 * @param client    The client index.
 */
void InfectFireEffects(int client)
{
    // Initialize vector variables.
    float clientloc[3];
    float direction[3] = {0.0, 0.0, 0.0};
    
    // Get client's position.
    GetClientAbsOrigin(client, clientloc);
    clientloc[2] += 30;
    
    bool explosion = GetConVarBool(g_hCvarsList[CVAR_INFECT_EXPLOSION]);
    if (explosion)
    {
        // Initialize explosion flags variable.
        int flags;
        
        // Set "nofireball" flag if fireball is disabled.
        bool fireball = GetConVarBool(g_hCvarsList[CVAR_INFECT_FIREBALL]);
        if (!fireball)
        {
            flags = flags | EXP_NOFIREBALL;
        }
        
        // Set "nosmoke" flag if smoke is disabled.
        bool smoke = GetConVarBool(g_hCvarsList[CVAR_INFECT_SMOKE]);
        if (!smoke)
        {
            flags = flags | EXP_NOSMOKE;
        }
        
        // Set "nosparks" flag if sparks are disabled.
        bool sparks = GetConVarBool(g_hCvarsList[CVAR_INFECT_SPARKS]);
        if (!sparks)
        {
            flags = flags | EXP_NOSPARKS;
        }
        
        // Create explosion at client's origin.
        VEffectsCreateExplosion(clientloc, flags);
    }
    
    // Emit scream sound if enabled.
    ZombieSoundsScream(client);
    
    // If energy splash effect is enabled, then continue.
    bool esplash = GetConVarBool(g_hCvarsList[CVAR_INFECT_ESPLASH]);
    if (esplash)
    {
        // Create energy splash effect.
        VEffectsCreateEnergySplash(clientloc, direction, true);
    }
    
    // If shake effect is enabled, then continue.
    bool shake = GetConVarBool(g_hCvarsList[CVAR_INFECT_SHAKE]);
    if (shake)
    {
        // Get shake info.
        float shakeamp = GetConVarFloat(g_hCvarsList[CVAR_INFECT_SHAKE_AMP]);
        float shakefrequency = GetConVarFloat(g_hCvarsList[CVAR_INFECT_SHAKE_FREQUENCY]);
        float shakeduration = GetConVarFloat(g_hCvarsList[CVAR_INFECT_SHAKE_DURATION]);
        
        // Shake client's screen.
        VEffectsShakeClientScreen(client, shakeamp, shakefrequency, shakeduration);
    }
}

/**
 * Sends list of clients to infect/human.
 *  
 * @param client    The client index.
 */
void InfectMenuClients(int client)
{
    // Create menu handle.
    Handle menu_infect_clients = CreateMenu(InfectMenuClientsHandle);
    
    // Set client as translation target.
    SetGlobalTransTarget(client);
    
    char title[MENU_LINE_TITLE_LENGTH];
    char clientoption[MENU_LINE_REG_LENGTH];
    char clientuserid[8];
    
    // x = Client index.
    for (int x = 1; x <= MaxClients; x++)
    {
        // If client isn't in-game, then stop.
        if (!IsClientInGame(x))
        {
            continue;
        }
        
        // If client isn't alive, then stop.
        if (!IsPlayerAlive(x))
        {
            continue;
        }
        
        // Get client info.
        GetClientName(x, clientoption, sizeof(clientoption));
        IntToString(GetClientUserId(x), clientuserid, sizeof(clientuserid));
        
        // Append client's current team to the option.
        if (InfectIsClientInfected(x))
        {
            Format(clientoption, sizeof(clientoption), "%s [%t]", clientoption, "Zombie");
        }
        else
        {
            Format(clientoption, sizeof(clientoption), "%s [%t]", clientoption, "Human");
        }
        
        // Add option to menu.
        AddMenuItem(menu_infect_clients, clientuserid, clientoption);
    }
    
    Format(title, sizeof(title), "%t\n ", "Infect menu clients title");
    SetMenuTitle(menu_infect_clients, title);
    
    // Create a "Back" button to the main admin menu.
    SetMenuExitBackButton(menu_infect_clients, true);
    
    // Send menu.
    DisplayMenu(menu_infect_clients, client, MENU_TIME_FOREVER);
}

/**
 * Called when client selects option in the infect clients menu, and handles it.
 * @param menu_infect_clients   Handle of the menu being used.
 * @param action                The action done on the menu (see menus.inc, enum MenuAction).
 * @param client                The client index.
 * @param slot                  The slot index selected (starting from 0).
 */ 
public int InfectMenuClientsHandle(Handle menu_infect_clients, MenuAction action, int client, int slot)
{
    // Client selected an option.
    if (action == MenuAction_Select)
    {
        // Get selected client index.
        int target = MenuGetClientIndex(menu_infect_clients, slot);
        
        // If target has left the server, then stop.
        if (!target)
        {
            // Re-send menu.
            InfectMenuClients(client);
            return 0;
        }
        
        // Create an array with a single slot and set target to it.
        int targets[1];
        targets[0] = target;
        
        // Toggle infect on the client.
        if (InfectIsClientInfected(target))
        {
            InfectManualHuman(client, targets, 1);
        }
        else
        {
            InfectManualInfect(client, targets, 1);
        }
        
        // Re-send menu.
        InfectMenuClients(client);
    }
    // Client closed the menu.
    if (action == MenuAction_Cancel)
    {
        // Client hit "Back" button.
        if (slot == MenuCancel_ExitBack)
        {
            // Re-open admin menu.
            ZAdminMenu(client);
        }
    }
    // Client hit "Exit" button.
    else if (action == MenuAction_End)
    {
        CloseHandle(menu_infect_clients);
    }
    
    return 0;
}

bool InfectHasZombieSpawned()
{
    return g_bZombieSpawned;
}

/**
 * Returns if a client is infected.
 * 
 * @param client    The client index.
 * @return          True if the client has been infected, false otherwise.
 */    
bool InfectIsClientInfected(int client)
{
    // If client is invalid, then stop.
    if (!ZRIsClientValid(client))
    {
        return false;
    }
    
    // Return client's zombie flag.
    return bZombie[client];
}

/**
 * Returns if a client is a human.
 * 
 * @param client    The client index.
 * @return          True if the client is a human, false otherwise.
 */
bool InfectIsClientHuman(int client)
{
    // If client is invalid, then stop.
    if (!ZRIsClientValid(client))
    {
        return true;
    }
    
    // Return opposite of client's zombie flag.
    return !bZombie[client];
}

/**
 * Infecting a client manually (via zr_infect or the "Zombie Management" menu)
 * 
 * @param client            The client index infecting another client.
 * @param targets           Array containing all clients to infect.
 * @param count             The number of clients in the array.
 * @param respawnoverride   (Optional) True to override respawn cvar.
 * @param respawn           (Optional) True to respawn client on infect.
 */
void InfectManualInfect(int client, int[] targets, int count, bool respawnoverride = false, bool respawn = false)
{
    bool zombiespawned = InfectHasZombieSpawned();
    
    // If zombie hasn't spawned, then make targetted player(s) mother zombies.
    if (!zombiespawned)
    {
        // Stop mother infect timer.
        if (tInfect != null)
        {
            KillTimer(tInfect);
            tInfect = null;
        }
        
        // Move all clients to CT
        for (int x = 1; x <= MaxClients; x++)
        {
            // If client isn't in-game, then stop.
            if (!IsClientInGame(x))
            {
                continue;
            }
            
            // If client is dead, then stop.
            if (!IsPlayerAlive(x))
            {
                continue;
            }
            
            // Switch client to CT team.
            CS_SwitchTeam(x, CS_TEAM_CT);
        }
        
        // Tell the plugin a mother zombie has spawned.
        g_bZombieSpawned = true;
    }
    
    char targetname[MAX_NAME_LENGTH];
    
    // x = Client index.
    for (int x = 0; x < count; x++)
    {
        // Get client's name for later use.
        GetClientName(targets[x], targetname, sizeof(targetname));
        
        // Check if client is a human before turning into zombie.
        if (!InfectIsClientHuman(targets[x]))
        {
            // If there was only 1 player targetted, then let admin know the command was unsuccessful.
            if (count == 1)
            {
                // Tell admin command was unsuccessful.
                TranslationReplyToCommand(client, "Infect command infect unsuccessful", targetname);
            }
            
            continue;
        }
        
        // If zombie hasn't spawned, then make targetted player(s) mother zombies.
        if (!zombiespawned)
        {
            // Turn client into a mother zombie.
            InfectHumanToZombie(targets[x], _, true, respawnoverride, respawn);
            
            // If there was only 1 player targetted, then let admin know the outcome of the command.
            if (count == 1)
            {
                TranslationReplyToCommand(client, "Infect command infect mother successful", targetname);
            }
            
            continue;
        }
        
        // Turn client into a zombie.
        InfectHumanToZombie(targets[x], _, false, respawnoverride, respawn);
        
        // If there was only 1 player targetted, then let admin know the outcome of the command.
        if (count == 1)
        {
            TranslationReplyToCommand(client, "Infect command infect successful", targetname);
        }
    }
}

/**
 * Infecting a client manually (via zr_human or the "Zombie Management" menu)
 * 
 * @param client    The client index changing a zombie to human.
 * @param targets   Array containing all clients to make human.
 * @param count     The number of clients in the array.
 * @param respawn   (Optional) True to respawn client upon changing to human.
 * @param protect   (Optional) True to protect client upon changing to human.
 */
void InfectManualHuman(int client, int[] targets, int count, bool respawn = false, bool protect = false)
{
    char targetname[MAX_NAME_LENGTH];
    
    // x = Client index.
    for (int x = 0; x < count; x++)
    {
        // Get client's name for later use.
        GetClientName(targets[x], targetname, sizeof(targetname));
        
        // Check if client is a human before turning into zombie.
        if (InfectIsClientInfected(targets[x]))
        {
            // Turn client into a zombie.
            InfectZombieToHuman(targets[x], respawn, protect);
            
            // If there was only 1 player targetted, then let admin know the outcome of the command.
            if (count == 1)
            {
                // Tell admin command was successful.
                TranslationReplyToCommand(client, "Infect command human successful", targetname);
            }
        }
        else
        {
            // If there was only 1 player targetted, then let admin know the command was unsuccessful.
            if (count == 1)
            {
                // Tell admin command was unsuccessful.
                TranslationReplyToCommand(client, "Infect command human unsuccessful", targetname);
            }
        }
    }
}

/**
 * Command callback (zr_infect)
 * Infects a client.
 *   
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action InfectInfectCommand(int client, int argc)
{
    // Check if privileged.
    if (!ZRIsClientPrivileged(client, OperationType_Generic))
    {
        TranslationReplyToCommand(client, "No access to command");
        return Plugin_Handled;
    }
    
    // If not enough arguments given, then stop.
    if (argc < 1)
    {
        TranslationReplyToCommand(client, "Infect command infect syntax");
        return Plugin_Handled;
    }
    
    char target[MAX_NAME_LENGTH];
    char targetname[MAX_NAME_LENGTH];
    int targets[MAXPLAYERS];
    bool tn_is_ml;
    int result;
    
    // Get targetname.
    GetCmdArg(1, target, sizeof(target));
    
    // Find a target.
    result = ProcessTargetString(target, client, targets, sizeof(targets), COMMAND_FILTER_ALIVE , targetname, sizeof(targetname), tn_is_ml);
        
    // Check if there was a problem finding a client.
    if (result <= 0)
    {
        ZRReplyToTargetError(client, result);
        return Plugin_Handled;
    }
    
    // Get respawn parameter.
    char strRespawn[8];
    GetCmdArg(2, strRespawn, sizeof(strRespawn));
    
    bool respawnoverride;
    bool respawn;
        
    // If parameter exists then cast it into a bool and feed it to infect function.
    if (strRespawn[0])
    {
        respawnoverride = true;
        respawn = view_as<bool>(StringToInt(strRespawn));
    }
    
    // Infect player.
    InfectManualInfect(client, targets, result, respawnoverride, respawn);
    
    return Plugin_Handled;
}

/**
 * Command callback (zr_human)
 * Turns a client into a human.
 *   
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action InfectHumanCommand(int client, int argc)
{
    // Check if privileged.
    if (!ZRIsClientPrivileged(client, OperationType_Generic))
    {
        TranslationReplyToCommand(client, "No access to command");
        return Plugin_Handled;
    }
    
    // If not enough arguments given, then stop.
    if (argc < 1)
    {
        TranslationReplyToCommand(client, "Infect command human syntax");
        return Plugin_Handled;
    }
    
    char target[MAX_NAME_LENGTH];
    char targetname[MAX_NAME_LENGTH];
    int targets[MAXPLAYERS];
    bool tn_is_ml;
    int result;
    
    // Get targetname.
    GetCmdArg(1, target, sizeof(target));
    
    // Find a target.
    result = ProcessTargetString(target, client, targets, sizeof(targets), COMMAND_FILTER_ALIVE , targetname, sizeof(targetname), tn_is_ml);
        
    // Check if there was a problem finding a client.
    if (result <= 0)
    {
        ZRReplyToTargetError(client, result);
        return Plugin_Handled;
    }
    
    // Get respawn&protect parameters
    char strRespawn[8];
    char strProtect[8];
    GetCmdArg(2, strRespawn, sizeof(strRespawn));
    GetCmdArg(3, strProtect, sizeof(strProtect));
    
    // If parameter exists then cast it into a bool and feed it to "humanize" function.
    bool respawn = (strRespawn[0]) ? (view_as<bool>(StringToInt(strRespawn))) : false;
    bool protect = (strProtect[0]) ? (view_as<bool>(StringToInt(strProtect))) : false;
    
    // Turn client into human.
    InfectManualHuman(client, targets, result, respawn, protect);
    
    return Plugin_Handled;
}

/**
 * Converts a string to an infection mode.
 *
 * @param mode      Mode string to convert.
 *
 * @return          Infection mode or InfectMode_Invalid on error.
 */
InfectMode InfectStringToMode(char[] mode)
{
    if (strlen(mode) == 0)
    {
        return InfectMode_Invalid;
    }
    
    if (StrEqual(mode, "dynamic", false))
    {
        return InfectMode_Dynamic;
    }
    else if (StrEqual(mode, "absolute", false))
    {
        return InfectMode_Absolute;
    }
    else if (StrEqual(mode, "range", false))
    {
        return InfectMode_Range;
    }
    
    return InfectMode_Invalid;
}

/**
 * Gets and validates the infection mode. On error it will log a warning.
 *
 * @return  Infection mode or InfectMode_Invalid on error.
 */
InfectMode InfectGetModeOrFail()
{
    char modeName[16];
    GetConVarString(g_hCvarsList[CVAR_INFECT_MZOMBIE_MODE], modeName, sizeof(modeName));
    
    InfectMode mode = InfectStringToMode(modeName);
    int ratio = GetConVarInt(g_hCvarsList[CVAR_INFECT_MZOMBIE_RATIO]);
    int min = GetConVarInt(g_hCvarsList[CVAR_INFECT_MZOMBIE_MIN]);
    int max = GetConVarInt(g_hCvarsList[CVAR_INFECT_MZOMBIE_MAX]);
    
    // Validate.
    switch (mode)
    {
        case InfectMode_Invalid:
        {
            LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Infect, "Config Validation", "Warning: Invalid infection mode (\"%s\"). Falling back to one mother zombie.", modeName);
        }
        case InfectMode_Dynamic:
        {
            if (ratio < 0)
            {
                LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Infect, "Config Validation", "Warning: Invalid infection ratio (\"%d\"). Must be zero or positive in dynamic mode. Falling back to one mother zombie.", ratio);
                return InfectMode_Invalid;
            }
        }
        case InfectMode_Absolute:
        {
            if (ratio == 0)
            {
                LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Infect, "Config Validation", "Warning: Invalid infection ratio (\"%d\"). Must be nonzero in absolute mode. Falling back to one mother zombie.", ratio);
                return InfectMode_Invalid;
            }
        }
        case InfectMode_Range:
        {
            bool failed = false;
            if (min <= 0)
            {
                LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Infect, "Config Validation", "Warning: Invalid infection range (\"%d\"). Cvar zr_infect_mzombie_min must be nonzero and positive. Falling back to one mother zombie.", min);
                failed = true;
            }
            if (max <= 0)
            {
                LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Infect, "Config Validation", "Warning: Invalid infection range (\"%d\"). Cvar zr_infect_mzombie_max must be nonzero and positive. Falling back to one mother zombie.", max);
                failed = true;
            }
            if (min > max || max < min)
            {
                LogEvent(false, LogType_Error, LOG_CORE_EVENTS, LogModule_Infect, "Config Validation", "Warning: Infection range values are overlapping or reversed. Check zr_infect_mzombie_min and zr_infect_mzombie_min. Falling back to one mother zombie.");
                failed = true;
            }
            
            if (failed)
            {
                return InfectMode_Invalid;
            }
        }
    }
    
    return mode;
}

#pragma newdecls optional