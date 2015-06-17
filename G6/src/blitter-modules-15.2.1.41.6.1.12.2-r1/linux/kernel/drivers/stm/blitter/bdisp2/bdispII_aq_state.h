#ifndef __BDISPII_AQ_STATE_H__
#define __BDISPII_AQ_STATE_H__


#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/bpa2.h>
#endif /* __KERNEL__ */
#include <linux/stm/blitter.h>
#include <linux/stm/bdisp2_nodegroups.h>
#include <linux/stm/bdisp2_features.h>

#include "state.h"

#include "bdisp2/bdispII_device_features.h"
#include "bdisp2/bdispII_aq_operations.h"

struct bdisp2_features {
	struct _stm_bdisp2_hw_features hw;

	struct bdisp2_pixelformat_info stm_blit_to_bdisp[STM_BLITTER_SF_COUNT];

	enum stm_blitter_surface_drawflags_e drawflags;
	enum stm_blitter_surface_blitflags_e blitflags;
};

enum bdisp2_palette_type {
	SG2C_NORMAL,
	SG2C_COLORALPHA              /* αout = colorα, RGBout = RGBin */,
	SG2C_INVCOLORALPHA           /* αout = 1-colorα, RGBout = RGBin */,
	SG2C_ALPHA_MUL_COLORALPHA    /* αout = α * colorα, RGBout = RGBin
					*/,
	SG2C_INV_ALPHA_MUL_COLORALPHA /* αout = 1-(α * colorα),
					RGBout = RGBin */,
	SG2C_DYNAMIC_COUNT           /* number of dynamic palettes */,

	SG2C_ONEALPHA_RGB      /* αout = 1, RGBout = RGBin */
							= SG2C_DYNAMIC_COUNT,
	SG2C_INVALPHA_ZERORGB  /* αout = 1 - Ain, RGBout = 0 */,
	SG2C_ALPHA_ZERORGB     /* αout = Ain, RGBout = 0 */,
	SG2C_ZEROALPHA_RGB     /* αout = 0, RGBout = RGBin */,
	SG2C_ZEROALPHA_ZERORGB /* αout = 0, RGBout = 0 */,
	SG2C_ONEALPHA_ZERORGB  /* αout = 1, RGBout = 0 */,

	SG2C_COUNT
};

#ifndef __KERNEL__
#  define __iomem
#endif /* __KERNEL__ */

struct stm_bdisp2_dma_area {
#ifdef __KERNEL__
	struct bpa2_part      *part;
#else /* __KERNEL__ */
	void                  *unused;
#endif /* __KERNEL__ */
	unsigned long          base;
	size_t                 size;
	volatile void __iomem *memory;
	bool                   cached;
};

struct stm_bdisp2_driver_data {
	/* blit nodes */
	struct stm_bdisp2_shared_area __iomem *bdisp_shared; /*!< shared area */
	struct stm_bdisp2_dma_area             bdisp_nodes;  /*!< node list   */

	volatile void __iomem *mmio_base; /*!< BDisp base address */

	struct stm_bdisp2_dma_area tables; /*!< CLUTs and filters */
	unsigned long __iomem *clut_virt[SG2C_DYNAMIC_COUNT]; /*!< CLUT
								addresses */

	struct stm_bdisp2_device_data *stdev;

	struct bdisp2_funcs funcs;
};

struct stm_bdisp2_device_setup {
	struct _BltNodeGroup00 ConfigGeneral;
	struct _BltNodeGroup01 ConfigTarget;
	struct target_loop {
		uint32_t BLT_TBA;
		uint32_t BLT_TTY;
		struct {
			uint16_t h; /*!< horizontal divider for x/w */
			uint16_t v; /*!< vertical divider for y/h */
		} coordinate_divider;
	} target_loop[2]; /*!< additional loops for planar targets,
			    loop[0] is Cb/Cr or Cb, loop[1] is Cr */
	unsigned int n_target_extra_loops;
	struct _BltNodeGroup02 ConfigColor;
	struct _BltNodeGroup03 ConfigSource1;
	struct _BltNodeGroup04 ConfigSource2;
	struct _BltNodeGroup05 ConfigSource3;
#ifdef BDISP2_SUPPORT_HW_CLIPPING
	struct _BltNodeGroup06 ConfigClip;
#endif
	struct _BltNodeGroup07 ConfigClut;
	struct _BltNodeGroup08 ConfigFilters;
	struct _BltNodeGroup09 ConfigFiltersChr;
	struct _BltNodeGroup10 ConfigFiltersLuma;
	struct _BltNodeGroup11 ConfigFlicker;
	struct _BltNodeGroup12 ConfigColorkey;
	struct _BltNodeGroup14 ConfigStatic;
	struct _BltNodeGroup15 ConfigIVMX;
	struct _BltNodeGroup16 ConfigOVMX;
	struct _BltNodeGroup18 ConfigVC1R;

	/* state shared between both draw and blit states */
	struct {
		__u32 dst_ckey[2];
		__u32 extra_blt_ins; /* for RGB32 to always enable plane mask */
		__u32 extra_blt_cic; /* for RGB32 to always enable plane mask */
	} all_states;

	/* drawing state */
	struct {
		struct _BltNodeGroup00 ConfigGeneral;
		__u32                  color;
		__u32                  color_ty;
		struct _BltNodeGroup07 ConfigClut;
	} drawstate;

	/* blitting state */
	struct {
		long source_w; /* the total width of the surface in 16.16 */
		long source_h; /* the total height of the surface in 16.16 */

		unsigned int srcFactorH; /*!< H factor for source1 and source2
					    coordinate increment, will normally
					    be == 1. For certain YCbCr pixel
					    formats it will be == 2, though. */
		unsigned int srcFactorV; /*!< V factor for source1 and source2
					    coordinate increment, will normally
					    be == 1. For certain YCbCr pixel
					    formats it will be == 2, though. */

		__u32 src_ckey[2];

		unsigned int canUseHWInputMatrix:1;
		unsigned int isOptimisedModulation:1;
		unsigned int bIndexTranslation:1;
		unsigned int bFixedPoint:1;
		unsigned int srcxy_fixed_point:1;
		unsigned int src_premultcolor:1;
		unsigned int bDeInterlacingTop:1;
		unsigned int bDeInterlacingBottom:1;
		unsigned int flicker_filter:1;
		unsigned int strict_input_rect:1;

		int rotate; /*!< Counter clockwise degrees of rotation.
			      90 180 270 and some special internal values are
			      supported, only. */
		__u32 blt_ins_src1_mode;

		int n_passes;
		struct {
			struct _BltNodeGroup00   ConfigGeneral;
			enum bdisp2_palette_type palette_type;
		} extra_passes[2];
	} blitstate;

	enum bdisp2_palette_type      palette_type;
	struct stm_blitter_palette_s *palette;
	bool                          do_expand;
	unsigned int                  upsampled_src_nbpix_min;
	unsigned int                  upsampled_dst_nbpix_min;

	/* these are per operation */
	int h_trgt_sign, h_src2_sign, v_trgt_sign, v_src2_sign;
};

struct stm_bdisp2_device_data {
	struct bdisp2_features features; /*!< hardware features */

	uint32_t queue_id; /*!< id of queue */

	size_t usable_nodes_size; /* shared->nodes_size might not be a
				      multiple of our
				      sizeof(<biggest_node>) */

	uint32_t node_irq_delay; /* every now and then, we want to see a node
				    completed interrupt, so a) we don't have
				    to wait for a complete list to finish,
				    and b) we don't want them to happen too
				    often */

	unsigned long clut_phys[SG2C_COUNT];
	unsigned long filter_8x8_phys;
	unsigned long filter_5x8_phys;

	uint32_t force_slow_path; /* force slow path copy / blit */
	uint32_t no_blend_optimisation; /* don't use fast direct fill/copy
	                                   during PorterDuff blends, even
					   if possible. */

	/* state etc */
	int v_flags; /*!< validation flags */

	/* this is per (stretch blit) - they can change without changing the
	   hardware state */
	__u32 hsrcinc; /* in 16.16 */
	__u32 vsrcinc; /* in 16.16 */

	struct stm_bdisp2_device_setup _setup;
	struct stm_bdisp2_device_setup *setup;

#ifdef STGFX2_CLUT_UNSAFE_MULTISESSION
	unsigned int clut_disabled:1;
#endif
};


enum stm_blitter_accel
bdisp2_state_supported(const struct stm_bdisp2_device_data *stdev,
		       const struct stm_blitter_state      *state,
		       enum stm_blitter_accel               accel);

void bdisp2_state_update(struct stm_bdisp2_driver_data *stdrv,
			 struct stm_bdisp2_device_data *stdev,
			 struct stm_blitter_state      *state,
			 enum stm_blitter_accel         accel);

int
bdisp2_check_memory_constraints(const struct stm_bdisp2_device_data * const stdev,
				unsigned long                        start,
				unsigned long                        end);


#endif /* __BDISPII_AQ_STATE_H__ */
