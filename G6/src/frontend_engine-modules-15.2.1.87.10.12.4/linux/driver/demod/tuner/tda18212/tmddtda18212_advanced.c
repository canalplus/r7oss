/**********************************************************************
Copyright (C) 2006-2009 NXP B.V.



This program is free software: you can redistribute it and/or modify

it under the terms of the GNU General Public License as published by

the Free Software Foundation, either version 3 of the License, or

(at your option) any later version.



This program is distributed in the hope that it will be useful,

but WITHOUT ANY WARRANTY; without even the implied warranty of

MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

GNU General Public License for more details.



You should have received a copy of the GNU General Public License

along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************/
/* --------------------------------------------------------------------------
 * FILE NAME:    tmddTDA182I2_Alt.c
 *
 * DESCRIPTION:  TDA182I2 standard APIs
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
#include <linux/kernel.h>
#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmbslfrontendtypes.h"

#include "tmddtda18212.h"
#include "tmddtda18212local.h"

#include "tmddtda18212instance.h"

/*-----------------------------------------------------------------------------
 * Project include files:
 * ----------------------------------------------------------------------------
 */

/*-----------------------------------------------------------------------------
 * Types and defines:
 * ---------------------------------------------------------------------------
 */

/*-----------------------------------------------------------------------------
 * Global data:
 * ----------------------------------------------------------------------------
 */

/* -------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetTM_D:
 *
 * DESCRIPTION: Get the TM_D bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t tmddTDA182I2GetTM_D(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* switch thermometer on */
		pObj->I2CMap.uBx04.bF.TM_ON = 1;

		/* write byte 0x04 */
		err = ddTDA182I2Write(pObj, 0x04, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* read byte 0x03 */
			err = ddTDA182I2Read(pObj, 0x03, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Read(0x%08X) failed.", tUnit));
		}

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx03.bF.TM_D;

			/* switch thermometer off */
			pObj->I2CMap.uBx04.bF.TM_ON = 0;

			/* write byte 0x04 */
			err = ddTDA182I2Write(pObj, 0x04, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetTM_ON:
 *
 * DESCRIPTION: Set the TM_ON bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmddTDA182I2SetTM_ON(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx04.bF.TM_ON = uValue;

		/* write byte 0x04 */
		err = ddTDA182I2Write(pObj, 0x04, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetTM_ON:
 *
 * DESCRIPTION: Get the TM_ON bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmddTDA182I2GetTM_ON(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x04 */
		err = ddTDA182I2Read(pObj, 0x04, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx04.bF.TM_ON;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPOR:
 *
 * DESCRIPTION: Get the POR bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmddTDA182I2GetPOR(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	 unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x05 */
		err = ddTDA182I2Read(pObj, 0x05, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx05.bF.POR;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RSSI_End:
 *
 * DESCRIPTION: Get the MSM_RSSI_End bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmddTDA182I2GetMSM_RSSI_End(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x08 */
		err = ddTDA182I2Read(pObj, 0x08, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx08.bF.MSM_RSSI_End;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_LOCalc_End:
 *
 * DESCRIPTION: Get the MSM_LOCalc_End bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_End(tmUnitSelect_t tUnit,/*I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x08 */
		err = ddTDA182I2Read(pObj, 0x08, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx08.bF.MSM_LOCalc_End;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RFCal_End:
 *
 * DESCRIPTION: Get the MSM_RFCal_End bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RFCal_End(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x08 */
		err = ddTDA182I2Read(pObj, 0x08, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx08.bF.MSM_RFCal_End;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_IRCAL_End:
 *
 * DESCRIPTION: Get the MSM_IRCAL_End bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_End(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x08 */
		err = ddTDA182I2Read(pObj, 0x08, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx08.bF.MSM_IRCAL_End;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RCCal_End:
 *
 * DESCRIPTION: Get the MSM_RCCal_End bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RCCal_End(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x08 */
		err = ddTDA182I2Read(pObj, 0x08, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx08.bF.MSM_RCCal_End;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIRQ_Enable:
 *
 * DESCRIPTION: Set the IRQ_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIRQ_Enable(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.IRQ_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIRQ_Enable:
 *
 * DESCRIPTION: Get the IRQ_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIRQ_Enable(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.IRQ_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetXtalCal_Enable:
 *
 * DESCRIPTION: Set the XtalCal_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetXtalCal_Enable(tmUnitSelect_t tUnit,/*I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.XtalCal_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetXtalCal_Enable:
 *
 * DESCRIPTION: Get the XtalCal_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetXtalCal_Enable(tmUnitSelect_t tUnit,/*I: Unit no.*/
	 unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.XtalCal_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RSSI_Enable:
 *
 * DESCRIPTION: Set the MSM_RSSI_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RSSI_Enable(tmUnitSelect_t tUnit,/*I:Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.MSM_RSSI_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RSSI_Enable:
 *
 * DESCRIPTION: Get the MSM_RSSI_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RSSI_Enable(tmUnitSelect_t tUnit,/*I:Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.MSM_RSSI_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_LOCalc_Enable:
 *
 * DESCRIPTION: Set the MSM_LOCalc_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_LOCalc_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.MSM_LOCalc_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_LOCalc_Enable:
 *
 * DESCRIPTION: Get the MSM_LOCalc_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.MSM_LOCalc_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RFCAL_Enable:
 *
 * DESCRIPTION: Set the MSM_RFCAL_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RFCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.MSM_RFCAL_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RFCAL_Enable:
 *
 * DESCRIPTION: Get the MSM_RFCAL_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RFCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.MSM_RFCAL_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_IRCAL_Enable:
 *
 * DESCRIPTION: Set the MSM_IRCAL_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_IRCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.MSM_IRCAL_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_IRCAL_Enable:
 *
 * DESCRIPTION: Get the MSM_IRCAL_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.MSM_IRCAL_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RCCal_Enable:
 *
 * DESCRIPTION: Set the MSM_RCCal_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RCCal_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx09.bF.MSM_RCCal_Enable = uValue;

		/* write byte 0x09 */
		err = ddTDA182I2Write(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RCCal_Enable:
 *
 * DESCRIPTION: Get the MSM_RCCal_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RCCal_Enable(tmUnitSelect_t tUnit,/*I:Unit #*/
	 unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x09 */
		err = ddTDA182I2Read(pObj, 0x09, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx09.bF.MSM_RCCal_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetXtalCal_Clear:
 *
 * DESCRIPTION: Set the XtalCal_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetXtalCal_Clear(tmUnitSelect_t tUnit,/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0A.bF.XtalCal_Clear = uValue;

		/* write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetXtalCal_Clear:
 *
 * DESCRIPTION: Get the XtalCal_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetXtalCal_Clear(tmUnitSelect_t tUnit,/* I: Unit no.*/
	   unsigned char *puValue/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0A */
		err = ddTDA182I2Read(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0A.bF.XtalCal_Clear;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RSSI_Clear:
 *
 * DESCRIPTION: Set the MSM_RSSI_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RSSI_Clear(tmUnitSelect_t tUnit,/*I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0A.bF.MSM_RSSI_Clear = uValue;

		/* write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RSSI_Clear:
 *
 * DESCRIPTION: Get the MSM_RSSI_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RSSI_Clear(tmUnitSelect_t tUnit,/*I: Unit no.*/
	  unsigned char *puValue/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0A */
		err = ddTDA182I2Read(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0A.bF.MSM_RSSI_Clear;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_LOCalc_Clear:
 *
 * DESCRIPTION: Set the MSM_LOCalc_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_LOCalc_Clear(tmUnitSelect_t tUnit,/*I:Unit #*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0A.bF.MSM_LOCalc_Clear = uValue;

		/* write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_LOCalc_Clear:
 *
 * DESCRIPTION: Get the MSM_LOCalc_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_Clear(tmUnitSelect_t tUnit,/*I: Unit #*/
	unsigned char *puValue/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0A */
		err = ddTDA182I2Read(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0A.bF.MSM_LOCalc_Clear;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RFCal_Clear:
 *
 * DESCRIPTION: Set the MSM_RFCal_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RFCal_Clear(tmUnitSelect_t tUnit,/*I:Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0A.bF.MSM_RFCal_Clear = uValue;

		/* write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RFCal_Clear:
 *
 * DESCRIPTION: Get the MSM_RFCal_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RFCal_Clear(tmUnitSelect_t tUnit,/*I:Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0A */
		err = ddTDA182I2Read(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK)
			/* get value */
			*puValue = pObj->I2CMap.uBx0A.bF.MSM_RFCal_Clear;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_IRCAL_Clear:
 *
 * DESCRIPTION: Set the MSM_IRCAL_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_IRCAL_Clear(tmUnitSelect_t tUnit,/*I:Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0A.bF.MSM_IRCAL_Clear = uValue;

		/* write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_IRCAL_Clear:
 *
 * DESCRIPTION: Get the MSM_IRCAL_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_Clear(tmUnitSelect_t tUnit,/*I:Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0A */
		err = ddTDA182I2Read(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0A.bF.MSM_IRCAL_Clear;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RCCal_Clear:
 *
 * DESCRIPTION: Set the MSM_RCCal_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RCCal_Clear(tmUnitSelect_t tUnit,/*I:Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0A.bF.MSM_RCCal_Clear = uValue;

		/* write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RCCal_Clear:
 *
 * DESCRIPTION: Get the MSM_RCCal_Clear bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RCCal_Clear(tmUnitSelect_t tUnit,/*I: Unit no*/
	unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0A */
		err = ddTDA182I2Read(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0A.bF.MSM_RCCal_Clear;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIRQ_Set:
 *
 * DESCRIPTION: Set the IRQ_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIRQ_Set(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.IRQ_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIRQ_Set:
 *
 * DESCRIPTION: Get the IRQ_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIRQ_Set(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0B.bF.IRQ_Set;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetXtalCal_Set:
 *
 * DESCRIPTION: Set the XtalCal_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetXtalCal_Set(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.XtalCal_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetXtalCal_Set:
 *
 * DESCRIPTION: Get the XtalCal_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetXtalCal_Set(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	 unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0B.bF.XtalCal_Set;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RSSI_Set:
 *
 * DESCRIPTION: Set the MSM_RSSI_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RSSI_Set(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.MSM_RSSI_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RSSI_Set:
 *
 * DESCRIPTION: Get the MSM_RSSI_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RSSI_Set(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0B.bF.MSM_RSSI_Set;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_LOCalc_Set:
 *
 * DESCRIPTION: Set the MSM_LOCalc_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_LOCalc_Set(tmUnitSelect_t tUnit,/* I:Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.MSM_LOCalc_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_LOCalc_Set:
 *
 * DESCRIPTION: Get the MSM_LOCalc_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_LOCalc_Set(tmUnitSelect_t tUnit,/* I:Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		/* get value */
		*puValue = pObj->I2CMap.uBx0B.bF.MSM_LOCalc_Set;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RFCal_Set:
 *
 * DESCRIPTION: Set the MSM_RFCal_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RFCal_Set(tmUnitSelect_t tUnit,/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.MSM_RFCal_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RFCal_Set:
 *
 * DESCRIPTION: Get the MSM_RFCal_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RFCal_Set(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0B.bF.MSM_RFCal_Set;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_IRCAL_Set:
 *
 * DESCRIPTION: Set the MSM_IRCAL_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_IRCAL_Set(tmUnitSelect_t tUnit,/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.MSM_IRCAL_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_IRCAL_Set:
 *
 * DESCRIPTION: Get the MSM_IRCAL_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_IRCAL_Set(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0B.bF.MSM_IRCAL_Set;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetMSM_RCCal_Set:
 *
 * DESCRIPTION: Set the MSM_RCCal_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetMSM_RCCal_Set(tmUnitSelect_t tUnit,/*I:Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0B.bF.MSM_RCCal_Set = uValue;

		/* write byte 0x0B */
		err = ddTDA182I2Write(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetMSM_RCCal_Set:
 *
 * DESCRIPTION: Get the MSM_RCCal_Set bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetMSM_RCCal_Set(tmUnitSelect_t tUnit,/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0B */
		err = ddTDA182I2Read(pObj, 0x0B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0B.bF.MSM_RCCal_Set;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetLT_Enable:
 *
 * DESCRIPTION: Set the LT_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetLT_Enable(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0C.bF.LT_Enable = uValue;

		/* write byte 0x0C */
		err = ddTDA182I2Write(pObj, 0x0C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetLT_Enable:
 *
 * DESCRIPTION: Get the LT_Enable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetLT_Enable(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0C */
		err = ddTDA182I2Read(pObj, 0x0C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0C.bF.LT_Enable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC1_6_15dB:
 *
 * DESCRIPTION: Set the AGC1_6_15dB bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetAGC1_6_15dB(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0C.bF.AGC1_6_15dB = uValue;

		/* write byte 0x0C */
		err = ddTDA182I2Write(pObj, 0x0C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC1_6_15dB:
 *
 * DESCRIPTION: Get the AGC1_6_15dB bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetAGC1_6_15dB(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0C */
		err = ddTDA182I2Read(pObj, 0x0C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0C.bF.AGC1_6_15dB;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGCs_Up_Step_assym:
 *
 * DESCRIPTION: Set the AGCs_Up_Step_assym bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetAGCs_Up_Step_assym(tmUnitSelect_t tUnit,/*I:Unit#*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0E.bF.AGCs_Up_Step_assym = uValue;

		/* write byte 0x0E */
		err = ddTDA182I2Write(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGCs_Up_Step_assym:
 *
 * DESCRIPTION: Get the AGCs_Up_Step_assym bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetAGCs_Up_Step_assym(tmUnitSelect_t tUnit,/*I:Unit#*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0E */
		err = ddTDA182I2Read(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0E.bF.AGCs_Up_Step_assym;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPulse_Shaper_Disable:
 *
 * DESCRIPTION: Set the Pulse_Shaper_Disable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetPulse_Shaper_Disable(tmUnitSelect_t tUnit,/*I:Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0E.bF.Pulse_Shaper_Disable = uValue;

		/* write byte 0x0E */
		err = ddTDA182I2Write(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPulse_Shaper_Disable:
 *
 * DESCRIPTION: Get the Pulse_Shaper_Disable bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetPulse_Shaper_Disable(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	  unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0E */
		err = ddTDA182I2Read(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0E.bF.Pulse_Shaper_Disable;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFAGC_Low_BW:
 *
 * DESCRIPTION: Set the RFAGC_Low_BW bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFAGC_Low_BW(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0F.bF.RFAGC_Low_BW = uValue;

		/* write byte 0x0F */
		err = ddTDA182I2Write(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFAGC_Low_BW:
 *
 * DESCRIPTION: Get the RFAGC_Low_BW bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFAGC_Low_BW(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	  unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x0F */
		err = ddTDA182I2Read(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx0F.bF.RFAGC_Low_BW;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGCs_Do_Step_assym:
 *
 * DESCRIPTION: Set the AGCs_Do_Step_assym bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetAGCs_Do_Step_assym(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx11.bF.AGCs_Do_Step_assym = uValue;

		/* write byte 0x11 */
		err = ddTDA182I2Write(pObj, 0x11, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGCs_Do_Step_assym:
 *
 * DESCRIPTION: Get the AGCs_Do_Step_assym bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGCs_Do_Step_assym(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x11 */
		err = ddTDA182I2Read(pObj, 0x11, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx11.bF.AGCs_Do_Step_assym;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetI2C_Clock_Mode:
 *
 * DESCRIPTION: Set the I2C_Clock_Mode bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetI2C_Clock_Mode(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx14.bF.I2C_Clock_Mode = uValue;

		/* write byte 0x14 */
		err = ddTDA182I2Write(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetI2C_Clock_Mode:
 *
 * DESCRIPTION: Get the I2C_Clock_Mode bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetI2C_Clock_Mode(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x14 */
		err = ddTDA182I2Read(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx14.bF.I2C_Clock_Mode;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetXtalOsc_AnaReg_En:
 *
 * DESCRIPTION: Set the XtalOsc_AnaReg_En bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetXtalOsc_AnaReg_En(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx14.bF.XtalOsc_AnaReg_En = uValue;

		/* write byte 0x14 */
		err = ddTDA182I2Write(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetXtalOsc_AnaReg_En:
 *
 * DESCRIPTION: Get the XtalOsc_AnaReg_En bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetXtalOsc_AnaReg_En(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x14 */
		err = ddTDA182I2Read(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx14.bF.XtalOsc_AnaReg_En;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetXTout:
 *
 * DESCRIPTION: Set the XTout bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetXTout(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx14.bF.XTout = uValue;

		/* write byte 0x14 */
		err = ddTDA182I2Write(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetXTout:
 *
 * DESCRIPTION: Get the XTout bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetXTout(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x14 */
		err = ddTDA182I2Read(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx14.bF.XTout;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRSSI_Meas:
 *
 * DESCRIPTION: Set the RSSI_Meas bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRSSI_Meas(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.RSSI_Meas = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRSSI_Meas:
 *
 * DESCRIPTION: Get the RSSI_Meas bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRSSI_Meas(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.RSSI_Meas;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRF_CAL_AV:
 *
 * DESCRIPTION: Set the RF_CAL_AV bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRF_CAL_AV(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.RF_CAL_AV = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRF_CAL_AV:
 *
 * DESCRIPTION: Get the RF_CAL_AV bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRF_CAL_AV(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.RF_CAL_AV;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRF_CAL:
 *
 * DESCRIPTION: Set the RF_CAL bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRF_CAL(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.RF_CAL = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRF_CAL:
 *
 * DESCRIPTION: Get the RF_CAL bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRF_CAL(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.RF_CAL;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_CAL_Loop:
 *
 * DESCRIPTION: Set the IR_CAL_Loop bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIR_CAL_Loop(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.IR_CAL_Loop = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_CAL_Loop:
 *
 * DESCRIPTION: Get the IR_CAL_Loop bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIR_CAL_Loop(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;
	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.IR_CAL_Loop;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_Cal_Image:
 *
 * DESCRIPTION: Set the IR_Cal_Image bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIR_Cal_Image(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.IR_Cal_Image = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_Cal_Image:
 *
 * DESCRIPTION: Get the IR_Cal_Image bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIR_Cal_Image(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.IR_Cal_Image;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_CAL_Wanted:
 *
 * DESCRIPTION: Set the IR_CAL_Wanted bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetIR_CAL_Wanted(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.IR_CAL_Wanted = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_CAL_Wanted:
 *
 * DESCRIPTION: Get the IR_CAL_Wanted bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetIR_CAL_Wanted(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.IR_CAL_Wanted;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRC_Cal:
 *
 * DESCRIPTION: Set the RC_Cal bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRC_Cal(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.RC_Cal = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRC_Cal:
 *
 * DESCRIPTION: Get the RC_Cal bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRC_Cal(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.RC_Cal;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetCalc_PLL:
 *
 * DESCRIPTION: Set the Calc_PLL bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetCalc_PLL(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx19.bF.Calc_PLL = uValue;

		/* read byte 0x19 */
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetCalc_PLL:
 *
 * DESCRIPTION: Get the Calc_PLL bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetCalc_PLL(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x19 */
		err = ddTDA182I2Read(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx19.bF.Calc_PLL;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetXtalCal_Launch:
 *
 * DESCRIPTION: Set the XtalCal_Launch bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetXtalCal_Launch(tmUnitSelect_t tUnit	/* I: Unit no.*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1A.bF.XtalCal_Launch = 1;

		/* write byte 0x1A */
		err = ddTDA182I2Write(pObj, 0x1A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		/* reset XtalCal_Launch (buffer only, no write) */
		pObj->I2CMap.uBx1A.bF.XtalCal_Launch = 0;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetXtalCal_Launch:
 *
 * DESCRIPTION: Get the XtalCal_Launch bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetXtalCal_Launch(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1A */
		err = ddTDA182I2Read(pObj, 0x1A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1A.bF.XtalCal_Launch;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPSM_AGC1:
 *
 * DESCRIPTION: Set the PSM_AGC1 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPSM_AGC1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1B.bF.PSM_AGC1 = uValue;

		/* read byte 0x1B */
		err = ddTDA182I2Write(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPSM_AGC1:
 *
 * DESCRIPTION: Get the PSM_AGC1 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPSM_AGC1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1B */
		err = ddTDA182I2Read(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1B.bF.PSM_AGC1;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPSMRFpoly:
 *
 * DESCRIPTION: Set the PSMRFpoly bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPSMRFpoly(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1B.bF.PSMRFpoly = uValue;

		/* read byte 0x1B */
		err = ddTDA182I2Write(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPSMRFpoly:
 *
 * DESCRIPTION: Get the PSMRFpoly bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPSMRFpoly(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1B */
		err = ddTDA182I2Read(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		/* get value */
		*puValue = pObj->I2CMap.uBx1B.bF.PSMRFpoly;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPSM_Mixer:
 *
 * DESCRIPTION: Set the PSM_Mixer bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPSM_Mixer(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1B.bF.PSM_Mixer = uValue;

		/* read byte 0x1B */
		err = ddTDA182I2Write(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPSM_Mixer:
 *
 * DESCRIPTION: Get the PSM_Mixer bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPSM_Mixer(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1B */
		err = ddTDA182I2Read(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1B.bF.PSM_Mixer;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPSM_Ifpoly:
 *
 * DESCRIPTION: Set the PSM_Ifpoly bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPSM_Ifpoly(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1B.bF.PSM_Ifpoly = uValue;

		/* read byte 0x1B */
		err = ddTDA182I2Write(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPSM_Ifpoly:
 *
 * DESCRIPTION: Get the PSM_Ifpoly bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPSM_Ifpoly(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1B */
		err = ddTDA182I2Read(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1B.bF.PSM_Ifpoly;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPSM_Lodriver:
 *
 * DESCRIPTION: Set the PSM_Lodriver bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPSM_Lodriver(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1B.bF.PSM_Lodriver = uValue;

		/* read byte 0x1B */
		err = ddTDA182I2Write(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPSM_Lodriver:
 *
 * DESCRIPTION: Get the PSM_Lodriver bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPSM_Lodriver(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1B */
		err = ddTDA182I2Read(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1B.bF.PSM_Lodriver;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetDCC_Bypass:
 *
 * DESCRIPTION: Set the DCC_Bypass bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetDCC_Bypass(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1C.bF.DCC_Bypass = uValue;

		/* read byte 0x1C */
		err = ddTDA182I2Write(pObj, 0x1C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDCC_Bypass:
 *
 * DESCRIPTION: Get the DCC_Bypass bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDCC_Bypass(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1C */
		err = ddTDA182I2Read(pObj, 0x1C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1C.bF.DCC_Bypass;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetDCC_Slow:
 *
 * DESCRIPTION: Set the DCC_Slow bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetDCC_Slow(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1C.bF.DCC_Slow = uValue;

		/* read byte 0x1C */
		err = ddTDA182I2Write(pObj, 0x1C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDCC_Slow:
 *
 * DESCRIPTION: Get the DCC_Slow bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDCC_Slow(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1C */
		err = ddTDA182I2Read(pObj, 0x1C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1C.bF.DCC_Slow;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetDCC_psm:
 *
 * DESCRIPTION: Set the DCC_psm bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetDCC_psm(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1C.bF.DCC_psm = uValue;

		/* read byte 0x1C */
		err = ddTDA182I2Write(pObj, 0x1C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDCC_psm:
 *
 * DESCRIPTION: Get the DCC_psm bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDCC_psm(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1C */
		err = ddTDA182I2Read(pObj, 0x1C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1C.bF.DCC_psm;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_GStep:
 *
 * DESCRIPTION: Set the IR_GStep bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIR_GStep(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1E.bF.IR_GStep = uValue - 40;

		/* read byte 0x1E */
		err = ddTDA182I2Write(pObj, 0x1E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_GStep:
 *
 * DESCRIPTION: Get the IR_GStep bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIR_GStep(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1E */
		err = ddTDA182I2Read(pObj, 0x1E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1E.bF.IR_GStep + 40;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_FreqLow_Sel:
 *
 * DESCRIPTION: Set the IR_FreqLow_Sel bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetIR_FreqLow_Sel(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1F.bF.IR_FreqLow_Sel = uValue;

		/* read byte 0x1F */
		err = ddTDA182I2Write(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_FreqLow_Sel:
 *
 * DESCRIPTION: Get the IR_FreqLow_Sel bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetIR_FreqLow_Sel(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1F */
		err = ddTDA182I2Read(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1F.bF.IR_FreqLow_Sel;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_FreqLow:
 *
 * DESCRIPTION: Set the IR_FreqLow bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIR_FreqLow(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1F.bF.IR_FreqLow = uValue;

		/* read byte 0x1F */
		err = ddTDA182I2Write(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_FreqLow:
 *
 * DESCRIPTION: Get the IR_FreqLow bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIR_FreqLow(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1F */
		err = ddTDA182I2Read(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1F.bF.IR_FreqLow;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_FreqMid:
 *
 * DESCRIPTION: Set the IR_FreqMid bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIR_FreqMid(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx20.bF.IR_FreqMid = uValue;

		/* read byte 0x20 */
		err = ddTDA182I2Write(pObj, 0x20, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_FreqMid:
 *
 * DESCRIPTION: Get the IR_FreqMid bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIR_FreqMid(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x20 */
		err = ddTDA182I2Read(pObj, 0x20, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx20.bF.IR_FreqMid;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetCoarse_IR_FreqHigh:
 *
 * DESCRIPTION: Set the Coarse_IR_FreqHigh bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetCoarse_IR_FreqHigh(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx21.bF.Coarse_IR_FreqHigh = uValue;

		/* write byte 0x21 */
		err = ddTDA182I2Write(pObj, 0x21, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetCoarse_IR_FreqHigh:
 *
 * DESCRIPTION: Get the Coarse_IR_FreqHigh bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetCoarse_IR_FreqHigh(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x21 */
		err = ddTDA182I2Read(pObj, 0x21, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx21.bF.Coarse_IR_FreqHigh;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_FreqHigh:
 *
 * DESCRIPTION: Set the IR_FreqHigh bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIR_FreqHigh(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx21.bF.IR_FreqHigh = uValue;

		/* read byte 0x21 */
		err = ddTDA182I2Write(pObj, 0x21, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_FreqHigh:
 *
 * DESCRIPTION: Get the IR_FreqHigh bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIR_FreqHigh(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x21 */
		err = ddTDA182I2Read(pObj, 0x21, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx21.bF.IR_FreqHigh;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPD_Vsync_Mgt:
 *
 * DESCRIPTION: Set the PD_Vsync_Mgt bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPD_Vsync_Mgt(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx22.bF.PD_Vsync_Mgt = uValue;

		/* write byte 0x22 */
		err = ddTDA182I2Write(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPD_Vsync_Mgt:
 *
 * DESCRIPTION: Get the PD_Vsync_Mgt bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPD_Vsync_Mgt(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x22 */
		err = ddTDA182I2Read(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx22.bF.PD_Vsync_Mgt;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetPD_Ovld:
 *
 * DESCRIPTION: Set the PD_Ovld bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetPD_Ovld(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx22.bF.PD_Ovld = uValue;

		/* write byte 0x22 */
		err = ddTDA182I2Write(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetPD_Ovld:
 *
 * DESCRIPTION: Get the PD_Ovld bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetPD_Ovld(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x22 */
		err = ddTDA182I2Read(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx22.bF.PD_Ovld;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC_Ovld_Timer:
 *
 * DESCRIPTION: Set the AGC_Ovld_Timer bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetAGC_Ovld_Timer(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx22.bF.AGC_Ovld_Timer = uValue;

		/* write byte 0x22 */
		err = ddTDA182I2Write(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC_Ovld_Timer:
 *
 * DESCRIPTION: Get the AGC_Ovld_Timer bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC_Ovld_Timer(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x22 */
		err = ddTDA182I2Read(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx22.bF.AGC_Ovld_Timer;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_Mixer_loop_off:
 *
 * DESCRIPTION: Set the IR_Mixer_loop_off bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetIR_Mixer_loop_off(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx23.bF.IR_Mixer_loop_off = uValue;

		/* read byte 0x23 */
		err = ddTDA182I2Write(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_Mixer_loop_off:
 *
 * DESCRIPTION: Get the IR_Mixer_loop_off bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetIR_Mixer_loop_off(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x23 */
		err = ddTDA182I2Read(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx23.bF.IR_Mixer_loop_off;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIR_Mixer_Do_step:
 *
 * DESCRIPTION: Set the IR_Mixer_Do_step bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetIR_Mixer_Do_step(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			      unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx23.bF.IR_Mixer_Do_step = uValue;

		/* read byte 0x23 */
		err = ddTDA182I2Write(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIR_Mixer_Do_step:
 *
 * DESCRIPTION: Get the IR_Mixer_Do_step bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetIR_Mixer_Do_step(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x23 */
		err = ddTDA182I2Read(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx23.bF.IR_Mixer_Do_step;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC1_loop_off:
 *
 * DESCRIPTION: Set the AGC1_loop_off bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetAGC1_loop_off(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx24.bF.AGC1_loop_off = uValue;

		/* read byte 0x24 */
		err = ddTDA182I2Write(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC1_loop_off:
 *
 * DESCRIPTION: Get the AGC1_loop_off bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC1_loop_off(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x24 */
		err = ddTDA182I2Read(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx24.bF.AGC1_loop_off;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC1_Do_step:
 *
 * DESCRIPTION: Set the AGC1_Do_step bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetAGC1_Do_step(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx24.bF.AGC1_Do_step = uValue;

		/* read byte 0x24 */
		err = ddTDA182I2Write(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC1_Do_step:
 *
 * DESCRIPTION: Get the AGC1_Do_step bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetAGC1_Do_step(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x24 */
		err = ddTDA182I2Read(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx24.bF.AGC1_Do_step;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetForce_AGC1_gain:
 *
 * DESCRIPTION: Set the Force_AGC1_gain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetForce_AGC1_gain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx24.bF.Force_AGC1_gain = uValue;

		/* read byte 0x24 */
		err = ddTDA182I2Write(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetForce_AGC1_gain:
 *
 * DESCRIPTION: Get the Force_AGC1_gain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetForce_AGC1_gain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x24 */
		err = ddTDA182I2Read(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx24.bF.Force_AGC1_gain;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC1_Gain:
 *
 * DESCRIPTION: Set the AGC1_Gain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetAGC1_Gain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx24.bF.AGC1_Gain = uValue;

		/* read byte 0x24 */
		err = ddTDA182I2Write(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC1_Gain:
 *
 * DESCRIPTION: Get the AGC1_Gain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetAGC1_Gain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x24 */
		err = ddTDA182I2Read(pObj, 0x24, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx24.bF.AGC1_Gain;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq0:
 *
 * DESCRIPTION: Set the RFCAL_Freq0 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq0(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx26.bF.RFCAL_Freq0 = uValue;

		/* read byte 0x26 */
		err = ddTDA182I2Write(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq0:
 *
 * DESCRIPTION: Get the RFCAL_Freq0 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq0(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x26 */
		err = ddTDA182I2Read(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx26.bF.RFCAL_Freq0;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq1:
 *
 * DESCRIPTION: Set the RFCAL_Freq1 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx26.bF.RFCAL_Freq1 = uValue;

		/* read byte 0x26 */
		err = ddTDA182I2Write(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq1:
 *
 * DESCRIPTION: Get the RFCAL_Freq1 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x26 */
		err = ddTDA182I2Read(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx26.bF.RFCAL_Freq1;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq2:
 *
 * DESCRIPTION: Set the RFCAL_Freq2 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx27.bF.RFCAL_Freq2 = uValue;

		/* read byte 0x27 */
		err = ddTDA182I2Write(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq2:
 *
 * DESCRIPTION: Get the RFCAL_Freq2 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x27 */
		err = ddTDA182I2Read(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx27.bF.RFCAL_Freq2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq3:
 *
 * DESCRIPTION: Set the RFCAL_Freq3 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq3(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx27.bF.RFCAL_Freq3 = uValue;

		/* read byte 0x27 */
		err = ddTDA182I2Write(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq3:
 *
 * DESCRIPTION: Get the RFCAL_Freq3 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq3(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x27 */
		err = ddTDA182I2Read(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx27.bF.RFCAL_Freq3;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq4:
 *
 * DESCRIPTION: Set the RFCAL_Freq4 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx28.bF.RFCAL_Freq4 = uValue;

		/* read byte 0x28 */
		err = ddTDA182I2Write(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq4:
 *
 * DESCRIPTION: Get the RFCAL_Freq4 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x28 */
		err = ddTDA182I2Read(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx28.bF.RFCAL_Freq4;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq5:
 *
 * DESCRIPTION: Set the RFCAL_Freq5 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx28.bF.RFCAL_Freq5 = uValue;

		/* read byte 0x28 */
		err = ddTDA182I2Write(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq5:
 *
 * DESCRIPTION: Get the RFCAL_Freq5 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x28 */
		err = ddTDA182I2Read(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx28.bF.RFCAL_Freq5;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq6:
 *
 * DESCRIPTION: Set the RFCAL_Freq6 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq6(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx29.bF.RFCAL_Freq6 = uValue;

		/* read byte 0x29 */
		err = ddTDA182I2Write(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq6:
 *
 * DESCRIPTION: Get the RFCAL_Freq6 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq6(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x29 */
		err = ddTDA182I2Read(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx29.bF.RFCAL_Freq6;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq7:
 *
 * DESCRIPTION: Set the RFCAL_Freq7 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq7(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx29.bF.RFCAL_Freq7 = uValue;

		/* read byte 0x29 */
		err = ddTDA182I2Write(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq7:
 *
 * DESCRIPTION: Get the RFCAL_Freq7 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq7(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x29 */
		err = ddTDA182I2Read(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx29.bF.RFCAL_Freq7;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq8:
 *
 * DESCRIPTION: Set the RFCAL_Freq8 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq8(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2A.bF.RFCAL_Freq8 = uValue;

		/* read byte 0x2A */
		err = ddTDA182I2Write(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq8:
 *
 * DESCRIPTION: Get the RFCAL_Freq8 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq8(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2A */
		err = ddTDA182I2Read(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2A.bF.RFCAL_Freq8;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq9:
 *
 * DESCRIPTION: Set the RFCAL_Freq9 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq9(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2A.bF.RFCAL_Freq9 = uValue;

		/* read byte 0x2A */
		err = ddTDA182I2Write(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq9:
 *
 * DESCRIPTION: Get the RFCAL_Freq9 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq9(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2A */
		err = ddTDA182I2Read(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2A.bF.RFCAL_Freq9;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq10:
 *
 * DESCRIPTION: Set the RFCAL_Freq10 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq10(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2B.bF.RFCAL_Freq10 = uValue;

		/* read byte 0x2B */
		err = ddTDA182I2Write(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq10:
 *
 * DESCRIPTION: Get the é0 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq10(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2B */
		err = ddTDA182I2Read(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2B.bF.RFCAL_Freq10;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Freq11:
 *
 * DESCRIPTION: Set the RFCAL_Freq11 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRFCAL_Freq11(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2B.bF.RFCAL_Freq11 = uValue;

		/* read byte 0x2B */
		err = ddTDA182I2Write(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Freq11:
 *
 * DESCRIPTION: Get the RFCAL_Freq11 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRFCAL_Freq11(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2B */
		err = ddTDA182I2Read(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2B.bF.RFCAL_Freq11;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset0:
 *
 * DESCRIPTION: Set the RFCAL_Offset0 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset0(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx26.bF.RFCAL_Offset_Cprog0 = uValue;

		/* read byte 0x26 */
		err = ddTDA182I2Write(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset0:
 *
 * DESCRIPTION: Get the RFCAL_Offset0 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset0(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x26 */
		err = ddTDA182I2Read(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx26.bF.RFCAL_Offset_Cprog0;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset1:
 *
 * DESCRIPTION: Set the RFCAL_Offset1 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx26.bF.RFCAL_Offset_Cprog1 = uValue;

		/* read byte 0x26 */
		err = ddTDA182I2Write(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset1:
 *
 * DESCRIPTION: Get the RFCAL_Offset1 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {

		/* read byte 0x26 */
		err = ddTDA182I2Read(pObj, 0x26, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx26.bF.RFCAL_Offset_Cprog1;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset2:
 *
 * DESCRIPTION: Set the RFCAL_Offset2 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx27.bF.RFCAL_Offset_Cprog2 = uValue;

		/* read byte 0x27 */
		err = ddTDA182I2Write(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset2:
 *
 * DESCRIPTION: Get the RFCAL_Offset2 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x27 */
		err = ddTDA182I2Read(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx27.bF.RFCAL_Offset_Cprog2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset3:
 *
 * DESCRIPTION: Set the RFCAL_Offset3 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset3(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx27.bF.RFCAL_Offset_Cprog3 = uValue;

		/* read byte 0x27 */
		err = ddTDA182I2Write(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset3:
 *
 * DESCRIPTION: Get the RFCAL_Offset3 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset3(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x27 */
		err = ddTDA182I2Read(pObj, 0x27, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx27.bF.RFCAL_Offset_Cprog3;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset4:
 *
 * DESCRIPTION: Set the RFCAL_Offset4 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx28.bF.RFCAL_Offset_Cprog4 = uValue;

		/* read byte 0x28 */
		err = ddTDA182I2Write(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset4:
 *
 * DESCRIPTION: Get the RFCAL_Offset4 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x28 */
		err = ddTDA182I2Read(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx28.bF.RFCAL_Offset_Cprog4;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset5:
 *
 * DESCRIPTION: Set the RFCAL_Offset5 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx28.bF.RFCAL_Offset_Cprog5 = uValue;

		/* read byte 0x28 */
		err = ddTDA182I2Write(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset5:
 *
 * DESCRIPTION: Get the RFCAL_Offset5 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x28 */
		err = ddTDA182I2Read(pObj, 0x28, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx28.bF.RFCAL_Offset_Cprog5;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset6:
 *
 * DESCRIPTION: Set the RFCAL_Offset6 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset6(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx29.bF.RFCAL_Offset_Cprog6 = uValue;

		/* read byte 0x29 */
		err = ddTDA182I2Write(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset6:
 *
 * DESCRIPTION: Get the RFCAL_Offset6 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset6(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x29 */
		err = ddTDA182I2Read(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx29.bF.RFCAL_Offset_Cprog6;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset7:
 *
 * DESCRIPTION: Set the RFCAL_Offset7 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset7(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx29.bF.RFCAL_Offset_Cprog7 = uValue;

		/* read byte 0x29 */
		err = ddTDA182I2Write(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset7:
 *
 * DESCRIPTION: Get the RFCAL_Offset7 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset7(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x29 */
		err = ddTDA182I2Read(pObj, 0x29, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx29.bF.RFCAL_Offset_Cprog7;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset8:
 *
 * DESCRIPTION: Set the RFCAL_Offset8 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset8(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2A.bF.RFCAL_Offset_Cprog8 = uValue;

		/* read byte 0x2A */
		err = ddTDA182I2Write(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset8:
 *
 * DESCRIPTION: Get the RFCAL_Offset8 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset8(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2A */
		err = ddTDA182I2Read(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2A.bF.RFCAL_Offset_Cprog8;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset9:
 *
 * DESCRIPTION: Set the RFCAL_Offset9 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset9(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2A.bF.RFCAL_Offset_Cprog9 = uValue;

		/* read byte 0x2A */
		err = ddTDA182I2Write(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset9:
 *
 * DESCRIPTION: Get the RFCAL_Offset9 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset9(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2A */
		err = ddTDA182I2Read(pObj, 0x2A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2A.bF.RFCAL_Offset_Cprog9;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset10:
 *
 * DESCRIPTION: Set the RFCAL_Offset10 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset10(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2B.bF.RFCAL_Offset_Cprog10 = uValue;

		/* read byte 0x2B */
		err = ddTDA182I2Write(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset10:
 *
 * DESCRIPTION: Get the é0 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset10(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2B */
		err = ddTDA182I2Read(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2B.bF.RFCAL_Offset_Cprog10;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_Offset11:
 *
 * DESCRIPTION: Set the RFCAL_Offset11 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_Offset11(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2B.bF.RFCAL_Offset_Cprog11 = uValue;

		/* read byte 0x2B */
		err = ddTDA182I2Write(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_Offset11:
 *
 * DESCRIPTION: Get the RFCAL_Offset11 bit(s) status
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_Offset11(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2B */
		err = ddTDA182I2Read(pObj, 0x2B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2B.bF.RFCAL_Offset_Cprog11;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC2_loop_off:
 *
 * DESCRIPTION: Set the AGC2_loop_off bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetAGC2_loop_off(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2C.bF.AGC2_loop_off = uValue;

		/* read byte 0x2C */
		err = ddTDA182I2Write(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC2_loop_off:
 *
 * DESCRIPTION: Get the AGC2_loop_off bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC2_loop_off(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2C */
		err = ddTDA182I2Read(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2C.bF.AGC2_loop_off;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetForce_AGC2_gain:
 *
 * DESCRIPTION: Set the Force_AGC2_gain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetForce_AGC2_gain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2C.bF.Force_AGC2_gain = uValue;

		/* write byte 0x2C */
		err = ddTDA182I2Write(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetForce_AGC2_gain:
 *
 * DESCRIPTION: Get the Force_AGC2_gain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetForce_AGC2_gain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2C */
		err = ddTDA182I2Read(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2C.bF.Force_AGC2_gain;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRF_Filter_Gv:
 *
 * DESCRIPTION: Set the RF_Filter_Gv bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRF_Filter_Gv(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2C.bF.RF_Filter_Gv = uValue;

		/* read byte 0x2C */
		err = ddTDA182I2Write(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRF_Filter_Gv:
 *
 * DESCRIPTION: Get the RF_Filter_Gv bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRF_Filter_Gv(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2C */
		err = ddTDA182I2Read(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2C.bF.RF_Filter_Gv;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetAGC2_Do_step:
 *
 * DESCRIPTION: Set the AGC2_Do_step bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetAGC2_Do_step(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2E.bF.AGC2_Do_step = uValue;

		/* read byte 0x2E */
		err = ddTDA182I2Write(pObj, 0x2E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC2_Do_step:
 *
 * DESCRIPTION: Get the AGC2_Do_step bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetAGC2_Do_step(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2E */
		err = ddTDA182I2Read(pObj, 0x2E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2E.bF.AGC2_Do_step;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRF_BPF_Bypass:
 *
 * DESCRIPTION: Set the RF_BPF_Bypass bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRF_BPF_Bypass(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				   unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2F.bF.RF_BPF_Bypass = uValue;

		/* read byte 0x2F */
		err = ddTDA182I2Write(pObj, 0x2F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRF_BPF_Bypass:
 *
 * DESCRIPTION: Get the RF_BPF_Bypass bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRF_BPF_Bypass(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2F */
		err = ddTDA182I2Read(pObj, 0x2F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2F.bF.RF_BPF_Bypass;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRF_BPF:
 *
 * DESCRIPTION: Set the RF_BPF bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRF_BPF(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx2F.bF.RF_BPF = uValue;

		/* read byte 0x2F */
		err = ddTDA182I2Write(pObj, 0x2F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRF_BPF:
 *
 * DESCRIPTION: Get the RF_BPF bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRF_BPF(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x2F */
		err = ddTDA182I2Read(pObj, 0x2F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx2F.bF.RF_BPF;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetUp_AGC5:
 *
 * DESCRIPTION: Get the Up_AGC5 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetUp_AGC5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Up_AGC5;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDo_AGC5:
 *
 * DESCRIPTION: Get the Do_AGC5 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDo_AGC5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Do_AGC5;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetUp_AGC4:
 *
 * DESCRIPTION: Get the Up_AGC4 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetUp_AGC4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Up_AGC4;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDo_AGC4:
 *
 * DESCRIPTION: Get the Do_AGC4 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDo_AGC4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Do_AGC4;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetUp_AGC2:
 *
 * DESCRIPTION: Get the Up_AGC2 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetUp_AGC2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Up_AGC2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDo_AGC2:
 *
 * DESCRIPTION: Get the Do_AGC2 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDo_AGC2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Do_AGC2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetUp_AGC1:
 *
 * DESCRIPTION: Get the Up_AGC1 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetUp_AGC1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Up_AGC1;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDo_AGC1:
 *
 * DESCRIPTION: Get the Do_AGC1 bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDo_AGC1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x31 */
		err = ddTDA182I2Read(pObj, 0x31, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx31.bF.Do_AGC1;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC2_Gain_Read:
 *
 * DESCRIPTION: Get the AGC2_Gain_Read bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC2_Gain_Read(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x32 */
		err = ddTDA182I2Read(pObj, 0x32, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx32.bF.AGC2_Gain_Read;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC1_Gain_Read:
 *
 * DESCRIPTION: Get the AGC1_Gain_Read bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC1_Gain_Read(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x32 */
		err = ddTDA182I2Read(pObj, 0x32, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx32.bF.AGC1_Gain_Read;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetTOP_AGC3_Read:
 *
 * DESCRIPTION: Get the TOP_AGC3_Read bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetTOP_AGC3_Read(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x33 */
		err = ddTDA182I2Read(pObj, 0x33, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx33.bF.TOP_AGC3_Read;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC5_Gain_Read:
 *
 * DESCRIPTION: Get the AGC5_Gain_Read bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC5_Gain_Read(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x34 */
		err = ddTDA182I2Read(pObj, 0x34, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx34.bF.AGC5_Gain_Read;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetAGC4_Gain_Read:
 *
 * DESCRIPTION: Get the AGC4_Gain_Read bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetAGC4_Gain_Read(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x34 */
		err = ddTDA182I2Read(pObj, 0x34, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx34.bF.AGC4_Gain_Read;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRSSI:
 *
 * DESCRIPTION: Set the RSSI bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRSSI(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx35.RSSI = uValue;

		/* write byte 0x35 */
		err = ddTDA182I2Write(pObj, 0x35, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRSSI:
 *
 * DESCRIPTION: Get the RSSI bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRSSI(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x35 */
		err = ddTDA182I2Read(pObj, 0x35, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx35.RSSI;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRSSI_AV:
 *
 * DESCRIPTION: Set the RSSI_AV bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRSSI_AV(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx36.bF.RSSI_AV = uValue;

		/* write byte 0x36 */
		err = ddTDA182I2Write(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRSSI_AV:
 *
 * DESCRIPTION: Get the RSSI_AV bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRSSI_AV(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x36 */
		err = ddTDA182I2Read(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx36.bF.RSSI_AV;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRSSI_Cap_Reset_En:
 *
 * DESCRIPTION: Set the RSSI_Cap_Reset_En bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRSSI_Cap_Reset_En(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			       unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx36.bF.RSSI_Cap_Reset_En = uValue;

		/* write byte 0x36 */
		err = ddTDA182I2Write(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRSSI_Cap_Reset_En:
 *
 * DESCRIPTION: Get the RSSI_Cap_Reset_En bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRSSI_Cap_Reset_En(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x36 */
		err = ddTDA182I2Read(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx36.bF.RSSI_Cap_Reset_En;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRSSI_Cap_Val:
 *
 * DESCRIPTION: Set the RSSI_Cap_Val bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetRSSI_Cap_Val(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx36.bF.RSSI_Cap_Val = uValue;

		/* write byte 0x36 */
		err = ddTDA182I2Write(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRSSI_Cap_Val:
 *
 * DESCRIPTION: Get the RSSI_Cap_Val bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetRSSI_Cap_Val(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x36 */
		err = ddTDA182I2Read(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx36.bF.RSSI_Cap_Val;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRSSI_Dicho_not:
 *
 * DESCRIPTION: Set the RSSI_Dicho_not bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRSSI_Dicho_not(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			    unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx36.bF.RSSI_Dicho_not = uValue;

		/* write byte 0x36 */
		err = ddTDA182I2Write(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRSSI_Dicho_not:
 *
 * DESCRIPTION: Get the RSSI_Dicho_not bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRSSI_Dicho_not(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x36 */
		err = ddTDA182I2Read(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx36.bF.RSSI_Dicho_not;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetDDS_Polarity:
 *
 * DESCRIPTION: Set the DDS_Polarity bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetDDS_Polarity(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx37.bF.DDS_Polarity = uValue;

		/* write byte 0x37 */
		err = ddTDA182I2Write(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetDDS_Polarity:
 *
 * DESCRIPTION: Get the DDS_Polarity bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetDDS_Polarity(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x37 */
		err = ddTDA182I2Read(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx37.bF.DDS_Polarity;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetRFCAL_DeltaGain:
 *
 * DESCRIPTION: Set the RFCAL_DeltaGain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2SetRFCAL_DeltaGain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			     unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx37.bF.RFCAL_DeltaGain = uValue;

		/* read byte 0x37 */
		err = ddTDA182I2Write(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetRFCAL_DeltaGain:
 *
 * DESCRIPTION: Get the RFCAL_DeltaGain bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t
tmddTDA182I2GetRFCAL_DeltaGain(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x37 */
		err = ddTDA182I2Read(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx37.bF.RFCAL_DeltaGain;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2SetIRQ_Polarity:
 *
 * DESCRIPTION: Set the IRQ_Polarity bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2SetIRQ_Polarity(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				  unsigned char uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx37.bF.IRQ_Polarity = uValue;

		/* read byte 0x37 */
		err = ddTDA182I2Write(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2GetIRQ_Polarity:
 *
 * DESCRIPTION: Get the IRQ_Polarity bit(s) status
 *
 * RETURN:      tmdd_ERR_TUNER_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_PARAMETER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2GetIRQ_Polarity(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x37 */
		err = ddTDA182I2Read(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx37.bF.IRQ_Polarity;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_0
 *
 * DESCRIPTION: Get the rfcal_log_0 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_0(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x38 */
		err = ddTDA182I2Read(pObj, 0x38, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx38.bF.rfcal_log_0;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_1
 *
 * DESCRIPTION: Get the rfcal_log_1 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_1(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x39 */
		err = ddTDA182I2Read(pObj, 0x39, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx39.bF.rfcal_log_1;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_2
 *
 * DESCRIPTION: Get the rfcal_log_2 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_2(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x3A */
		err = ddTDA182I2Read(pObj, 0x3A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx3A.bF.rfcal_log_2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_3
 *
 * DESCRIPTION: Get the rfcal_log_3 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_3(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x3B */
		err = ddTDA182I2Read(pObj, 0x3B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx3B.bF.rfcal_log_3;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_4
 *
 * DESCRIPTION: Get the rfcal_log_4 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_4(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x3C */
		err = ddTDA182I2Read(pObj, 0x3C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx3C.bF.rfcal_log_4;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_5
 *
 * DESCRIPTION: Get the rfcal_log_5 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_5(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x3D */
		err = ddTDA182I2Read(pObj, 0x3D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx3D.bF.rfcal_log_5;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_6
 *
 * DESCRIPTION: Get the rfcal_log_6 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_6(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x3E */
		err = ddTDA182I2Read(pObj, 0x3E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx3E.bF.rfcal_log_6;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_7
 *
 * DESCRIPTION: Get the rfcal_log_7 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_7(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x3F */
		err = ddTDA182I2Read(pObj, 0x3F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx3F.bF.rfcal_log_7;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_8
 *
 * DESCRIPTION: Get the rfcal_log_8 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_8(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x40 */
		err = ddTDA182I2Read(pObj, 0x40, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx40.bF.rfcal_log_8;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_9
 *
 * DESCRIPTION: Get the rfcal_log_9 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_9(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x41 */
		err = ddTDA182I2Read(pObj, 0x41, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx41.bF.rfcal_log_9;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_10
 *
 * DESCRIPTION: Get the rfcal_log_10 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_10(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue	/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x42 */
		err = ddTDA182I2Read(pObj, 0x42, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx42.bF.rfcal_log_10;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2Getrfcal_log_11
 *
 * DESCRIPTION: Get the rfcal_log_11 bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2Getrfcal_log_11(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	unsigned char *puValue/* I: Address of the var to output item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x43 */
		err = ddTDA182I2Read(pObj, 0x43, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx43.bF.rfcal_log_11;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/* ------------------------------------------------------------------------
 * FUNCTION:    tmddTDA182I2LaunchRF_CAL
 *
 * DESCRIPTION: Launch the RF_CAL bit(s) status
 *
 * RETURN:      ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_BAD_UNIT_NUMBER
 * ddTDA182I2_ERR_NOT_INITIALIZED
 * tmdd_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmddTDA182I2LaunchRF_CAL(tmUnitSelect_t tUnit/* I: Unit no.*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err != TM_OK)
		return err;
	/* set Calc_PLL & RF_CAL */
	pObj->I2CMap.uBx19.MSM_byte_1 = 0x21;

	/* write byte 0x19 */
	err = ddTDA182I2Write(pObj, 0x19, 1);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK) {
		/* trigger MSM_Launch */
		pObj->I2CMap.uBx1A.bF.MSM_Launch = 1;

		/* write byte 0x1A */
		err = ddTDA182I2Write(pObj, 0x1A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2Write(0x%08X) failed.", tUnit));

		/* reset MSM_Launch (buffer only, no write) */
		pObj->I2CMap.uBx1A.bF.MSM_Launch = 0;

		if (pObj->bIRQWait) {
			if (err == TM_OK) {
				err =
				    ddTDA182I2WaitIRQ(pObj, 1700, 50, 0x0C);
				tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				     "ddTDA182I2WaitIRQ(0x%08X) failed.",
					     tUnit));
			}

		}
	}

	(void)ddTDA182I2MutexRelease(pObj);

	return err;
}
