/**
 *****************************************************************************
 * @file dsilabs.h
 * @author Franck MARILLET
 *
 * @brief This file is part of the stm_fe Library.
 *
 * Copyright (C) 2014 Technicolor
 * All Rights Reserved
 *
 * This program contains proprietary information which is a trade
 * secret of Technicolor and/or its affiliates and also is protected as
 * an unpublished work under applicable Copyright laws. Recipient is
 * to retain this program in confidence and is not permitted to use or
 * make copies thereof other than as permitted in a written agreement
 * with Technicolor, unless otherwise expressly allowed by applicable laws
 *
 ******************************************************************************/
#ifndef _DSILABS_H
#define _DSILABS_H

int stm_fe_silabs_init (struct stm_fe_demod_s * priv);
int stm_fe_silabs_tune (struct stm_fe_demod_s * priv,bool *lock);
int stm_fe_silabs_scan(struct stm_fe_demod_s *priv, bool * lock);
int stm_fe_silabs_tracking(struct stm_fe_demod_s * priv);
int stm_fe_silabs_status(struct stm_fe_demod_s *priv, bool * locked);
int stm_fe_silabs_unlock(struct stm_fe_demod_s *priv);
int stm_fe_silabs_attach (struct stm_fe_demod_s * demod_priv);
int stm_fe_silabs_detach(struct stm_fe_demod_s *demod_priv);
int stm_fe_silabs_term(struct stm_fe_demod_s *demod_priv);
int stm_fe_silabs_abort(struct stm_fe_demod_s *priv, bool abort);
int stm_fe_silabs_standby(struct stm_fe_demod_s *priv, bool standby);

#endif /* _DSILABS_H */
