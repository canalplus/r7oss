#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/string.h>
#include <linux/bpa2.h>
#include <linux/crc32.h>

#define BUFFER_SIZE  512

struct file *Filep;
char RWBuffer[BUFFER_SIZE];
bool GenerateCrc ;
unsigned long reference_crc[BUFFER_SIZE];
int ref_number = 0;
int check_number = 0;

/* create/parse reference file and initialise global variables*/
/* This has to be called when initialising the test           */
int init_crc(char *filename , int generate)
{
  mm_segment_t old_fs;
  int res = 0;
  unsigned long crc_value = 0;

  GenerateCrc = !!generate;
  old_fs = get_fs ();
  set_fs (KERNEL_DS);

  Filep = filp_open (filename, GenerateCrc ? (O_RDWR | O_CREAT) : O_RDONLY, 0);
  if (IS_ERR (Filep))
  {
    res = PTR_ERR (Filep);
    Filep = NULL;
    printk("BLITTER_CRC: open references' file failed !\n");
    set_fs (old_fs);
    return res;
  }

  /* get references if !generating */
  if (!GenerateCrc)
  {
    while(Filep->f_op->read (Filep, RWBuffer, BUFFER_SIZE , &Filep->f_pos))
    {
      sscanf(RWBuffer, "CRC: %lx\n", &crc_value);
      reference_crc[ref_number] = crc_value;
      ref_number++;
    }
  }

  set_fs (old_fs);
  return res;
}
EXPORT_SYMBOL(init_crc);


/* compute crc values and compare/update references */
int check_crc(void * buffer, size_t size )
{
  unsigned long computed_crc = 0;
  mm_segment_t old_fs;

  if (!Filep)
    return -1;

  old_fs = get_fs ();
  set_fs (KERNEL_DS);
  computed_crc = crc32(0 , buffer, size);

  if(GenerateCrc)
  {
    sprintf(RWBuffer, "CRC: %lx\n", computed_crc);
    Filep->f_op->write (Filep, RWBuffer, BUFFER_SIZE , &Filep->f_pos);
  }
  else
  {
    if (computed_crc == reference_crc[check_number])
      printk("BLITTER_CRC: computed crc %lx is equal to ref_crc %lx \n",
             computed_crc, reference_crc[check_number]);
    else
      printk("BLITTER_CRC: computed crc %lx is NOT equal to ref_crc %lx !\n",
             computed_crc, reference_crc[check_number]);
    check_number++;
  }

  set_fs (old_fs);
  return 0;
}
EXPORT_SYMBOL(check_crc);


int term_crc(void)
{
  mm_segment_t old_fs;

  old_fs = get_fs ();
  set_fs (KERNEL_DS);
  if (Filep != NULL)
  {
    filp_close (Filep, NULL);
    Filep = NULL;
  }
  GenerateCrc = 0;
  ref_number = 0;
  check_number = 0;
  set_fs (old_fs);
return 0;
}
EXPORT_SYMBOL(term_crc);

MODULE_LICENSE("GPL");
