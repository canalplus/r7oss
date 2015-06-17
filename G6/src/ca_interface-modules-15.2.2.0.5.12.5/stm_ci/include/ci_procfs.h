#ifndef	_STM_CI_PROCFS_H
#define	_STM_CI_PROCFS_H

int ci_create_procfs(uint32_t ci_num,stm_ci_t *ci_handle_p);
void ci_delete_procfs(uint32_t ci_num, stm_ci_t	*ci_handle_p);
#endif
