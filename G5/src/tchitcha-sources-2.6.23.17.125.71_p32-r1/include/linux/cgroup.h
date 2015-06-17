#ifndef _LINUX_CGROUP_H
#define _LINUX_CGROUP_H

/*
 * Control groups are not backported - we use a few compatibility
 * defines to be able to use the upstream sched.c as-is:
 */
#define task_pid_nr(task)		(task)->pid
#define task_pid_vnr(task)		(task)->pid
#define find_task_by_vpid(pid)		find_task_by_pid(pid)

#endif
