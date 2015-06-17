/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : alsa_backend_ops.h - player device definitions
Author :           Daniel

Date        Modification                                    Name
----        ------------                                    --------
19-Jul-07   Created                                         Daniel

************************************************************************/

#ifndef H_ALSA_BACKEND_OPS
#define H_ALSA_BACKEND_OPS

typedef void           *component_handle_t;
typedef void           (*substream_callback_t)(void *, unsigned int);

struct alsa_substream_descriptor
{
    void *hw_buffer;
    unsigned int hw_buffer_size;
    
    unsigned int channels;
    unsigned int sampling_freq;
    unsigned int bytes_per_sample;
    
    substream_callback_t callback;
    void *user_data;
};

struct alsa_backend_operations
{
    struct module                      *owner;
    
    int (*mixer_get_instance)          (int StreamId, component_handle_t* Classoid);                                 
    int (*mixer_set_module_parameters) (component_handle_t component, void *data, unsigned int size);
    
    int (*mixer_alloc_substream)       (component_handle_t Component, int *SubStreamId);
    int (*mixer_free_substream)        (component_handle_t Component, int SubStreamId);
    int (*mixer_setup_substream)       (component_handle_t Component, int SubStreamId,
                                        struct alsa_substream_descriptor *Descriptor);
    int (*mixer_prepare_substream)     (component_handle_t Component, int SubStreamId);
    int (*mixer_start_substream)       (component_handle_t Component, int SubStreamId);
    int (*mixer_stop_substream)        (component_handle_t Component, int SubStreamId);
};

int register_alsa_backend       (char                           *name,
                                 struct alsa_backend_operations *alsa_backend_ops);
                                 
#endif /* H_ALSA_BACKEND_OPS */
