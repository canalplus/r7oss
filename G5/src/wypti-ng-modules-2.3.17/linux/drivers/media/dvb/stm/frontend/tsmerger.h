/*
 * 
 * Generic TSMerger driver
 * 
 * 
 */

#ifndef _TSMERGER_H_
#define _TSMERGER_H_

/* Parameters for device */
#if defined(ST5100) || defined(ST5301) || defined(CONFIG_CPU_SUBTYPE_STB7100)
#ifdef CONFIG_CPU_SUBTYPE_STB5301
  #define TSM_BASE_ADDRESS 0x21100000
  #define TSM_BASE_SIZE    0x400

  #define TSM_NUM_INPUT_STREAMS 0x5
  #define TSM_NUM_OUPUT_STREAMS 0x2

  #define TSM_INPUT_TSIS       (1 << 0 | 1 << 1 | 1 << 2)
  #define TSM_INPUT_SWTS       (1 << 3)
  #define TSM_INPUT_PTIALT     (1 << 4)

  #define TSM_OUTPUT_PTI       (1 << 0)
  #define TSM_OUTPUT_1394      (1 << 1)
#endif

#ifdef CONFIG_CPU_SUBTYPE_STB7100
  #define TSM_BASE_ADDRESS 0x19242000
  #define TSM_BASE_SIZE    0x900

  #define TSM_NUM_INPUT_STREAMS 0x5
  #define TSM_NUM_OUPUT_STREAMS 0x2

  #define TSM_INPUT_TSIS       (1 << 0 | 1 << 1 | 1 << 2)
  #define TSM_INPUT_SWTS       (1 << 3)
  #define TSM_INPUT_PTIALT     (1 << 4)

  #define TSM_OUTPUT_PTI       (1 << 0)
  #define TSM_OUTPUT_1394      (1 << 1)

  #define TSM_NUM_PTI_ALT_OUT  1
  #define TSM_NUM_1394_ALT_OUT 1
#endif

#else /* It's the generic TSMerger, therefore only the following parameters need be specified */
#if 0
  #define TSM_BASE_ADDRESS     ??

  #define TSM_NO_INPUT_STREAMS ??
  #define TSM_NO_OUPUT_STREAMS ??

  #define TSM_INPUT_TSIS       ()
  #define TSM_INPUT_SWTS       ()
  #define TSM_INPUT_PTIALT     ()

  #define TSM_OUTPUT_PTI       ()
  #define TSM_OUTPUT_1394      ()
  
  #define TSM_NUM_PTI_ALT_OUT  ()
  #define TSM_NUM_1394_ALT_OUT ()
#endif
#endif

#if defined(ST5100) || defined(ST5301) // defined(CPU_SUBTYPE_STB7100)
  #define TSM_SYSTEM_CFG       (0x03F0)
  #define TSM_SW_RESET         (0x03F8)
#else
  #define TSM_SYSTEM_CFG       (0x0800 + ((TSM_NUM_PTI_ALT_OUT + TSM_NUM_1394_ALT_OUT) * 0x10) )
  #define TSM_SW_RESET         (0x0810 + ((TSM_NUM_PTI_ALT_OUT + TSM_NUM_1394_ALT_OUT) * 0x10) )
#endif

#define TSM_CFG_BYPASS_NORMAL    (0x0 << 0)
//#define TSM_CFG_BYPASS_TSIS      (0x2 << 0)
//#define TSM_CFG_BYPASS_SWTS      (0x3 << 0)
//#define TSM_CFG_SYSTEM_PAUSE     (1   << 2)
#define TSM_CFG_BYPASS_SWTS0      ((1 << 2 | 0x2) | (1 << 5 | 0x2 << 3)) 
#define TSM_CFG_BYPASS_TSIN0      ((1 << 2 | 0x0) | (1 << 5 | 0x0 << 3)) 

/* TSM Destination register */
#if defined(ST5100) || defined(ST5301) //|| defined(CPU_SUBTYPE_STB7100)
  #define TSM_DESTINATION(input)  (0x0200 + (0x38 * input))
#else
  #define TSM_DESTINATION(input)  (0x0200 + (0x10 * input))
#endif

/* TSM PTI Alt Out configuration */
#if defined(ST5100) || defined(ST5301) //|| defined(CPU_SUBTYPE_STB7100)
  #define TSM_PTI_ALT_OUT_CFG(x)  (0x300)
#else
  #define TSM_PTI_ALT_OUT_CFG(x)  (0x800 + (n*0x10))
#endif
#define TSM_PTI_ALT_OUT_PACE(cycle)  (cycle & 0xffff)
#define TSM_PTI_ALT_OUT_AUTO_PACE    (1 << 16)


/* TSM 1394 configuration registers */
#if defined(ST5100) || defined(ST5301) //|| defined(CPU_SUBTYPE_STB7100)
  #define TSM_1394_CFG(x)   (0x338)
#else
  #define TSM_1394_CFG(x)   (0x0800 + (TSM_NUM_PTI_ALT_OUT * 0x10))
#endif
#define TSM_1394_PACE(cycles)         (cycles & 0xffff)
#define TSM_1394_CLKSRC_NOT_INPUTCLK  (1 << 16)
#define TSM_1394_DIR_OUT_NOT_IN       (1 << 17)
#define TSM_1394_REMOVE_TAGGING_BYTES (1 << 18)

/* TSM SWTS configuration registers */
#if defined(ST5100) || defined(ST5301) //|| defined(CPU_SUBTYPE_STB7100)
  #define TSM_SWTS_CFG(x)   (0x2e0)
#else
  #define TSM_SWTS_CFG(x)   (0x0600 + (x*0x10))
#endif
#define TSM_SWTS_PACE(bytes)      (bytes & 0xffff)
#define TSM_SWTS_AUTO_PACE        (1 << 16)
#define TSM_SWTS_REQ_TRIG(level)  (((level) & 0xf) << 24)
#define TSM_SWTS_REQ              (1 << 31)

/* TSM SWTS fifo */
#define TSM_SWTS_FIFO(input)        (input * 0x20)

/*
 * Merger output configuration 
 */
#if defined(ST5100) || defined(ST5301) //|| defined(CPU_SUBTYPE_STB7100)
  #define TSM_PROG_COUNTER(n)       (0x240 + (n * 0x8))
#else
  #define TSM_PROG_COUNTER(n)       (0x400 + (n * 0x10))
#endif
#define TSM_COUNTER_VALUE(value)  (value & 0xffffff)
#define TSM_AUTOMATIC_COUNTER     (1 << 24)
#define TSM_COUNTER_INITILIZED    (1 << 25)
#define TSM_COUNTER_INC(clocks)   ((clocks & 0xf) << 28)

/*
 * DataPointers
 */
#if defined(ST5100) || defined(ST5301) //|| defined(CPU_SUBTYPE_STB7100)
  #define TSM_POINTERS(n)
#else
  #define TSM_POINTERS(n)           (0x850 + (n * 0x10))
#endif

/*
 * TSIS Input configuration
 */
#define TSM_STREAM_CONF(n)        (n * 0x20)
#define TSM_SERIAL_NOT_PARALLEL   (1 << 0)
#define TSM_SYNC_NOT_ASYNC        (1 << 1)
#define TSM_ALIGN_BYTE_SOP        (1 << 2)
#define TSM_ASYNC_SOP_TOKEN       (1 << 3)
#define TSM_INVERT_BYTECLK        (1 << 4)
#define TSM_ADD_TAG_BYTES         (1 << 5)
#define TSM_REPLACE_ID_TAG        (1 << 6)
#define TSM_STREAM_ON             (1 << 7)
#define TSM_RAM_ALLOC_START(size) ((size & 0xff) << 8)
#define TSM_PRIORITY(priority)    ((priority & 0xf) << 16)

#define TSM_STREAM_SYNC(n)        ((n * 0x20) + 0x08)
#define TSM_SYNC(no_packets)      ((no_packets & 0xf)    << 0)
#define TSM_DROP(no_packets)      ((no_packets & 0xf)    << 4)
#define TSM_SOP_TOKEN(token)      ((token      & 0xff)   << 8)
#define TSM_PACKET_LENGTH(length) ((length     & 0xffff) << 16)

#define TSM_STREAM_STATUS(n)           ((n * 0x20) + 0x10)
#define TSM_STREAM_LOCK                (1 << 0)
#define TSM_INPUTFIFO_OVERFLOW         (1 << 1)
#define TSM_RAM_OVERFLOW               (1 << 2)
#define TSM_ERRONEOUS_PACKETS(value)   ((value >> 3) & 0x1f)
#define TSM_STRAM_COUNTER_VALUE(value) ((value >> 8) & 0xffffff)

#define TSM_STREAM_CONF2(n)            ((n * 0x20) + 0x18)
#define TSM_CHANNEL_RESET              (1 << 0)
#define TSM_DISABLE_MID_PACKET_ERROR   (1 << 1)
#define TSM_DISABLE_START_PACKET_ERROR (1 << 2)
#define TSM_SHORT_PACKET_COUNT         (value) ((value >> 27) & 0x3f)

#endif //_TSMERGER_H_
