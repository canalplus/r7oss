#ifndef	_CI_HANDLER_H
#define	_CI_HANDLER_H


irqreturn_t ci_detect_handler(int irq, void *dev);
irqreturn_t ci_data_handler(int irq, void *dev);
#endif
