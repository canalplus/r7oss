#ifndef __STM_BLIT_STRINGS_H__
#define __STM_BLIT_STRINGS_H__


const char *
stm_blitter_get_surface_format_name(stm_blitter_surface_format_t fmt);
const char *
stm_blitter_get_porter_duff_name(stm_blitter_porter_duff_rule_t pd);

char *
stm_blitter_get_accel_string(enum stm_blitter_accel accel);
char *
stm_blitter_get_modified_string(enum stm_blitter_state_modification_flags mod);
char *
stm_blitter_get_drawflags_string(stm_blitter_surface_drawflags_t flags);
char *
stm_blitter_get_blitflags_string(stm_blitter_surface_blitflags_t flags);


#endif /* __STM_BLIT_STRINGS_H__ */
