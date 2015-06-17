/* Based on :
   zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>

#include "dsmcc-compress.h"
#include "dsmcc-debug.h"

#define CHUNK 16384

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
static int inf(FILE *source, FILE *dest)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit(&strm);
	if (ret != Z_OK)
		return ret;

	/* decompress until deflate stream ends or end of file */
	do
	{
		strm.avail_in = fread(in, 1, CHUNK, source);
		if (ferror(source))
		{
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do
		{
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret)
			{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;
					/* no break */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&strm);
					return ret;
			}
			have = CHUNK - strm.avail_out;
			if (fwrite(out, 1, have, dest) != have || ferror(dest))
			{
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* wlog a zlib or i/o error */
static void log_zerr(int ret, FILE *input, FILE *output)
{
	switch (ret)
	{
		case Z_ERRNO:
			if (ferror(input))
				DSMCC_ERROR("Error reading compressed file");
			if (ferror(output))
				DSMCC_ERROR("Error writing uncompressed file");
			break;
		case Z_STREAM_ERROR:
			DSMCC_ERROR("Invalid compression level");
			break;
		case Z_DATA_ERROR:
			DSMCC_ERROR("Invalid or incomplete deflate data");
			break;
		case Z_MEM_ERROR:
			DSMCC_ERROR("Out of memory");
			break;
		case Z_VERSION_ERROR:
			DSMCC_ERROR("zlib version mismatch");
			break;
		default:
			DSMCC_ERROR("Unexpected zlib error %d", ret);
			break;
	}
}

bool dsmcc_inflate_file(const char *filename)
{
	FILE *input, *output;
	char *tmpfilename;
	int ret, tmp;

	tmpfilename = malloc(strlen(filename) + 8);
	sprintf(tmpfilename, "%s.XXXXXX", filename);

	input = fopen(filename, "r");

	tmp = mkstemp(tmpfilename);
	output = fdopen(tmp, "w");

	DSMCC_DEBUG("Uncompressed file %s to %s", filename, tmpfilename);

	ret = inf(input, output);
	if (ret != Z_OK)
		log_zerr(ret, input, output);

	fclose(input);
	fclose(output);

	if (ret == Z_OK)
	{
		DSMCC_DEBUG("Renaming uncompressed file from %s to %s", tmpfilename, filename);
		rename(tmpfilename, filename);
		return 1;
	}
	else
	{
		unlink(tmpfilename);
		return 0;
	}
}
