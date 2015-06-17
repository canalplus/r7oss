/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cectest/cec_test_interface.c
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/*
   @file     cec_test_interface.c
   @brief

 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <vibe_debug.h>

#include "cec_debug.h"
#include "cec_core.h"
#include "cec_test_interface.h"
#include "cec_token.h"

static ssize_t
cec_test_sysfs_show(struct kobject *,
            struct attribute * ,
            char *);
static ssize_t
cec_test_sysfs_store(struct kobject *,
            struct attribute * ,
            const char *,
            size_t );

static cec_test_interface_q_t *interface_param_q_head_p=NULL;
static cec_test_interface_q_t *interface_param_data_q_head_p=NULL;

static struct rt_mutex     *lock_test_q_param_p;

void * cec_allocate (unsigned int size)
{
    void *p;
     if( size <= 128*1024 )
        p = (void *)kmalloc( size, GFP_KERNEL );
    else
        p = vmalloc( size );

    if(p)
        memset(p,0,size);

    return p;
}

int cec_free(void *Address)
{
    unsigned long  Addr = (unsigned long)Address;

     if( Address == NULL ){
        cec_error_trace(CEC_API,
                "Attempt to free null pointer>> %s@%d\n",__FUNCTION__,
                __LINE__);
        return -EINVAL;
    }
    if( (unsigned int)Addr >= VMALLOC_START && (unsigned int)Addr < VMALLOC_END ){
        /* We found the address in the table, it is a vmalloc allocation */
        vfree(Address);
    }
    else{
        /* Address not found... trying kfree ! */
        kfree(Address);
    }

    return 0;
}

int   cec_thread_create(struct task_struct  **thread,
                   void (*task_entry)(void* param),
                   void * parameter,
                   const char *name,
                   int        *thread_settings)
{
    int                 sched_policy;
    struct sched_param  sched_param;
    struct task_struct      *taskp;

    /* create the task and leave it suspended */
    taskp = kthread_create((int(*)(void *))task_entry, parameter, name);
    if (IS_ERR(taskp))
        return -ENOMEM;

    taskp->flags |= PF_NOFREEZE;

    /* Set thread scheduling settings */
    sched_policy               = thread_settings[0];
    sched_param.sched_priority = thread_settings[1];
    if ( sched_setscheduler(taskp, sched_policy, &sched_param) )
    {
        TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
                name, sched_policy, sched_param.sched_priority);
    }

    wake_up_process(taskp);

    *thread = taskp;

    return 0;
}

int  cec_wait_thread(struct task_struct **task)
{

    if (NULL == *task) {
        cec_error_trace(CEC_API,"FATAL ERROR: Task cannot be null\n");
        return -EINVAL;
    }
    kthread_stop(*task);

    return 0;
}

void cec_sleep_milli_sec( unsigned int Value )
{
    sigset_t allset, oldset;
    unsigned int  Jiffies = ((Value*HZ)/1000)+1;

    /* Block all signals during the sleep (i.e. work correctly when called via ^C) */
    sigfillset(&allset);
    sigprocmask(SIG_BLOCK, &allset, &oldset);

    /* Sleep */
    set_current_state( TASK_INTERRUPTIBLE );
    schedule_timeout( Jiffies );

    /* Restore the original signal mask */
    sigprocmask(SIG_SETMASK, &oldset, NULL);
}

int cec_q_remove_node(cec_q_t **head_p, uint32_t key, cec_q_t **node_p)
{
    cec_q_t         *cur_p = NULL;
    cec_q_t         *prev_p = NULL;
    int     error = -EINVAL;

    if(*head_p == NULL){
        return -EINVAL;
    }

    cur_p = *head_p;
    prev_p = *head_p;

    /*Head is the node , we are looking for*/
    if(key == cur_p->key){
        /*Update the head*/
        *node_p = cur_p;
        *head_p = cur_p->next_p;
        return 0;
    }
    while(cur_p != NULL){
        if(cur_p->key == key){
            *node_p = cur_p;
            prev_p->next_p = cur_p->next_p;
            error = 0;
            break;
        }
        prev_p = cur_p;
        cur_p = cur_p->next_p;
    }

    return error;
}

int cec_q_insert_node(cec_q_t **head_p, cec_q_t *node_p, uint32_t key, void *data_p)
{
    cec_q_t     *cur_p = NULL;

    node_p->next_p =  NULL;

    /*This is the first element*/
    if(*head_p == NULL){
        *head_p = node_p;
        node_p->key = key;
        node_p->data_p = data_p;
        return 0;
    }

    cur_p = *head_p;

    while(cur_p->next_p != NULL){
        cur_p = cur_p->next_p;
    }

    cur_p->next_p = node_p;
    node_p->key = key;
    node_p->data_p = data_p;

    return 0;
}

int cec_test_deregister_cmd(cec_test_reg_param_t *cmd_param_p)
{
    int         error = 0;
    cec_test_interface_q_t          *interface_q_param_p=NULL;
    cec_test_interface_param_t      *cur_interface_param_p=NULL;

    cec_test_debug(">>> %s@%d >>> \n", __func__, __LINE__);
    if (rt_mutex_lock_interruptible(lock_test_q_param_p, true) == -EDEADLK)
        cec_error_trace(CEC_API, "%p will cause a deadlock\n", lock_test_q_param_p);
    error = cec_q_remove_node(
        &(interface_param_q_head_p),
        (uint32_t)(cmd_param_p->interface_func_p),
        &interface_q_param_p);
    cur_interface_param_p = CEC_Q_GET_DATA(
        interface_q_param_p,
        cec_test_interface_param_t);
    cec_free(cur_interface_param_p);
    rt_mutex_unlock(lock_test_q_param_p);
    cec_test_debug("<<< %s@%d <<< \n", __func__, __LINE__);

    return error;
}


int cec_test_register_cmd(cec_test_reg_param_t *cmd_param_p)
{
    cec_test_interface_param_t          *interface_param_p;

    cec_test_debug(">>> %s@%d >>> \n", __func__, __LINE__);
    interface_param_p = (cec_test_interface_param_t*)
                cec_allocate(sizeof(*interface_param_p));
    if(NULL==interface_param_p){
        cec_test_print("%s@%d: Error in command registration\n",
                __func__,
                __LINE__);
        return -ENOMEM;
    }

    memset((void*)interface_param_p->func_str,
            '\0',
            CEC_TEST_MAX_STR_LEN);
    memset((void*)interface_param_p->info,
            '\0',
            CEC_TEST_MAX_CMD_DESC_LEN);
    strncpy(interface_param_p->func_str ,
            cmd_param_p->func_str,
            (CEC_TEST_MAX_STR_LEN-1));
    strncpy(interface_param_p->info,
            cmd_param_p->info,
            (CEC_TEST_MAX_CMD_DESC_LEN-1));
    interface_param_p->interface_func_p = cmd_param_p->interface_func_p;
    interface_param_p->user_data_p = cmd_param_p->user_data_p;

    cec_test_debug("%15s:%d - %s\n",
            interface_param_p->func_str,
            strlen(interface_param_p->func_str),
            interface_param_p->info);


    if (rt_mutex_lock_interruptible(lock_test_q_param_p, true) == -EDEADLK)
        cec_error_trace(CEC_API, "%p will cause a deadlock\n", lock_test_q_param_p);
    cec_q_insert_node(&(interface_param_q_head_p),
            &(interface_param_p->interface_q_node),
            (uint32_t)interface_param_p->interface_func_p,
            (void*)interface_param_p);
    rt_mutex_unlock(lock_test_q_param_p);
    cec_test_debug("<<< %s@%d <<< \n", __func__, __LINE__);

    return 0;

}


int cec_test_register_param(cec_test_param_t *int_param_p)
{
    cec_test_interface_param_t  *interface_param_p;

    cec_test_debug("\n>>> %s@%d >>> \n", __func__, __LINE__);
    interface_param_p = (cec_test_interface_param_t*)cec_allocate(sizeof(*interface_param_p));
    if(NULL==interface_param_p){
        cec_test_print("%s@%d: Error in Parameter registration\n",__func__,__LINE__);
        return -ENOMEM;
    }

    memset((void*)interface_param_p->func_str,
            '\0',
            CEC_TEST_MAX_STR_LEN);
    memset((void*)interface_param_p->info,
            '\0',
            CEC_TEST_MAX_CMD_DESC_LEN);
    strncpy(interface_param_p->func_str ,
            int_param_p->param_str,
            (CEC_TEST_MAX_STR_LEN-1));
    interface_param_p->user_data_p = (int *)cec_allocate(sizeof(int));

    *(int *)(interface_param_p->user_data_p) = *(int *)(int_param_p->user_data_p);

    cec_test_debug("\nInserting %15s Length :%d - Value : %d\n",
            interface_param_p->func_str,
            strlen(interface_param_p->func_str),
            *(int *)interface_param_p->user_data_p);



    if (rt_mutex_lock_interruptible(lock_test_q_param_p, true) == -EDEADLK)
        cec_error_trace(CEC_API, "%p will cause a deadlock\n", lock_test_q_param_p);
    cec_q_insert_node(&(interface_param_data_q_head_p),
            &(interface_param_p->interface_q_node),
            (uint32_t)&(interface_param_p->interface_q_node),
            (void*)interface_param_p);
    rt_mutex_unlock(lock_test_q_param_p);

    cec_test_debug("\n<<< %s@%d <<< \n\n", __func__, __LINE__);

    return 0;

}


int cec_test_deregister_param(cec_test_param_t *int_param_p)
{
    int error = 0;
    cec_test_interface_q_t *cur_interface_q_param_p;
    cec_test_interface_param_t *cur_interface_param_p;
    int cmp_result=0;
    uint8_t node_no = 0;

    cec_test_debug("\n>>> %s@%d >>> \n", __func__, __LINE__);
    cur_interface_q_param_p = interface_param_data_q_head_p;

    if (rt_mutex_lock_interruptible(lock_test_q_param_p, true) == -EDEADLK)
        cec_error_trace(CEC_API, "%p will cause a deadlock\n", lock_test_q_param_p);

    while(cur_interface_q_param_p != NULL){

        cur_interface_param_p = CEC_Q_GET_DATA(
                cur_interface_q_param_p,
                cec_test_interface_param_t);

        cmp_result = strcmp(int_param_p->param_str,
                cur_interface_param_p->func_str);

        if(!(cmp_result)){

            error = cec_q_remove_node(
                    &(interface_param_data_q_head_p),
                    (uint32_t)&(cur_interface_param_p->interface_q_node),
                    &cur_interface_q_param_p);

            cec_test_debug("%15s:%d - %s Error : %d \n",
                    cur_interface_param_p->func_str,
                    strlen(cur_interface_param_p->func_str),
                    cur_interface_param_p->info, error);

            cec_free(cur_interface_param_p->user_data_p);

            cec_free(cur_interface_param_p);

            break;
        }

        cur_interface_q_param_p = CEC_Q_GET_NEXT(
                (&(cur_interface_param_p->interface_q_node))
                );
    }

    cur_interface_q_param_p = interface_param_data_q_head_p;

    while(cur_interface_q_param_p != NULL){

        node_no++;
        cur_interface_param_p = CEC_Q_GET_DATA(
                cur_interface_q_param_p,
                cec_test_interface_param_t);

        cec_test_debug("Node (%u) : %15s \n",
                node_no,cur_interface_param_p->func_str);
        cur_interface_q_param_p = CEC_Q_GET_NEXT(
                (&(cur_interface_param_p->interface_q_node)));
    }

    rt_mutex_unlock(lock_test_q_param_p);

    cec_test_debug("\n<<< %s@%d <<< \n", __func__, __LINE__);

    return error;

}


int cec_test_get_param_data(char *param_str_p,
        cec_type_t mytype,
        void *data_p,
        uint8_t size)
{
    int error = 0;
    cec_test_interface_q_t *cur_interface_q_param_p;
    cec_test_interface_param_t        *cur_interface_param_p;
    int cmp_result=0;
    uint32_t                len;
    char                    *data;

    cec_test_debug("\n>>> %s@%d >>> \n", __func__, __LINE__);
    len = strlen(param_str_p);
    data = cec_allocate(size);
    cec_test_debug("\nReceived param_str : %s\n", param_str_p);
    cur_interface_q_param_p = interface_param_data_q_head_p;

    while(cur_interface_q_param_p != NULL){

        cur_interface_param_p = CEC_Q_GET_DATA(
                cur_interface_q_param_p,
                cec_test_interface_param_t);

        cmp_result = strcmp(param_str_p,
                cur_interface_param_p->func_str);

        cec_test_debug("\n%s@%d \n \tArg: %s Parameter :%s Value : %d\n",
                __func__,
                __LINE__,
                param_str_p,
                cur_interface_param_p->func_str,
                *(int *)cur_interface_param_p->user_data_p);

        if(!(cmp_result)){

            cec_test_debug("\nParameter Match !!!\n");

            switch(mytype){

                case INTEGER:
                    *(int *)data_p =
                    *(int *)cur_interface_param_p->user_data_p;
                    break;
                case STRING:
                    strncpy(
                    (char *)data_p,
                    (char *)cur_interface_param_p->user_data_p,
                    size);
                    break;
                case HEX:
                    *(int *)data_p =
                    *(int *)cur_interface_param_p->user_data_p;
                    break;
                case UNKNOWN:
                    cec_free(data);
                    return -EINVAL;
                default:
                    cec_free(data);
                    return -EINVAL;
            }
            break;
        }

        cur_interface_q_param_p = CEC_Q_GET_NEXT(
                (&(cur_interface_param_p->interface_q_node))
                );

    }

    cec_free(data);

    return error;

}


static ssize_t
cec_test_sysfs_show(struct kobject *kobj,
        struct attribute *attr,
        char *buf)
{

    cec_test_interface_q_t        *cur_interface_q_param_p;
    cec_test_interface_param_t        *cur_interface_param_p;
    uint8_t                 local_len = 0;
    uint32_t                len =0;

    cec_test_debug( "Reading Data from Attribute \n");

    cur_interface_q_param_p = interface_param_q_head_p;

    while(cur_interface_q_param_p != NULL) {

        cur_interface_param_p = CEC_Q_GET_DATA(
                cur_interface_q_param_p,
                cec_test_interface_param_t);
        local_len = sprintf(buf , "%25s:\n ",
                cur_interface_param_p->func_str);
        cec_test_debug("%25s - %s\n",
                cur_interface_param_p->func_str,
                cur_interface_param_p->info);
        cur_interface_q_param_p = CEC_Q_GET_NEXT(
                (&(cur_interface_param_p->interface_q_node))
                );

        len += local_len;
        buf += local_len;
    }

    return (len);

}

static ssize_t
cec_test_sysfs_store(struct kobject *kobj,
        struct attribute *attr,
        const char *buf,
        size_t len)
{
    cec_test_interface_q_t        *cur_interface_q_param_p;
    cec_test_interface_param_t        *cur_interface_param_p;
    cec_test_func_t           interface_func_p = NULL;

    char *dummy_p=NULL;
    int cmp_result=0;
    uint32_t mylen;
    char  *test_case;
    char delimiters[]= " ";

    cec_test_debug(" %s>>>@ %d>>>  ",__func__, __LINE__);

    dummy_p = (char *)buf;
    dummy_p[len-1] = '\0';
    test_case = strsep(&dummy_p,delimiters);
    mylen = len;
    cur_interface_q_param_p = interface_param_q_head_p;

    cec_test_debug( "Writing Data [%s of length %u ]to Attribute\n",
            test_case,
            mylen);
    while(cur_interface_q_param_p != NULL){

        cur_interface_param_p = CEC_Q_GET_DATA(
                cur_interface_q_param_p,
                cec_test_interface_param_t);
        cmp_result = strcmp(test_case, cur_interface_param_p->func_str);

        cec_test_debug("Arg:%s:%d List:%s\n",
                dummy_p,
                len ,
                cur_interface_param_p->func_str);

        if(!(cmp_result)){
            interface_func_p =
                (cec_test_func_t)cur_interface_param_p->interface_func_p;
            break;
        }

        cur_interface_q_param_p = CEC_Q_GET_NEXT(
                (&(cur_interface_param_p->interface_q_node))
                );
    }

    if(interface_func_p) {
        cec_test_debug(">>> %s@ %d >>> \n", __func__, __LINE__);
        interface_func_p(dummy_p, cur_interface_param_p->user_data_p);
    }

    return mylen;

}

struct cec_test_attr_s {
    struct attribute attr;
};

static struct cec_test_attr_s cec_test_attr = {
    .attr.name="test",
    .attr.mode = 0666,
};

static struct attribute * cec_test_kobj_attr[] = {
    &cec_test_attr.attr,
    NULL
};

struct kobject *cec_test_kobj_p;
static struct sysfs_ops cec_test_sysfs_ops;
static struct kobj_type cec_test_kobj_type;

int cec_init_test_module(void)
{
    int error = -1;
    struct kset *kset_p;
    struct kobject *kobj_p;

    lock_test_q_param_p = cec_allocate (sizeof(struct rt_mutex));
    rt_mutex_init(lock_test_q_param_p);
    cec_test_sysfs_ops.show = cec_test_sysfs_show;
    cec_test_sysfs_ops.store = cec_test_sysfs_store;

    cec_test_kobj_type.sysfs_ops = &cec_test_sysfs_ops;
    cec_test_kobj_type.default_attrs = cec_test_kobj_attr;

    cec_test_print(">>> %s@%d MUTEX:0x%x \n",
            __func__,
            __LINE__,
            (uint32_t)lock_test_q_param_p);

    /*
     * Create a kset with the name of "vibe",
     * located under /sys/kernel/
     */
    kset_p = kset_create_and_add("vibe", NULL, kernel_kobj);
    if (!kset_p)
        return -ENOMEM;


    kobj_p = kzalloc(sizeof(*kobj_p), GFP_KERNEL);
    if (!kobj_p)
        return -ENOMEM;

    /*
     * As we have a kset for this kobject, we need to set it before calling
     * the kobject core.
     */

    kobj_p->kset = kset_p;

    /*
     * Initialize and add the kobject to the kernel.  All the default files
     * will be created here.
     */

    /*Initialize kobject*/
    kobject_init(kobj_p, &cec_test_kobj_type);
    if (kobject_add(kobj_p, NULL, "%s", "cec")) {
      error = -1;
      cec_test_print("Sysfs creation failed\n");
      kobject_put(kobj_p);
      kfree(kobj_p);
      kobj_p = NULL;
    }
    else
      error = 0;

    cec_test_kobj_p = kobj_p;
    cec_test_print("<<< %s@%d <<< \n", __func__, __LINE__);

    return error;

    /*As we have already specified a kset for this
     * kobject, we don't have to set a parent for the kobject, the kobject
     * will be placed beneath that kset automatically.
     */
}

void cec_term_test_module(void)
{

    int error = 0;
    cec_test_interface_q_t *cur_interface_q_param_p;
    cec_test_interface_param_t *cur_interface_param_p;
    struct kset *kset_p;
    struct kobject *kobj_p = cec_test_kobj_p;

    if (!kobj_p) {
      return;
    }

    cec_test_print(">>> %s@%d >>> \n", __func__, __LINE__);
    kset_p = kobj_p->kset;
    if (rt_mutex_lock_interruptible(lock_test_q_param_p, true) == -EDEADLK)
        cec_error_trace(CEC_API, "%p will cause a deadlock\n", lock_test_q_param_p);

    while(interface_param_q_head_p != NULL){
        cur_interface_param_p = CEC_Q_GET_DATA(
                interface_param_q_head_p,
                cec_test_interface_param_t);
        error = cec_q_remove_node(
                &(interface_param_q_head_p),
                (uint32_t)(cur_interface_param_p->interface_func_p),
                &cur_interface_q_param_p);
        cec_free(cur_interface_param_p);

    }

    while(interface_param_data_q_head_p != NULL){

        cec_test_debug("\nEntering Loop\n");
        cur_interface_param_p = CEC_Q_GET_DATA(
                interface_param_data_q_head_p,
                cec_test_interface_param_t);

        error = cec_q_remove_node(
                &(interface_param_data_q_head_p),
                (uint32_t)&(cur_interface_param_p->interface_q_node),
                &cur_interface_q_param_p);

        cec_test_debug("%15s:%d - %s\n",
                cur_interface_param_p->func_str,
                strlen(cur_interface_param_p->func_str),
                cur_interface_param_p->info);

        cec_free(cur_interface_param_p->user_data_p);
        cec_free(cur_interface_param_p);

        cur_interface_q_param_p = CEC_Q_GET_NEXT(
                (&(cur_interface_param_p->interface_q_node))
                );

    }

    rt_mutex_unlock(lock_test_q_param_p);
    rt_mutex_destroy (lock_test_q_param_p);
    cec_free((void*)lock_test_q_param_p);

    kset_unregister(kset_p);
    kobject_put(kobj_p);
    kfree(kobj_p);

    cec_test_print("<<< %s@%d <<< \n", __func__, __LINE__);

}
