/*
 * PC-Speaker driver for Linux
 *
 * Copyright (C) 2001-2007  Stas Sergeev
 */

#ifndef __PCSP_INPUT_H__
#define __PCSP_INPUT_H__

int __init pcspkr_input_init(struct input_dev **dev);
int __exit pcspkr_input_remove(struct input_dev *dev);

#endif
