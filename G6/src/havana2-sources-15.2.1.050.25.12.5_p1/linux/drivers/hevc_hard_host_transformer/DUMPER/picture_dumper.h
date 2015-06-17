#ifndef _PICTURE_DUMPER_H
#define _PICTURE_DUMPER_H

void O4_dump(const char* dirname, unsigned int decode_index, void* luma, void* chroma, unsigned int luma_size, unsigned int width, unsigned int height);
void R2B_dump(const char* dirname, int decimated, unsigned int decode_index, void* luma, void* chroma, unsigned int width, unsigned int height, unsigned int stride);
void buffer_dump(const char* dirname, const char* buffername, void* buffer, unsigned int bytes);
#endif
