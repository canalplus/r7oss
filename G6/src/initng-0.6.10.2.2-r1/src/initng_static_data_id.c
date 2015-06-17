/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include "initng_active_db.h"
#include "initng_static_data_id.h"
#include "initng_service_data_types.h"

/*
 * Here is where we define, the default options, and
 * relate to, by pointers
 * 
 * Description:
 * { "name_of_opt", TYPE_OF_OPT, STRING_LEN_OF("name_of_opt") }
 */
s_entry USE = { "use", STRINGS, NULL,
	"Need to be up if they are in this runlevel."
};
s_entry NEED = { "need", STRINGS, NULL, "Need to be up, ignore if not found."
};
s_entry REQUIRE = { "require", STRINGS, NULL, "Need to be up, fail if not found." };
s_entry FROM_FILE = { "from_file", STRING, NULL, NULL };
s_entry ENV = { "env", VARIABLE_STRING, NULL, "Sets an environmental variable."
};
s_entry RESTARTING = { "internal_restarting", SET, NULL, NULL };

/*
 * add some default options, that is needed by core, and should
 * be in option_db by default.
 * This function is called, from main(), initialization.
 */
void initng_static_data_id_register_defaults(void)
{
	initng_service_data_type_register(&USE);
	initng_service_data_type_register(&NEED);
	initng_service_data_type_register(&REQUIRE);
	initng_service_data_type_register(&FROM_FILE);
	initng_service_data_type_register(&ENV);

	initng_service_data_type_register(&RESTARTING);

}
