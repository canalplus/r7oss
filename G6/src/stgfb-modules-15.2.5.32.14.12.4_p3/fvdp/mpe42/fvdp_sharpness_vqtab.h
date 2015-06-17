/***********************************************************************
 *
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
/* Define to prevent recursive inclusion */
#ifndef FVDP_SHARP_VQTAB_H
#define FVDP_SHARP_VQTAB_H

/* Includes ----------------------------------------------------------------- */

    /* C++ support */
#ifdef __cplusplus
    extern "C" {
#endif

/* Private Constants -------------------------------------------------------- */

// default VQData tables
VQ_SHARPNESS_Parameters_t SharpnessDefaultVQTable =
{
    {
        sizeof(VQ_SHARPNESS_Parameters_t),
        FVDP_SHARPNESS,
        VQVERSION(7,0)
    },

    { // Data Range
        127,    // MHF_SHARPNESS_LUMA_MAX    127
        { // MainLumaHMax
            128,    // MHF_ENH_Y_LOSHOOT_TOL_MAX    128
            128,    // MHF_ENH_Y_LUSHOOT_TOL_MAX    128
            96,    // MHF_ENH_Y_SOSHOOT_TOL_MAX    96
            96,    // MHF_ENH_Y_SUSHOOT_TOL_MAX    96
            96,    // MHF_ENH_Y_LGAIN_MAX    96
            128,    // MHF_ENH_Y_SGAIN_MAX    128
            128,    // MHF_ENH_Y_FINALLGAIN_MAX    128
            64, // MHF_ENH_Y_FINALSGAIN_MAX 64
            64,    // MHF_ENH_Y_FINALGAIN_MAX    64
            20,    // MHF_ENH_Y_DELTA_MAX    20
            0,    // MHF_ENH_Y_SLOPE_MAX    0
            64,    // MHF_ENH_Y_THRESH_MAX    64
            40,    // MHF_ENH_Y_HIGH_SLOPE_AGC_MAX    40
            40,    // MHF_ENH_Y_LOW_SLOPE_AGC_MAX    40
            200,    // MHF_ENH_Y_HIGH_THRESH_AGC_MAX    200
            50    // MHF_ENH_Y_LOW_THRESH_AGC_MAX    50
        },    // MainLumaHMax
        127,    // MVF_SHARPNESS_LUMA_MAX    127
        { // MainLumaVMax
            128,    // MVF_ENH_Y_LOSHOOT_TOL_MAX    128
            128,    // MVF_ENH_Y_LUSHOOT_TOL_MAX    128
            96,    // MVF_ENH_Y_SOSHOOT_TOL_MAX    96
            96,    // MVF_ENH_Y_SUSHOOT_TOL_MAX    96
            64,    // MVF_ENH_Y_LGAIN_MAX    64
            128,    // MVF_ENH_Y_SGAIN_MAX    128
            128,    // MVF_ENH_Y_FINALLGAIN_MAX    128
            64, // MVF_ENH_Y_FINALSGAIN_MAX 64
            64,    // MVF_ENH_Y_FINALGAIN_MAX    64
            20,    // MVF_ENH_Y_DELTA_MAX    20
            0,    // MVF_ENH_Y_SLOPE_MAX    0
            64,    // MVF_ENH_Y_THRESH_MAX    64
            40,    // MVF_ENH_Y_HIGH_SLOPE_AGC_MAX    40
            40,    // MVF_ENH_Y_LOW_SLOPE_AGC_MAX    40
            200,    // MVF_ENH_Y_HIGH_THRESH_AGC_MAX    200
            50    // MVF_ENH_Y_LOW_THRESH_AGC_MAX    50
        },    // MainLumaVMax
        127,    // MHF_SHARPNESS_CHROMA_MAX    127
        { // MainChromaHMax
            16,    // MHF_ENH_UV_LOSHOOT_TOL_MAX    16
            16,    // MHF_ENH_UV_LUSHOOT_TOL_MAX    16
            16,    // MHF_ENH_UV_SOSHOOT_TOL_MAX    16
            16, // MHF_ENH_UV_SUSHOOT_TOL_MAX 16
            64, // MHF_ENH_UV_LGAIN_MAX 64
            128, // MHF_ENH_UV_SGAIN_MAX 128
            96, // MHF_ENH_UV_FINALLGAIN_MAX 96
            128, // MHF_ENH_UV_FINALSGAIN_MAX 128
            196,    // MHF_ENH_UV_FINALGAIN_MAX    196
            20,    // MHF_ENH_UV_DELTA_MAX    20
            0,    // MHF_ENH_UV_SLOPE_MAX    0
            64,    // MHF_ENH_UV_THRESH_MAX    64
            40,    // MHF_ENH_UV_HIGH_SLOPE_AGC_MAX    40
            40,    // MHF_ENH_UV_LOW_SLOPE_AGC_MAX    40
            30,    // MHF_ENH_UV_HIGH_THRESH_AGC_MAX    30
            30    // MHF_ENH_UV_LOW_THRESH_AGC_MAX    30
        },    // MainChromaHMax
        127, // MVF_SHARPNESS_CHROMA_MAX 127
        { // MainChromaVMax
            16,    // MVF_ENH_UV_LOSHOOT_TOL_MAX    16
            16,    // MVF_ENH_UV_LUSHOOT_TOL_MAX    16
            16,    // MVF_ENH_UV_SOSHOOT_TOL_MAX    16
            16,    // MVF_ENH_UV_SUSHOOT_TOL_MAX    16
            96, // MVF_ENH_UV_LGAIN_MAX 96
            128,    // MVF_ENH_UV_SGAIN_MAX    128
            128,    // MVF_ENH_UV_FINALLGAIN_MAX    128
            128,    // MVF_ENH_UV_FINALSGAIN_MAX    128
            196,    // MVF_ENH_UV_FINALGAIN_MAX    196
            20,    // MVF_ENH_UV_DELTA_MAX    20
            0,    // MVF_ENH_UV_SLOPE_MAX    0
            64,    // MVF_ENH_UV_THRESH_MAX    64
            40,    // MVF_ENH_UV_HIGH_SLOPE_AGC_MAX    40
            40,    // MVF_ENH_UV_LOW_SLOPE_AGC_MAX    40
            30,    // MVF_ENH_UV_HIGH_THRESH_AGC_MAX    30
            30    // MVF_ENH_UV_LOW_THRESH_AGC_MAX    30
        },    // MainChromaVMax
        { // MainNLHMax
            64,    // Threshold1    64
            15,    // Gain1    15
            200,    // Threshold2    200
            15    // Gain2    15
        },    // MainNLHMax
        { // MainNLVMax
            64,    // Threshold1    64
            15,    // Gain1    15
            200,    // Threshold2    200
            15    // Gain2    15
        },    // MainNLVMax
        { // MainNCHMax
            20    // MHF_NOISE_CORING_CTRL_THRESH_MAX    20
        },    // MainNCHMax
        { // MainNCVMax
            20    // MVF_NOISE_CORING_CTRL_THRESH_MAX    20
        },    // MainNCVMax
        0,    // MHF_SHARPNESS_LUMA_MIN    0
        { // MainLumaHMin
            0,    // MHF_ENH_Y_LOSHOOT_TOL_MIN    0
            0,    // MHF_ENH_Y_LUSHOOT_TOL_MIN    0
            0,    // MHF_ENH_Y_SOSHOOT_TOL_MIN    0
            0,    // MHF_ENH_Y_SUSHOOT_TOL_MIN    0
            0,    // MHF_ENH_Y_LGAIN_MIN    0
            0,    // MHF_ENH_Y_SGAIN_MIN    0
            0,    // MHF_ENH_Y_FINALLGAIN_MIN    0
            0,    // MHF_ENH_Y_FINALSGAIN_MIN    0
            0,    // MHF_ENH_Y_FINALGAIN_MIN    0
            20,    // MHF_ENH_Y_DELTA_MIN    20
            0,    // MHF_ENH_Y_SLOPE_MIN    0
            64,    // MHF_ENH_Y_THRESH_MIN    64
            1,    // MHF_ENH_Y_HIGH_SLOPE_AGC_MIN    1
            1,    // MHF_ENH_Y_LOW_SLOPE_AGC_MIN    1
            0,    // MHF_ENH_Y_HIGH_THRESH_AGC_MIN    0
            0    // MHF_ENH_Y_LOW_THRESH_AGC_MIN    0
        },    // MainLumaHMin
        0,    // MVF_SHARPNESS_LUMA_MIN    0
        { // MainLumaVMin
            0,    // MVF_ENH_Y_LOSHOOT_TOL_MIN    0
            0,    // MVF_ENH_Y_LUSHOOT_TOL_MIN    0
            0,    // MVF_ENH_Y_SOSHOOT_TOL_MIN    0
            0,    // MVF_ENH_Y_SUSHOOT_TOL_MIN    0
            0,    // MVF_ENH_Y_LGAIN_MIN    0
            0,    // MVF_ENH_Y_SGAIN_MIN    0
            0,    // MVF_ENH_Y_FINALLGAIN_MIN    0
            0,    // MVF_ENH_Y_FINALSGAIN_MIN    0
            0,    // MVF_ENH_Y_FINALGAIN_MIN    0
            20,    // MVF_ENH_Y_DELTA_MIN    20
            0,    // MVF_ENH_Y_SLOPE_MIN    0
            64,    // MVF_ENH_Y_THRESH_MIN    64
            1,    // MVF_ENH_Y_HIGH_SLOPE_AGC_MIN    1
            1,    // MVF_ENH_Y_LOW_SLOPE_AGC_MIN    1
            0,    // MVF_ENH_Y_HIGH_THRESH_AGC_MIN    0
            0    // MVF_ENH_Y_LOW_THRESH_AGC_MIN    0
        },    // MainLumaVMin
        0,    // MHF_SHARPNESS_CHROMA_MIN    0
        { // MainChromaHMin
            0,    // MHF_ENH_UV_LOSHOOT_TOL_MIN    0
            0,    // MHF_ENH_UV_LUSHOOT_TOL_MIN    0
            0,    // MHF_ENH_UV_SOSHOOT_TOL_MIN    0
            0,    // MHF_ENH_UV_SUSHOOT_TOL_MIN    0
            0,    // MHF_ENH_UV_LGAIN_MIN    0
            0,    // MHF_ENH_UV_SGAIN_MIN    0
            0,    // MHF_ENH_UV_FINALLGAIN_MIN    0
            0,    // MHF_ENH_UV_FINALSGAIN_MIN    0
            0,    // MHF_ENH_UV_FINALGAIN_MIN    0
            20,    // MHF_ENH_UV_DELTA_MIN    20
            0,    // MHF_ENH_UV_SLOPE_MIN    0
            64,    // MHF_ENH_UV_THRESH_MIN    64
            1,    // MHF_ENH_UV_HIGH_SLOPE_AGC_MIN    1
            1,    // MHF_ENH_UV_LOW_SLOPE_AGC_MIN    1
            0,    // MHF_ENH_UV_HIGH_THRESH_AGC_MIN    0
            0    // MHF_ENH_UV_LOW_THRESH_AGC_MIN    0
        },    // MainChromaHMin
        0,    // MVF_SHARPNESS_CHROMA_MIN    0
        { // MainChromaVMin
            0,    // MVF_ENH_UV_LOSHOOT_TOL_MIN    0
            0,    // MVF_ENH_UV_LUSHOOT_TOL_MIN    0
            0,    // MVF_ENH_UV_SOSHOOT_TOL_MIN    0
            0,    // MVF_ENH_UV_SUSHOOT_TOL_MIN    0
            0,    // MVF_ENH_UV_LGAIN_MIN    0
            0,    // MVF_ENH_UV_SGAIN_MIN    0
            0,    // MVF_ENH_UV_FINALLGAIN_MIN    0
            0,    // MVF_ENH_UV_FINALSGAIN_MIN    0
            0,    // MVF_ENH_UV_FINALGAIN_MIN    0
            20,    // MVF_ENH_UV_DELTA_MIN    20
            0,    // MVF_ENH_UV_SLOPE_MIN    0
            64,    // MVF_ENH_UV_THRESH_MIN    64
            1,    // MVF_ENH_UV_HIGH_SLOPE_AGC_MIN    1
            1,    // MVF_ENH_UV_LOW_SLOPE_AGC_MIN    1
            0,    // MVF_ENH_UV_HIGH_THRESH_AGC_MIN    0
            0    // MVF_ENH_UV_LOW_THRESH_AGC_MIN    0
        },    // MainChromaVMin
        { // MainNLHMin
            0,    // Threshold1    0
            0,    // Gain1    0
            100,    // Threshold2    100
            0    // Gain2    0
        },    // MainNLHMin
        { // MainNLVMin
            0,    // Threshold1    0
            0,    // Gain1    0
            100,    // Threshold2    100
            0    // Gain2    0
        },    // MainNLVMin
        { // MainNCHMin
            0    // MHF_NOISE_CORING_CTRL_THRESH_MIN    0
        },    // MainNCHMin
        { // MainNCVMin
            0    // MVF_NOISE_CORING_CTRL_THRESH_MIN    0
        }    // MainNCVMin
    }, // Data Range
    51, // Number of steps 51
    { // Steps
        { // Step_00
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            15, // NoiseCoring 15
            { // Luma
                50, // OverShoot 50
                25, // LargeGain 25
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                25, // LargeGain 25
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_00
        { // Step_01
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            15, // NoiseCoring 15
            { // Luma
                50, // OverShoot 50
                25, // LargeGain 25
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                25, // LargeGain 25
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_01
        { // Step_02
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            15, // NoiseCoring 15
            { // Luma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                25, // LargeGain 25
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_02
        { // Step_03
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            15, // NoiseCoring 15
            { // Luma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_03
        { // Step_04
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            15, // NoiseCoring 15
            { // Luma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_04
        { // Step_05
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            20, // NoiseCoring 20
            { // Luma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                26, // LargeGain 26
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_05
        { // Step_06
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            20, // NoiseCoring 20
            { // Luma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_06
        { // Step_07
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            20, // NoiseCoring 20
            { // Luma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_07
        { // Step_08
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            20, // NoiseCoring 20
            { // Luma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_08
        { // Step_09
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            20, // NoiseCoring 20
            { // Luma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                27, // LargeGain 27
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_09
        { // Step_10
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            25, // NoiseCoring 25
            { // Luma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_10
        { // Step_11
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            25, // NoiseCoring 25
            { // Luma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_11
        { // Step_12
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            25, // NoiseCoring 25
            { // Luma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_12
        { // Step_13
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            25, // NoiseCoring 25
            { // Luma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_13
        { // Step_14
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            25, // NoiseCoring 25
            { // Luma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_14
        { // Step_15
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            30, // NoiseCoring 30
            { // Luma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                28, // LargeGain 28
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
           }, // Step_15
           { // Step_16
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            30, // NoiseCoring 30
            { // Luma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_16
        { // Step_17
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            30, // NoiseCoring 30
            { // Luma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_17
        { // Step_18
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            30, // NoiseCoring 30
            { // Luma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_18
        { // Step_19
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            30, // NoiseCoring 30
            { // Luma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                29, // LargeGain 29
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_19
        { // Step_20
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            35, // NoiseCoring 35
            { // Luma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_20
        { // Step_21
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            35, // NoiseCoring 35
            { // Luma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_21
        { // Step_22
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            35, // NoiseCoring 35
            { // Luma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_22
        { // Step_23
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            35, // NoiseCoring 35
            { // Luma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                30, // LargeGain 30
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_23
        { // Step_24
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            35, // NoiseCoring 35
            { // Luma
                50, // OverShoot 50
                31, // LargeGain 31
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                31, // LargeGain 31
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_24
        { // Step_25
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            40, // NoiseCoring 40
            { // Luma
                50, // OverShoot 50
                31, // LargeGain 31
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                31, // LargeGain 31
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_25
        { // Step_26
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            40, // NoiseCoring 40
            { // Luma
                50, // OverShoot 50
                32, // LargeGain 32
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                32, // LargeGain 32
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_26
        { // Step_27
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            40, // NoiseCoring 40
            { // Luma
                50, // OverShoot 50
                32, // LargeGain 32
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                32, // LargeGain 32
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_27
        { // Step_28
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            40, // NoiseCoring 40
            { // Luma
                50, // OverShoot 50
                33, // LargeGain 33
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                33, // LargeGain 33
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_28
        { // Step_29
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            40, // NoiseCoring 40
            { // Luma
                50, // OverShoot 50
                34, // LargeGain 34
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                34, // LargeGain 34
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_29
        { // Step_30
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            45, // NoiseCoring 45
            { // Luma
                50, // OverShoot 50
                35, // LargeGain 35
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                35, // LargeGain 35
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_30
        { // Step_31
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            45, // NoiseCoring 45
            { // Luma
                50, // OverShoot 50
                35, // LargeGain 35
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                35, // LargeGain 35
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
       }, // Step_31
        { // Step_32
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            45, // NoiseCoring 45
            { // Luma
                50, // OverShoot 50
                36, // LargeGain 36
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                36, // LargeGain 36
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_32
        { // Step_33
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            45, // NoiseCoring 45
            { // Luma
                50, // OverShoot 50
                37, // LargeGain 37
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                37, // LargeGain 37
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_33
        { // Step_34
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            45, // NoiseCoring 45
            { // Luma
                50, // OverShoot 50
                38, // LargeGain 38
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                38, // LargeGain 38
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_34
        { // Step_35
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            50, // NoiseCoring 50
            { // Luma
                50, // OverShoot 50
                39, // LargeGain 39
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                39, // LargeGain 39
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_35
        { // Step_36
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            50, // NoiseCoring 50
            { // Luma
                50, // OverShoot 50
                40, // LargeGain 40
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                40, // LargeGain 40
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_36
        { // Step_37
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            50, // NoiseCoring 50
            { // Luma
                50, // OverShoot 50
                41, // LargeGain 41
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                41, // LargeGain 41
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_37
        { // Step_38
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            50, // NoiseCoring 50
            { // Luma
                50, // OverShoot 50
                42, // LargeGain 42
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                42, // LargeGain 42
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_38
        { // Step_39
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            50, // NoiseCoring 50
            { // Luma
                50, // OverShoot 50
                43, // LargeGain 43
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                 50, // OverShoot 50
                 43, // LargeGain 43
                 50, // SmallGain 50
                 50, // FinalGain 50
                 50 // AGC 50
            } // Chroma
        }, // Step_39
        { // Step_40
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            55, // NoiseCoring 55
            { // Luma
                 50, // OverShoot 50
                 44, // LargeGain 44
                 50, // SmallGain 50
                 50, // FinalGain 50
                 50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                44, // LargeGain 44
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_40
        { // Step_41
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            55, // NoiseCoring 55
            { // Luma
                50, // OverShoot 50
                44, // LargeGain 44
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                44, // LargeGain 44
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_41
        { // Step_42
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            55, // NoiseCoring 55
            { // Luma
                50, // OverShoot 50
                45, // LargeGain 45
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                45, // LargeGain 45
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_42
        { // Step_43
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            55, // NoiseCoring 55
            { // Luma
                50, // OverShoot 50
                46, // LargeGain 46
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                46, // LargeGain 46
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_43
        { // Step_44
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            55, // NoiseCoring 55
            { // Luma
                50, // OverShoot 50
                47, // LargeGain 47
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                47, // LargeGain 47
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_44
        { // Step_45
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            60, // NoiseCoring 60
            { // Luma
                50, // OverShoot 50
                47, // LargeGain 47
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                47, // LargeGain 47
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_45
        { // Step_46
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            60, // NoiseCoring 60
            { // Luma
                50, // OverShoot 50
                48, // LargeGain 48
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                48, // LargeGain 48
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_46
        { // Step_47
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            60, // NoiseCoring 60
            { // Luma
                50, // OverShoot 50
                48, // LargeGain 48
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                48, // LargeGain 48
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_47
        { // Step_48
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            60, // NoiseCoring 60
            { // Luma
                50, // OverShoot 50
                49, // LargeGain 49
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                49, // LargeGain 49
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_48
        { // Step_49
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            60, // NoiseCoring 60
            { // Luma
                50, // OverShoot 50
                50, // LargeGain 50
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                50, // LargeGain 50
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        }, // Step_49
        { // Step_50
            50, // NonLinear_Thresh1 50
            50, // NonLinear_Gain1 50
            50, // NonLinear_Thresh2 50
            50, // NonLinear_Gain2 50
            60, // NoiseCoring 60
            { // Luma
                50, // OverShoot 50
                50, // LargeGain 50
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            }, // Luma
            { // Chroma
                50, // OverShoot 50
                50, // LargeGain 50
                50, // SmallGain 50
                50, // FinalGain 50
                50 // AGC 50
            } // Chroma
        } // Step_50
    } // Steps
};

/* Exported Types ----------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FVDP_SCALER_VQTAB_H */

