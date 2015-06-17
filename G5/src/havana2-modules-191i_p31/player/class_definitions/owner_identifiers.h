/************************************************************************
Copyright (C) 2006 STMicroelectronics. All Rights Reserved.

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

Source file name : owner_identifiers.h
Author :           Nick

A list of pre-defined owner identifiers used to record the placement of buffers



Date        Modification                                    Name
----        ------------                                    --------
17-Nov-06   Created                                         Nick

************************************************************************/

#ifndef H_OWNER_IDENTIFIERS
#define H_OWNER_IDENTIFIERS

enum
{
    IdentifierGetInjectBuffer	= 1,
    IdentifierInSequenceCall,
    IdentifierDrain,

    IdentifierCollator,
    IdentifierFrameParser,
    IdentifierCodec,
    IdentifierManifestor,

    IdentifierProcessCollateToParse,
    IdentifierProcessParseToDecode,
    IdentifierProcessDecodeToManifest,
    IdentifierProcessPostManifest,

    IdentifierAttachedToOtherBuffer,
    IdentifierReverseDecodeStack,
    IdentifierFrameParserMarkerFrame,
    IdentifierNonDecodedFrameList,

    IdentifierH264Intermediate,
    IdentifierExternal,

    IdentifierManifestorClone
};

#endif



