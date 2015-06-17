/*
 * drivers/stm/pio_i.h
 *
 * (c) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Internal defintions to allow sharing between pio.c and pio10.c
 */

struct stpio_port;

void stpio_irq_handler(const struct stpio_port *port);
int stpio_get_resources(struct platform_device *pdev,
			unsigned long *start, unsigned long *size,
			int *irq);
struct stpio_port *stpio_init_port(int portno, unsigned long start,
				   char *name);
void stpio_init_irq(int portno);
int stpio_remove_port(struct stpio_port *port);

#ifdef CONFIG_STM_PIO10
void stpio10_early_init(struct platform_device *pdev, unsigned long start);
#else
static inline
void stpio10_early_init(struct platform_device *pdev, unsigned long start)
{}
#endif
