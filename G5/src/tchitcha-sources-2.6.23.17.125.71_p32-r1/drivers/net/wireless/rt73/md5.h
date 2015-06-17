/***************************************************************************
 * RT2x00 SourceForge Project - http://rt2x00.serialmonkey.com             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   Licensed under the GNU GPL                                            *
 *   Original code supplied under license from RaLink Inc, 2004.           *
 ***************************************************************************/

/***************************************************************************
 *	Module Name:	md5.h
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *
 ***************************************************************************/

#ifndef MD5_H
#define MD5_H

#define MD5_MAC_LEN 16
#define SHA_DIGEST_LEN 20

struct MD5Context {
    u32 buf[4];
    u32 bits[2];
    u8 in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char *buf, unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(u32 buf[4], u32 in[16]);

typedef struct MD5Context MD5_CTX;


void md5_mac(u8 *key, size_t key_len, u8 *data, size_t data_len, u8 *mac);
void hmac_md5(u8 *key,  size_t key_len, u8 *data, size_t data_len, u8 *mac);

//
// SHA context
//
typedef struct
{
	ULONG		H[5];
	ULONG		W[80];
	INT 		lenW;
	ULONG		sizeHi, sizeLo;
}	SHA_CTX;

VOID 	SHAInit(SHA_CTX *ctx);
VOID 	SHAHashBlock(SHA_CTX *ctx);
VOID	SHAUpdate(SHA_CTX *ctx, unsigned char *dataIn, int len);
VOID	SHAFinal(SHA_CTX *ctx, unsigned char hashout[20]);

#endif // MD5_H



#ifndef _AES_H
#define _AES_H

typedef struct
{
    uint32 erk[64];     // encryption round keys
    uint32 drk[64];     // decryption round keys
    int nr;             // number of rounds
}aes_context;

int  aes_set_key( aes_context *ctx, uint8 *key, int nbits );
void aes_encrypt( aes_context *ctx, uint8 input[16], uint8 output[16] );
void aes_decrypt( aes_context *ctx, uint8 input[16], uint8 output[16] );

void hmac_sha1(unsigned char *text, int text_len, unsigned char *key, int key_len, unsigned char *digest);
void F(char *password, unsigned char *ssid, int ssidlength, int iterations, int count, unsigned char *output);
int PasswordHash(char *password, unsigned char *ssid, int ssidlength, unsigned char *output);

#endif // aes.h


