#ifndef _STM_CI_DEBUGFS_H
#define _STM_CI_DEBUGFS_H

int ci_create_debugfs(uint32_t ci_num,stm_ci_t *ci_handle_p);
void ci_delete_debugfs(uint32_t ci_num, stm_ci_t *ci_handle_p);
#endif
