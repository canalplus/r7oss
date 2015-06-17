/*
 *  Copyright (c) by Jaroslav Kysela (Perex soft)
 */

#include "driver.h"
#define __TABLES_ALLOC__
#include "tables.h"

unsigned short ultra_lvol_to_gvol_raw( unsigned int vol )
{
  unsigned short e, m, tmp;
  
  if ( vol > 65535 ) vol = 65535;
  tmp = vol;
  e = 7;
  if ( tmp < 128 )
    {
      while ( e > 0 && tmp < ( 1 << e ) ) e--;
    }
   else
    {
      while ( tmp > 255 ) { tmp >>= 1; e++; }
    }
  m = vol - ( 1 << e );
  if ( m > 0 )
    {
      if ( e > 8 ) m >>= e - 8; else
      if ( e < 8 ) m <<= 8 - e;
      m &= 255;
    }

  return ( e << 8 ) | m;
}

unsigned int ultra_gvol_to_lvol_raw( unsigned short gf1_vol )
{
  unsigned int rvol;
  unsigned short e, m;
  
  if ( !gf1_vol ) return 0;
  e = gf1_vol >> 8;
  m = (unsigned char)gf1_vol;
  rvol = 1 << e;
  if ( e > 8 )
    return rvol | (m << (e - 8));
  return rvol | (m >> (8 - e));
}

unsigned int ultra_calc_ramp_rate( ultra_card_t *card, unsigned short start, unsigned short end, unsigned int us )
{
  static unsigned char vol_rates[ 19 ] = {
    23, 24, 26, 28, 29, 31, 32, 34,
    36, 37, 39, 40, 42, 44, 45, 47,
    49, 50, 52
  };  
  unsigned short range, increment, value, i;
  
  start >>= 4;
  end >>= 4;
  if ( start < end )
    us /= end - start;
   else
    us /= start - end;
  range = 4;
  value = card -> gf1.enh_mode ? vol_rates[ 0 ] : vol_rates[ card -> gf1.active_voices - 14 ];
  for ( i = 0; i < 3; i++ )
    {
      if ( us < value )
        {
          range = i;
          break;
        }
       else
        value <<= 3;
    }
  if ( range == 4 )
    {
      range = 3;
      increment = 1;
    }
   else
    increment = ( value + ( value >> 1 ) ) / us;
  return ( range << 6 ) | ( increment & 0x3f );
}

unsigned short ultra_translate_freq( ultra_card_t *card, unsigned int freq2 )
{
  if ( freq2 < 50 ) freq2 = 50;
#ifdef ULTRACFG_DEBUG_INSTRUMENTS
  if ( freq2 & 0xf8000000 )
    printk( "ultra_translate_freq: overflow - freq = 0x%x\n", freq2 );
#endif
  return ( ( freq2 << 9 ) + ( card -> gf1.playback_freq >> 1 ) ) / card -> gf1.playback_freq;
}

unsigned short ultra_translate_voice_volume( ultra_card_t *card, unsigned short volume )
{
  unsigned short new_volume, tmp;

  switch ( volume & ULTRA_VOLUME_MASK ) {
    case ULTRA_VOLUME_LINEAR:
      new_volume = ultra_lvol_to_gvol_raw( ( volume & ULTRA_VOLUME_DATA ) << 2 );
      tmp = VOLUME( card -> mixer.soft_volume_level );
      if ( tmp > new_volume ) return 0;
      return new_volume - tmp;
    case ULTRA_VOLUME_MIDI:
      new_volume = VOLUME( card -> mixer.soft_volume_level ) + VOLUME( ( volume & ULTRA_VOLUME_DATA ) >> 7 );
      if ( new_volume > MAX_VOLUME ) new_volume = MAX_VOLUME;
      return MAX_VOLUME - new_volume;
    case ULTRA_VOLUME_EXP:
      return volume & 0x0fff;
  }
  return 0;
}

short ultra_compute_vibrato( short cents, unsigned short fc_register )
{
  static short vibrato_table[] = {
  	0, 0,		32, 592,	61, 1175,	93, 1808,
  	124, 2433,	152, 3007,	182, 3632,	213, 4290,
  	241, 4834,	255, 5200
  };
  
  long depth;
  short *vi1, *vi2, pcents, v1;
  
  pcents = cents < 0 ? -cents : cents;
  for ( vi1 = vibrato_table, vi2 = vi1 + 2; pcents > *vi2; vi1 = vi2, vi2 += 2 );
  v1 = *(vi1 + 1);
  /* The FC table above is a list of pairs. The first number in the pair     */
  /* is the cents index from 0-255 cents, and the second number in the       */
  /* pair is the FC adjustment needed to change the pitch by the indexed     */
  /* number of cents. The table was created for an FC of 32768.              */
  /* The following expression does a linear interpolation against the        */
  /* approximated log curve in the table above, and then scales the number   */
  /* by the FC before the LFO. This calculation also adjusts the output      */
  /* value to produce the appropriate depth for the hardware. The depth      */
  /* is 2 * desired FC + 1.                                                  */
  depth = (((int)(*(vi2 + 1) - *vi1) * (pcents - *vi1) / (*vi2 - *vi1)) + v1) * fc_register >> 14;
  if ( depth ) depth++;
  if ( depth > 255 ) depth = 255;
  return cents < 0 ? -(short)depth : (short)depth;
}

unsigned short ultra_compute_pitchbend( unsigned short pitchbend, unsigned short sens )
{  
  static long log_table[] = { 1024, 1085, 1149, 1218, 1290, 1367, 1448, 1534, 1625, 1722, 1825, 1933 };
  int wheel, sensitivity;
  unsigned int mantissa, f1, f2;
  unsigned short semitones, f1_index, f2_index, f1_power, f2_power;
  char bend_down = 0;
  int bend;

  if ( !sens ) return 1024;
  wheel = (int)pitchbend - 8192;
  sensitivity = ( (int)sens * wheel ) / 128;
  if ( sensitivity < 0 )
    {
      bend_down = 1;
      sensitivity = -sensitivity;
    }
  semitones = (unsigned int)(sensitivity >> 13);
  mantissa = sensitivity % 8192;
  f1_index = semitones % 12;
  f2_index = (semitones + 1) % 12;
  f1_power = semitones / 12;
  f2_power = (semitones + 1) / 12;
  f1 = log_table[ f1_index ] << f1_power;
  f2 = log_table[ f2_index ] << f2_power;
  bend = (int)((((f2 - f1) * mantissa) >> 13) + f1);
  if ( bend_down ) bend = 1048576L / bend;
  return bend;
}

unsigned short ultra_compute_freq( unsigned int freq, unsigned int rate, unsigned short mix_rate )
{
  unsigned int fc;
  int scale = 0;
  
  while ( freq >= 4194304L ) { scale++; freq >>= 1; }
  fc = ( freq << 10 ) / rate;
#ifdef ULTRACFG_DEBUG_INSTRUMENTS
  if ( fc > 97391L )
    printk( "patch: (1) fc frequency overflow - %u\n", fc );
#endif
  fc = ( fc * 44100UL ) / mix_rate;
  while ( scale-- ) fc <<= 1;
#ifdef ULTRACFG_DEBUG_INSTRUMENTS
  if ( fc > 65535L )
    printk( "patch: (2) fc frequency overflow - %u\n", fc );
#endif
  return (unsigned short)fc;
}

unsigned int ultra_compute_patch_rate( ultra_wave_t *wave )
{
#ifdef ULTRACFG_DEBUG_INSTRUMENTS
  if ( wave -> data.patch.root_frequency > 4869577L )
    printk( "patch: root frequency overflow - %u\n", wave -> data.patch.root_frequency );
#endif
  return ( wave -> data.patch.root_frequency * ( 4410UL / 5UL ) ) /
         ( wave -> data.patch.sample_rate / 50UL );
}

ultra_wave_t *ultra_select_patch_wave( ultra_wave_t *start, unsigned int freq )
{
  int delta, min_delta = 0x7fffffff;
  ultra_wave_t *result = NULL;
  
  while ( start )
    {
      if ( start -> data.patch.low_frequency >= freq &&
           start -> data.patch.high_frequency <= freq ) return start;
      delta = (int)freq - (int)start -> data.patch.root_frequency;
      if ( delta < 0 ) delta = -delta;
      if ( delta < min_delta )
        {
          result = start;
          min_delta = delta;
        }
      start = start -> next;
    }
  return result;
}

void ultra_gf1_init_dma_transfer( ultra_card_t *card,
			    unsigned int addr, unsigned char *buf, int count, 
                            short w_unsigned, short w_16bit )
{
  unsigned long flags;
  unsigned int address;
  unsigned char dma_cmd;
  unsigned short ultra_dma1;
  unsigned int address_high;

#if 0
  PRINTK( "dma_transfer: addr=0x%x, buf=0x%lx, count=0x%x\n", addr, (long)buf, count );
#endif

  ultra_dma1 = card -> dmas[ ULTRA_DMA_GPLAY ] -> dma;
  
  if ( ultra_dma1 > 3 )
    {
      if ( card -> gf1.enh_mode )
        address = addr >> 1;
       else
        {
          if ( addr & 0x1f )
            {
              PRINTK( "ultra_dma_transfer: unaligned address (0x%x)?\n", addr );
              return;
            }
          address = ( addr & 0x000c0000 ) | ( ( addr & 0x0003ffff ) >> 1 );
        }
    }
   else
    address = addr;
    
  dma_cmd = 0x21;			/* IRQ enable */
  if ( w_unsigned ) dma_cmd |= 0x80;	/* invert hight bit */
  if ( w_16bit ) 
    {
      dma_cmd |= 0x40;
      count++; count &= ~1;
    }
  if ( ultra_dma1 > 3 )
    {
      dma_cmd |= 0x04;
      count++; count &= ~1;
    }
  CLI( &flags );
  ultra_write8( card, GF1_GB_DRAM_DMA_CONTROL, 0 );  /* disable GF1 DMA */
  disable_dma( ultra_dma1 );
  ultra_look8( card, GF1_GB_DRAM_DMA_CONTROL );
#if 0
  PRINTK( "address = 0x%x, count = 0x%x, dma_cmd = 0x%x\n", address << 1, count, dma_cmd );
#endif
  if ( card -> gf1.enh_mode )
    {
      address_high = ( ( address >> 16 ) & 0x000000f0 ) | ( address & 0x0000000f );
      ultra_write16( card, GF1_GW_DRAM_DMA_LOW, (unsigned short)(address >> 4) );
      ultra_write8( card, GF1_GB_DRAM_DMA_HIGH, (unsigned char)address_high );
    }
   else
    ultra_write16( card, GF1_GW_DRAM_DMA_LOW, (unsigned short)(address >> 4) );
  clear_dma_ff( ultra_dma1 );
  set_dma_mode( ultra_dma1, DMA_MODE_WRITE );
  set_dma_addr( ultra_dma1, (unsigned long)virt_to_bus( buf ) );
  set_dma_count( ultra_dma1, count );
  enable_dma( ultra_dma1 );
  ultra_max_latches( card, 3 );
  ultra_write8( card, GF1_GB_DRAM_DMA_CONTROL, dma_cmd );
  STI( &flags );
}

void ultra_gf1_done_dma_transfer( ultra_card_t *card )
{
  unsigned long flags;
  
  CLI( &flags );
  disable_dma( card -> dmas[ ULTRA_DMA_GPLAY ] -> dma );
  ultra_write8( card, GF1_GB_DRAM_DMA_CONTROL, 0 );	/* disable GF1 DMA */
  ultra_look8( card, GF1_GB_DRAM_DMA_CONTROL );
  STI( &flags );
}

void ultra_gf1_init_rdma_transfer( ultra_card_t *card, unsigned char *buf, int count, unsigned char rctrl_reg )
{
  unsigned long flags;
  unsigned short ultra_dma2;

#if 0
  printk( "rdma transfer???? - buf = 0x%x, count = 0x%x, reg = 0x%x\n", (int)buf, count, rctrl_reg );
#endif
  ultra_dma2 = card -> dmas[ ULTRA_DMA_GRECORD ] -> dma;
  CLI( &flags );
  disable_dma( ultra_dma2 );
  ultra_write8( card, GF1_GB_REC_DMA_CONTROL, 0 );  /* disable sampling */
  ultra_look8( card, GF1_GB_REC_DMA_CONTROL );  /* Sampling Control Register */
  clear_dma_ff( ultra_dma2 );
  set_dma_mode( ultra_dma2, DMA_MODE_READ );
  set_dma_addr( ultra_dma2, (long)virt_to_bus( buf ) );
  set_dma_count( ultra_dma2, count );
  enable_dma( ultra_dma2 );
  ultra_write8( card, GF1_GB_REC_DMA_CONTROL, rctrl_reg );  /* go!!!! */
  STI( &flags );
}

void ultra_gf1_done_rdma_transfer( ultra_card_t *card )
{
  unsigned long flags;

  CLI( &flags );
  disable_dma( card -> dmas[ ULTRA_DMA_GRECORD ] -> dma );
  ultra_write8( card, GF1_GB_REC_DMA_CONTROL, 0 );  /* disable sampling */
  ultra_look8( card, GF1_GB_REC_DMA_CONTROL );  /* Sampling Control Register */
  STI( &flags );
}

/*
 * must be called when interrups are disabled
 */

void ultra_max_latches( ultra_card_t *card, int record )
{
  int mask;
  
  mask = record ? 0x10 : 0x20;
  if ( record == 3 ) mask = 0x30;
  if ( card -> max_ctrl_flag &&
       card -> dmas[ record ? ULTRA_DMA_CRECORD : ULTRA_DMA_CPLAY ] -> dma > 3 )
    {
      OUTB( card -> max_cntrl_val & mask, GUSP( card, MAXCNTRLPORT ) );
      ultra_delay( card );
      OUTB( card -> max_cntrl_val, GUSP( card, MAXCNTRLPORT ) );
    }
}

/*
 *
 */

static unsigned short get_effects_mask( ultra_card_t *card, int value )
{
  if ( card -> gf1.effects && card -> gf1.effects -> chip_type == ULTRA_EFFECT_CHIP_INTERWAVE )
    return card -> gf1.effects -> chip.interwave.voice_output[ value ];
  return 0;
}

void ultra_set_effects_volume( ultra_card_t *card, ultra_voice_t *voice, unsigned short one, unsigned short three, int both )
{
  unsigned long flags;
  unsigned char emask;
  unsigned short evol;
  unsigned short master;
  
#if 0
  printk( "one = 0x%x, three = 0x%x\n", one, three );
#endif
  if ( !card -> gf1.enh_mode ) return;
  /* The InterWave chip can only set one effect level, not two, so   */
  /* if the difference of the levels are within a certain threshold  */
  /* then the larger will be used. Otherwise only one will be set.   */
  master = VOLUME( card -> mixer.effect_volume_level );
  one += master;
  three += master;
  if ( one > MAX_VOLUME ) one = MAX_VOLUME;
  if ( three > MAX_VOLUME ) three = MAX_VOLUME;
  one = MAX_VOLUME - one;
  three = MAX_VOLUME - three;
  if ( one && three )
    {
      if ( one - three > 128 )
        three = 0;		/* one is much louder than three */
       else
      if ( three - one > 128 )	/* three is much louder than one */
        one = 0;		/* max attenuation */
      if ( one && three )
        {
          one = ( three > one ) ? three : one;
          three = one;
        }
    }
  emask = 0;
  evol = MAX_VOLUME << 4;
  if ( one )
    {
      emask |= get_effects_mask( card, 0 );
      evol = ( MAX_VOLUME - one ) << 4;
    }
  if ( three )
    {
      emask |= get_effects_mask( card, 2 );
      evol = ( MAX_VOLUME - three ) << 4;
    }
#if 0
  printk( "emask = 0x%x, evol = 0x%x\n", emask, evol );
#endif
  CLI( &flags );
  gf1_select_voice( card, voice -> number );
  if ( both ) ultra_write16( card, GF1_VW_EFFECT_VOLUME, evol );
  ultra_write16( card, GF1_VW_EFFECT_VOLUME_FINAL, evol );
  ultra_write8( card, GF1_VB_ACCUMULATOR, emask );
  STI( &flags );
}
