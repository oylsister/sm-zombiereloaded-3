/*
 * ============================================================================
 *
 *  Zombie:Reloaded
 *
 *  File:          commands.inc
 *  Type:          Core
 *  Description:   Console command creation and hooking.
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

/**
 * Commands are created/hooked.
 */
void CommandsInit()
{
    // Forward event to modules. (create commands)
    ConfigOnCommandsCreate();
    LogOnCommandsCreate();
    ClassOnCommandsCreate();
    WeaponsOnCommandsCreate();
    HitgroupsOnCommandsCreate();
    InfectOnCommandsCreate();
    MenuOnCommandsCreate();
    ZAdminOnCommandsCreate();
    AntiStickOnCommandsCreate();
    ZCookiesOnCommandsCreate();
    ZSpawnOnCommandsCreate();
    ZTele_OnCommandsCreate();
    ZHPOnCommandsCreate();
    VolOnCommandsCreate();
    ZombieSoundsOnCommandsCreate();
    ImmunityOnCommandsCreate();
    
    #if defined ADD_VERSION_INFO
        VersionOnCommandsCreate();
    #endif
    
    DebugOnCommandsCreate();
    
    // Forward event to modules. (hook commands)
    ClassOnCommandsHook();
}
