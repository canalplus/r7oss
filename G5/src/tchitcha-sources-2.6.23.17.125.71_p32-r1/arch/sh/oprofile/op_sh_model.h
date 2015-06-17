/**
 * @file op_sh_model.h
 * interface to SH machine specific operations
 *
 * @remark Copyright 2007 STMicroelectronics
 * @remark Read the file COPYING
 *
 * @author Dave Peverley
 */

#ifndef OP_SH_MODEL_H
#define OP_SH_MODEL_H

struct op_sh_model_spec {
        int (*init)(void);
        unsigned int num_counters;
        int (*setup_ctrs)(void);
        int (*start)(void);
        void (*stop)(void);
        char *name;
};

#if defined(CONFIG_OPROFILE_TMU)
extern struct op_sh_model_spec op_sh7109_spec;
#else
extern struct op_sh_model_spec op_shtimer_spec;
#endif

extern void sh_backtrace(struct pt_regs * const regs, unsigned int depth);


#endif /* OP_SH_MODEL_H */
