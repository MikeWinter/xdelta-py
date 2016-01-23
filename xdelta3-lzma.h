/* xdelta 3 - delta compression tools and library
 * Copyright (C) 2012, 2013, 2014, 2015.  Joshua P. MacDonald
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Note: The use of the _easy_ decoder means we're not calling the
 * xd3_stream malloc hooks.  TODO(jmacd) Fix if anyone cares. */

#ifndef _XDELTA3_LZMA_H_
#define _XDELTA3_LZMA_H_

#include <lzma.h>

typedef struct _xd3_lzma_stream xd3_lzma_stream;

struct _xd3_lzma_stream {
  lzma_stream lzma;
  lzma_options_lzma options;
  lzma_filter filters[2];
};

xd3_sec_stream* 
xd3_lzma_alloc (xd3_stream *stream)
{
  return (xd3_sec_stream*) xd3_alloc (stream, sizeof (xd3_lzma_stream), 1);
}

void
xd3_lzma_destroy (xd3_stream *stream, xd3_sec_stream *sec_stream)
{
  xd3_lzma_stream *ls = (xd3_lzma_stream*) sec_stream;
  lzma_end (&ls->lzma);
  xd3_free (stream, ls);
}

int
xd3_lzma_init (xd3_stream *stream, xd3_lzma_stream *sec, int is_encode)
{
  int ret;

  memset (&sec->lzma, 0, sizeof(sec->lzma));

  if (is_encode)
    {
      int preset = (stream->flags & XD3_COMPLEVEL_MASK) >> XD3_COMPLEVEL_SHIFT;

      if (lzma_lzma_preset(&sec->options, preset)) 
	{
	  stream->msg = "invalid lzma preset";
	  return XD3_INVALID;
	}

      sec->filters[0].id = LZMA_FILTER_LZMA2;
      sec->filters[0].options = &sec->options;
      sec->filters[1].id = LZMA_VLI_UNKNOWN;

      ret = lzma_stream_encoder (&sec->lzma, &sec->filters[0], LZMA_CHECK_NONE);
    }
  else 
    {
      ret = lzma_stream_decoder (&sec->lzma, UINT64_MAX, LZMA_TELL_NO_CHECK);
    }
  
  if (ret != LZMA_OK)
    {
      stream->msg = "lzma stream init failed";
      return XD3_INTERNAL;
    }

  return 0;
}

int xd3_decode_lzma (xd3_stream *stream, xd3_lzma_stream *sec,
		     const uint8_t **input_pos,
		     const uint8_t  *const input_end,
		     uint8_t       **output_pos,
		     const uint8_t  *const output_end)
{
  uint8_t *output = *output_pos;
  const uint8_t *input = *input_pos;
  size_t avail_in = input_end - input;
  size_t avail_out = output_end - output;

  sec->lzma.avail_in = avail_in;
  sec->lzma.next_in = input;
  sec->lzma.avail_out = avail_out;
  sec->lzma.next_out = output;
  
  while (1) 
    {
      int lret = lzma_code (&sec->lzma, LZMA_RUN);

      switch (lret)
	{
	case LZMA_NO_CHECK: 
	case LZMA_OK:
	  if (sec->lzma.avail_out == 0) 
	    {
	      (*output_pos) = sec->lzma.next_out;
	      (*input_pos) = sec->lzma.next_in;
	      return 0;
	    }
	  break;

	default:
	  stream->msg = "lzma decoding error";
	  return XD3_INTERNAL;
	}
    }
}

#if XD3_ENCODER

int xd3_encode_lzma (xd3_stream *stream, 
		     xd3_lzma_stream *sec, 
		     xd3_output   *input,
		     xd3_output   *output,
		     xd3_sec_cfg  *cfg)

{
  lzma_action action = LZMA_RUN;

  cfg->inefficient = 1;  /* Can't skip windows */
  sec->lzma.next_in = NULL;
  sec->lzma.avail_in = 0;
  sec->lzma.next_out = (output->base + output->next);
  sec->lzma.avail_out = (output->avail - output->next);

  while (1)
    {
      int lret;
	  size_t nwrite;
      if (sec->lzma.avail_in == 0 && input != NULL)
	{
	  sec->lzma.avail_in = input->next;
	  sec->lzma.next_in = input->base;
	  
	  if ((input = input->next_page) == NULL)
	    {
	      action = LZMA_SYNC_FLUSH;
	    }
	}

      lret = lzma_code (&sec->lzma, action);

      nwrite = (output->avail - output->next) - sec->lzma.avail_out;

      if (nwrite != 0) 
	{
	  output->next += nwrite;

	  if (output->next == output->avail)
	    {
	      if ((output = xd3_alloc_output (stream, output)) == NULL)
		{
		  return ENOMEM;
		}
	      
	      sec->lzma.next_out = output->base;
	      sec->lzma.avail_out = output->avail;
	    }
	}

      switch (lret)
	{
	case LZMA_OK:
	  break;

	case LZMA_STREAM_END:
	  return 0;

	default:
	  stream->msg = "lzma encoding error";
	  return XD3_INTERNAL;
	}
    }

  return 0;
}

#endif /* XD3_ENCODER */

#endif /* _XDELTA3_LZMA_H_ */
