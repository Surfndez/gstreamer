/* -*- Mode: C; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2 -*- */
/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* #define GST_DEBUG_ENABLED */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

/* taken from linux/cdrom.h */
#define CD_MSF_OFFSET       150	/* MSF numbering offset of first frame */
#define CD_SECS              60	/* seconds per minute */
#define CD_FRAMES            75	/* frames per second */

#include "gstcdparanoia.h"


static GstElementDetails cdparanoia_details = {
  "CD Audio (cdda) Source, Paranoia IV",
  "Source/File",
  "LGPL",
  "Read audio from CD in paranoid mode",
  VERSION,
  "Erik Walthinsen <omega@cse.ogi.edu>",
  "(C) 2000",
};

GST_PAD_TEMPLATE_FACTORY (cdparanoia_src_factory,
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_CAPS_NEW (
    "cdparanoia_src",
    "audio/raw",
	"format", 	GST_PROPS_STRING ("int"),
	"law", 		GST_PROPS_INT (0),
	"endianness", 	GST_PROPS_INT (G_BYTE_ORDER),
	"signed", 	GST_PROPS_BOOLEAN (TRUE),
	"width", 	GST_PROPS_INT (16),
	"depth", 	GST_PROPS_INT (16),
	"rate", 	GST_PROPS_INT (44100),
	"channels", 	GST_PROPS_INT (2),
	"chunksize", 	GST_PROPS_INT (CD_FRAMESIZE_RAW)
  )
);


/********** Define useful types for non-programmatic interfaces **********/
#define GST_TYPE_PARANOIA_MODE (gst_paranoia_mode_get_type())
static GType
gst_paranoia_mode_get_type (void)
{
  static GType paranoia_mode_type = 0;
  static GEnumValue paranoia_modes[] = {
    {0, "0", "Disable paranoid checking"},
    {1, "1", "cdda2wav-style overlap checking"},
    {2, "2", "Full paranoia"},
    {0, NULL, NULL},
  };

  if (!paranoia_mode_type) {
    paranoia_mode_type = g_enum_register_static ("GstParanoiaMode", paranoia_modes);
  }
  return paranoia_mode_type;
}

#define GST_TYPE_PARANOIA_ENDIAN (gst_paranoia_endian_get_type())
static GType
gst_paranoia_endian_get_type (void)
{
  static GType paranoia_endian_type = 0;
  static GEnumValue paranoia_endians[] = {
    {0, "0", "treat drive as little endian"},
    {1, "1", "treat drive as big endian"},
    {0, NULL, NULL},
  };

  if (!paranoia_endian_type) {
    paranoia_endian_type = g_enum_register_static ("GstParanoiaEndian", paranoia_endians);
  }
  return paranoia_endian_type;
}

/********** Standard stuff for signals and arguments **********/
/* CDParanoia signals and args */
enum
{
  SMILIE_CHANGE,
  TRANSPORT_ERROR,
  UNCORRECTED_ERROR,
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_LOCATION,
  ARG_GENERIC_DEVICE,
  ARG_DEFAULT_SECTORS,
  ARG_SEARCH_OVERLAP,
  ARG_ENDIAN,
  ARG_READ_SPEED,
  ARG_TOC_OFFSET,
  ARG_TOC_BIAS,
  ARG_NEVER_SKIP,
  ARG_ABORT_ON_SKIP,
  ARG_PARANOIA_MODE,
  ARG_SMILIE,
  ARG_DISCID
};


static void 		cdparanoia_class_init 		(CDParanoiaClass *klass);
static void 		cdparanoia_init 		(CDParanoia *cdparanoia);

static void 		cdparanoia_set_property 	(GObject *object, guint prop_id, 
							 const GValue *value, GParamSpec *pspec);
static void 		cdparanoia_get_property 	(GObject *object, guint prop_id,
							 GValue *value, GParamSpec *pspec);

static GstBuffer*	cdparanoia_get 			(GstPad *pad);
static gboolean 	cdparanoia_event 		(GstPad *pad, GstEvent *event);
static const GstEventMask*
			cdparanoia_get_event_mask 	(GstPad *pad);
static const GstFormat*
			cdparanoia_get_formats 		(GstPad *pad);
static gboolean 	cdparanoia_convert 		(GstPad *pad,
				    			 GstFormat src_format,
				    			 gint64 src_value, 
							 GstFormat *dest_format, 
							 gint64 *dest_value);
static gboolean 	cdparanoia_query 		(GstPad *pad, GstPadQueryType type,
		     					 GstFormat *format, gint64 *value);

static GstElementStateReturn cdparanoia_change_state (GstElement *element);


static GstElementClass *parent_class = NULL;
static guint cdparanoia_signals[LAST_SIGNAL] = { 0 };

static GstFormat track_format;
static GstFormat sector_format;

GType
cdparanoia_get_type (void)
{
  static GType cdparanoia_type = 0;

  if (!cdparanoia_type) {
    static const GTypeInfo cdparanoia_info = {
      sizeof (CDParanoiaClass), NULL,
      NULL,
      (GClassInitFunc) cdparanoia_class_init,
      NULL,
      NULL,
      sizeof (CDParanoia),
      0,
      (GInstanceInitFunc) cdparanoia_init,
    };

    cdparanoia_type = g_type_register_static (GST_TYPE_ELEMENT, "CDParanoia", &cdparanoia_info, 0);
  }
  return cdparanoia_type;
}

static void
cdparanoia_class_init (CDParanoiaClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  char *success = strerror_tr[0];

  success = NULL;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  cdparanoia_signals[SMILIE_CHANGE] =
    g_signal_new ("smilie_change", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CDParanoiaClass, smilie_change), NULL, NULL,
		  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
  cdparanoia_signals[TRANSPORT_ERROR] =
    g_signal_new ("transport_error", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CDParanoiaClass, transport_error), NULL, NULL,
		  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
  cdparanoia_signals[UNCORRECTED_ERROR] =
    g_signal_new ("uncorrected_error", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (CDParanoiaClass, uncorrected_error), NULL, NULL,
		  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_LOCATION, 
    g_param_spec_string ("location", "location", "location", 
	                 NULL, G_PARAM_READWRITE));	
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_GENERIC_DEVICE, 
    g_param_spec_string ("generic_device", "Generic device", "Use specified generic scsi device", 
	                 NULL, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_DEFAULT_SECTORS, 
    g_param_spec_int ("default_sectors", "Default sectors", 
	    	      "Force default number of sectors in read to n sectors", 
		      0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SEARCH_OVERLAP, 
    g_param_spec_int ("search_overlap", "Search overlap", 
	    	      "Force minimum overlap search during verification to n sectors", 
		       0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ENDIAN, 
    g_param_spec_enum ("endian", "Endian", "Force endian on drive", 
		       GST_TYPE_PARANOIA_ENDIAN, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_READ_SPEED, 
    g_param_spec_int ("read_speed", "Read speed", "Read from device at specified speed", 
		      G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_TOC_OFFSET,
    g_param_spec_int("toc_offset", "TOC offset", "Add <n> sectors to the values reported",
                     G_MININT,G_MAXINT,0,G_PARAM_READWRITE)); 
  g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_TOC_BIAS,
    g_param_spec_boolean("toc_bias", "TOC bias",
	    		 "Assume that the beginning offset of track 1 as reported in the TOC "
			 "will be addressed as LBA 0.  Necessary for some Toshiba drives to "
			 "get track boundaries",
                         TRUE,G_PARAM_READWRITE)); 
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_NEVER_SKIP, 
    g_param_spec_int ("never_skip", "Never skip", 
	    	      "never accept any less than perfect data reconstruction (don't allow "
		      "'V's) but if [n] is given, skip after [n] retries without progress.", 
		      0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ABORT_ON_SKIP, 
    g_param_spec_boolean ("abort_on_skip", "Abort on skip", "Abort on imperfect reads/skips", 
		          TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_PARANOIA_MODE, 
    g_param_spec_enum ("paranoia_mode", "Paranoia mode", "Type of checking to perform", 
		       GST_TYPE_PARANOIA_MODE, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_DISCID,
    g_param_spec_string ("discid", "discid", "The dics id",
			 NULL, G_PARAM_READABLE));

  gobject_class->set_property = cdparanoia_set_property;
  gobject_class->get_property = cdparanoia_get_property;

  gstelement_class->change_state = cdparanoia_change_state;
}

static void
cdparanoia_init (CDParanoia *cdparanoia)
{
  cdparanoia->srcpad =
    gst_pad_new_from_template (GST_PAD_TEMPLATE_GET (cdparanoia_src_factory), "src");
  gst_pad_set_get_function (cdparanoia->srcpad, cdparanoia_get);
  gst_pad_set_event_function (cdparanoia->srcpad, cdparanoia_event);
  gst_pad_set_event_mask_function (cdparanoia->srcpad, cdparanoia_get_event_mask);
  gst_pad_set_convert_function (cdparanoia->srcpad, cdparanoia_convert);
  gst_pad_set_query_function (cdparanoia->srcpad, cdparanoia_query);
  gst_pad_set_formats_function (cdparanoia->srcpad, cdparanoia_get_formats);

  gst_element_add_pad (GST_ELEMENT (cdparanoia), cdparanoia->srcpad);

  cdparanoia->device = g_strdup ("/dev/cdrom");
  cdparanoia->generic_device = NULL;
  cdparanoia->start_sector = -1;
  cdparanoia->end_sector = -1;
  cdparanoia->cur_sector = -1;
  cdparanoia->start_track = -1;
  cdparanoia->end_track = -1;
  cdparanoia->last_track = -1;
  cdparanoia->default_sectors = -1;
  cdparanoia->search_overlap = -1;
  cdparanoia->endian = 0;
  cdparanoia->read_speed = -1;
  cdparanoia->toc_offset = 0;
  cdparanoia->toc_bias = FALSE;
  cdparanoia->never_skip = FALSE;
  cdparanoia->paranoia_mode = 2;
  cdparanoia->abort_on_skip = FALSE;

  cdparanoia->cur_sector = 0;
  cdparanoia->seq = 0;

  cdparanoia->no_tracks = 0;
  cdparanoia->total_seconds = 0;
}


static void
cdparanoia_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  CDParanoia *src;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_CDPARANOIA (object));

  src = CDPARANOIA (object);

  switch (prop_id) {
    case ARG_LOCATION:
      if (src->device)
	g_free (src->device);
      /* clear the filename if we get a NULL (is that possible?) */
      if (!g_ascii_strcasecmp (g_value_get_string (value), ""))
	src->device = NULL;
      /* otherwise set the new filename */
      else
	src->device = g_strdup (g_value_get_string (value));
      break;
    case ARG_GENERIC_DEVICE:

      if (src->generic_device)
	g_free (src->generic_device);
      /* reset the device if we get a NULL (is that possible?) */
      if (!g_ascii_strcasecmp (g_value_get_string (value), ""))
	src->generic_device = NULL;
      /* otherwise set the new filename */
      else
	src->generic_device = g_strdup (g_value_get_string (value));
      break;
    case ARG_DEFAULT_SECTORS:
      src->default_sectors = g_value_get_int (value);
      break;
    case ARG_SEARCH_OVERLAP:
      src->search_overlap = g_value_get_int (value);
      break;
    case ARG_ENDIAN:
      src->endian = g_value_get_enum (value);
      break;
    case ARG_READ_SPEED:
      src->read_speed = g_value_get_int (value);
      cdda_speed_set (src->d, src->read_speed);
      break;
    case ARG_TOC_OFFSET:
      src->toc_offset = g_value_get_int (value);
      break;
    case ARG_TOC_BIAS:
      src->toc_bias = g_value_get_boolean (value);
      break;
    case ARG_NEVER_SKIP:
      src->never_skip = g_value_get_int (value);
      break;
    case ARG_ABORT_ON_SKIP:
      src->abort_on_skip = g_value_get_boolean (value);
      break;
    case ARG_PARANOIA_MODE:
      src->paranoia_mode = g_value_get_enum (value);
      break;
    default:
      break;
  }

}

static void
cdparanoia_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  CDParanoia *src;

  g_return_if_fail (GST_IS_CDPARANOIA (object));

  src = CDPARANOIA (object);

  switch (prop_id) {
    case ARG_LOCATION:
      g_value_set_string (value, src->device);
      break;
    case ARG_GENERIC_DEVICE:
      g_value_set_string (value, src->generic_device);
      break;
    case ARG_DEFAULT_SECTORS:
      g_value_set_int (value, src->default_sectors);
      break;
    case ARG_SEARCH_OVERLAP:
      g_value_set_int (value, src->search_overlap);
      break;
    case ARG_ENDIAN:
      g_value_set_enum (value, src->endian);
      break;
    case ARG_READ_SPEED:
      g_value_set_int (value, src->read_speed);
      break;
    case ARG_TOC_OFFSET:
      g_value_set_int (value, src->toc_offset);
      break;
    case ARG_TOC_BIAS:
      g_value_set_boolean (value, src->toc_bias);
      break;
    case ARG_NEVER_SKIP:
      g_value_set_int (value, src->never_skip);
      break;
    case ARG_ABORT_ON_SKIP:
      g_value_set_boolean (value, src->abort_on_skip);
      break;
    case ARG_PARANOIA_MODE:
      g_value_set_enum (value, src->paranoia_mode);
      break;
    case ARG_DISCID:
      g_value_set_string (value, src->discid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
cdparanoia_callback (long inpos, int function)
{
}

static GstBuffer *
cdparanoia_get (GstPad *pad)
{
  CDParanoia *src;
  GstBuffer *buf;
  static gint16 *cdda_buf;

  g_return_val_if_fail (pad != NULL, NULL);
  src = CDPARANOIA (gst_pad_get_parent (pad));
  g_return_val_if_fail (GST_FLAG_IS_SET (src, CDPARANOIA_OPEN), NULL);

  /* read a sector */
  cdda_buf = paranoia_read (src->p, cdparanoia_callback);

  /* update current sector and stop things if appropriate */
  src->cur_sector++;

  if (src->cur_sector == src->end_sector) {
    GST_DEBUG (0, "setting EOS");
    gst_element_set_eos (GST_ELEMENT (src));

    buf = GST_BUFFER (gst_event_new (GST_EVENT_EOS));
  } else {
    /* have to copy the buffer for now since we don't own it... */
    /* FIXME must ask monty about allowing ownership transfer */
    buf = gst_buffer_new_and_alloc (CD_FRAMESIZE_RAW);
    memcpy (GST_BUFFER_DATA (buf), cdda_buf, CD_FRAMESIZE_RAW);

    GST_BUFFER_TIMESTAMP (buf) = ((CD_FRAMESIZE_RAW >> 2) * src->seq * GST_SECOND) / 44100;
    src->seq++;
  }

  /* we're done, push the buffer off now */
  return buf;
}

/* need some stuff to get a discid (cdparanoia doesn't do cddb but lets
 * not stop other ppl doing it ;-) */
typedef int byte;

typedef struct
{
  byte m;
  byte s;
  byte f;
}
toc_msf;

/* cdparanoia provides the toc in lba format dang we need it in msf so
 * we have to convert it */
static inline void
lba_to_msf (const gint lba, byte *m, byte *s, byte *f)
{
  gint lba2 = lba;

  lba2 += CD_MSF_OFFSET;
  lba2 &= 0xffffff;
  *m = lba2 / (CD_SECS * CD_FRAMES);
  lba2 %= (CD_SECS * CD_FRAMES);
  *s = lba2 / CD_FRAMES;
  *f = lba2 % CD_FRAMES;
  *f += (*m) * 60 * 75;
  *f += (*s) * 75;
}

static void
lba_toc_to_msf_toc (TOC *lba_toc, toc_msf *msf_toc, gint tracks)
{
  gint i;

  for (i = 0; i <= tracks; i++)
    lba_to_msf (lba_toc[i].dwStartSector, &msf_toc[i].m, &msf_toc[i].s, &msf_toc[i].f);
}

/* the cddb hash function */
static guint
cddb_sum (gint n)
{
  guint ret;

  ret = 0;
  while (n > 0) {
    ret += (n % 10);
    n /= 10;
  }
  return ret;
}

static void
cddb_discid (gchar *discid, toc_msf *toc, gint tracks)
{
  guint i = 0, t = 0, n = 0;

  while (i < tracks) {
    n = n + cddb_sum ((toc[i].m * 60) + toc[i].s);
    i++;
  }
  t = ((toc[tracks].m * 60) + toc[tracks].s) - ((toc[0].m * 60)
						+ toc[0].s);
  sprintf (discid, "%08x", ((n % 0xff) << 24 | t << 8 | tracks));
}

/* get all the cddb info at once */
static void
get_cddb_info (TOC *toc, gint tracks, gchar *discid, gint64 *offsets, gint64 *total_seconds)
{
  toc_msf msf_toc[MAXTRK];
  gint i;
  gint64 *p = offsets;

  lba_toc_to_msf_toc (toc, &msf_toc[0], tracks);
  cddb_discid (discid, &msf_toc[0], tracks);

  for (i = 0; i < tracks; i++) {
    *p++ = msf_toc[i].f;
  }

  *total_seconds = msf_toc[tracks].f / 75;

}

/* open the file, necessary to go to RUNNING state */
static gboolean
cdparanoia_open (CDParanoia *src)
{
  gint i;
  gint paranoia_mode;

  g_return_val_if_fail (!GST_FLAG_IS_SET (src, CDPARANOIA_OPEN), FALSE);

  GST_DEBUG_ENTER ("(\"%s\",...)", gst_element_get_name (GST_ELEMENT (src)));

  /* find the device */
  if (src->generic_device != NULL) {
    src->d = cdda_identify_scsi (src->generic_device, src->device, FALSE, NULL);
  } else {
    if (src->device != NULL) {
      src->d = cdda_identify (src->device, FALSE, NULL);
    } else {
      src->d = cdda_identify ("/dev/cdrom", FALSE, NULL);
    }
  }

  /* fail if the device couldn't be found */
  if (src->d == NULL) {
    GST_DEBUG (0, "couldn't open device");
    return FALSE;
  }

  /* set verbosity mode */
  cdda_verbose_set (src->d, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

  /* set various other parameters */
  if (src->default_sectors != -1) {
    if ((src->default_sectors < 0) || (src->default_sectors > 100)) {
      GST_DEBUG (0, "default sector read size must be 1 <= n <= 100");
      cdda_close (src->d);
      src->d = NULL;
      return FALSE;
    } else {
      src->d->nsectors = src->default_sectors;
      src->d->bigbuff = src->default_sectors * CD_FRAMESIZE_RAW;
    }
  }
  if (src->search_overlap != -1) {
    if ((src->search_overlap < 0) || (src->search_overlap > 75)) {
      GST_DEBUG (0, "search overlap must be 0 <= n <= 75");
      cdda_close (src->d);
      src->d = NULL;
      return FALSE;
    }
  }

  /* open the disc */
  if (cdda_open (src->d)) {
    GST_DEBUG (0, "couldn't open disc");
    cdda_close (src->d);
    src->d = NULL;
    return FALSE;
  }

  /* set up some more stuff */
  src->no_tracks = src->d->tracks;

  /* I don't like this here i would prefer it under get_cddb_info but for somereason
   * when leaving the function it clobbers the allocated mem and all is lost bugger
   */

  get_cddb_info (&src->d->disc_toc[0], src->no_tracks, src->discid,
		 src->offsets, &src->total_seconds);

  g_object_freeze_notify (G_OBJECT (src));
  g_object_notify (G_OBJECT (src), "discid");
  g_object_thaw_notify (G_OBJECT (src));

  if (src->toc_bias) {
    src->toc_offset -= cdda_track_firstsector (src->d, 1);
  }
  for (i = 0; i < src->d->tracks + 1; i++) {
    src->d->disc_toc[i].dwStartSector += src->toc_offset;
  }

  if (src->read_speed != -1) {
    cdda_speed_set (src->d, src->read_speed);
  }

  /* if the start_track is set, override the start_sector */
  if (src->start_track != -1) {
    src->start_sector = cdda_track_firstsector (src->d, src->start_track);
    /* if neither start_track nor start_sector is set, */
  } else if (src->start_sector == -1) {
    src->start_sector = cdda_disc_firstsector (src->d);
  }
  /* if the end_track is set, override the end_sector */
  if (src->end_track != -1) {
    src->end_sector = cdda_track_lastsector (src->d, src->end_track);
    /* if neither end_track nor end_sector is set, */
  } else if (src->end_sector == -1) {
    src->end_sector = cdda_disc_lastsector (src->d);
  }

  /* set last_track */
  src->last_track = cdda_tracks (src->d);

  /* create the paranoia struct and set it up */
  src->p = paranoia_init (src->d);
  if (src->p == NULL) {
    GST_DEBUG (0, "couldn't create paranoia struct");
    return FALSE;
  }

  if (src->paranoia_mode == 0)
    paranoia_mode = PARANOIA_MODE_DISABLE;
  else if (src->paranoia_mode == 1)
    paranoia_mode = PARANOIA_MODE_OVERLAP;
  else
    paranoia_mode = PARANOIA_MODE_FULL;
  if (src->never_skip)
    paranoia_mode |= PARANOIA_MODE_NEVERSKIP;
  paranoia_modeset (src->p, paranoia_mode);

  if (src->search_overlap != -1) {
    paranoia_overlapset (src->p, src->search_overlap);
  }

  src->cur_sector = src->start_sector;
  paranoia_seek (src->p, src->cur_sector, SEEK_SET);
  GST_DEBUG (0, "successfully seek'd to beginning of disk");

  GST_FLAG_SET (src, CDPARANOIA_OPEN);

  GST_DEBUG_LEAVE ("");

  return TRUE;
}

/* close the file */
static void
cdparanoia_close (CDParanoia *src)
{
  g_return_if_fail (GST_FLAG_IS_SET (src, CDPARANOIA_OPEN));

  /* kill the paranoia state */
  paranoia_free (src->p);
  src->p = NULL;

  src->total_seconds = 0LL;
  /* close the disc */
  cdda_close (src->d);
  src->d = NULL;

  GST_FLAG_UNSET (src, CDPARANOIA_OPEN);
}

static GstElementStateReturn
cdparanoia_change_state (GstElement *element)
{
  CDParanoia *cdparanoia;

  g_return_val_if_fail (GST_IS_CDPARANOIA (element), GST_STATE_FAILURE);

  cdparanoia = CDPARANOIA (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_NULL_TO_READY:
      break;
    case GST_STATE_READY_TO_PAUSED:
      if (!cdparanoia_open (CDPARANOIA (element))) {
	g_warning ("cdparanoia: failed opening cd");
	return GST_STATE_FAILURE;
      }
      cdparanoia->seq = 0;
      break;
    case GST_STATE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_READY:
      cdparanoia_close (CDPARANOIA (element));
      cdparanoia->seq = 0;
      break;
    case GST_STATE_READY_TO_NULL:
      break;
    default:
      break;
  }

  /* if we haven't failed already, give the parent class a chance too ;-) */
  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}

static const GstEventMask *
cdparanoia_get_event_mask (GstPad *pad)
{
  static const GstEventMask masks[] = {
    {GST_EVENT_SEEK, GST_SEEK_METHOD_SET | GST_SEEK_METHOD_CUR},
    {0,}
  };

  return masks;
}

static gboolean
cdparanoia_event (GstPad *pad, GstEvent *event)
{
  CDParanoia *src;
  gint64 offset, endoffset;
  int format, start_sector, end_sector;

  src = CDPARANOIA (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      format = GST_EVENT_SEEK_FORMAT (event);
      offset = GST_EVENT_SEEK_OFFSET (event);

      if (format == sector_format) {
	start_sector = (int) offset;
      } 
      else if (format == track_format) {
	start_sector = cdda_track_firstsector (src->d, (int) offset);
      } 
      else {
	goto error;
      }

      switch (GST_EVENT_SEEK_METHOD (event)) {
	case GST_SEEK_METHOD_SET:
	  src->start_sector = start_sector;;
	  src->cur_sector = src->start_sector;

	  paranoia_seek (src->p, src->start_sector, SEEK_SET);
	  GST_DEBUG (0, "seeked to %d", src->start_sector);
	  break;
	case GST_SEEK_METHOD_CUR:
	  src->start_sector += start_sector;
	  src->cur_sector = src->start_sector;

	  paranoia_seek (src->p, src->start_sector, SEEK_SET);
	  GST_DEBUG (0, "seeked to %d", src->start_sector);
	  break;
	default:
	  break;
      }
      break;
    case GST_EVENT_SEEK_SEGMENT:
      format = GST_EVENT_SEEK_FORMAT (event);
      offset = GST_EVENT_SEEK_OFFSET (event);
      endoffset = GST_EVENT_SEEK_ENDOFFSET (event);

      if (format == sector_format) {
	start_sector = (int) offset;
	end_sector = (int) endoffset;
      }
      else if (format == track_format) {
	start_sector = cdda_track_firstsector (src->d, (int) offset);
	end_sector = cdda_track_lastsector (src->d, (int) endoffset);
      }
      else {
	goto error;
      }

      /* Pretend these are tracks for testing */
      src->start_sector = start_sector;
      src->end_sector = end_sector;
      src->cur_sector = src->start_sector;

      paranoia_seek (src->p, src->start_sector, SEEK_SET);
      GST_DEBUG (0, "seeked from %d to %d", src->start_sector, src->end_sector);
      break;
    default:
      goto error;
      break;
  }

  gst_event_unref (event);
  return TRUE;

error:
  g_print ("Event error\n");
  gst_event_unref (event);
  return FALSE;
}

static const GstFormat *
cdparanoia_get_formats (GstPad *pad)
{
  static GstFormat formats[] = {
    GST_FORMAT_TIME,
    GST_FORMAT_UNITS,
    0,
    0,
    0
  };

  formats[2] = track_format;
  formats[3] = sector_format;

  return formats;
}

static gboolean
cdparanoia_convert (GstPad *pad,
		    GstFormat src_format, gint64 src_value, 
		    GstFormat *dest_format, gint64 *dest_value)
{
  gboolean res = TRUE;
  CDParanoia *src;

  src = CDPARANOIA (gst_pad_get_parent (pad));

  switch (src_format) {
    case GST_FORMAT_TIME:
      break;
    case GST_FORMAT_UNITS:
      break;
    default:
      if (src_format == track_format) {
	switch (*dest_format) {
          case GST_FORMAT_TIME:
            *dest_value = src->offsets[src_value] / 75;
	    break;
          case GST_FORMAT_UNITS:
	    break;
	  default:
	    break;
	}
      }
      else if (src_format == sector_format) {
      }
      else 
	res = FALSE;
      break;
  }

  return res;
}

static gboolean
cdparanoia_query (GstPad *pad, GstPadQueryType type,
		  GstFormat *format, gint64 *value)
{
  gboolean res = TRUE;
  CDParanoia *src;

  src = CDPARANOIA (gst_pad_get_parent (pad));

  if (!GST_FLAG_IS_SET (src, CDPARANOIA_OPEN))
    return FALSE;

  switch (type) {
    case GST_PAD_QUERY_TOTAL:
      switch (*format) {
  	case GST_FORMAT_TIME:
          *value = src->total_seconds;	
          break;
  	case GST_FORMAT_UNITS:
	  res = FALSE;
          break;
        default:
          if (*format == track_format) {
            *value = src->no_tracks;	
          }
          else if (*format == sector_format) {
            *value = cdda_disc_lastsector (src->d);
          }
	  else
            res = FALSE;
	  break;
      }
      break;
    case GST_PAD_QUERY_POSITION:
      switch (*format) {
        default:
	  break;
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}


static gboolean
plugin_init (GModule *module, GstPlugin *plugin)
{
  GstElementFactory *factory;

  /* create an elementfactory for the cdparanoia element */
  factory = gst_element_factory_new ("cdparanoia", 
		                     GST_TYPE_CDPARANOIA, 
				     &cdparanoia_details);
  g_return_val_if_fail (factory != NULL, FALSE);

  /* register the source's caps */
  gst_element_factory_add_pad_template (factory, 
		  GST_PAD_TEMPLATE_GET (cdparanoia_src_factory));

  /* Register the track format */
  track_format = gst_format_register ("track", "CD track");
  sector_format = gst_format_register ("sector", "CD sector");

  /* and add the cdparanoia element factory to the plugin */
  gst_plugin_add_feature (plugin, GST_PLUGIN_FEATURE (factory));

  return TRUE;
}

GstPluginDesc plugin_desc = {
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "cdparanoia",
  plugin_init
};
