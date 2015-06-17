// Picture Dump to filesystem
// dumps Omega4 and R2B pictures to disk

#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>

#include "picture_dumper.h"

struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    // int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
	// err = PTR_ERR(filp);
	return NULL;
    }
    return filp;
}

static int file_write(struct file* file, unsigned long long offset, void* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, (unsigned char*)data, size, &offset);

    set_fs(oldfs);
    return ret;
}

/*
static int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}
*/

static void file_close(struct file* file) {
    filp_close(file, NULL);
}

static void file_append(struct file* file, unsigned int* offset, void* data, unsigned int size)
{
	file_write(file, *offset, data, size);
	(*offset) += size;
}

void O4_dump(const char* dirname, unsigned int decode_index, void* luma, void* chroma, unsigned int luma_size, unsigned int width, unsigned int height)
{
	char filename[256];
	struct file* file;
	unsigned int word;
	unsigned int chroma_size = luma_size / 2;
	unsigned int offset = 0;

	snprintf(filename, sizeof(filename), "%s/%d.O4.gam", dirname, decode_index);
	filename[sizeof(filename) - 1] = '\0';

	file = file_open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	if (!file)
	{
		pr_err("Error: O4_dump: failed to open %s for writing\n", filename);
		return;
	}

	// Write Header
	word = 0x420F0006; // 4:2:0 tag | Size of header in words
	file_append(file, &offset, &word, 4);
	word = 0x001100A0; // Aspect ratio (16:9) | Omega4 tag
	file_append(file, &offset, &word, 4);
	file_append(file, &offset, &width, 4);
	file_append(file, &offset, &height, 4);
	file_append(file, &offset, &luma_size, 4);
	file_append(file, &offset, &chroma_size, 4);

	// Pixels
	file_append(file, &offset, luma,   luma_size);
	file_append(file, &offset, chroma, chroma_size);

	file_close(file);
}

void R2B_dump(const char* dirname, int decimated, unsigned int decode_index, void* luma, void* chroma, unsigned int width, unsigned int height, unsigned int stride)
{
	char filename[256];
	struct file* file;
	unsigned int word;
	unsigned int luma_size, chroma_size;
	unsigned int offset = 0;

	snprintf(filename, sizeof(filename), "%s/%d.R2B.%s.gam", dirname, decode_index, decimated ? "dec" : "full");
	filename[sizeof(filename) - 1] = '\0';

	file = file_open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	if (!file)
	{
		pr_err("Error: R2B_dump: failed to open %s for writing\n", filename);
		return;
	}

	// Write Header
	word = 0x420F0006; // 4:2:0 tag | Size of header in words
	file_append(file, &offset, &word, 4);
	word = 0x001100B0; // Aspect ratio (16:9) | R2B tag
	file_append(file, &offset, &word, 4);
	word = (stride << 16) | width;
	file_append(file, &offset, &word, 4);
	file_append(file, &offset, &height, 4);
	luma_size = stride * height;
	chroma_size = luma_size / 2;
	file_append(file, &offset, &luma_size, 4);
	file_append(file, &offset, &chroma_size, 4);

	// Pixels
	file_append(file, &offset, luma,   luma_size);
	file_append(file, &offset, chroma, chroma_size);

	file_close(file);
}

void buffer_dump(const char* dirname, const char* buffername, void* buffer, unsigned int bytes)
{
	char filename[256];
	struct file* file;
	static int id = 0;

	snprintf(filename, sizeof(filename), "%s/%s.%d.bin", dirname, buffername, id++);
	filename[sizeof(filename) - 1] = '\0';

	file = file_open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
	if (!file)
	{
		pr_err("Error: buffer_dump: failed to open %s for writing\n", filename);
		return;
	}
	file_write(file, 0, buffer, bytes);
	file_close(file);
}
