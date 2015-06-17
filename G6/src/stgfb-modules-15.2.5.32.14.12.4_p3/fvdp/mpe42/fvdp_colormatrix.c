/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_color.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_color.h"
#include "../fvdp_vqdata.h"
//#include "fvdp.h"
/* Global Variables --------------------------------------------------------- */

/* Definitions   ------------------------------------------------------------ */
#define CSC_REG32_Write(Addr,Val)              REG32_Write(Addr,Val)
#define CSC_REG32_Read(Addr)                   REG32_Read(Addr)
#define CSC_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr,BitsToSet)
#define CSC_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr,BitsToClear)
#define CSC_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                             REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet)

/* Private Types ------------------------------------------------------------ */

/* Hardware Bit Resolution */
#define MATRIX_COEFF_MAX 0x3FFF
#define MATRIX_COEFF_MIN (-(MATRIX_COEFF_MAX+1))
#define MATRIX_OFFSET_MAX 0x7FFF
#define MATRIX_OFFSET_MIN (-(MATRIX_OFFSET_MAX+1))
#define PARAM_COEFF_MAX 0xFFF //expects {4.8} format
#define TOTAL_R_SHIFTS 34

#define REG_OFFSET (4)

#define OFFSET_IN_1 (0x0000)
#define OFFSET_IN_2 (OFFSET_IN_1+REG_OFFSET)
#define OFFSET_IN_3 (OFFSET_IN_2+REG_OFFSET)

#define COEF_11         (OFFSET_IN_3 + REG_OFFSET)
#define COEF_12         (COEF_11 + REG_OFFSET)
#define COEF_13         (COEF_12 + REG_OFFSET)
#define COEF_21         (COEF_13 + REG_OFFSET)
#define COEF_22         (COEF_21 + REG_OFFSET)
#define COEF_23         (COEF_22 + REG_OFFSET)
#define COEF_31         (COEF_23 + REG_OFFSET)
#define COEF_32         (COEF_31 + REG_OFFSET)
#define COEF_33         (COEF_32 + REG_OFFSET)

#define OFFSET_OUT_1 (COEF_33 + REG_OFFSET)
#define OFFSET_OUT_2 (OFFSET_OUT_1+ REG_OFFSET)
#define OFFSET_OUT_3 (OFFSET_OUT_2+ REG_OFFSET)


#define TRUNC_BOTH_BOUNDS(X,MIN,MAX)    {if (X > MAX) {X = MAX;}else if (X < MIN) {X = MIN;}}
#define TRUNC_UPPER_BOUND(X,MAX)    {if (X > MAX) {X = MAX;}}
#define CHECK_BOTH_BOUNDS(X,MIN,MAX)    {if ((X > MAX)||(X < MIN)) {return (FVDP_ERROR);}}
#define CHECK_UPPER_BOUND(X,MAX)    {if (X > MAX) {return (FVDP_ERROR);}}
#define LSHFT(X,Y)              ((X) <<= (Y))
#define RSHFT(X,Y)              ((X) >>= (Y))


/* Private Variables (static)------------------------------------------------ */
typedef struct ColorMatrix_Coef_s
{
    int32_t MultCoef11;
    int32_t MultCoef12;
    int32_t MultCoef13;
    int32_t MultCoef21;
    int32_t MultCoef22;
    int32_t MultCoef23;
    int32_t MultCoef31;
    int32_t MultCoef32;
    int32_t MultCoef33;
    int32_t InOffset1;
    int32_t InOffset2;
    int32_t InOffset3;
    int32_t OutOffset1;
    int32_t OutOffset2;
    int32_t OutOffset3;
} ColorMatrix_Coef_t; // CSC matrix coefficients

/*
YUV BT709 to RGB [0~1023]
*/
static const ColorMatrix_Coef_t ColorMatrix_DefaultYUV709ToRGBVQTable =
{


        0x12A0, -0x0368, -0x088B,    /* 1.164    -0.213    -0.534 */
        0x12A0,  0x21D7,  0x0000,    /* 1.164    2.115    0.000 */
        0x12A0,  0x0000,  0x1CB0,    /* 1.164    0.000    1.793 */

         -1024,   -8192,   -8192,    /*input offset*/
             0,       0,       0     /*output offset*/

};

/*
YUV BT601 to RGB [64~940]
*/
static const ColorMatrix_Coef_t ColorMatrix_DefaultYUV601ToVideoRangeRGBVQTable =
{


        0x1000,  -0x560, -0xB2B,     /* 1.000     -0.336    -0.698 */
        0x1000,  0x1BB6,    0x0,     /*1.000     1.732    0.000  */
        0x1000,     0x0, 0x15F0,     /* 1.000    0.000    1.371 */

         -1024,   -8192,  -8192,     /*input offset */
          1024,    1024,   1024      /*output offset */

};

/*
YUV BT709 to RGB [64~940]
*/
static const ColorMatrix_Coef_t ColorMatrix_DefaultYUV709ToVideoRangeRGBVQTable =
{


        0x1000,  -0x2EE, -0x758,     /* 1.000    -0.183    -0.459 */
        0x1000,  0x1D0E,    0x0,     /* 1.000    1.816    0.000 */
        0x1000,     0x0, 0x18A4,     /* 1.000    0.000    1.540 */

         -1024,   -8192,  -8192,     /* input offset */
          1024,    1024,   1024      /* output offset */

};

/*
YUV BT601 to RGB [0~1023]
*/
static const ColorMatrix_Coef_t ColorMatrix_DefaultYUV601ToRGBVQTable =
{


        0x12A0, -0x0642, -0x0D02,    /* 1.164    -0.391    -0.813 */
        0x12A0,  0x204A,  0x0000,    /* 1.164    2.018    0.000 */
        0x12A0,  0x0000,  0x1989,    /* 1.164    0.000    1.596 */

         -1024,   -8192,   -8192,    /*input offset*/
             0,       0,       0     /*output offset*/

};
/*
Unity
*/
static const ColorMatrix_Coef_t ColorMatrix_DefaultUnityVQTable =
{


        0x1000,       0,       0,    /* 1.000    0.000    0.000 */
             0,  0x1000,       0,    /* 0.000    1.000    0.000 */
             0,       0,  0x1000,    /* 0.000    0.000    1.000 */

             0,        0,       0,   /*input offset*/
             0,        0,       0    /*output offset*/

};
/*  Function Prototypes ---------------------------------------------- */

/* Private Function Prototypes ---------------------------------------------- */
static int16_t Sine (int16_t Degree);
static int16_t Cosine (int16_t Degree);
static FVDP_Result_t ColorMatrix_SetYUVCoeff (uint32_t ColorMatrix_Offset1BaseAddress,  ColorMatrix_Coef_t * pMatrix, ColorMatrix_Adjuster_t* pAdjuster);
static FVDP_Result_t ColorMatrix_SetRGBCoeff (uint32_t ColorMatrix_Offset1BaseAddress, ColorMatrix_Adjuster_t* pAdjuster);
static FVDP_Result_t ColorMatrix_SetOffset (uint32_t ColorMatrix_Offset1BaseAddress,  ColorMatrix_Coef_t* pMatrix, ColorMatrix_Adjuster_t* pAdjuster);
static FVDP_Result_t ColorMatrix_SetColor(uint32_t ColorMatrix_Offset1BaseAddress,  bool IsRGB, ColorMatrix_Adjuster_t* pAdjuster, ColorMatrix_Coef_t * pMatrix);
static void ColorMatrix_WriteMatrix(uint32_t ColorMatrix_Offset1BaseAddress, int64_t * pMatrix);


/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : ColorMatrix_Update
Description : Depending on input and output color space, it uses apropriate default color
              conversion table, and calls set color function


Parameters  : uint32_t ColorMatrix_Offset1BaseAddress - base address of hardware registers
              FVDP_ColorSpace_t ColorSpaceIn - color space on input to the matrix
              FVDP_ColorSpace_t ColorSpaceOutput - color space on output from the matrix
              ColorMatrix_Adjuster_t * ColorControl - user color controls
Assumptions :
Limitations :
Returns     : FVDP_OK - if everything is Ok
              FVDP_ERROR - if color space is incorrect or other error

*******************************************************************************/
FVDP_Result_t ColorMatrix_Update (uint32_t ColorMatrix_Offset1BaseAddress,
                                 FVDP_ColorSpace_t ColorSpaceIn,
                                 FVDP_ColorSpace_t ColorSpaceOutput ,
                                 ColorMatrix_Adjuster_t * ColorControl)
{
    bool IsRGB = FALSE;
    FVDP_Result_t result = FVDP_OK;
    ColorMatrix_Coef_t ColorMatrix_Matrix;

    if (ColorSpaceOutput == RGB)
    {
        if(ColorSpaceIn == sdYUV)
        {
            ColorMatrix_Matrix = ColorMatrix_DefaultYUV601ToRGBVQTable;
        }
        else if(ColorSpaceIn == hdYUV)
        {
            ColorMatrix_Matrix = ColorMatrix_DefaultYUV709ToRGBVQTable;
        }
        else if(ColorSpaceIn == RGB861)
        {
            ColorMatrix_Matrix =ColorMatrix_DefaultUnityVQTable;
        }
        else if(ColorSpaceIn == RGB || ColorSpaceIn == sRGB)
        {
            IsRGB = TRUE;
            ColorMatrix_Matrix = ColorMatrix_DefaultUnityVQTable;
        }
        else
        {
            return (FVDP_ERROR);
        }
    }
    else if (ColorSpaceOutput == RGB861)
    {
        if(ColorSpaceIn == sdYUV)
        {
            ColorMatrix_Matrix = ColorMatrix_DefaultYUV601ToVideoRangeRGBVQTable;
        }
        else if(ColorSpaceIn == hdYUV)
        {
            ColorMatrix_Matrix = ColorMatrix_DefaultYUV709ToVideoRangeRGBVQTable;
        }
        else if(ColorSpaceIn == RGB861)
        {
            ColorMatrix_Matrix =ColorMatrix_DefaultUnityVQTable;
        }
        else if(ColorSpaceIn == RGB || ColorSpaceIn == sRGB)
        {
            IsRGB = TRUE;
            ColorMatrix_Matrix = ColorMatrix_DefaultUnityVQTable;
        }
        else
        {
            return (FVDP_ERROR);
        }
    }
    else
    {
        return (FVDP_ERROR);
    }

    result = ColorMatrix_SetColor(ColorMatrix_Offset1BaseAddress, IsRGB, ColorControl, &ColorMatrix_Matrix);

    return result;
}

/*******************************************************************************
Name        : ColorMatrix_SetColor
Description : Updates the color control level on the selected channel.

Parameters  : MatrixId - matrix id
              IsRGB - whether processing in RGB or not
              Adjuster_p - pointer to color control structure
              Matrix_p - base matrix
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK if no error
              FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t ColorMatrix_SetColor(uint32_t ColorMatrix_MatrixBaseAddress,bool IsRGB,
                            ColorMatrix_Adjuster_t* pAdjuster, ColorMatrix_Coef_t * pMatrix)
{
    FVDP_Result_t Status = FVDP_OK;

    /*
        Set Offsets
    */
    Status |= ColorMatrix_SetOffset(ColorMatrix_MatrixBaseAddress, pMatrix, pAdjuster);

    /*
        Set Coefficients
    */
    if (IsRGB)
    {
        Status |= ColorMatrix_SetRGBCoeff(ColorMatrix_MatrixBaseAddress, pAdjuster);
    }
    else
    {
        Status |= ColorMatrix_SetYUVCoeff(ColorMatrix_MatrixBaseAddress, pMatrix, pAdjuster);
    }

    return Status;
}

/*******************************************************************************
Name        : ColorMatrix_SetOffset
Description : Compute color control parameters and write to OVP 3x3 input and
              output offset registers. This function will perform boundary check on
              input parameters. The error message FVDP_ERROR
              will be returned if any of the input parameters are out-of-bound.
              However, after all calculations and immediately prior to writing
              the calculated values to the registers, another boundary check will
              be performed and if any values are out-of-bound, the values will be capped
              (not cropped) and no error message will be returned.
Parameters  : MatrixId - matrix id
              pMatrix - pointer to base matrix
              pAdjuster - pointer to Adjuster values.
              The following adjusters are used.
                    Brightness - color control parameter in {N.0} notation
                    RedOffset - color control parameter in {N.0} notation
                    GreenOffset - color control parameter in {N.0} notation
                    BlueOffset - color control parameter in {N.0} notation
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK if no error
              FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t ColorMatrix_SetOffset(uint32_t ColorMatrix_MatrixBaseAddress, ColorMatrix_Coef_t* pMatrix, ColorMatrix_Adjuster_t* pAdjuster)
{
    int32_t ROffset, GOffset, BOffset;

    CHECK_BOTH_BOUNDS(pAdjuster->Brightness, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);
    CHECK_BOTH_BOUNDS(pAdjuster->RedOffset, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);
    CHECK_BOTH_BOUNDS(pAdjuster->GreenOffset, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);
    CHECK_BOTH_BOUNDS(pAdjuster->BlueOffset, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);

    /*
        Get RGB offset values
    */
    ROffset = pAdjuster->Brightness + pAdjuster->RedOffset + pMatrix->OutOffset1;
    GOffset = pAdjuster->Brightness + pAdjuster->GreenOffset + pMatrix->OutOffset2;
    BOffset = pAdjuster->Brightness + pAdjuster->BlueOffset + pMatrix->OutOffset3;

    TRUNC_BOTH_BOUNDS(ROffset, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);
    TRUNC_BOTH_BOUNDS(GOffset, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);
    TRUNC_BOTH_BOUNDS(BOffset, MATRIX_OFFSET_MIN, MATRIX_OFFSET_MAX);

    CSC_REG32_Write (OFFSET_OUT_1 + ColorMatrix_MatrixBaseAddress, GOffset);
    CSC_REG32_Write (OFFSET_OUT_2 + ColorMatrix_MatrixBaseAddress, BOffset);
    CSC_REG32_Write (OFFSET_OUT_3 + ColorMatrix_MatrixBaseAddress, ROffset);

    CSC_REG32_ClearAndSet(OFFSET_IN_1 + ColorMatrix_MatrixBaseAddress, CCF_P_IN_OFFSET1_MASK, CCF_P_IN_OFFSET1_OFFSET, pMatrix->InOffset1);
    CSC_REG32_ClearAndSet(OFFSET_IN_2 + ColorMatrix_MatrixBaseAddress, CCF_P_IN_OFFSET1_MASK, CCF_P_IN_OFFSET1_OFFSET, pMatrix->InOffset2);
    CSC_REG32_ClearAndSet(OFFSET_IN_3 + ColorMatrix_MatrixBaseAddress, CCF_P_IN_OFFSET1_MASK, CCF_P_IN_OFFSET1_OFFSET, pMatrix->InOffset3);

    return (FVDP_OK);
}

/*******************************************************************************
Name        : ColorMatrix_SetRGBCoeff
Description : Compute RGB color control parameters based on base matrix and write
              result to OVP 3x3 hardware. This function will perform boundary check on
              input parameters. The error message FVDP_ERROR
              will be returned if any of the input parameters are out-of-bound.
              However, after all calculations and immediately prior to writing
              the calculated values to the registers, another boundary check will
              be performed and if any values are out-of-bound, the values will be capped
              (not cropped) and no error message will be returned.

Parameters  : MatrixId - matrix id
              pMatrix - pointer to base matrix
              pAdjuster - pointer to Adjuster values.
              The following adjusters are used.
                Contrast - color control parameter in {4.8} notation
                RedGain - color control parameter in {4.8} notation
                GreenGain - color control parameter in {4.8} notation
                BlueGain - color control parameter in {4.8} notation
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK if no error
              FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t ColorMatrix_SetRGBCoeff (uint32_t ColorMatrix_MatrixBaseAddress, ColorMatrix_Adjuster_t* pAdjuster)
{
    uint32_t RGain, GGain, BGain;
    uint64_t  sMatrix[9]={0,0,0,0,0,0,0,0,0};

    CHECK_UPPER_BOUND(pAdjuster->Contrast, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->RedGain, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->GreenGain, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->BlueGain, PARAM_COEFF_MAX);

    /*
        Multiply RGB gain values with same contrast value to program RGB gains.
        This is done by multiplying 2 (4.8 notation) value to obtain a (Signed 8.16) value
        then convert (Signed 8.16) to a (Signed 3.10) since this is what the HW expects.
        Achieve by shifting right 6 bits.
    */
    RGain = ((uint32_t)pAdjuster->RedGain) * pAdjuster->Contrast;
    RSHFT(RGain, 6);

    GGain = ((uint32_t)pAdjuster->GreenGain) * pAdjuster->Contrast;
    RSHFT(GGain, 6);

    BGain = ((uint32_t)pAdjuster->BlueGain) * pAdjuster->Contrast;
    RSHFT(BGain, 6);

    TRUNC_UPPER_BOUND(RGain, MATRIX_COEFF_MAX);
    TRUNC_UPPER_BOUND(GGain, MATRIX_COEFF_MAX);
    TRUNC_UPPER_BOUND(BGain, MATRIX_COEFF_MAX);

    sMatrix[0] = (uint32_t)GGain;
    sMatrix[4] = (uint32_t)BGain;
    sMatrix[8] = (uint32_t)RGain;

    ColorMatrix_WriteMatrix(ColorMatrix_MatrixBaseAddress, sMatrix);

    return (FVDP_OK);
}
/*******************************************************************************
Name        : ColorMatrix_SetYUVCoeff
Description : Compute YUV color control parameters based on base matrix and write
              result to OVP 3x3 hardware. This function will perform boundary check on
              input parameters. The error message FVDP_ERROR
              will be returned if any of the input parameters are out-of-bound.
              However, after all calculations and immediately prior to writing
              the calculated values to the registers, another boundary check will
              be performed and if any values are out-of-bound, the values will be capped
              (not cropped) and no error message will be returned.

Parameters  : MatrixId - matrix id
              pMatrix - pointer to base matrix
              pAdjuster - pointer to Adjuster values.
              The following adjusters are used.
                Contrast - color control parameter in {4.8} notation
                Saturation - color control parameter in {4.8} notation
                Hue - color control parameter in {4.8} notation
                RedGain - color control parameter in {4.8} notation
                GreenGain - color control parameter in {4.8} notation
                BlueGain - color control parameter in {4.8} notation
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK if no error
              FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t ColorMatrix_SetYUVCoeff (uint32_t ColorMatrix_MatrixBaseAddress,
                    ColorMatrix_Coef_t* pMatrix, ColorMatrix_Adjuster_t* pAdjuster)
{
    int64_t  sMatrix[9];
    int64_t  sLocalSaturation, sLocalContrast, sSatCosHue, sSatSinHue,sLocalContrast8;
    int64_t  sTemp;
    uint16_t i;



    CHECK_UPPER_BOUND(pAdjuster->Contrast, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->Saturation, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->RedGain, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->GreenGain, PARAM_COEFF_MAX);
    CHECK_UPPER_BOUND(pAdjuster->BlueGain, PARAM_COEFF_MAX);

    /*
        Get Contrast value
    */
    sLocalContrast = (int64_t)pAdjuster->Contrast;
    sLocalContrast8 = sLocalContrast;
    /*
        Convert uint16_t to int64_t (from Unsigned 4.8 to Signed 4.24)
        fixed point format (0x100 equal 0x1.000)
        1 sign bit, 3 integer bits and 24 fraction bits.
        By shift left 16 bits.
    */
    LSHFT(sLocalContrast, 16);

    /*
       Get Saturation value
    */
    sLocalSaturation = (int64_t)pAdjuster->Saturation;
    /*
        Convert uint16_t to int64_t (from Unsigned 4.8 to Signed 4.12)
        fixed point format (0x100 equal 0x1.000)
        1 sign bit, 3 integer bits and 12 fraction bits.
        By shift left 4 bits.
    */
    LSHFT(sLocalSaturation, 4);

    /*
        For hue values, the default library supports 1/4 degree increments for any degree [0..360]
        By changing the stepsize, min/max ranges the range can be limited.
    */
    sSatSinHue = (int64_t)Sine(pAdjuster->Hue);
    sSatCosHue = (int64_t)Cosine(pAdjuster->Hue);
    /*
        Multiply Saturation and Cosine(Hue) at position [1,1] and [2,2] in 3x3 matrix
        Saturation * Cosine(Hue)
        (Signed 8.24) = (Signed 4.12) * (Signed 4.12)
    */
    sTemp = sLocalSaturation * sSatCosHue;
    sSatCosHue = (int64_t)sTemp; /* Store Sat*Cos(Hue)*/

    /*
        Multiply Saturation and Cosine(Hue) at position [1,2] and [2,1] in 3x3 matrix
        Saturation * Sine(Hue)
        (Signed 8.24) = (Signed 4.12) * (Signed 4.12)
    */
    sTemp = sLocalSaturation * sSatSinHue;
    sSatSinHue = (int64_t)sTemp; /* Store Sat*Sin(Hue)*/

    /*
        Perform the final matrix calculation now that all the necessary parameters are available
        [a b c]   [ Contr         0            0          ]
        [d e f] X [ 0         Sat*cos(Hue)  Sat*sin(Hue)  ]where a-i are the conversion coefficients
        [g h i]   [ 0        -Sat*sin(Hue)  Sat*cos(Hue)  ]

        Instead of performing matrix multiplication at run time the multiplication will result in the following equation:
        [[ G * a * Cont] * (Y) [G * b * S * cos(hue) - G * c * S * sin(hue)] * (U)   [G * b * S * sin(hue) + B * c * S * cos(hue)] * (V)  ]
        [[ B * d * Cont] * (Y) [B * e * S * cos(hue) - G * f * S * sin(hue)] * (U)   [B * e * S * sin(hue) + B * f * S * cos(hue)] * (V)  ]
        [[ R * g * Cont] * (Y) [R * h * S * cos(hue) - G * i * S * sin(hue)] * (U)   [R * h * S * sin(hue) + B * i * S * cos(hue)] * (V)  ]
        (Signed 12.36) = (Signed 8.24) * (Signed 4.12)
    */
    // Updated programming model, contrast is applied to all coefficients
    // and some of them need to be normalized
    sMatrix[0] =  sLocalContrast * pMatrix->MultCoef11;

    sMatrix[1] = sLocalContrast8 * ((sSatCosHue * pMatrix->MultCoef12) -
                                (sSatSinHue * pMatrix->MultCoef13));
    RSHFT(sMatrix[1],8); // normalization shift after additional contrast multiplication.

    sMatrix[2] = sLocalContrast8 * ((sSatSinHue * pMatrix->MultCoef12) +
                                (sSatCosHue * pMatrix->MultCoef13));
    RSHFT(sMatrix[2],8);

    sMatrix[3] = sLocalContrast * pMatrix->MultCoef21;

    sMatrix[4] = sLocalContrast8 * ((sSatCosHue * pMatrix->MultCoef22) -
                                (sSatSinHue * pMatrix->MultCoef23));
    RSHFT(sMatrix[4],8);

    sMatrix[5] = sLocalContrast8 * ((sSatSinHue * pMatrix->MultCoef22) +
                                (sSatCosHue * pMatrix->MultCoef23));
    RSHFT(sMatrix[5],8);

    sMatrix[6] = sLocalContrast * pMatrix->MultCoef31;

    sMatrix[7] = sLocalContrast8 * ((sSatCosHue * pMatrix->MultCoef32) -
                                (sSatSinHue * pMatrix->MultCoef33));
    RSHFT(sMatrix[7],8);

    sMatrix[8] = sLocalContrast8 * ((sSatSinHue * pMatrix->MultCoef32) +
                                (sSatCosHue * pMatrix->MultCoef33));
    RSHFT(sMatrix[8],8);
    /*
        RGB Gain adjusment (in YUV domain) will be used for color temperature adjustments

        Convert uint16_t to (from Unsigned 4.8 to Signed 4.12)
        fixed point format (0x100 equal 0x1.000)
        1 sign bit, 3 integer bits and 12 fraction bits.
        By shifting left 4 bits.

        Multiply by RGB gains for RGB gain adjustment in YUV domain
        (Signed 16.44) = (Signed 4.8) * (Signed 12.36)
    */
    sMatrix[0] = pAdjuster->GreenGain * sMatrix[0];
    sMatrix[1] = pAdjuster->GreenGain * sMatrix[1];
    sMatrix[2] = pAdjuster->GreenGain * sMatrix[2];
    sMatrix[3] = pAdjuster->BlueGain * sMatrix[3];
    sMatrix[4] = pAdjuster->BlueGain * sMatrix[4];
    sMatrix[5] = pAdjuster->BlueGain * sMatrix[5];
    sMatrix[6] = pAdjuster->RedGain * sMatrix[6];
    sMatrix[7] = pAdjuster->RedGain * sMatrix[7];
    sMatrix[8] = pAdjuster->RedGain * sMatrix[8];

    /*
        Write the coefficients
    */
    for (i = 0; i < 9; i++)
    {
        /*
            Right shifting int64_t (Signed 16.44) by TOTAL_R_SHIFTS bits
            to convert to format that the HW expects. This is achieved by shifting right
            TOTAL_R_SHIFTS bits in two stages: first shifting right by (TOTAL_R_SHIFTS - 1) bits,
            then round up, then continue to shift the last bit
        */
        RSHFT (sMatrix[i], TOTAL_R_SHIFTS-1);

        if (sMatrix[i] >= 0)
            sMatrix[i] += 1;/*round up*/
        else
            sMatrix[i] -= 1;/*round up*/
        RSHFT (sMatrix[i], 1);

        /* Truncate cell value to HW's expected value*/
        TRUNC_BOTH_BOUNDS(  sMatrix[i], MATRIX_COEFF_MIN, MATRIX_COEFF_MAX);
    }
    ColorMatrix_WriteMatrix(ColorMatrix_MatrixBaseAddress, sMatrix);

    return (FVDP_OK);
}
/*******************************************************************************
Name        : ColorMatrix_WriteMatrix
Description : Writes 9 color coefficients passe in the matrix, to the hardware registers

Parameters  : uint32_t ColorMatrix_MatrixBaseAddress - register base address, starting from offset1
              int64_t * pMatrix - pointer to the matrix containing 9 coefficients
Assumptions :
Limitations :
Returns     : void

*******************************************************************************/
static void ColorMatrix_WriteMatrix(uint32_t ColorMatrix_MatrixBaseAddress, int64_t * pMatrix)
{
    uint16_t i=0;
    CSC_REG32_Write(COEF_11 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_12 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_13 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_21 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_22 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_23 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_31 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_32 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
    CSC_REG32_Write(COEF_33 + ColorMatrix_MatrixBaseAddress, (int32_t)pMatrix[i++]);
}





/*******************************************************************************
Name        : ColorMatrix_GetColorRange
Description : Updates the color control level on the selected channel.

Parameters  : ColorControlSel selects the adjuster for the selected global color
              pMin Pointer to Min Value for the requested Control.
              pMax Pointer to Max Value for the requested Control
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
              FVDP_OK if no error
              FVDP_ERROR
*******************************************************************************/
FVDP_Result_t ColorMatrix_GetColorRange(FVDP_PSIControl_t  ColorControlSel, int32_t *pMin, int32_t *pMax, int32_t *pStep)
{
    FVDP_Result_t ErrorCode = FVDP_OK;

    *pStep = 1;

    switch (ColorControlSel)
    {
    case FVDP_BRIGHTNESS:
    case FVDP_RED_OFFSET:
    case FVDP_GREEN_OFFSET:
    case FVDP_BLUE_OFFSET:
        *pMin = MATRIX_OFFSET_MIN;
        *pMax = MATRIX_OFFSET_MAX;
        break;

    case FVDP_CONTRAST:
    case FVDP_SATURATION:
    case FVDP_RED_GAIN:
    case FVDP_GREEN_GAIN:
    case FVDP_BLUE_GAIN:
        *pMin = 0;
        *pMax = PARAM_COEFF_MAX;
        break;

    case FVDP_HUE:
        *pMin = (-360<<2);
        *pMax = (360<<2);
        break;

    default:
        ErrorCode = FVDP_ERROR;
        break;
    }
    return ErrorCode;
}

/*******************************************************************************
Name        : FVDP_Result_t ColorMatrix_MuteControl (FVDP_CH_t CH, bool options)
Description :    Mute output of CSC
Parameters  :
              ColorMatrix_MuteColor_t - black or other color to blank the screen
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t ColorMatrix_MuteControl (uint32_t ColorMatrix_MatrixBaseAddress, ColorMatrix_MuteColor_t color)
{

    CSC_REG32_ClearAndSet(OFFSET_IN_1 + ColorMatrix_MatrixBaseAddress, CCF_P_IN_OFFSET1_MASK, CCF_P_IN_OFFSET1_OFFSET, 0x0);
    CSC_REG32_ClearAndSet(OFFSET_IN_2 + ColorMatrix_MatrixBaseAddress, CCF_P_IN_OFFSET1_MASK, CCF_P_IN_OFFSET1_OFFSET, 0x0);
    CSC_REG32_ClearAndSet(OFFSET_IN_3 + ColorMatrix_MatrixBaseAddress, CCF_P_IN_OFFSET1_MASK, CCF_P_IN_OFFSET1_OFFSET, 0x0);

    CSC_REG32_Write(COEF_11 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_12 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_13 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_21 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_22 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_23 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_31 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_32 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write(COEF_33 + ColorMatrix_MatrixBaseAddress, 0);

    CSC_REG32_Write (OFFSET_OUT_1 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write (OFFSET_OUT_2 + ColorMatrix_MatrixBaseAddress, 0);
    CSC_REG32_Write (OFFSET_OUT_3 + ColorMatrix_MatrixBaseAddress, 0);

    if (color < MC_BLACK)
    {
        if (color == MC_RED) {
          CSC_REG32_Write (OFFSET_OUT_1 + ColorMatrix_MatrixBaseAddress, MATRIX_OFFSET_MAX/2);
        } else if (color == MC_GREEN) {
          CSC_REG32_Write (OFFSET_OUT_2 + ColorMatrix_MatrixBaseAddress, MATRIX_OFFSET_MAX/2);
        } else {
          CSC_REG32_Write (OFFSET_OUT_3 + ColorMatrix_MatrixBaseAddress, MATRIX_OFFSET_MAX/2);
        }
    }

    return (FVDP_OK);
}

/*
    Support higher precision (1/4 degree increments)
*/
static const int16_t  SinCos_Lut[361] =
{
    0x0000, 0x0012, 0x0024, 0x0036, 0x0047, 0x0059, 0x006B, 0x007D, 0x008F, 0x00A1,  /*  0.00 -  2.25*/
    0x00B3, 0x00C5, 0x00D6, 0x00E8, 0x00FA, 0x010C, 0x011E, 0x0130, 0x0141, 0x0153,  /*  2.50 -  4.75*/
    0x0165, 0x0177, 0x0189, 0x019A, 0x01AC, 0x01BE, 0x01D0, 0x01E1, 0x01F3, 0x0205,  /*  5.00 -  7.25*/
    0x0217, 0x0228, 0x023A, 0x024C, 0x025D, 0x026F, 0x0281, 0x0292, 0x02A4, 0x02B6,  /*  7.50 -  9.75*/
    0x02C7, 0x02D9, 0x02EA, 0x02FC, 0x030E, 0x031F, 0x0331, 0x0342, 0x0354, 0x0365,  /* 10.00 - 12.25*/
    0x0377, 0x0388, 0x0399, 0x03AB, 0x03BC, 0x03CE, 0x03DF, 0x03F0, 0x0402, 0x0413,  /* 12.50 - 14.75*/
    0x0424, 0x0435, 0x0447, 0x0458, 0x0469, 0x047A, 0x048B, 0x049C, 0x04AE, 0x04BF,  /* 15.00 - 17.25*/
    0x04D0, 0x04E1, 0x04F2, 0x0503, 0x0514, 0x0525, 0x0536, 0x0546, 0x0557, 0x0568,  /* 17.50 - 19.75*/
    0x0579, 0x058A, 0x059A, 0x05AB, 0x05BC, 0x05CD, 0x05DD, 0x05EE, 0x05FE, 0x060F,  /* 20.00 - 22.25*/
    0x061F, 0x0630, 0x0640, 0x0651, 0x0661, 0x0672, 0x0682, 0x0692, 0x06A3, 0x06B3,  /* 22.50 - 24.75*/
    0x06C3, 0x06D3, 0x06E3, 0x06F3, 0x0704, 0x0714, 0x0724, 0x0734, 0x0744, 0x0753,  /* 25.00 - 27.25*/
    0x0763, 0x0773, 0x0783, 0x0793, 0x07A2, 0x07B2, 0x07C2, 0x07D1, 0x07E1, 0x07F1,  /* 27.50 - 29.75*/
    0x0800, 0x080F, 0x081F, 0x082E, 0x083E, 0x084D, 0x085C, 0x086B, 0x087B, 0x088A,  /* 30.00 - 32.25*/
    0x0899, 0x08A8, 0x08B7, 0x08C6, 0x08D5, 0x08E4, 0x08F2, 0x0901, 0x0910, 0x091F,  /* 32.50 - 34.75*/
    0x092D, 0x093C, 0x094B, 0x0959, 0x0968, 0x0976, 0x0984, 0x0993, 0x09A1, 0x09AF,  /* 35.00 - 37.25*/
    0x09BD, 0x09CC, 0x09DA, 0x09E8, 0x09F6, 0x0A04, 0x0A12, 0x0A20, 0x0A2D, 0x0A3B,  /* 37.50 - 39.75*/
    0x0A49, 0x0A57, 0x0A64, 0x0A72, 0x0A7F, 0x0A8D, 0x0A9A, 0x0AA7, 0x0AB5, 0x0AC2,  /* 40.00 - 42.25*/
    0x0ACF, 0x0ADC, 0x0AE9, 0x0AF7, 0x0B04, 0x0B10, 0x0B1D, 0x0B2A, 0x0B37, 0x0B44,  /* 42.50 - 44.75*/
    0x0B50, 0x0B5D, 0x0B69, 0x0B76, 0x0B82, 0x0B8F, 0x0B9B, 0x0BA7, 0x0BB4, 0x0BC0,  /* 45.00 - 47.25*/
    0x0BCC, 0x0BD8, 0x0BE4, 0x0BF0, 0x0BFC, 0x0C08, 0x0C13, 0x0C1F, 0x0C2B, 0x0C36,  /* 47.50 - 49.75*/
    0x0C42, 0x0C4D, 0x0C59, 0x0C64, 0x0C6F, 0x0C7A, 0x0C86, 0x0C91, 0x0C9C, 0x0CA7,  /* 50.00 - 52.25*/
    0x0CB2, 0x0CBC, 0x0CC7, 0x0CD2, 0x0CDD, 0x0CE7, 0x0CF2, 0x0CFC, 0x0D07, 0x0D11,  /* 52.50 - 54.75*/
    0x0D1B, 0x0D25, 0x0D30, 0x0D3A, 0x0D44, 0x0D4E, 0x0D58, 0x0D61, 0x0D6B, 0x0D75,  /* 55.00 - 57.25*/
    0x0D7F, 0x0D88, 0x0D92, 0x0D9B, 0x0DA4, 0x0DAE, 0x0DB7, 0x0DC0, 0x0DC9, 0x0DD2,  /* 57.50 - 59.75*/
    0x0DDB, 0x0DE4, 0x0DED, 0x0DF6, 0x0DFE, 0x0E07, 0x0E10, 0x0E18, 0x0E21, 0x0E29,  /* 60.00 - 62.25*/
    0x0E31, 0x0E39, 0x0E42, 0x0E4A, 0x0E52, 0x0E5A, 0x0E61, 0x0E69, 0x0E71, 0x0E79,  /* 62.50 - 64.75*/
    0x0E80, 0x0E88, 0x0E8F, 0x0E97, 0x0E9E, 0x0EA5, 0x0EAC, 0x0EB3, 0x0EBA, 0x0EC1,  /* 65.00 - 67.25*/
    0x0EC8, 0x0ECF, 0x0ED6, 0x0EDC, 0x0EE3, 0x0EEA, 0x0EF0, 0x0EF6, 0x0EFD, 0x0F03,  /* 67.50 - 69.75*/
    0x0F09, 0x0F0F, 0x0F15, 0x0F1B, 0x0F21, 0x0F27, 0x0F2C, 0x0F32, 0x0F38, 0x0F3D,  /* 70.00 - 72.25*/
    0x0F42, 0x0F48, 0x0F4D, 0x0F52, 0x0F57, 0x0F5C, 0x0F61, 0x0F66, 0x0F6B, 0x0F70,  /* 72.50 - 74.75*/
    0x0F74, 0x0F79, 0x0F7E, 0x0F82, 0x0F86, 0x0F8B, 0x0F8F, 0x0F93, 0x0F97, 0x0F9B,  /* 75.00 - 77.25*/
    0x0F9F, 0x0FA3, 0x0FA6, 0x0FAA, 0x0FAE, 0x0FB1, 0x0FB5, 0x0FB8, 0x0FBB, 0x0FBF,  /* 77.50 - 79.75*/
    0x0FC2, 0x0FC5, 0x0FC8, 0x0FCB, 0x0FCE, 0x0FD0, 0x0FD3, 0x0FD6, 0x0FD8, 0x0FDB,  /* 80.00 - 82.25*/
    0x0FDD, 0x0FDF, 0x0FE1, 0x0FE4, 0x0FE6, 0x0FE8, 0x0FEA, 0x0FEB, 0x0FED, 0x0FEF,  /* 82.50 - 84.75*/
    0x0FF0, 0x0FF2, 0x0FF3, 0x0FF5, 0x0FF6, 0x0FF7, 0x0FF8, 0x0FF9, 0x0FFA, 0x0FFB,  /* 85.00 - 87.25*/
    0x0FFC, 0x0FFD, 0x0FFE, 0x0FFE, 0x0FFF, 0x0FFF, 0x0FFF, 0x1000, 0x1000, 0x1000,  /* 87.50 - 89.75*/
    0x1000
};


/*******************************************************************************
Name        : Sine
Description : Calculates result of sine given a degree.
              The function uses a look up table that stores the value
              of every integer angle in the first quadrant - Q1 [0..90]
              degrees. The function outputs a signed 4.12 value.
Parameters  : Degree - Angle (in degrees) multiplied by 4 to get more resolution
Assumptions :
Limitations :
Returns     : int16_t type value
              - Result of sine given a degree
*******************************************************************************/
static int16_t Sine (int16_t Degree)
{
    int16_t index;
    int16_t r;

    /* Move angle to range 0 .. 360*/
    Degree %= (360 << 2);
    if (Degree < 0)
        Degree += (360 << 2);

    /* Find equivalent angle in range 0 .. 90*/
    if (Degree <= (90 << 2))
        index = Degree;
    else if (Degree <= (180 << 2))
        index = (180 << 2) - Degree;
    else if (Degree <= (270 << 2))
        index = Degree - (180 << 2);
    else
        index = (360 << 2) - Degree;

    /* Lookup value of sin(degree)*/
    r = SinCos_Lut[index];

    if(Degree >= (180 << 2))
        return(-r);
    else
        return(r);
}

/***********************************************************************
Name        : Cosine
Description : Calculates result of cosine given a degree.
              The function uses a look up table that stores the value
              of every integer angle in the first quadrant - Q1 [0..90]
              degrees. The function outputs a signed 4.12 value.
Parameters  : Degree - Angle (in degrees) multiplied by 4 to get more resolution
Assumptions :
Limitations :
Returns     : int16_t type value
              - Result of cosine given a degree
***********************************************************************/
static int16_t Cosine (int16_t Degree)
{
    return (Sine(Degree + (90 << 2)));
}
