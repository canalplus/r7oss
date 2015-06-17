/*
 *  linux/fs/binfmt_capwrap.c
 *
 *  Copyright (C) 2002  Neil Schemenauer
 *  Copyright (C) 2010  Wyplay SAS
 *  Released under GPL v2.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/binfmts.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/smp_lock.h>
#include <linux/mount.h>
#include <linux/fs.h>


static int parse_cap(char **cp, unsigned int *cap)
{
	char *tok, *endp;
	int n;
	if ((tok = strsep(cp, " \t")) == NULL)
		return 0;
	n = simple_strtol(tok, &endp, 16);
	if (*endp != '\0')
		return 0;
	*cap = n;
	return 1;
}

static void grant_capabilities(struct linux_binprm *bprm, unsigned int caps)
{
	/* This may have to be more complicated if the kernel
	 * representation of capabilities changes.  Right now it's trivial.
	 */
	bprm->cap_effective |= caps;
	bprm->cap_permitted |= caps;
}

static int load_capwrap(struct linux_binprm *bprm, struct pt_regs *regs)
{
        struct inode * inode = bprm->file->f_dentry->d_inode;
	int mode = inode->i_mode;
	char exec_name[BINPRM_BUF_SIZE];
	char *cp, *tok;
	unsigned int caps = 0;
	struct file *file;
	int retval;

	/* must have magic */
	if ((bprm->buf[0] != '&'))
		return -ENOEXEC;

	/* must be owned by root */
	if (inode->i_uid != 0)
		return -ENOEXEC;

	/* must be SUID */
	if ((bprm->file->f_vfsmnt->mnt_flags & MNT_NOSUID) || !(mode & S_ISUID))
		return -ENOEXEC;

	/*
	 * Okay, parse the wrapper.
	 */

	allow_write_access(bprm->file);
	fput(bprm->file);
	bprm->file = NULL;

	/* terminate first line */
	bprm->buf[BINPRM_BUF_SIZE - 1] = '\0';
	if ((cp = strchr(bprm->buf, '\n')) == NULL)
		cp = bprm->buf+BINPRM_BUF_SIZE-1;
	*cp = '\0';

	/* name */
	cp = bprm->buf+1;
	if ((tok = strsep(&cp, " \t")) == NULL)
		return -ENOEXEC;
	strcpy(exec_name, tok);

	/* capabilities to add */
	if (!parse_cap(&cp, &caps))
		return -ENOEXEC;

	/* Restart the process with real executable's dentry. */
	file = open_exec(exec_name);
	if (IS_ERR(file))
		return PTR_ERR(file);

	/* prevent infinite recursion */
        if (file->f_dentry->d_inode == inode) {
		allow_write_access(file);
		fput(file);
		return -ENOEXEC;
	}

	bprm->file = file;
	retval = prepare_binprm(bprm);
	if (retval < 0) {
		allow_write_access(file);
		fput(file);
		return retval;
	}

	grant_capabilities(bprm, caps);

#ifdef CAPWRAP_DEBUG
	printk(KERN_DEBUG "capwrap: granted %s %x effective now %x\n", exec_name, caps, bprm->cap_effective);
#endif

	return search_binary_handler(bprm, regs);
}

struct linux_binfmt capwrap_format = {
	.module		= THIS_MODULE,
	.load_binary	= load_capwrap,
};

static int __init init_capwrap_binfmt(void)
{
	return register_binfmt(&capwrap_format);
}

static void __exit exit_capwrap_binfmt(void)
{
	unregister_binfmt(&capwrap_format);
}

core_initcall(init_capwrap_binfmt);
module_exit(exit_capwrap_binfmt);
MODULE_LICENSE("GPL");

