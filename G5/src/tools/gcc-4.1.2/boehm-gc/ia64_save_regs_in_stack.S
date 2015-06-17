        .text
        .align 16
        .global GC_save_regs_in_stack
        .proc GC_save_regs_in_stack
GC_save_regs_in_stack:
        .bodyfoo.mpg
        flushrs
        ;;
        mov r8=ar.bsp
        br.ret.sptk.few rp
        .endp GC_save_regs_in_stack

#ifdef __linux__
	.section .note.GNU-stack,"",@progbits
#endif
