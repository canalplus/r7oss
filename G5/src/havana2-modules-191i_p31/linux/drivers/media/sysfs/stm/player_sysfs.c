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
 * Implementation of SYSFS interface
************************************************************************/

#include <linux/device.h>
#include <linux/fb.h>
#include "sysfs_module.h"
#include "player_sysfs.h"
#include "player_interface.h"

#define PLAYBACK_MAX_NUMBER     8
#define STREAM_MAX_NUMBER       16
#define DEFAULT_ERR_THRESHOLD   5

static int PlayerSetEvent (struct player_event_s*       Event);
static struct mutex SysfsWriteLock;

enum attribute_id_e {
    ATTRIBUTE_ID_input_format,
    ATTRIBUTE_ID_decode_errors,
    ATTRIBUTE_ID_number_channels,
    ATTRIBUTE_ID_sample_frequency,
    ATTRIBUTE_ID_number_of_samples_processed,
    ATTRIBUTE_ID_supported_input_format,
    ATTRIBUTE_ID_MAX
};

struct attribute_data_s
{
    player_component_handle_t *component;
    //unsigned int               type;
    //bool 			in_use;
    //player_stream_handle_t     	*stream_owner;
};

struct stream_data_s
{
    unsigned int                id;
    player_stream_handle_t     *stream;
    struct class_device         stream_class_device;
    dev_t                       stream_dev_t;
    struct attribute_data_s     attribute_data[ATTRIBUTE_ID_MAX];
    bool			notify_on_destroy; //!< True, if the stream has attributes
};

struct playback_data_s
{
    unsigned int                id;
    player_playback_handle_t   *playback;
    struct class_device        *playback_class_device;
    dev_t                       playback_dev_t;
    struct stream_data_s        stream_data[STREAM_MAX_NUMBER];
};

static struct playback_data_s   playback_data[PLAYBACK_MAX_NUMBER];
static int                      playback_count          = 0;

/*{{{  Player2 class and its intimate attributes*/

static unsigned long notify_count = 0;
static unsigned int  err_threshold = DEFAULT_ERR_THRESHOLD;

static ssize_t show_notify_count(struct class *cls, char *buf)
{
	return sprintf(buf, "%lu\n", notify_count);
}

static ssize_t show_err_threshold(struct class *cls, char *buf)
{
	return sprintf(buf, "%u\n", err_threshold);
}

unsigned int player_sysfs_get_err_threshold()
{
    return err_threshold;
}

static ssize_t store_err_threshold(struct class *cls, char *buf)
{
	sscanf(buf, "%i\n", &err_threshold);
    return sizeof(err_threshold);

}
static struct class_attribute player2_class_attributes[] = {
	__ATTR(notify, 0400, show_notify_count, NULL),
	__ATTR(err_threshold, 0644, show_err_threshold, store_err_threshold),
	__ATTR_NULL,
};

static struct class player2_class = {
	.name = "player2",
	.class_attrs = player2_class_attributes,
};

static void do_notify(void)
{
	notify_count++;
	sysfs_notify(&player2_class.subsys.kobj, NULL, "notify");
}
/*}}}  */
/*{{{  show_generic_attribute*/
ssize_t show_generic_attribute (struct class_device *class_dev, char *buf, enum attribute_id_e attr_id, const char *attr_name)
{
    int Result = 0;
    union attribute_descriptor_u value;
    struct stream_data_s 	*streamdata	= container_of (class_dev, struct stream_data_s, stream_class_device);
    struct attribute_data_s 	*attributedata 	= &streamdata->attribute_data[attr_id];

    Result = ComponentGetAttribute (attributedata->component, attr_name, &value);
    if (Result < 0)
    {
	SYSFS_DEBUG("%d: ComponentGetAttribute failed\n", __LINE__);
	return sprintf(buf, "%s not available\n", attr_name);
    }

    switch(attr_id)
    {
    	case ATTRIBUTE_ID_input_format:
    	case ATTRIBUTE_ID_number_channels:
    	    return sprintf(buf, "%s\n", value.ConstCharPointer);
    	    break;
    	case ATTRIBUTE_ID_decode_errors:
    	case ATTRIBUTE_ID_sample_frequency:
    	    return sprintf(buf, "%i\n", value.Int);
    	    break;
    	case ATTRIBUTE_ID_number_of_samples_processed:
    	    return sprintf(buf, "%lld\n", value.UnsignedLongLongInt);
    	    break;
    	case ATTRIBUTE_ID_supported_input_format:
    	    return sprintf(buf, "%i\n", value.Bool);
    	    break;
    	default:
    	    return sprintf(buf, "%s not available\n", attr_name);
    	    break;
    }
}
/*}}}  */
/*{{{  store_generic_attribute*/
ssize_t store_generic_attribute (struct class_device *class_dev, const char *buf, enum attribute_id_e attr_id, const char *attr_name, size_t count)
{
    int Result = 0;

    union attribute_descriptor_u value;
    struct stream_data_s 	*streamdata	= container_of (class_dev, struct stream_data_s, stream_class_device);
    struct attribute_data_s 	*attributedata 	= &streamdata->attribute_data[attr_id];

    switch(attr_id)
    {
    	case ATTRIBUTE_ID_decode_errors:
    	    sscanf(buf, "%i", &(value.Int));
    	    if (value.Int != 0)
    		    return -EINVAL;
    	    break;
    	default:
    	    return -EIO;
    	    break;
    }

    Result = ComponentSetAttribute(attributedata->component, attr_name, &value);
    if (Result < 0)
    {
	SYSFS_DEBUG("%d: ComponentSetAttribute failed\n", __LINE__);
	return -EIO;
    }

    return count;
}
/*}}}  */

#define SHOW(x) 								\
    ssize_t show_ ## x (struct class_device *class_dev, char *buf) 		\
    {										\
	return show_generic_attribute(class_dev, buf, ATTRIBUTE_ID_ ## x, #x);  \
    }

#define STORE(x) 								\
    ssize_t store_ ## x (struct class_device *class_dev, const char *buf, size_t count) 		\
    {										\
	return store_generic_attribute(class_dev, buf, ATTRIBUTE_ID_ ## x, #x, count);\
    }

/* creation of show_xx methods */

SHOW(input_format);
SHOW(decode_errors);
SHOW(number_channels);
SHOW(sample_frequency);
SHOW(supported_input_format);
SHOW(number_of_samples_processed);

/* creation of store_xx methods */

STORE(decode_errors);

/* creation of class_device_attr_xx attributes */

CLASS_DEVICE_ATTR(input_format, S_IRUGO, show_input_format, NULL);
CLASS_DEVICE_ATTR(supported_input_format, S_IRUGO, show_supported_input_format, NULL);
CLASS_DEVICE_ATTR(decode_errors, S_IWUSR | S_IRUGO, show_decode_errors, store_decode_errors);
CLASS_DEVICE_ATTR(sample_frequency, S_IRUGO, show_sample_frequency, NULL);
CLASS_DEVICE_ATTR(number_channels, S_IRUGO, show_number_channels, NULL);
CLASS_DEVICE_ATTR(number_of_samples_processed, S_IRUGO, show_number_of_samples_processed, NULL);

/*{{{  get_playback*/
struct playback_data_s* get_playback (void *playback)
{
    int p;

    //SYSFS_DEBUG("Playback = %p\n", playback);
    for (p = 0; p < PLAYBACK_MAX_NUMBER; p++)
    {
        //SYSFS_DEBUG("****** Checking (%d) %p\n", p, playback_data[p].playback);
        if (playback_data[p].playback == playback)
            break;
    }
    if (p == PLAYBACK_MAX_NUMBER)
        return NULL;

    return &playback_data[p];
}
/*}}}  */
/*{{{  get_stream*/
struct stream_data_s* get_stream (struct playback_data_s* PlaybackData, void* stream)
{
    int s;

    //SYSFS_DEBUG("Playback = %p, Stream = %p\n", PlaybackData->playback, stream);
    for (s = 0; s < STREAM_MAX_NUMBER; s++)
    {
        //SYSFS_DEBUG("******** Checking (%d, %d) Stream %p\n", s, PlaybackData->stream_data[s].id, PlaybackData->stream_data[s].stream);
        if (PlaybackData->stream_data[s].stream == stream)
            break;
    }
    if (s == STREAM_MAX_NUMBER)
        return NULL;

    return &PlaybackData->stream_data[s];
}
/*}}}  */

/*
 * The following function is used by other modules (i.e. stmdvb) when they need to retrieve the proper device class
 * having player playback and stream playback structures pointers
 */

/*{{{  player_sysfs_get_class_device*/
struct class_device* player_sysfs_get_class_device(void *playback, void* stream)
{
    struct playback_data_s* 	PlaybackData;
    struct stream_data_s*	StreamData;

    mutex_lock(&SysfsWriteLock);

    if (playback == NULL)
    {
    	SYSFS_ERROR("Playback is  NULL. Nothing to do. \n");
    	goto err0;
    }

    /* Find specific playback */
    PlaybackData    = get_playback (playback);   
    if (!PlaybackData)
    {
        SYSFS_ERROR ("This playback does not exist\n");
        goto err0;
    }
    else if (stream == NULL)
    {
       mutex_unlock(&SysfsWriteLock);
       return (PlaybackData->playback_class_device);
    }

    /* Find specific stream */
    StreamData              = get_stream (PlaybackData, stream);
    if (!StreamData)
    {
        SYSFS_ERROR("This stream does not exist\n");
        goto err0;
    }

    mutex_unlock(&SysfsWriteLock);

    return &(StreamData->stream_class_device);
err0:
	mutex_unlock(&SysfsWriteLock);
	return NULL;
}
/*}}}  */
/*{{{  player_sysfs_get_player_class_device*/
struct class * player_sysfs_get_player_class(void)
{
    return(&player2_class);
}
/*}}}  */

/*{{{  player_sysfs_get_class_id*/
int player_sysfs_get_stream_id(struct class_device* stream_dev)
{
     struct stream_data_s *streamdata = NULL;

     mutex_lock(&SysfsWriteLock);

     streamdata = container_of (stream_dev, struct stream_data_s, stream_class_device);

     mutex_unlock(&SysfsWriteLock);

     return streamdata->id;
}
/*}}}  */
/*{{{  player_sysfs_new_attribute_notification */
void player_sysfs_new_attribute_notification(struct class_device* stream_dev)
{
    if (stream_dev) {
	struct stream_data_s *streamdata;

	/* the compiler cannot make this function type-safe (we don't know
	 * that stream_dev points to a stream device rather than some
	 * other device). fortunately all valid stream_dev pointers live
	 * within a known block of static memory so we can search for API
	 * abuse at runtime.
	 */
	BUG_ON(((char *) stream_dev < ((char *) playback_data)) ||
	       ((char *) stream_dev >= ((char *) playback_data) + sizeof(playback_data)));
	
	streamdata = container_of (stream_dev, struct stream_data_s, stream_class_device);
	streamdata->notify_on_destroy = true;
    }

    do_notify();
}
/*}}}  */
/*{{{  event_playback_created_handler*/
int event_playback_created_handler(struct player_event_s* Event)
{
    struct playback_data_s* PlaybackData;

    if (!Event->playback)
    {
        SYSFS_ERROR("One or more parameters are NULL. Nothing to do. \n");
        return 1;
    }

    /* Check that this playback does not already exist. In that case, do nothing */
    PlaybackData    = get_playback (Event->playback);
    if (PlaybackData) return 0;

    /* Search for free slot */
    PlaybackData    = get_playback (NULL);
    if (!PlaybackData)
    {
        SYSFS_ERROR("Unable to create register playback - too many playbacks\n");
        return -ENOMEM;
    }

    /* Populate structure */
    PlaybackData->playback                  = Event->playback;

    /* Create a class device and register it with sysfs ( = sysfs/class/player2/playback0,1,2,3....) */
    PlaybackData->playback_dev_t            = MKDEV(0,0);
    PlaybackData->playback_class_device     = class_device_create(&player2_class,            // pointer to the struct class that this device should be registered to
                                                      NULL,                                 // pointer to the parent struct class_device of this new device, if any
                                                      PlaybackData->playback_dev_t,         // the dev_t for the (char) device to be added
                                                      NULL,                                 // a pointer to a struct device that is associated with this class device
                                                      "playback%d", PlaybackData->id);      // string for the class device's name
    if (IS_ERR(PlaybackData->playback_class_device))
    {
        SYSFS_ERROR("Unable to create playback class device (%d)\n", (int)PlaybackData->playback_class_device);
        PlaybackData->playback_class_device         = NULL;
        PlaybackData->playback                      = NULL;
        return -ENODEV;
    }

    /* Increase playback counter */
    playback_count++;

    return 0;
}
/*}}}  */
/*{{{  event_playback_terminated_handler*/
int event_playback_terminated_handler(struct player_event_s* Event)
{
    struct playback_data_s* PlaybackData;

    if (!Event->playback)
    {
        SYSFS_ERROR("One or more parameters are NULL. Nothing to do. \n");
        return 1;
    }

    /* Find specific playback */
    PlaybackData    = get_playback (Event->playback);
    if (!PlaybackData)
    {
        SYSFS_ERROR ("This playback does not exist\n");
        return 1;
    }

    /* Delete playback0,1,2.. class device */
    //SYSFS_DEBUG("Destroying %x, %p\n", PlaybackData->playback_dev_t, PlaybackData->playback_class_device);
    //class_device_destroy (player2_class, PlaybackData->playback_dev_t);

    //SYSFS_DEBUG("Unregistering %p\n", PlaybackData->playback_class_device);
    class_device_unregister (PlaybackData->playback_class_device);

    /* Zero playback entry in table */
    PlaybackData->playback_class_device             = NULL;
    PlaybackData->playback                          = NULL;

    /* Decrease playback counter */
    playback_count--;

    return 0;
}
/*}}}  */
/*{{{  event_stream_created_handler*/
int event_stream_created_handler(struct player_event_s* Event)
{
    int Result = 0;
    struct playback_data_s* 	PlaybackData;
    struct stream_data_s*	StreamData;

    if ((!Event->playback) || (!Event->stream))
    {
        SYSFS_ERROR("One or more parameters are NULL. Nothing to do. \n");
        return 1;
    }

    /* Find specific playback */
    PlaybackData            = get_playback (Event->playback);
    if (!PlaybackData)
    {
        SYSFS_ERROR("This playback does not exist\n");
        return 1;
    }

    /* Check that this stream does not already exist. In that case, do nothing */
    StreamData              = get_stream (PlaybackData, Event->stream);
    if (StreamData) return 0;

    /* Search for free slot */
    StreamData              = get_stream (PlaybackData, NULL);
    if (!StreamData)
    {
        SYSFS_ERROR("Unable to create stream - too many streams on playback %d\n", PlaybackData->id);
        return -ENOMEM;
    }

    /* Populate structure */
    StreamData->stream              = Event->stream;

    /* Create a class device and register it with sysfs ( = sysfs/class/player2/playbackx/stream0,1,2,3...) */
#if 0
    StreamData->stream_dev_t        = MKDEV(0,0);
    StreamData->stream_class_device = class_device_create (player2_class,           // pointer to the struct class that this device should be registered to
                                            PlaybackData->playback_class_device,    // pointer to the parent struct class_device of this new device, if any
                                            StreamData->stream_dev_t,               // the dev_t for the (char) device to be added
                                            NULL,                                   // a pointer to a struct device that is associated with this class device
                                            "stream%d", StreamData->id);            // string for the class device's name
#endif

    // Populate the class structure in similar way to class_device_create()
    memset(&StreamData->stream_class_device, 0, sizeof(struct class_device));
    StreamData->stream_class_device.devt = StreamData->stream_dev_t = MKDEV(0,0);
    StreamData->stream_class_device.dev = NULL;
    StreamData->stream_class_device.class = &player2_class;
    StreamData->stream_class_device.parent = PlaybackData->playback_class_device;
    //StreamData->stream_class_device.release = class_device_create_release;
    //StreamData->stream_class_device.uevent = class_device_create_uevent;
    snprintf(StreamData->stream_class_device.class_id, BUS_ID_SIZE, "stream%d", StreamData->id);
    Result = class_device_register (&StreamData->stream_class_device);
    if (Result || IS_ERR(&StreamData->stream_class_device))
    {
        SYSFS_ERROR("Unable to create stream_class_device %d (%d)\n", StreamData->id, (int)&StreamData->stream_class_device);
        StreamData->stream          = NULL;
        return -ENODEV;
    }

    return 0;
}
/*}}}  */
/*{{{  event_stream_terminated_handler*/
int event_stream_terminated_handler(struct player_event_s* Event)
{
    struct playback_data_s* 	PlaybackData;
    struct stream_data_s*	StreamData;

    if ((!Event->playback) || (!Event->stream))
    {
        SYSFS_ERROR("One or more parameters are NULL. Nothing to do. \n");
        return 1;
    }

    /* Find specific playback */
    PlaybackData            = get_playback (Event->playback);
    if (!PlaybackData)
    {
        SYSFS_ERROR("This playback does not exist\n");
        return 1;
    }

    /* Find specific stream */
    StreamData              = get_stream (PlaybackData, Event->stream);
    if (!StreamData)
    {
        SYSFS_ERROR("This stream does not exist\n");
        return 1;
    }

    /* Delete stream0,1,2.. class device */
    // SYSFS_DEBUG("Unregistering %p\n", StreamData->stream_class_device);
    // NOTE: I suspect a bug in class_device_unregister, infact class_device_del works fine,
    // but if I add class_device_put I have a segemntation fault. It should be investigated better.
    // class_device_unregister (&StreamData->stream_class_device);

    class_device_del(&StreamData->stream_class_device);
    //class_device_put(&StreamData->stream_class_device);

    if (StreamData->notify_on_destroy)
	    do_notify();

    /* Remove stream reference from playback database */
    StreamData->stream              = NULL;

    return 0;
}
/*}}}  */
/*{{{  event_attribute_created_handler*/
int event_attribute_created_handler(struct player_event_s* Event)
{
    struct playback_data_s* 			PlaybackData;
    struct stream_data_s*			StreamData;
    const struct class_device_attribute*        attribute       = NULL;
    unsigned int 				attribute_id    = 0;
    int 					Result 		= 0;

    if ((!Event->component) || (!Event->playback) || (!Event->stream))
    {
        SYSFS_ERROR("One or more parameters are NULL. Nothing to do. \n");
        return 1;
    }

    mutex_lock(&SysfsWriteLock);

    switch (Event->code)
    {
    case PLAYER_EVENT_INPUT_FORMAT_CREATED:
        SYSFS_DEBUG("PLAYER_EVENT_INPUT_FORMAT_CREATED\n");
        attribute_id 	    = ATTRIBUTE_ID_input_format;
        attribute           = &class_device_attr_input_format;
    	break;
    case PLAYER_EVENT_SUPPORTED_INPUT_FORMAT_CREATED:
        SYSFS_DEBUG("PLAYER_EVENT_SUPPORTED_INPUT_FORMAT_CREATED\n");
        attribute_id 	    = ATTRIBUTE_ID_supported_input_format;
        attribute           = &class_device_attr_supported_input_format;
    	break;
    case PLAYER_EVENT_DECODE_ERRORS_CREATED:
        SYSFS_DEBUG("PLAYER_EVENT_DECODE_ERRORS_CREATED\n");
        attribute_id 	    = ATTRIBUTE_ID_decode_errors;
        attribute           = &class_device_attr_decode_errors;
    	break;
    case PLAYER_EVENT_SAMPLE_FREQUENCY_CREATED:
        SYSFS_DEBUG("PLAYER_EVENT_SAMPLE_FREQUENCY_CREATED\n");
        attribute_id 	    = ATTRIBUTE_ID_sample_frequency;
        attribute           = &class_device_attr_sample_frequency;
    	break;
    case PLAYER_EVENT_NUMBER_OF_SAMPLES_PROCESSED:
        SYSFS_DEBUG("PLAYER_EVENT_NUMBER_OF_SAMPLES_PROCESSED\n");
        attribute_id 	    = ATTRIBUTE_ID_number_of_samples_processed;
        attribute           = &class_device_attr_number_of_samples_processed;
    	break;
    default:
        SYSFS_DEBUG("PLAYER_EVENT_NUMBER_CHANNELS_CREATED\n");
        attribute_id 	    = ATTRIBUTE_ID_number_channels;
        attribute           = &class_device_attr_number_channels;
    	break;
    }

    /* Find specific playback */
    PlaybackData            = get_playback (Event->playback);
    if (!PlaybackData)
    {
        SYSFS_ERROR("This playback does not exist\n");
        goto err1;
    }

    /* Find specific stream */
    StreamData              = get_stream (PlaybackData, Event->stream);
    if (!StreamData)
    {
	/* Stream doesn't exist -> create new stream and get it */
        Result 		= event_stream_created_handler(Event);
        if (Result)
        {
        	mutex_unlock(&SysfsWriteLock);
        	return Result;
        }

        StreamData      = get_stream (PlaybackData, Event->stream);
        if (!StreamData)
        {
            SYSFS_ERROR("This stream does not exist\n");
            goto err1;
        }
    }

    /* Populate structure */
    StreamData->attribute_data[attribute_id].component   = Event->component;

    /* Create attribute */
    Result  = class_device_create_file(&StreamData->stream_class_device, attribute);
    if (Result)
    {
        SYSFS_ERROR("class_device_create_file failed (%d)\n", Result);
        goto err1;
    }

    mutex_unlock(&SysfsWriteLock);

    StreamData->notify_on_destroy = true;
    do_notify();

    return 0;
err1:
    mutex_unlock(&SysfsWriteLock);
    return 1;
}
/*}}}  */
/*{{{  event_input_format_changed_handler*/
int event_input_format_changed_handler(struct player_event_s* Event)
{
    struct playback_data_s* 			PlaybackData;
    struct stream_data_s*			StreamData;

    if ((!Event->playback) || (!Event->stream) || (!Event->component))
    {
        SYSFS_ERROR("%d: One or more parameters are NULL. Nothing to do. \n", __LINE__);
        return 1;
    }

    /* Find specific playback */
    PlaybackData            = get_playback (Event->playback);
    if (!PlaybackData)
    {
        SYSFS_ERROR("This playback does not exist\n");
        return 1;
    }

    /* Find specific stream */
    StreamData              = get_stream (PlaybackData, Event->stream);
    if (!StreamData)
    {
        SYSFS_ERROR("This stream does not exist\n");
        return 1;
    }

    SYSFS_DEBUG("Signalling hotplug notification\n");
    sysfs_notify (&(StreamData->stream_class_device.kobj), NULL, "input_format");

    return 0;
}
/*}}}  */
/*{{{  dump*/
void dump (void)
{
    int p;
    int s;

    for (p = 0; p < PLAYBACK_MAX_NUMBER; p++)
    {
        if (playback_data[p].playback)
        {
            SYSFS_DEBUG("***** (%d)  Playback %p, dev_t %x, class_device %p\n", p, playback_data[p].playback,
                          playback_data[p].playback_dev_t, playback_data[p].playback_class_device);
            for (s = 0; s < STREAM_MAX_NUMBER; s++)
            {
                if (playback_data[p].stream_data[s].stream)
                    SYSFS_DEBUG("     ***** (%d)  Stream %p\n", s, playback_data[p].stream_data[s].stream);
            }
        }
    }
}
/*}}}  */
/*{{{  SysfsInit*/
void SysfsInit (void)
{
    int res;
    int p;
    int s;

    SYSFS_DEBUG("\n");

    res = class_register(&player2_class);
    if (0 != res) {
        SYSFS_ERROR ("Unable to create 'player2' class\n");
        return; // init call is void so we can't propagate error
    }

    for (p = 0; p < PLAYBACK_MAX_NUMBER; p++)
    {
        memset (&playback_data[p], 0, sizeof (struct playback_data_s));
        playback_data[p].id     = p;
        for (s = 0; s < STREAM_MAX_NUMBER; s++)
            playback_data[p].stream_data[s].id  = s;
    }

    PlayerRegisterEventSignalCallback (PlayerSetEvent);

    mutex_init(&SysfsWriteLock);
}
/*}}}  */
/*{{{  SysfsDelete*/
void SysfsDelete (void)
{
    SYSFS_DEBUG("\n");


    if (playback_count)
    {
	SYSFS_ERROR("Delete called while playbacks still exists\n");
	return;
    }

    class_unregister(&player2_class);

    mutex_destroy(&SysfsWriteLock);
}
/*}}}  */

/*{{{  PlayerSetEvent*/
static int PlayerSetEvent (struct player_event_s*       Event)
{
    int Result = 0;

    /*SYSFS_DEBUG("\n");*/
    //dump ();

    /*SYSFS_DEBUG("DEBUG Event->playback  = %p\n",Event->playback);*/
    /*SYSFS_DEBUG("DEBUG Event->stream    = %p\n",Event->stream);*/
    /*SYSFS_DEBUG("DEBUG Event->component = %p\n",Event->component);*/

    switch (Event->code)
    {
        case PLAYER_EVENT_PLAYBACK_CREATED:
        {
            SYSFS_DEBUG("PLAYER_EVENT_PLAYBACK_CREATED\n");

            Result = event_playback_created_handler(Event);
	    if (Result) return Result;

            break;
        }

        case PLAYER_EVENT_PLAYBACK_TERMINATED:
        {
            SYSFS_DEBUG("PLAYER_EVENT_PLAYBACK_TERMINATED\n");

            Result = event_playback_terminated_handler(Event);
	    if (Result) return Result;

            break;
        }

        case PLAYER_EVENT_INPUT_FORMAT_CREATED:
        case PLAYER_EVENT_SUPPORTED_INPUT_FORMAT_CREATED:
        case PLAYER_EVENT_DECODE_ERRORS_CREATED:
        case PLAYER_EVENT_SAMPLE_FREQUENCY_CREATED:
        case PLAYER_EVENT_NUMBER_CHANNELS_CREATED:
        case PLAYER_EVENT_NUMBER_OF_SAMPLES_PROCESSED:
        {
            Result = event_attribute_created_handler(Event);
            if (Result) return Result;

            break;
        }

        case PLAYER_EVENT_STREAM_CREATED:
        {
            SYSFS_DEBUG("PLAYER_EVENT_STREAM_CREATED\n");

            Result = event_stream_created_handler(Event);
            if (Result) return Result;

            break;
        }

        case PLAYER_EVENT_STREAM_TERMINATED:
        {
            SYSFS_DEBUG("PLAYER_EVENT_STREAM_TERMINATED\n");

            Result = event_stream_terminated_handler(Event);
	    if (Result) return Result;

	    break;
        }

        case PLAYER_EVENT_INPUT_FORMAT_CHANGED:
        {
            SYSFS_DEBUG("PLAYER_EVENT_INPUT_FORMAT_CHANGED\n");

            Result = event_input_format_changed_handler(Event);
	    if (Result) return Result;

            break;
        }

        default:
        {
            SYSFS_DEBUG("EVENT UNKNOWN %x\n", Event->code);
            break;
        }
    }

    return Result;
}
/*}}}  */

EXPORT_SYMBOL(SysfsInit);
EXPORT_SYMBOL(player_sysfs_get_class_device);
EXPORT_SYMBOL(player_sysfs_get_player_class);
EXPORT_SYMBOL(player_sysfs_get_stream_id);
EXPORT_SYMBOL(player_sysfs_new_attribute_notification);
EXPORT_SYMBOL(player_sysfs_get_err_threshold);
