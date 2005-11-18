/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjmedia/sound.h>

/*
 * Null Factory Operations
 */
static pj_status_t null_sound_init(void);
static const char *null_sound_get_name(void);
static pj_status_t null_sound_destroy(void);
static pj_status_t null_sound_enum_devices(int *count, char *dev_names[]);
static pj_status_t null_sound_create_dev(const char *dev_name, pj_snd_dev *dev);
static pj_status_t null_sound_destroy_dev(pj_snd_dev *dev);


/*
 * Null Device Operations
 */
static pj_status_t null_sound_dev_open( pj_snd_dev *dev, pj_snd_role_t role );
static pj_status_t null_sound_dev_close( pj_snd_dev *dev );
static pj_status_t null_sound_dev_play( pj_snd_dev *dev );
static pj_status_t null_sound_dev_record( pj_snd_dev *dev );


static pj_snd_dev_factory null_sound_factory = 
{
    &null_sound_init,
    &null_sound_get_name,
    &null_sound_destroy,
    &null_sound_enum_devices,
    &null_sound_create_dev,
    &null_sound_destroy_dev
};

static struct pj_snd_dev_op null_sound_dev_op = 
{
    &null_sound_dev_open,
    &null_sound_dev_close,
    &null_sound_dev_play,
    &null_sound_dev_record
};

PJ_DEF(pj_snd_dev_factory*) pj_nullsound_get_factory()
{
    return &null_sound_factory;
}

static pj_status_t null_sound_init(void)
{
    return 0;
}

static const char *null_sound_get_name(void)
{
    return "nullsound";
}

static pj_status_t null_sound_destroy(void)
{
    return 0;
}

static pj_status_t null_sound_enum_devices(int *count, char *dev_names[])
{
    *count = 1;
    dev_names[0] = "nullsound";
    return 0;
}

static pj_status_t null_sound_create_dev(const char *dev_name, pj_snd_dev *dev)
{
    PJ_UNUSED_ARG(dev_name);
    dev->op = &null_sound_dev_op;
    return 0;
}

static pj_status_t null_sound_destroy_dev(pj_snd_dev *dev)
{
    PJ_UNUSED_ARG(dev);
    return 0;
}


/*
 * Null Device Operations
 */
static pj_status_t null_sound_dev_open( pj_snd_dev *dev, pj_snd_role_t role )
{
    PJ_UNUSED_ARG(dev);
    PJ_UNUSED_ARG(role);
    return 0;
}

static pj_status_t null_sound_dev_close( pj_snd_dev *dev )
{
    PJ_UNUSED_ARG(dev);
    return 0;
}

static pj_status_t null_sound_dev_play( pj_snd_dev *dev )
{
    PJ_UNUSED_ARG(dev);
    return 0;
}

static pj_status_t null_sound_dev_record( pj_snd_dev *dev )
{
    PJ_UNUSED_ARG(dev);
    return 0;
}

