#ifndef _STM_SCR_DEBUGFS_H
#define _STM_SCR_DEBUGFS_H

#ifdef CONFIG_PROC_FS
void scr_dump_atr(scr_atr_parsed_data_t data);

int scr_create_procfs(uint32_t scr_num,scr_ctrl_t *scr_handle_p);
void scr_delete_procfs(uint32_t scr_num, scr_ctrl_t *scr_handle_p);
#endif
#endif
