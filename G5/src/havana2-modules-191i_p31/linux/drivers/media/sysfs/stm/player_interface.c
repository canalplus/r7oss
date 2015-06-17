/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : player_interface.c - player interface
Author :           Julian


Date        Modification                                    Name
----        ------------                                    --------
28-Apr-08   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/mutex.h>

#include "sysfs_module.h"
#include "player_sysfs.h"
#include "player_interface.h"
#include "player_interface_ops.h"

/* The player interface context provides access to the STM streaming engine (Player2) */
struct PlayerInterfaceContext_s
{
    char*                               Name;
    struct player_interface_operations* Ops;
};

/*{{{  register_player_interface*/
static struct PlayerInterfaceContext_s*         PlayerInterface;

int register_player_interface  (char*                                   Name,
                                struct player_interface_operations*     Ops)
{
    if (PlayerInterface == NULL)
    {
        INTERFACE_ERROR ("Cannot register player interface  %s - not created\n", Name);
        return -ENOMEM;
    }
    PlayerInterface->Ops        = Ops;
    PlayerInterface->Name       = kmalloc (strlen (Name) + 1,  GFP_KERNEL);
    if (PlayerInterface->Name != NULL)
        strcpy (PlayerInterface->Name, Name);

    SysfsInit   ();     /* This must be done after the player has registered itself */

    return 0;

}
EXPORT_SYMBOL(register_player_interface);
/*}}}  */

/*{{{  PlayerInterfaceInit*/
int PlayerInterfaceInit (void)
{
    PlayerInterface     = kmalloc (sizeof (struct PlayerInterfaceContext_s), GFP_KERNEL);
    if (PlayerInterface == NULL)
    {
        INTERFACE_ERROR("Unable to create player interface context - no memory\n");
        return -ENOMEM;
    }

    memset (PlayerInterface, 0, sizeof (struct PlayerInterfaceContext_s));

    PlayerInterface->Ops       = NULL;

    return 0;
}
/*}}}  */
/*{{{  PlayerInterfaceDelete*/
int PlayerInterfaceDelete ()
{
    if (PlayerInterface->Name != NULL)
        kfree (PlayerInterface->Name);
    PlayerInterface->Name       = NULL;

    kfree (PlayerInterface);
    PlayerInterface     = NULL;
    return 0;
}
/*}}}  */

/*{{{  ComponentGetAttribute*/
int ComponentGetAttribute      (player_component_handle_t       Component,
                                const char*                     Attribute,
                                union attribute_descriptor_u*   Value)
{
    int         Result  = 0;

    Result = PlayerInterface->Ops->component_get_attribute (Component, Attribute, Value);
    if (Result < 0)
        INTERFACE_ERROR("component_get_attribute failed\n");

    return Result;
}
/*}}}  */
/*{{{  ComponentSetAttribute*/
int ComponentSetAttribute      (player_component_handle_t       Component,
                                const char*                     Attribute,
                                union attribute_descriptor_u*   Value)
{
    int         Result  = 0;

    INTERFACE_DEBUG("\n");

    Result = PlayerInterface->Ops->component_set_attribute (Component, Attribute, Value);
    if (Result < 0)
        INTERFACE_ERROR("component_set_attribute failed\n");

    return Result;
}
/*}}}  */
/*{{{  PlayerRegisterEventSignalCallback*/
player_event_signal_callback PlayerRegisterEventSignalCallback (player_event_signal_callback    Callback)
{
    return PlayerInterface->Ops->player_register_event_signal_callback (Callback);
}
/*}}}  */

