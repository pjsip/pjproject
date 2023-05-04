/* 
 * Copyright (C) 2019 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/vid_conf.h>
#include <pjmedia/clock.h>
#include <pjmedia/converter.h>
#include <pjmedia/errno.h>
#include <pj/array.h>
#include <pj/log.h>
#include <pj/os.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)


#define CONF_NAME       "vidconf"
#define CONF_SIGN       PJMEDIA_SIG_VID_CONF

/* If set, conf will stop clock when there is no ports connection. However,
 * this may cause stuck if port remove/disconnect is called from the clock
 * callback. So better disable this for now.
 */
#define AUTO_STOP_CLOCK 0

/* Maximum number of consecutive errors that will only be printed once. */
#define MAX_ERR_COUNT 150

/* Clockrate for video timestamp unit */
#define TS_CLOCK_RATE   90000

#define THIS_FILE       "vid_conf.c"
#define TRACE_(x)       PJ_LOG(5,x)


/* Forward declarations */
typedef struct op_entry op_entry;

/*
 * Conference bridge.
 */
struct pjmedia_vid_conf
{
    pjmedia_vid_conf_setting opt;       /**< Settings.                      */
    pj_pool_t            *pool;         /**< Pool.                          */
    unsigned              port_cnt;     /**< Current number of ports.       */
    unsigned              connect_cnt;  /**< Total number of connections    */
    pj_mutex_t           *mutex;        /**< Conference mutex.              */
    struct vconf_port   **ports;        /**< Array of ports.                */
    pjmedia_clock        *clock;        /**< Clock.                         */

    op_entry             *op_queue;     /**< Queue of operations.           */
    op_entry             *op_queue_free;/**< Queue of free entries.         */
};


/*
 * Rendering state: converter, layout settings, etc.
 */
typedef struct render_state
{
    pjmedia_format_id   src_fmt_id;     /**< Source format ID.              */
    pjmedia_rect_size   src_frame_size; /**< Source frame size.             */
    pjmedia_rect        src_rect;       /**< Source region to be rendered.  */

    pjmedia_format_id   dst_fmt_id;     /**< Destination format ID.         */
    pjmedia_rect_size   dst_frame_size; /**< Destination frame size.        */
    pjmedia_rect        dst_rect;       /**< Destination region.            */

    pjmedia_converter   *converter;     /**< Converter.                     */

} render_state;


/*
 * Conference bridge port.
 */
typedef struct vconf_port
{
    pj_pool_t           *pool;          /**< Pool.                          */
    unsigned             idx;           /**< Port index.                    */
    pj_str_t             name;          /**< Port name.                     */
    pjmedia_port        *port;          /**< Video port.                    */
    pjmedia_format       format;        /**< Copy of port format info.      */
    pj_uint32_t          ts_interval;   /**< Port put/get interval.         */
    pj_timestamp         ts_next;       /**< Time for next put/get_frame(). */
    void                *get_buf;       /**< Buffer for get_frame().        */
    pj_size_t            get_buf_size;  /**< Buffer size for get_frame().   */
    pj_size_t            get_frm_size;	/**< Frame size for get_frame().    */
    pj_bool_t            got_frame;     /**< Last get_frame() got frame?    */
    void                *put_buf;       /**< Buffer for put_frame().        */
    pj_size_t            put_buf_size;  /**< Buffer size for put_frame().   */
    pj_size_t            put_frm_size;	/**< Frame size for put_frame().    */
    unsigned             listener_cnt;  /**< Number of listeners.           */
    unsigned            *listener_slots;/**< Array of listeners (for info). */

    unsigned             transmitter_cnt;/**<Number of transmitters.        */
    unsigned            *transmitter_slots;/**< Array of transmitters.      */
    pj_pool_t          **render_pool;   /**< Array of pool for render state */
    render_state       **render_states; /**< Array of render_state (one for
                                             each transmitter).             */

    pj_status_t           last_err;     /**< Last error status.             */
    unsigned              last_err_cnt; /**< Last error count.              */
} vconf_port;


/* Prototypes */
static void on_clock_tick(const pj_timestamp *ts, void *user_data);
static pj_status_t render_src_frame(vconf_port *src, vconf_port *sink,
                                    unsigned transmitter_idx);
static void update_render_state(pjmedia_vid_conf *vid_conf, vconf_port *cp);
static void cleanup_render_state(vconf_port *cp,
                                 unsigned transmitter_idx);


/* As we don't hold mutex in the clock tick, some video conference operations
 * that change video conference states need to be synchronized with the clock.
 * So some steps of the operations needs to be executed within the clock tick
 * context, especially the steps related to changing ports connection and
 * updating rendering states.
 */

/* Synchronized operation type enumeration. */
typedef enum op_type
{
    OP_UNKNOWN,
    OP_REMOVE_PORT,
    OP_CONNECT_PORTS,
    OP_DISCONNECT_PORTS,
    OP_UPDATE_PORT
} op_type;

/* Synchronized operation parameter. */
typedef union op_param
{
    struct {
        unsigned port;
    } remove_port;

    struct {
        unsigned src;
        unsigned sink;
    } connect_ports;

    struct {
        unsigned src;
        unsigned sink;
    } disconnect_ports;

    struct {
        unsigned port;
    } update_port;

} op_param;

/* Synchronized operation list entry. */
typedef struct op_entry {
    PJ_DECL_LIST_MEMBER(struct op_entry);
    op_type          type;
    op_param         param;
} op_entry;

/* Prototypes of synchronized operation */
static void op_remove_port(pjmedia_vid_conf *conf, const op_param *prm);
static void op_connect_ports(pjmedia_vid_conf *conf, const op_param *prm);
static void op_disconnect_ports(pjmedia_vid_conf *conf, const op_param *prm);
static void op_update_port(pjmedia_vid_conf *conf, const op_param *prm);

static op_entry* get_free_op_entry(pjmedia_vid_conf *conf)
{
    op_entry *ope = NULL;

    /* Get from empty list if any, otherwise, allocate a new one */
    if (!pj_list_empty(conf->op_queue_free)) {
        ope = conf->op_queue_free->next;
        pj_list_erase(ope);
    } else {
        ope = PJ_POOL_ZALLOC_T(conf->pool, op_entry);
    }
    return ope;
}

static void handle_op_queue(pjmedia_vid_conf *conf)
{
    op_entry *op, *next_op;
    
    op = conf->op_queue->next;
    while (op != conf->op_queue) {
        next_op = op->next;
        pj_list_erase(op);

        switch(op->type) {
            case OP_REMOVE_PORT:
                op_remove_port(conf, &op->param);
                break;
            case OP_CONNECT_PORTS:
                op_connect_ports(conf, &op->param);
                break;
            case OP_DISCONNECT_PORTS:
                op_disconnect_ports(conf, &op->param);
                break;
            case OP_UPDATE_PORT:
                op_update_port(conf, &op->param);
                break;
            default:
                pj_assert(!"Invalid sync-op in video conference");
                break;
        }

        op->type = OP_UNKNOWN;
        pj_list_push_back(conf->op_queue_free, op);
        op = next_op;
    }
}


/*
 * Initialize video conference settings with default values.
 */
PJ_DEF(void) pjmedia_vid_conf_setting_default(pjmedia_vid_conf_setting *opt)
{
    pj_bzero(opt, sizeof(*opt));
    opt->max_slot_cnt = 32;
    opt->frame_rate = 60;
}


/*
 * Create a video conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_create(
                                        pj_pool_t *parent_pool,
                                        const pjmedia_vid_conf_setting *opt,
                                        pjmedia_vid_conf **p_vid_conf)
{
    pj_pool_t *pool;
    pjmedia_vid_conf *vid_conf;
    pjmedia_clock_param clock_param;
    pj_status_t status;

    PJ_ASSERT_RETURN(parent_pool && p_vid_conf, PJ_EINVAL);

    /* Create own pool */
    pool = pj_pool_create(parent_pool->factory, "vidconf", 500, 500, NULL);
    if (!pool) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in alloc"));
        return PJ_ENOMEM;
    }

    /* Allocate conf structure */
    vid_conf = PJ_POOL_ZALLOC_T(pool, pjmedia_vid_conf);
    PJ_ASSERT_RETURN(vid_conf, PJ_ENOMEM);
    vid_conf->pool = pool;

    /* Init settings */
    if (opt) {
        vid_conf->opt = *opt;
    } else {
        pjmedia_vid_conf_setting_default(&vid_conf->opt);
    }

    /* Allocate ports */
    vid_conf->ports = (vconf_port**)
                      pj_pool_zalloc(pool, vid_conf->opt.max_slot_cnt *
                                           sizeof(vconf_port*));
    if (!vid_conf->ports) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in alloc ports"));
        pj_pool_safe_release(&vid_conf->pool);
        return PJ_ENOMEM;
    }

    /* Create mutex */
    status = pj_mutex_create_recursive(pool, CONF_NAME, &vid_conf->mutex);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Create failed in create mutex"));
        pjmedia_vid_conf_destroy(vid_conf);
        return status;
    }

    /* Create clock */
    pj_bzero(&clock_param, sizeof(clock_param));
    clock_param.clock_rate = TS_CLOCK_RATE;
    clock_param.usec_interval = 1000000 / vid_conf->opt.frame_rate;
    status = pjmedia_clock_create2(pool, &clock_param, 0, &on_clock_tick,
                                   vid_conf, &vid_conf->clock);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1, (THIS_FILE, status, "Create failed in create clock"));
        pjmedia_vid_conf_destroy(vid_conf);
        return status;
    }

    /* Allocate synchronized operation queues */
    vid_conf->op_queue = PJ_POOL_ZALLOC_T(pool, op_entry);
    vid_conf->op_queue_free = PJ_POOL_ZALLOC_T(pool, op_entry);
    if (!vid_conf->op_queue || !vid_conf->op_queue_free) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in create queues"));
        pjmedia_vid_conf_destroy(vid_conf);
        return PJ_ENOMEM;
    }
    pj_list_init(vid_conf->op_queue);
    pj_list_init(vid_conf->op_queue_free);

    /* Done */
    *p_vid_conf = vid_conf;

    PJ_LOG(4,(THIS_FILE, "Created video conference bridge with %d ports",
              vid_conf->opt.max_slot_cnt));

    return PJ_SUCCESS;
}


/*
 * Destroy video conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_destroy(pjmedia_vid_conf *vid_conf)
{
    unsigned i;

    PJ_ASSERT_RETURN(vid_conf, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Video conference bridge destroy requested"));

    /* Destroy clock */
    if (vid_conf->clock) {
        pjmedia_clock_destroy(vid_conf->clock);
        vid_conf->clock = NULL;
    }

    /* Remove any registered ports (at least to cleanup their pool) */
    for (i=0; i < vid_conf->opt.max_slot_cnt; ++i) {
        if (vid_conf->ports[i]) {
            op_param prm;
            prm.remove_port.port = i;
            op_remove_port(vid_conf, &prm);
        }
    }

    /* Destroy mutex */
    if (vid_conf->mutex) {
        pj_mutex_destroy(vid_conf->mutex);
        vid_conf->mutex = NULL;
    }

    /* Release pool */
    if (vid_conf->pool) {
        pj_pool_safe_release(&vid_conf->pool);
    }

    PJ_LOG(4,(THIS_FILE, "Video conference bridge destroyed"));

    return PJ_SUCCESS;
}


/*
 * Add a media port to the video conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_add_port( pjmedia_vid_conf *vid_conf,
                                               pj_pool_t *parent_pool,
                                               pjmedia_port *port,
                                               const pj_str_t *name,
                                               void *opt,
                                               unsigned *p_slot)
{
    pj_pool_t *pool = NULL;
    vconf_port *cport = NULL;
    unsigned index;
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(5,(THIS_FILE, "Add port %s requested", port->info.name.ptr));

    PJ_ASSERT_RETURN(vid_conf && parent_pool && port, PJ_EINVAL);
    PJ_ASSERT_RETURN(port->info.fmt.type==PJMEDIA_TYPE_VIDEO &&
                     port->info.fmt.detail_type==PJMEDIA_FORMAT_DETAIL_VIDEO,
                     PJ_EINVAL);
    PJ_UNUSED_ARG(opt);

    /* If name is not specified, use the port's name */
    if (!name)
        name = &port->info.name;

    pj_mutex_lock(vid_conf->mutex);

    if (vid_conf->port_cnt >= vid_conf->opt.max_slot_cnt) {
        PJ_PERROR(3,(THIS_FILE, PJ_ETOOMANY, "Add port %s failed", name->ptr));
        pj_assert(!"Too many ports");
        pj_mutex_unlock(vid_conf->mutex);
        return PJ_ETOOMANY;
    }

    /* Find empty port in the conference bridge. */
    for (index=0; index < vid_conf->opt.max_slot_cnt; ++index) {
        if (vid_conf->ports[index] == NULL)
            break;
    }
    pj_assert(index != vid_conf->opt.max_slot_cnt);

    /* Create pool */
    pool = pj_pool_create(parent_pool->factory, name->ptr, 500, 500, NULL);
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    /* Create port. */
    cport = PJ_POOL_ZALLOC_T(pool, vconf_port);
    if (!cport) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    /* Set pool, port, index, and name */
    cport->pool = pool;
    cport->port = port;
    cport->format = port->info.fmt;
    cport->idx  = index;
    pj_strdup_with_null(pool, &cport->name, name);

    /* Setup port's group lock if not yet */
    if (!port->grp_lock) {
        status = pjmedia_port_init_grp_lock(port, pool, NULL);
        if (status != PJ_SUCCESS)
            goto on_error;
    }

    /* Increase port ref count */
    pjmedia_port_add_ref(port);

    /* Init put/get_frame() intervals */
    {
        pjmedia_ratio *fps = &port->info.fmt.det.vid.fps;
        pj_uint32_t vconf_interval = (pj_uint32_t)
                                     (TS_CLOCK_RATE * 1.0 /
                                     vid_conf->opt.frame_rate);
        cport->ts_interval = (pj_uint32_t)(TS_CLOCK_RATE * 1.0 /
                                           fps->num * fps->denum);

        /* Normalize the interval */
        if (cport->ts_interval < vconf_interval) {
            cport->ts_interval = vconf_interval;
            PJ_LOG(3,(THIS_FILE, "Warning: frame rate of port %s is higher "
                                 "than video conference bridge (%d > %d)",
                                 name->ptr, (int)(fps->num/fps->denum),
                                 vid_conf->opt.frame_rate));
        }
    }

    /* Allocate buffer for put/get_frame() */
    {
        const pjmedia_video_format_info *vfi;
        pjmedia_video_apply_fmt_param vafp;

        vfi = pjmedia_get_video_format_info(NULL, port->info.fmt.id);
        if (!vfi) {
            PJ_LOG(3,(THIS_FILE, "pjmedia_vid_conf_add_port(): "
                                 "unrecognized format %04X",
                                 port->info.fmt.id));
            status = PJMEDIA_EBADFMT;
            goto on_error;
        }

        pj_bzero(&vafp, sizeof(vafp));
        vafp.size = port->info.fmt.det.vid.size;
        status = (*vfi->apply_fmt)(vfi, &vafp);
        if (status != PJ_SUCCESS) {
            PJ_LOG(3,(THIS_FILE, "pjmedia_vid_conf_add_port(): "
                                 "Failed to apply format %04X",
                                 port->info.fmt.id));
            goto on_error;
        }
        if (port->put_frame) {
            cport->put_buf_size = cport->put_frm_size = vafp.framebytes;
            cport->put_buf = pj_pool_zalloc(cport->pool, cport->put_buf_size);

            /* Initialize sink buffer with black color. */
            status = pjmedia_video_format_fill_black(&port->info.fmt,
                                                     cport->put_buf,
                                                     cport->put_buf_size);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(3,(THIS_FILE, status,
                             "Warning: failed to init sink buffer "
                             " with black"));
            }
        }
        if (port->get_frame) {
            cport->get_buf_size = cport->get_frm_size = vafp.framebytes;
            cport->get_buf = pj_pool_zalloc(cport->pool, cport->get_buf_size);

            /* Initialize source buffer with black color. */
            status = pjmedia_video_format_fill_black(&port->info.fmt,
                                                     cport->get_buf,
                                                     cport->get_buf_size);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(3,(THIS_FILE, status,
                             "Warning: failed to init source buffer "
                             "with black"));
            }
        }
    }

    /* Create listener array */
    cport->listener_slots = (unsigned*)
                            pj_pool_zalloc(pool,
                                           vid_conf->opt.max_slot_cnt *
                                           sizeof(unsigned));
    if (!cport->listener_slots) {       
        status = PJ_ENOMEM;
        goto on_error;
    }

    /* Create transmitter array */
    cport->transmitter_slots = (unsigned*)
                               pj_pool_zalloc(pool,
                                              vid_conf->opt.max_slot_cnt *
                                              sizeof(unsigned));    
    if (!cport->transmitter_slots) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    /* Create pointer-to-render_state array */
    cport->render_states = (render_state**)
                           pj_pool_zalloc(pool,
                                          vid_conf->opt.max_slot_cnt *
                                          sizeof(render_state*));

    if (!cport->render_states) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    /* Create pointer-to-render-pool array */
    cport->render_pool = (pj_pool_t**)
                         pj_pool_zalloc(pool,
                                        vid_conf->opt.max_slot_cnt *
                                        sizeof(pj_pool_t*));    
    if (!cport->render_pool) {
        status = PJ_ENOMEM;
        goto on_error;
    }

    /* Register the conf port. */
    vid_conf->ports[index] = cport;
    vid_conf->port_cnt++;

    PJ_LOG(4,(THIS_FILE,"Added port %d (%.*s)",
              index, (int)cport->name.slen, cport->name.ptr));

    pj_mutex_unlock(vid_conf->mutex);

    /* Done. */
    if (p_slot) {
        *p_slot = index;
    }

    return PJ_SUCCESS;

on_error:
    if (pool)
        pj_pool_release(pool);

    PJ_PERROR(3, (THIS_FILE, status, "Add port %s failed", name->ptr));

    pj_mutex_unlock(vid_conf->mutex);
    return status;
}


/*
 * Remove a media port from the video conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_remove_port( pjmedia_vid_conf *vid_conf,
                                                  unsigned slot)
{
    vconf_port *cport;
    op_entry *ope;

    PJ_LOG(5,(THIS_FILE, "Port %d remove requested", slot));

    PJ_ASSERT_RETURN(vid_conf && slot<vid_conf->opt.max_slot_cnt, PJ_EINVAL);

    pj_mutex_lock(vid_conf->mutex);

    /* Port must be valid. */
    cport = vid_conf->ports[slot];
    if (cport == NULL) {
        PJ_PERROR(3, (THIS_FILE, PJ_EINVAL, "Remove port failed"));
        pj_mutex_unlock(vid_conf->mutex);
        return PJ_EINVAL;
    }

    PJ_LOG(4,(THIS_FILE, "Video port %d remove queued", slot));

    /* Queue the operation */
    ope = get_free_op_entry(vid_conf);
    ope->type = OP_REMOVE_PORT;
    ope->param.remove_port.port = slot;
    pj_list_push_back(vid_conf->op_queue, ope);

    pj_mutex_unlock(vid_conf->mutex);

    return PJ_SUCCESS;
}


static void op_remove_port(pjmedia_vid_conf *vid_conf,
                           const op_param *prm)
{
    unsigned slot = prm->remove_port.port;
    vconf_port *cport = vid_conf->ports[slot];

    pj_assert(cport);

    /* Disconnect slot -> listeners */
    while (cport->listener_cnt) {
        op_param p;
        p.disconnect_ports.src = slot;
        p.disconnect_ports.sink = cport->listener_slots[0];
        op_disconnect_ports(vid_conf, &p);
    }

    /* Disconnect transmitters -> slot */
    while (cport->transmitter_cnt) {
        op_param p;
        p.disconnect_ports.src = cport->transmitter_slots[0];
        p.disconnect_ports.sink = slot;
        op_disconnect_ports(vid_conf, &p);
    }

    /* Remove the port. */
    vid_conf->ports[slot] = NULL;
    --vid_conf->port_cnt;

    PJ_LOG(4,(THIS_FILE,"Removed port %d (%.*s)",
              slot, (int)cport->name.slen, cport->name.ptr));

    /* Decrease port ref count */
    pjmedia_port_dec_ref(cport->port);

    /* Release pool */
    pj_pool_safe_release(&cport->pool);

    if (AUTO_STOP_CLOCK && vid_conf->connect_cnt == 0) {
        pj_status_t status;

        /* Warning: will stuck if this is called from the clock thread */
        status = pjmedia_clock_stop(vid_conf->clock);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(3, (THIS_FILE, status, "Failed to stop clock"));
        }
    }
}

/*
 * Get number of ports currently registered in the video conference bridge.
 */
PJ_DEF(unsigned) pjmedia_vid_conf_get_port_count(pjmedia_vid_conf *vid_conf)
{
    return vid_conf->port_cnt;
}


/*
 * Enumerate occupied slots in the video conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_enum_ports( pjmedia_vid_conf *vid_conf,
                                                 unsigned slots[],
                                                 unsigned *count)
{
    unsigned i, tmp_count=0;

    PJ_ASSERT_RETURN(vid_conf && slots && count, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(vid_conf->mutex);

    for (i=0; i<vid_conf->opt.max_slot_cnt && tmp_count<*count; ++i) {
        if (!vid_conf->ports[i])
            continue;

        slots[tmp_count++] = i;
    }

    /* Unlock mutex */
    pj_mutex_unlock(vid_conf->mutex);

    *count = tmp_count;

    return PJ_SUCCESS;
}


/*
 * Get port info.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_get_port_info(
                                            pjmedia_vid_conf *vid_conf,
                                            unsigned slot,
                                            pjmedia_vid_conf_port_info *info)
{
    vconf_port *cp;

    /* Check arguments */
    PJ_ASSERT_RETURN(vid_conf && slot<vid_conf->opt.max_slot_cnt, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(vid_conf->mutex);

    /* Port must be valid. */
    cp = vid_conf->ports[slot];
    if (cp == NULL) {
        pj_mutex_unlock(vid_conf->mutex);
        return PJ_EINVAL;
    }

    info->slot = slot;
    info->name = cp->name;
    pjmedia_format_copy(&info->format, &cp->port->info.fmt);
    info->listener_cnt = cp->listener_cnt;
    info->listener_slots = cp->listener_slots;
    info->transmitter_cnt = cp->transmitter_cnt;
    info->transmitter_slots = cp->transmitter_slots;

    /* Unlock mutex */
    pj_mutex_unlock(vid_conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Enable unidirectional video flow from the specified source slot to
 * the specified sink slot.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_connect_port(
                                            pjmedia_vid_conf *vid_conf,
                                            unsigned src_slot,
                                            unsigned sink_slot,
                                            void *opt)
{
    vconf_port *src_port, *dst_port;
    unsigned i;

    PJ_LOG(5,(THIS_FILE, "Connect ports %d->%d requested",
                         src_slot, sink_slot));

    /* Check arguments */
    PJ_ASSERT_RETURN(vid_conf &&
                     src_slot<vid_conf->opt.max_slot_cnt && 
                     sink_slot<vid_conf->opt.max_slot_cnt, PJ_EINVAL);
    PJ_UNUSED_ARG(opt);

    pj_mutex_lock(vid_conf->mutex);

    /* Ports must be valid. */
    src_port = vid_conf->ports[src_slot];
    dst_port = vid_conf->ports[sink_slot];
    if (!src_port || !src_port->port->get_frame ||
        !dst_port || !dst_port->port->put_frame)
    {
        PJ_LOG(3,(THIS_FILE,"Failed connecting video ports, make sure that "
                            "source has get_frame() & sink has put_frame()"));
        pj_mutex_unlock(vid_conf->mutex);
        return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot)
            break;
    }

    /* Queue the operation */
    if (i == src_port->listener_cnt) {
        op_entry *ope;

        PJ_LOG(4,(THIS_FILE, "Video connect ports %d->%d queued",
                             src_slot, sink_slot));

        ope = get_free_op_entry(vid_conf);
        ope->type = OP_CONNECT_PORTS;
        ope->param.connect_ports.src = src_slot;
        ope->param.connect_ports.sink = sink_slot;
        pj_list_push_back(vid_conf->op_queue, ope);
    }

    /* Start clock (if not yet) */
    if (vid_conf->connect_cnt == 0) {
        pj_status_t status;
        status = pjmedia_clock_start(vid_conf->clock);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(2, (THIS_FILE, status, "Failed to start clock"));
            pj_mutex_unlock(vid_conf->mutex);
            return status;
        }
    }

    pj_mutex_unlock(vid_conf->mutex);

    return PJ_SUCCESS;
}

static void op_connect_ports(pjmedia_vid_conf *vid_conf,
                             const op_param *prm)
{
    unsigned src_slot, sink_slot;
    vconf_port *src_port, *dst_port;
    unsigned i;

    /* Ports must be valid. */
    src_slot = prm->connect_ports.src;
    sink_slot = prm->connect_ports.sink;
    src_port = vid_conf->ports[src_slot];
    dst_port = vid_conf->ports[sink_slot];
    pj_assert(src_port && src_port->port && src_port->port->get_frame);
    pj_assert(dst_port && dst_port->port && dst_port->port->put_frame);

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot)
            return;
    }

    /* Connect ports */
    src_port->listener_slots[src_port->listener_cnt] = sink_slot;
    dst_port->transmitter_slots[dst_port->transmitter_cnt] = src_slot;
    ++src_port->listener_cnt;
    ++dst_port->transmitter_cnt;
    ++vid_conf->connect_cnt;

    update_render_state(vid_conf, dst_port);

    PJ_LOG(4,(THIS_FILE,"Port %d (%.*s) transmitting to port %d (%.*s)",
              src_slot,
              (int)src_port->name.slen,
              src_port->name.ptr,
              sink_slot,
              (int)dst_port->name.slen,
              dst_port->name.ptr));
}

/*
 * Disconnect unidirectional video flow from the specified source to
 * the specified sink slot.
 */
PJ_DEF(pj_status_t) pjmedia_vid_conf_disconnect_port(
                                            pjmedia_vid_conf *vid_conf,
                                            unsigned src_slot,
                                            unsigned sink_slot)
{
    vconf_port *src_port, *dst_port;
    unsigned i, j;

    PJ_LOG(5,(THIS_FILE, "Disconnect ports %d->%d requested",
                         src_slot, sink_slot));

    /* Check arguments */
    PJ_ASSERT_RETURN(vid_conf &&
                     src_slot<vid_conf->opt.max_slot_cnt && 
                     sink_slot<vid_conf->opt.max_slot_cnt, PJ_EINVAL);

    pj_mutex_lock(vid_conf->mutex);

    /* Ports must be valid. */
    src_port = vid_conf->ports[src_slot];
    dst_port = vid_conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        PJ_PERROR(3,(THIS_FILE, PJ_EINVAL,
                     "Disconnect ports failed, src=0x%p dst=0x%p",
                     src_port, dst_port));
        pj_mutex_unlock(vid_conf->mutex);
        return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot)
            break;
    }
    for (j=0; j<dst_port->transmitter_cnt; ++j) {
        if (dst_port->transmitter_slots[j] == src_slot)
            break;
    }

    if (i != src_port->listener_cnt && j != dst_port->transmitter_cnt) {
        op_entry *ope;

        pj_assert(src_port->listener_cnt > 0 && 
                  src_port->listener_cnt < vid_conf->opt.max_slot_cnt);
        pj_assert(dst_port->transmitter_cnt > 0 && 
                  dst_port->transmitter_cnt < vid_conf->opt.max_slot_cnt);

        /* Queue the operation */
        PJ_LOG(4,(THIS_FILE, "Video disconnect ports %d->%d queued",
                             src_slot, sink_slot));

        ope = get_free_op_entry(vid_conf);
        ope->type = OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = src_slot;
        ope->param.disconnect_ports.sink = sink_slot;
        pj_list_push_back(vid_conf->op_queue, ope);
    } else {
        PJ_PERROR(3,(THIS_FILE, PJ_EINVAL,
                     "Disconnect ports failed, src=0x%p dst=0x%p",
                     src_port, dst_port));
    }

    pj_mutex_unlock(vid_conf->mutex);

    return PJ_SUCCESS;
}

static void op_disconnect_ports(pjmedia_vid_conf *vid_conf,
                                const op_param *prm)
{
    unsigned src_slot, sink_slot;
    vconf_port *src_port, *dst_port;
    unsigned i, j, k;

    /* Ports must be valid. */
    src_slot = prm->disconnect_ports.src;
    sink_slot = prm->disconnect_ports.sink;
    src_port = vid_conf->ports[src_slot];
    dst_port = vid_conf->ports[sink_slot];
    pj_assert(src_port && dst_port);

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot)
            break;
    }
    for (j=0; j<dst_port->transmitter_cnt; ++j) {
        if (dst_port->transmitter_slots[j] == src_slot)
            break;
    }

    if (i == src_port->listener_cnt || j == dst_port->transmitter_cnt)
        return;

    pj_assert(src_port->listener_cnt > 0 && 
              src_port->listener_cnt < vid_conf->opt.max_slot_cnt);
    pj_assert(dst_port->transmitter_cnt > 0 && 
              dst_port->transmitter_cnt < vid_conf->opt.max_slot_cnt);

    /* Cleanup all render states of the sink */
    for (k=0; k<dst_port->transmitter_cnt; ++k)
        cleanup_render_state(dst_port, k);

    /* Update listeners array of the source and transmitters array of
     * the sink.
     */
    pj_array_erase(src_port->listener_slots, sizeof(unsigned), 
                   src_port->listener_cnt, i);
    pj_array_erase(dst_port->transmitter_slots, sizeof(unsigned), 
                   dst_port->transmitter_cnt, j);
    --src_port->listener_cnt;
    --dst_port->transmitter_cnt;

    /* Update render states of the sink */
    update_render_state(vid_conf, dst_port);

    --vid_conf->connect_cnt;

    if (AUTO_STOP_CLOCK && vid_conf->connect_cnt == 0) {
        pj_status_t status;
        /* Warning: will stuck if this is called from the clock thread */
        status = pjmedia_clock_stop(vid_conf->clock);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4, (THIS_FILE, status, "Failed to stop clock"));
            return;
        }
    }

    PJ_LOG(4,(THIS_FILE,
              "Port %d (%.*s) stop transmitting to port %d (%.*s)",
              src_slot,
              (int)src_port->name.slen,
              src_port->name.ptr,
              sink_slot,
              (int)dst_port->name.slen,
              dst_port->name.ptr));
}


/*
 * Internal functions.
 */

/* Compare FPS of two formats, return 0 if equal */
static int cmp_fps(const pjmedia_format *fmt1, const pjmedia_format *fmt2)
{
    return (fmt1->det.vid.fps.num != fmt2->det.vid.fps.num ||
            fmt1->det.vid.fps.denum != fmt2->det.vid.fps.denum);
}


/* Compare size of two formats, return 0 if equal */
static int cmp_size(const pjmedia_format *fmt1, const pjmedia_format *fmt2)
{
    /* Note that format ID changes may cause buffer size change */
    return (fmt1->id != fmt2->id ||
            fmt1->det.vid.size.w != fmt2->det.vid.size.w ||
            fmt1->det.vid.size.h != fmt2->det.vid.size.h);
}


static void on_clock_tick(const pj_timestamp *now, void *user_data)
{
    pjmedia_vid_conf *vid_conf = (pjmedia_vid_conf*)user_data;
    unsigned ci, i;
    pj_int32_t ts_diff;
    pjmedia_frame frame;
    pj_status_t status;

    /* Perform any queued operations that need to be synchronized with
     * the clock such as connect, disonnect, remove, update.
     */
    if (!pj_list_empty(vid_conf->op_queue)) {
        pj_mutex_lock(vid_conf->mutex);
        handle_op_queue(vid_conf);
        pj_mutex_unlock(vid_conf->mutex);
    }

    /* No mutex from this point! Otherwise it may cause deadlock as
     * put_frame()/get_frame() may invoke callback.
     *
     * Note that video conference states (e.g: port connection, render state)
     * must not be changed when the execution reaches this point, so
     * operations that change the states must be queued or sync-ed with
     * the clock.
     */

    /* Iterate all (sink) ports */
    for (i=0, ci=0; i<vid_conf->opt.max_slot_cnt &&
                    ci<vid_conf->port_cnt; ++i)
    {
        unsigned j;
        pj_bool_t frame_rendered = PJ_FALSE;
        pj_bool_t ts_incremented = PJ_FALSE;
        vconf_port *sink = vid_conf->ports[i];
        pjmedia_format *cur_fmt, *new_fmt;

        /* Skip empty port */
        if (!sink)
            continue;

        /* Increment occupied port counter */
        ++ci;

        /* Skip non-sink port */
        if (!sink->port->put_frame)
            continue;

        if (sink->ts_next.u64 == 0)
            sink->ts_next = *now;

        /* Skip if too early for put_frame(), note:
         * early = (now < ts_next)
         * But careful for timestamp wrapped around.
         */
        ts_diff = pj_timestamp_diff32(&sink->ts_next, now);
        if (ts_diff < 0 || ts_diff > TS_CLOCK_RATE)
            continue;

        /* There is a possibility that the sink port's format has
         * changed, but we haven't received the event yet.
         */
        cur_fmt = &sink->format;
        new_fmt = &sink->port->info.fmt;
        if (cmp_fps(cur_fmt, new_fmt) || cmp_size(cur_fmt, new_fmt)) {
            op_param prm;
            prm.update_port.port = sink->idx;
            op_update_port(vid_conf, &prm);
        }

        /* Iterate transmitters of this sink port */
        for (j=0; j < sink->transmitter_cnt; ++j) {
            vconf_port *src = vid_conf->ports[sink->transmitter_slots[j]];
            pj_int32_t src_ts_diff;

            if (src->ts_next.u64 == 0)
                src->ts_next = *now;

            /* Is it time for src->get_frame()? yes, if (now >= ts_next) */
            src_ts_diff = pj_timestamp_diff32(&src->ts_next, now);
            if (src_ts_diff >= 0) {

                /* Call src->get_frame().
                 * Possible optimization: if this src only has one listener,
                 * perhaps we can skip this src buffer and directly render it
                 * to sink buffer (but still need buffer if conversion any).
                 */
                pj_bzero(&frame, sizeof(frame));
                frame.type = PJMEDIA_FRAME_TYPE_VIDEO;
                frame.timestamp = *now;
                frame.buf = src->get_buf;
                frame.size = src->get_frm_size;
                status = pjmedia_port_get_frame(src->port, &frame);
                if (status != PJ_SUCCESS) {
                    PJ_PERROR(5, (THIS_FILE, status,
                                  "Failed to get frame from port %d [%s]!",
                                  src->idx, src->port->info.name.ptr));
                    src->got_frame = PJ_FALSE;
                } else {
                    src->got_frame = (frame.size == src->get_frm_size);

                    /* There is a possibility that the source port's format has
                     * changed, but we haven't received the event yet.
                     */
                    cur_fmt = &src->format;
                    new_fmt = &src->port->info.fmt;
                    if (cmp_fps(cur_fmt, new_fmt) ||
                        cmp_size(cur_fmt, new_fmt))
                    {
                        op_param prm;
                        prm.update_port.port = src->idx;
                        op_update_port(vid_conf, &prm);
                    }
                }

                /* Update next src put/get */
                pj_add_timestamp32(&src->ts_next, src->ts_interval);
                ts_incremented = src==sink;
            }

            if (src->got_frame) {
                /* Render src get buffer to sink put buffer (based on
                 * sink layout settings, if any)
                 */
                status = render_src_frame(src, sink, j);
                if (status == PJ_SUCCESS) {
                    frame_rendered = PJ_TRUE;
                } else {
                    PJ_PERROR(5, (THIS_FILE, status,
                                  "Failed to render frame from port %d [%s] "
                                  "to port %d [%s]",
                                  src->idx, src->port->info.name.ptr,
                                  sink->idx, sink->port->info.name.ptr));
                }
            }
        }

        /* Call sink->put_frame()
         * Note that if transmitter_cnt==0, we should still call put_frame()
         * with zero frame size, as sink may need to send keep-alive packets
         * and get timestamp update.
         */
        pj_bzero(&frame, sizeof(frame));
        frame.type = PJMEDIA_FRAME_TYPE_VIDEO;
        frame.timestamp = *now;
        if (frame_rendered) {
            frame.buf = sink->put_buf;
            frame.size = sink->put_frm_size;
        }
        status = pjmedia_port_put_frame(sink->port, &frame);
        if (frame_rendered && status != PJ_SUCCESS) {
            sink->last_err_cnt++;
            if (sink->last_err != status ||
                sink->last_err_cnt % MAX_ERR_COUNT == 0)
            {
                if (sink->last_err != status)
                    sink->last_err_cnt = 1;
                sink->last_err = status;
                PJ_PERROR(5, (THIS_FILE, status,
                              "Failed (%d time(s)) to put frame to port %d"
                              " [%s]!", sink->last_err_cnt,
                              sink->idx, sink->port->info.name.ptr));
            }
        } else {
            sink->last_err = status;
            sink->last_err_cnt = 0;
        }

        /* Update next put/get, careful that it may have been updated
         * if this port transmits to itself!
         */
        if (!ts_incremented) {
            pj_add_timestamp32(&sink->ts_next, sink->ts_interval);
        }
    }
}

static pj_bool_t is_landscape(const pjmedia_rect_size *size) {
    return (size->w >= size->h);
}

/* Adjust a frame size to match ratio specified in the ref_size.
 * Either dimension of the frame may be cropped (but will never be
 * expanded).
 */
static void match_ratio_crop(pjmedia_rect_size *size,
                             const pjmedia_rect_size *ref_size)
{
    pjmedia_rect_size res;

    /* Try match width first */
    res.w = size->w;
    res.h = ref_size->h * size->w / ref_size->w;
    
    /* If original height turns out to be shorther, match height */
    if (size->h < res.h) {
        res.w = ref_size->w * size->h / ref_size->h;
        res.h = size->h;
    }

    *size = res;
    return;
}

/* Cleanup rendering states, called when a transmitter is disconnected
 * from a listener, or before reinit-ing rendering state of a listener
 * when new connection has just been made.
 */
static void cleanup_render_state(vconf_port *cp,
                                 unsigned transmitter_idx)
{
    render_state *rs = cp->render_states[transmitter_idx];
    if (rs && rs->converter)
    {
        pjmedia_converter_destroy(rs->converter);
        rs->converter = NULL;
    }
    cp->render_states[transmitter_idx] = NULL;

    if (cp->render_pool[transmitter_idx]) {
        pj_pool_safe_release(&cp->render_pool[transmitter_idx]);

        TRACE_((THIS_FILE, "Cleaned up render state for connection %d->%d",
                cp->transmitter_slots[transmitter_idx], cp->idx));
    }
}


/* This function will do:
 * - Recalculate layout setting, i.e: get video pos and size
 *   for each transmitter
 * - Check if converter is needed and setup it.
 * - Those above things will be stored in render_state and
 *   will be used by render_src_frame()
 */
static void update_render_state(pjmedia_vid_conf *vid_conf, vconf_port *cp)
{
    pjmedia_format_id fmt_id, tr_fmt_id[4];
    pjmedia_rect_size size, tr_size[4];
    unsigned i;
    pj_status_t status;

    fmt_id = cp->port->info.fmt.id;
    size   = cp->port->info.fmt.det.vid.size;

    /* Nothing to render, just return */
    if (cp->transmitter_cnt == 0)
        return;

    TRACE_((THIS_FILE, "Updating render state for port id %d (%d sources)..",
            cp->idx, cp->transmitter_cnt));

    for (i = 0; i < cp->transmitter_cnt; ++i) {
        vconf_port *tr = vid_conf->ports[cp->transmitter_slots[i]];

        /* Cleanup render states & pool */
        cleanup_render_state(cp, i);

        /* Gather format ID, size of each transmitter */
        tr_fmt_id[i] = tr->port->info.fmt.id;
        tr_size[i]   = tr->port->info.fmt.det.vid.size;
    }

    /* If only one transmitter and it has matched format & size, just use
     * plain memcpy(). Usually preview window or call stream window will
     * have matched format & size with its source.
     */
    if (cp->transmitter_cnt == 1 && fmt_id == tr_fmt_id[0] &&
        size.w == tr_size[0].w && size.h == tr_size[0].h)
    {
        TRACE_((THIS_FILE, "This port only has single source with "
                           "matched format & size, no conversion needed"));
        return;
    }

    for (i = 0; i < cp->transmitter_cnt && i < 4; ++i) {
        pj_pool_t *pool;
        render_state *rs;
        pjmedia_conversion_param cparam;
        char tmp_buf[32];

        /* Create pool & render state */
        pj_ansi_snprintf(tmp_buf, sizeof(tmp_buf), "vcport_rs_%d->%d",
                         cp->transmitter_slots[i], cp->idx);
        pool = pj_pool_create(cp->pool->factory, tmp_buf, 128, 128, NULL);
        cp->render_pool[i] = pool;
        rs = cp->render_states[i] = PJ_POOL_ZALLOC_T(pool, render_state);

        TRACE_((THIS_FILE, "Created render state for connection %d->%d",
                           cp->transmitter_slots[i], cp->idx));

        /* Setup format & frame */
        rs->src_fmt_id = tr_fmt_id[i];
        rs->dst_fmt_id = fmt_id;
        rs->src_frame_size = tr_size[i];
        rs->dst_frame_size = size;

        /* For now, draw the whole source frame, will adjust ratio later */
        rs->src_rect.coord.x = rs->src_rect.coord.y = 0;
        rs->src_rect.size = tr_size[i];

        /* Setup layout */
        if (cp->transmitter_cnt == 1) {
            rs->dst_rect.coord.x = rs->dst_rect.coord.y = 0;
            rs->dst_rect.size = size;
        } else if (cp->transmitter_cnt == 2) {
            if (is_landscape(&size)) {
                /*
                 *          |
                 * Source 0 | Source 1
                 *          |
                 */
                rs->dst_rect.coord.x = i * (size.w/2);
                rs->dst_rect.coord.y = 0;
                rs->dst_rect.size.w = size.w / 2;
                rs->dst_rect.size.h = size.h;
            } else {
                /*
                 * Source 0
                 * --------
                 * Source 1
                 */
                rs->dst_rect.coord.x = 0;
                rs->dst_rect.coord.y = i * (size.h/2);
                rs->dst_rect.size.w = size.w;
                rs->dst_rect.size.h = size.h / 2;
            }
        } else if (cp->transmitter_cnt == 3) {
            if (is_landscape(&size)) {
                /*
                 *          | Source 1
                 * Source 0 |---------
                 *          | Source 2
                 */
                rs->dst_rect.coord.x = (i==0)? 0 : size.w/2;
                rs->dst_rect.coord.y = (i!=2)? 0 : size.h/2;
                rs->dst_rect.size.w = size.w / 2;
                rs->dst_rect.size.h = (i==0)? size.h : size.h/2;
            } else {
                /*
                 * Source 0
                 * --------
                 * Source 1
                 * --------
                 * Source 2
                 */
                rs->dst_rect.coord.x = 0;
                rs->dst_rect.coord.y = i * size.h/3;
                rs->dst_rect.size.w = size.w;
                rs->dst_rect.size.h = size.h/3;
            }
        } else if (cp->transmitter_cnt == 4) {
            if (is_landscape(&size)) {
                /*
                 * Source 0 | Source 1
                 * ---------|---------
                 * Source 2 | Source 3
                 */
                rs->dst_rect.coord.x = (i%2==0)? 0 : size.w/2;
                rs->dst_rect.coord.y = (i/2==0)? 0 : size.h/2;
                rs->dst_rect.size.w = size.w/2;
                rs->dst_rect.size.h = size.h/2;
            } else {
                /*
                 * Source 0
                 * --------
                 * Source 1
                 * --------
                 * Source 2
                 * --------
                 * Source 3
                 */
                rs->dst_rect.coord.x = 0;
                rs->dst_rect.coord.y = i * size.h/4;
                rs->dst_rect.size.w = size.w;
                rs->dst_rect.size.h = size.h/4;
            }
        }

        /* Adjust source size to match aspect ratio of rendering space. */
        match_ratio_crop(&rs->src_rect.size, &rs->dst_rect.size);

        /* Now adjust source position after source size adjustment. */
        if (rs->src_rect.size.w < tr_size[i].w)
            rs->src_rect.coord.x = (tr_size[i].w - rs->src_rect.size.w)/2;
        if (rs->src_rect.size.h < tr_size[i].h)
            rs->src_rect.coord.y = (tr_size[i].h - rs->src_rect.size.h)/2;

        TRACE_((THIS_FILE, "src#%d=%s/%dx%d->%dx%d@%d,%d dst=%dx%d@%d,%d",
                           i, pjmedia_fourcc_name(tr_fmt_id[i], tmp_buf),
                           tr_size[i].w, tr_size[i].h,
                           rs->src_rect.size.w, rs->src_rect.size.h,
                           rs->src_rect.coord.x, rs->src_rect.coord.y,
                           rs->dst_rect.size.w, rs->dst_rect.size.h,
                           rs->dst_rect.coord.x, rs->dst_rect.coord.y));

        /* Create converter */
        pjmedia_format_init_video(&cparam.src, rs->src_fmt_id,
                                  rs->src_rect.size.w,
                                  rs->src_rect.size.h,
                                  0, 1);
        pjmedia_format_init_video(&cparam.dst, rs->dst_fmt_id,
                                  rs->dst_rect.size.w,
                                  rs->dst_rect.size.h,
                                  0, 1);
        status = pjmedia_converter_create(NULL, pool, &cparam,
                                          &rs->converter);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(THIS_FILE, status,
                         "Port %d failed creating converter "
                         "for source %d", cp->idx, i));
        }
    }
}

/* Render frame from source to sink buffer based on rendering settings. */
static pj_status_t render_src_frame(vconf_port *src, vconf_port *sink,
                                    unsigned transmitter_idx)
{
    pj_status_t status;
    render_state *rs = sink->render_states[transmitter_idx];

    /* There is a possibility that the sink port's format has
     * changed, but we haven't received the event yet.
     */
    if (pj_memcmp(&sink->format, &sink->port->info.fmt,
                  sizeof(pjmedia_format)))
    {
        return PJMEDIA_EVID_BADFORMAT;
    }

    if (sink->transmitter_cnt == 1 && (!rs || !rs->converter)) {
        /* The only transmitter and no conversion needed */
        if (src->get_frm_size != sink->put_frm_size)
            return PJMEDIA_EVID_BADFORMAT;
        pj_memcpy(sink->put_buf, src->get_buf, src->get_frm_size);
    } else if (rs && rs->converter) {
        pjmedia_frame src_frame, dst_frame;
        
        pj_bzero(&src_frame, sizeof(src_frame));
        src_frame.buf = src->get_buf;
        src_frame.size = src->get_frm_size;

        pj_bzero(&dst_frame, sizeof(dst_frame));
        dst_frame.buf = sink->put_buf;
        dst_frame.size = sink->put_frm_size;

        status = pjmedia_converter_convert2(rs->converter,
                                            &src_frame,
                                            &rs->src_frame_size,
                                            &rs->src_rect.coord,
                                            &dst_frame,
                                            &rs->dst_frame_size,
                                            &rs->dst_rect.coord,
                                            NULL);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(THIS_FILE, status,
                         "Port id %d: converter failed in "
                         "rendering frame from port id %d",
                         sink->idx, transmitter_idx));
            return status;
        }
    }

    return PJ_SUCCESS;
}


/* Update or refresh port states from video port info. */
PJ_DEF(pj_status_t) pjmedia_vid_conf_update_port( pjmedia_vid_conf *vid_conf,
                                                  unsigned slot)
{
    vconf_port *cport;
    op_entry *ope;

    PJ_LOG(5,(THIS_FILE, "Update port %d requested", slot));

    PJ_ASSERT_RETURN(vid_conf && slot<vid_conf->opt.max_slot_cnt, PJ_EINVAL);

    pj_mutex_lock(vid_conf->mutex);

    /* Port must be valid. */
    cport = vid_conf->ports[slot];
    if (cport == NULL) {
        PJ_PERROR(3,(THIS_FILE, PJ_EINVAL, "Update port failed"));
        pj_mutex_unlock(vid_conf->mutex);
        return PJ_EINVAL;
    }

    /* Queue the operation */
    ope = get_free_op_entry(vid_conf);
    ope->type = OP_UPDATE_PORT;
    ope->param.update_port.port = slot;
    pj_list_push_back(vid_conf->op_queue, ope);

    PJ_LOG(4,(THIS_FILE, "Update port %d queued", slot));
    pj_mutex_unlock(vid_conf->mutex);

    return PJ_SUCCESS;
}


static void op_update_port(pjmedia_vid_conf *vid_conf,
                           const op_param *prm)
{
    unsigned slot = prm->update_port.port;
    vconf_port *cport = vid_conf->ports[slot];
    pjmedia_format *old_fmt;
    pjmedia_format *new_fmt;

    /* Port must be valid. */
    pj_assert(cport);

    /* Get the old & new formats */
    old_fmt = &cport->format;
    new_fmt = &cport->port->info.fmt;

    /* Update put/get_frame() intervals */
    if (cmp_fps(new_fmt, old_fmt))
    {
        pjmedia_ratio *fps = &new_fmt->det.vid.fps;
        pj_uint32_t vconf_interval = (pj_uint32_t)
                                     (TS_CLOCK_RATE * 1.0 /
                                     vid_conf->opt.frame_rate);
        cport->ts_interval = (pj_uint32_t)(TS_CLOCK_RATE * 1.0 /
                                           fps->num * fps->denum);

        /* Normalize the interval */
        if (cport->ts_interval < vconf_interval) {
            cport->ts_interval = vconf_interval;
            PJ_LOG(3,(THIS_FILE, "Warning: frame rate of port %s is higher "
                                 "than video conference bridge (%d > %d)",
                                 cport->name.ptr, (int)(fps->num/fps->denum),
                                 vid_conf->opt.frame_rate));
        }

        PJ_LOG(4,(THIS_FILE,
                  "Port %d (%s): updated frame rate %d -> %d",
                  slot, cport->name.ptr,
                  (int)(old_fmt->det.vid.fps.num/old_fmt->det.vid.fps.denum),
                  (int)(fps->num/fps->denum)));
    }

    /* Update buffer for put/get_frame() */
    if (cmp_size(new_fmt, old_fmt))
    {
        const pjmedia_video_format_info *vfi;
        pjmedia_video_apply_fmt_param vafp;
        pj_status_t status;
        unsigned i;

        vfi = pjmedia_get_video_format_info(NULL, new_fmt->id);
        if (!vfi) {
            PJ_LOG(1,(THIS_FILE, "pjmedia_vid_conf_update_port(): "
                                 "unrecognized format %04X",
                                 new_fmt->id));
            return;
        }

        pj_bzero(&vafp, sizeof(vafp));
        vafp.size = new_fmt->det.vid.size;
        status = (*vfi->apply_fmt)(vfi, &vafp);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1,(THIS_FILE, status,
                         "pjmedia_vid_conf_update_port(): "
                         "Failed to apply format %04X",
                        new_fmt->id));
            return;
        }
        if (cport->port->put_frame) {
            if (cport->put_buf_size < vafp.framebytes) {
                cport->put_buf = pj_pool_zalloc(cport->pool, vafp.framebytes);
                cport->put_buf_size = vafp.framebytes;
            }
            cport->put_frm_size = vafp.framebytes;

            /* Initialize sink buffer with black color. */
            status = pjmedia_video_format_fill_black(&cport->port->info.fmt,
                                                     cport->put_buf,
                                                     cport->put_buf_size);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4,(THIS_FILE, status,
                             "Warning: failed to init sink buffer "
                             " with black"));
            }
        }
        if (cport->port->get_frame) {
            if (cport->get_buf_size < vafp.framebytes) {
                cport->get_buf = pj_pool_zalloc(cport->pool, vafp.framebytes);
                cport->get_buf_size = vafp.framebytes;
            }
            cport->get_frm_size = vafp.framebytes;

            /* When source port is updated, buffer should contain a new image
             * with the correct latest format already, so don't fill black
             * and don't reset the got_frame flag.
             */
            //cport->got_frame = PJ_FALSE;
        }

        /* Update render state */
        update_render_state(vid_conf, cport);

        /* Update render state of listeners */
        for (i=0; i < cport->listener_cnt; ++i) {
            vconf_port *sink = vid_conf->ports[cport->listener_slots[i]];
            update_render_state(vid_conf, sink);
        }

        PJ_LOG(4,(THIS_FILE,
                  "Port %d (%s): updated frame size %dx%d -> %dx%d",
                  slot, cport->name.ptr,
                  old_fmt->det.vid.size.w, old_fmt->det.vid.size.h,
                  new_fmt->det.vid.size.w, new_fmt->det.vid.size.h));
    }

    /* Update cport format info */
    cport->format = *new_fmt;
    PJ_LOG(4,(THIS_FILE, "Port %d updated", slot));
}


#endif /* PJMEDIA_HAS_VIDEO */
