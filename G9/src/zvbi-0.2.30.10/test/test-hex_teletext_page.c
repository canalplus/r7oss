#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "libzvbi.h"

/* debug log */
#define txt_log(...) printf(__VA_ARGS__)

void assert_string(const int line, const char* file, int check);
#define assert(TST) assert_string( __LINE__ , __FILE__ , TST);

typedef struct gbl_info {        
  vbi_export * m_pngExporter;
  vbi_dvb_demux * m_demuxer;
  vbi_decoder * m_decoder;
#define SOFTSUB_SLICED_NB 64
  vbi_sliced sliced[SOFTSUB_SLICED_NB];
} gbl_info;

gbl_info* txtdata;

void assert_string(const int line, const char* file, int check)
{
  printf("%s %i : %s\n", file, line, check ? "SUCCESS": "FAILURE");
  if (!check) exit(0);
}

typedef struct page_id {
        int num;
        int sub_num;
} page_id;

#define TOTAL_PAGES 12
page_id expect_page[TOTAL_PAGES] = {
{0x102, 0x00},
{0x560, 0x00},
{0x561, 0x00},
{0x562, 0x00},
{0x888, 0x00},
{0x4c7, 0x10},
{0x563, 0x00},
{0x59d, 0x10},
{0x59e, 0x10},
{0x5b7, 0x10},
{0x5bb, 0x10},
{0x5bc, 0x10}
};

static int page_found = 0;

static void event_handler (vbi_event * ev, void * user_data)
{
  if ( ev->type == VBI_EVENT_TTX_PAGE ) {		
    txt_log("Found page 0x%03x, sub page 0x%02x\n",ev->ev.ttx_page.pgno, ev->ev.ttx_page.subno);
    assert(ev->ev.ttx_page.pgno == expect_page[page_found].num);
    assert(ev->ev.ttx_page.subno == expect_page[page_found].sub_num);
    page_found++;
  }	
}

/**
 * init the component, this must be done first
 */
void txt_init_lib()
{
  int error;
  txtdata = calloc(1, sizeof(*txtdata));
  assert(txtdata != NULL); 
  txtdata->m_decoder = vbi_decoder_new();
  assert(txtdata->m_decoder != NULL);
  error = vbi_event_handler_add(txtdata->m_decoder, VBI_EVENT_TTX_PAGE, event_handler, NULL);
  txtdata->m_demuxer = vbi_dvb_pes_demux_new( NULL , NULL );
  assert(txtdata->m_demuxer != NULL);
}

int txt_render(int size, uint8_t *pdata)
{
  const uint8_t *bp;
  int left;
  int lines;
  int64_t pts;
  vbi_sliced *psliced = txtdata->sliced;

  bp = pdata;
  left = size;
  if (txtdata) {
    while ( left > 0 ) {			
      lines = vbi_dvb_demux_cor( txtdata->m_demuxer, psliced, SOFTSUB_SLICED_NB ,&pts, &bp, &left );
      if ( lines > 0 ) {
	vbi_decode( txtdata->m_decoder, psliced, lines, pts/90000.0 );
      }
    }
  }
  return 0;
}

#define TXT_BLOCK_COUNT 184

int test_page_number_hex()
{
  FILE* f;
  int count;
  char buf[TXT_BLOCK_COUNT];

  f = fopen("vbi_teletext.bin", "r");
  assert(f != NULL);

  while ((count = fread(buf, TXT_BLOCK_COUNT, 1, f)) > 0)
    {
      txt_render(TXT_BLOCK_COUNT, buf);
    }
  assert(page_found == TOTAL_PAGES);
}

int main()
{
  char buf[200];
  txtdata = malloc(sizeof(struct gbl_info));

  assert(txtdata);
  txt_init_lib();

  test_page_number_hex();

  free(txtdata);

  return 0;
}
