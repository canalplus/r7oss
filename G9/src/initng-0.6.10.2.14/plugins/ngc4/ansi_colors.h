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

#ifndef ANSI_COLORS_H
#define ANSI_COLORS_H

/* reset */
#define C_OFF "\033[0m"

/* Special */
#define C_BOLD_ON "\033[1m"
#define C_ITALICS_ON "\033[3m"
#define C_UNDERLINE_ON "\033[4m"
#define C_INVERSE_ON "\033[7m"
#define C_STRIKETROUTH_ON "\033[9m"
#define C_BOLD_OFF "\033[22m"
#define C_ITALICS_OFF "\033[27m"
#define C_UNDERLINE_OFF "\033[24m"
#define C_INVERSE_OFF "\033[27m"
#define C_STRIKETROUTH_OFF "\033[29m"

/* foreground colors */
#define C_FG_BLACK "\033[30m"
#define C_FG_RED "\033[31m"
#define C_FG_GREEN "\033[32m"
#define C_FG_YELLOW "\033[33m"
#define C_FG_BLUE "\033[34m"
#define C_FG_MAGENTA "\033[35m"
#define C_FG_CYAN "\033[36m"
#define C_FG_WHITE "\033[37m"
#define C_FG_DEFAULT "\033[39m"
#define C_FG_LIGHT_BLUE "\033[34;01m"
#define C_FG_NEON_GREEN "\033[32;01m"
#define C_FG_LIGHT_RED "\033[01;31m"

/* background colors  */
#define C_BG_BLACK "\033[40m"
#define C_BG_RED "\033[41m"
#define C_BG_GREEN "\033[42m"
#define C_BG_YELLOW "\033[43m"
#define C_BG_BLUE "\033[44m"
#define C_BG_MAGENTA "\033[45m"
#define C_BG_CYAN "\033[46m"
#define C_BG_WHITE "\033[47m"
#define C_BG_DEFAULT "\033[49m"

/* movements */
#define MOVE_TO_R "\033[36G"
#define MOVE_TO_P "\033[A\033[90G"

#endif
