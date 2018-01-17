/* Copyright (C) 2014 Wyplay, All Rights Reserved.
 * This source code and any compilation or derivative thereof is the
 * proprietary information of Wyplay and is confidential in nature.
 * Under no circumstances is this software to be exposed to or placed under
 * an Open Source License of any type without the expressed written permission
 * of Wyplay. */

#include "lxc_genesis.h"

__attribute__((constructor(101))) void lxc_genesis_setup_constructor () {
    lxc_genesis_setup();
}
