/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/conference.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/resample.h>
#include <pjmedia/silencedet.h>
#include <pjmedia/sound_port.h>
#include <pjmedia/stereo.h>
#include <pj/os.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#if !defined(PJMEDIA_CONF_USE_SWITCH_BOARD) || PJMEDIA_CONF_USE_SWITCH_BOARD==0

/* CONF_DEBUG enables detailed operation of the conference bridge.
 * Beware that it prints large amounts of logs (several lines per frame).
 */
//#define CONF_DEBUG
#ifdef CONF_DEBUG
#   include <stdio.h>
#   define TRACE_(x)   PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif


/* REC_FILE macro enables recording of the samples written to the sound
 * device. The file contains RAW PCM data with no header, and has the
 * same settings (clock rate etc) as the conference bridge.
 * This should only be enabled when debugging audio quality *only*.
 */
//#define REC_FILE    "confrec.pcm"
#ifdef REC_FILE
static FILE *fhnd_rec;
#endif


#define THIS_FILE       "conference.c"

#define RX_BUF_COUNT        PJMEDIA_SOUND_BUFFER_COUNT

#define BYTES_PER_SAMPLE    2

#define SIGNATURE           PJMEDIA_CONF_BRIDGE_SIGNATURE
#define SIGNATURE_PORT      PJMEDIA_SIG_PORT_CONF_PASV
/* Normal level is hardcodec to 128 in all over places */
#define NORMAL_LEVEL        128
#define SLOT_TYPE           unsigned
#define INVALID_SLOT        ((SLOT_TYPE)-1)


/* These are settings to control the adaptivity of changes in the
 * signal level of the ports, so that sudden change in signal level
 * in the port does not cause misaligned signal (which causes noise).
 */
#if defined(PJMEDIA_CONF_USE_AGC) && PJMEDIA_CONF_USE_AGC != 0
#   define ATTACK_A     ((conf->clock_rate / conf->samples_per_frame) >> 4)
#   define ATTACK_B     1
#   define DECAY_A      0
#   define DECAY_B      1

#   define SIMPLE_AGC(last, target) \
    if (target >= last) \
        target = (ATTACK_A*(last+1)+ATTACK_B*target)/(ATTACK_A+ATTACK_B); \
    else \
        target = (DECAY_A*last+DECAY_B*target)/(DECAY_A+DECAY_B)
#else
#   define SIMPLE_AGC(last, target)
#endif

#define MAX_LEVEL   (32767)
#define MIN_LEVEL   (-32768)

#define IS_OVERFLOW(s) ((s > MAX_LEVEL) || (s < MIN_LEVEL))

#ifndef PJMEDIA_CONF_USE_MULTI_THREADING
#define PJMEDIA_CONF_USE_MULTI_THREADING 0
#endif

#ifndef PJ_HAS_THREAD_AFFINITY
#define PJ_HAS_THREAD_AFFINITY 0
#endif

#ifndef PJMEDIA_CONF_USE_SIMD
#define PJMEDIA_CONF_USE_SIMD 0
#endif

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#ifndef PJMEDIA_CONF_CACHE_LINE_SIZE
#define PJMEDIA_CONF_CACHE_LINE_SIZE 64
#endif

#ifndef PJMEDIA_CONF_THREAD_POOL_SIZE
#define PJMEDIA_CONF_THREAD_POOL_SIZE 8
#endif

#ifndef PJMEDIA_CONF_USE_NUMA
#define PJMEDIA_CONF_USE_NUMA 0
#endif

#ifndef PJMEDIA_CONF_USE_WORK_STEALING
#define PJMEDIA_CONF_USE_WORK_STEALING 0
#endif

#ifndef PJMEDIA_CONF_REBALANCE_ACTIVE_ONLY
#define PJMEDIA_CONF_REBALANCE_ACTIVE_ONLY 1
#endif

#ifndef PJMEDIA_CONF_USE_WORK_STEALING
#define PJMEDIA_CONF_USE_WORK_STEALING 1
#endif

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0

#ifndef PJMEDIA_CONF_WORK_QUEUE_SIZE
#define PJMEDIA_CONF_WORK_QUEUE_SIZE 256
#endif

#ifndef PJMEDIA_CONF_STEAL_THRESHOLD
#define PJMEDIA_CONF_STEAL_THRESHOLD 25  /* Steal when queue < 25% full */
#endif

#ifndef PJMEDIA_CONF_STEAL_ATTEMPTS
#define PJMEDIA_CONF_STEAL_ATTEMPTS 3    /* Try N victims before giving up */
#endif

/* Work item structure */
typedef struct work_item {
    unsigned port_idx;
    pj_atomic_t *taken;  /* Atomic flag to prevent double-processing */
} work_item;

/* Lock-free work queue for each worker */
typedef struct work_queue {
    work_item *items;
    unsigned capacity;
    pj_atomic_t *head;      /* Producer index */
    pj_atomic_t *tail;      /* Consumer index */
    pj_atomic_t *size;      /* Current size for quick checks */

    /* Cache line padding */
    char pad[PJMEDIA_CONF_CACHE_LINE_SIZE -
             (sizeof(void*) + sizeof(unsigned) + 3*sizeof(pj_atomic_t*))];
} work_queue;

#endif /* PJMEDIA_CONF_USE_WORK_STEALING */
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
  #if !defined(PJ_HAS_BARRIER) || PJ_HAS_BARRIER == 0
    #error "Multi-threaded conference support requires PJ_HAS_BARRIER"
  #endif
#endif

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#if (!defined(PJ_HAS_THREAD_AFFINITY) || PJ_HAS_THREAD_AFFINITY == 0)
    #if defined(PJ_LINUX) && PJ_LINUX != 0
        #include <pthread.h>
        #include <sched.h>
    #elif defined(PJ_DARWINOS) && PJ_DARWINOS != 0
        #include <pthread.h>
    #endif
#endif

/* Platform-specific includes */
#if defined(PJ_LINUX) || defined(PJ_ANDROID)
    #include <sys/prctl.h>
#endif

#if defined(PJ_DARWIN)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#endif

#if defined(PJ_WIN32)
    #include <windows.h>
#endif

#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0
    #if defined(PJ_LINUX)
        #include <dirent.h>
        #include <unistd.h>
        #include <sys/syscall.h>

        /* Linux NUMA memory policy flags */
        #define MPOL_DEFAULT     0
        #define MPOL_PREFERRED   1
        #define MPOL_BIND        2
        #define MPOL_INTERLEAVE  3

        /* NUMA system calls */
        static long numa_mbind(void *addr, unsigned long len, int mode,
                              const unsigned long *nodemask, unsigned long maxnode,
                              unsigned flags)
        {
            return syscall(SYS_mbind, addr, len, mode, nodemask, maxnode, flags);
        }

        static long numa_set_mempolicy(int mode, const unsigned long *nodemask,
                                      unsigned long maxnode)
        {
            return syscall(SYS_set_mempolicy, mode, nodemask, maxnode);
        }

        /* Check if NUMA is available */
        static pj_bool_t numa_available(void)
        {
            DIR *dir = opendir("/sys/devices/system/node");
            if (dir) {
                struct dirent *entry;
                int node_count = 0;

                while ((entry = readdir(dir)) != NULL) {
                    if (strncmp(entry->d_name, "node", 4) == 0) {
                        node_count++;
                    }
                }
                closedir(dir);

                return node_count > 1 ? PJ_TRUE : PJ_FALSE;
            }
            return PJ_FALSE;
        }

        /* Get number of NUMA nodes */
        static int numa_max_node(void)
        {
            DIR *dir = opendir("/sys/devices/system/node");
            int max_node = 0;

            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strncmp(entry->d_name, "node", 4) == 0) {
                        int node_id;
                        if (sscanf(entry->d_name, "node%d", &node_id) == 1) {
                            if (node_id > max_node)
                                max_node = node_id;
                        }
                    }
                }
                closedir(dir);
            }
            return max_node;
        }

        /* Get NUMA node for CPU */
        static int numa_node_of_cpu(int cpu)
        {
            char path[256];
            FILE *f;
            int node = 0;
            int n;
            int max_node = numa_max_node();

            for (n = 0; n <= max_node; n++) {
                pj_ansi_snprintf(path, sizeof(path),
                                "/sys/devices/system/node/node%d/cpulist", n);
                f = fopen(path, "r");
                if (f) {
                    char buffer[4096];
                    if (fgets(buffer, sizeof(buffer), f)) {
                        /* Parse CPU ranges like "0-3,8-11" */
                        char *p = buffer;
                        while (*p) {
                            int start, end;
                            if (sscanf(p, "%d-%d", &start, &end) == 2) {
                                if (cpu >= start && cpu <= end) {
                                    fclose(f);
                                    return n;
                                }
                                while (*p && *p != ',') p++;
                            } else if (sscanf(p, "%d", &start) == 1) {
                                if (cpu == start) {
                                    fclose(f);
                                    return n;
                                }
                                while (*p && *p != ',') p++;
                            }
                            if (*p == ',') p++;
                        }
                    }
                    fclose(f);
                }
            }
            return node;
        }

        /* Set preferred NUMA node */
        static void numa_set_preferred(int node)
        {
            if (node < 0) {
                /* Reset to default policy */
                numa_set_mempolicy(MPOL_DEFAULT, NULL, 0);
            } else {
                unsigned long nodemask[16] = {0};
                unsigned long maxnode = sizeof(nodemask) * 8;

                /* Set bit for preferred node */
                nodemask[node / (sizeof(unsigned long) * 8)] =
                    1UL << (node % (sizeof(unsigned long) * 8));

                numa_set_mempolicy(MPOL_PREFERRED, nodemask, maxnode);
            }
        }

        /* Get current preferred NUMA node (simplified) */
        static int numa_preferred(void)
        {
            /* Return -1 to indicate default policy */
            return -1;
        }

        /* Set local memory allocation */
        static void numa_set_localalloc(void)
        {
            numa_set_mempolicy(MPOL_DEFAULT, NULL, 0);
        }

        #define HAS_NUMA_SUPPORT 1

    #elif defined(PJ_WIN32)
        #include <windows.h>

        static pj_bool_t numa_available(void)
        {
            ULONG highest_node_number = 0;
            if (!GetNumaHighestNodeNumber(&highest_node_number)) {
                return PJ_FALSE;
            }
            return highest_node_number > 0 ? PJ_TRUE : PJ_FALSE;
        }

        static int numa_max_node(void)
        {
            ULONG highest_node_number = 0;
            if (GetNumaHighestNodeNumber(&highest_node_number)) {
                return (int)highest_node_number;
            }
            return 0;
        }

        static int numa_node_of_cpu(int cpu)
        {
            UCHAR node_number = 0;
            if (GetNumaProcessorNode((UCHAR)cpu, &node_number)) {
                return (int)node_number;
            }
            return 0;
        }

        static void numa_set_preferred(int node)
        {
            /* Windows doesn't have direct equivalent */
            PJ_UNUSED_ARG(node);
        }

        static int numa_preferred(void) { return -1; }
        static void numa_set_localalloc(void) { }

        #define HAS_NUMA_SUPPORT 1
    #else
        /* Fallback for other platforms */
        static pj_bool_t numa_available(void) { return PJ_FALSE; }
        static int numa_max_node(void) { return 0; }
        static int numa_node_of_cpu(int cpu) { PJ_UNUSED_ARG(cpu); return 0; }
        static void numa_set_preferred(int node) { PJ_UNUSED_ARG(node); }
        static int numa_preferred(void) { return -1; }
        static void numa_set_localalloc(void) { }

        #define HAS_NUMA_SUPPORT 0
    #endif
#endif

/* SIMD support */
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
    #include <immintrin.h>
    #define SIMD_ALIGNMENT 32
    #define IS_ALIGNED(ptr, align) (((uintptr_t)(ptr) & ((align) - 1)) == 0)
#endif

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
/* Define memory barrier for different platforms */
#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    #define MEMORY_BARRIER() __asm__ __volatile__("mfence" ::: "memory")
#elif defined(__GNUC__)
    #define MEMORY_BARRIER() __sync_synchronize()
#elif defined(_MSC_VER)
    #include <windows.h>
    #define MEMORY_BARRIER() MemoryBarrier()
#else
    /* Fallback - compiler barrier only */
    #define MEMORY_BARRIER() do { volatile int dummy = 0; (void)dummy; } while(0)
#endif

#endif /* PJMEDIA_CONF_USE_WORK_STEALING */
#endif  /* PJMEDIA_CONF_USE_MULTI_THREADING */

/* Multi-threading types */
#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
/* Worker thread context */
typedef struct worker_context {
    pjmedia_conf       *conf;
    unsigned            worker_id;
    unsigned            cpu_id;
    pj_thread_t        *thread;

    /* Performance counters */
    pj_atomic_t        *frames_processed;
    pj_atomic_t        *ports_processed;
    pj_timestamp        last_work_time;

    /* Work assignment */
    unsigned            port_start;
    unsigned            port_end;
    pj_bool_t           active;

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
    work_queue         *local_queue;
    pj_atomic_t        *steal_attempts;
    pj_atomic_t        *successful_steals;
    unsigned            victim_idx;      /* Next victim to steal from */
#endif /* PJMEDIA_CONF_USE_WORK_STEALING */

    /* Cache-line padding */
    char                pad[PJMEDIA_CONF_CACHE_LINE_SIZE];
} worker_context;

/* Load balancing statistics */
typedef struct load_stats {
    pj_uint32_t         avg_processing_ns;
    pj_uint32_t         peak_processing_ns;
    unsigned            ports_assigned;
    pj_timestamp        last_rebalance;
} load_stats;

#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */

/*
 * DON'T GET CONFUSED WITH TX/RX!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. So TX means the bridge
 * is transmitting to the port, RX means the bridge is receiving from the
 * port.
 */


/**
 * This is a port connected to conference bridge.
 */
struct conf_port
{
    pj_pool_t           *pool;          /**< Pool.                          */
    pj_str_t             name;          /**< Port name.                     */
    pjmedia_port        *port;          /**< get_frame() and put_frame()    */
    pjmedia_port_op      rx_setting;    /**< Can we receive from this port  */
    pjmedia_port_op      tx_setting;    /**< Can we transmit to this port   */
    unsigned             listener_cnt;  /**< Number of listeners.           */
    SLOT_TYPE           *listener_slots;/**< Array of listeners.            */
    unsigned            *listener_adj_level;
                                        /**< Array of listeners' level
                                             adjustment.                    */
    unsigned             transmitter_cnt;/**<Number of transmitters.        */

    /* Shortcut for port info. */
    unsigned             clock_rate;    /**< Port's clock rate.             */
    unsigned             samples_per_frame; /**< Port's samples per frame.  */
    unsigned             channel_count; /**< Port's channel count.          */

    /* Calculated signal levels: */
    unsigned             tx_level;      /**< Last tx level to this port.    */
    unsigned             rx_level;      /**< Last rx level from this port.  */

    /* The normalized signal level adjustment.
     * A value of 128 (NORMAL_LEVEL) means there's no adjustment.
     */
    unsigned             tx_adj_level;  /**< Adjustment for TX.             */
    unsigned             rx_adj_level;  /**< Adjustment for RX.             */
    pj_int16_t          *adj_level_buf; /**< The adjustment buffer.         */

    /* Resample, for converting clock rate, if they're different. */
    pjmedia_resample    *rx_resample;
    pjmedia_resample    *tx_resample;

    /* RX buffer is temporary buffer to be used when there is mismatch
     * between port's sample rate or ptime with conference's sample rate
     * or ptime. The buffer is used for sampling rate conversion AND/OR to
     * buffer the samples until there are enough samples to fulfill a
     * complete frame to be processed by the bridge.
     *
     * When both sample rate AND ptime of the port match the conference
     * settings, this buffer will not be created.
     *
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t          *rx_buf;        /**< The RX buffer.                 */
    unsigned             rx_buf_cap;    /**< Max size, in samples           */
    unsigned             rx_buf_count;  /**< # of samples in the buf.       */

    /* Mix buf is a temporary buffer used to mix all signal received
     * by this port from all other ports. The mixed signal will be
     * automatically adjusted to the appropriate level whenever
     * there is possibility of clipping.
     *
     * This buffer contains samples at bridge's clock rate.
     * The size of this buffer is equal to samples per frame of the bridge.
     */

    int                  mix_adj;       /**< Adjustment level for mix_buf.  */
    int                  last_mix_adj;  /**< Last adjustment level.         */
    pj_int32_t          *mix_buf;       /**< Total sum of signal.           */

    /* Tx buffer is a temporary buffer to be used when there's mismatch
     * between port's clock rate or ptime with conference's sample rate
     * or ptime. This buffer is used as the source of the sampling rate
     * conversion AND/OR to buffer the samples until there are enough
     * samples to fulfill a complete frame to be transmitted to the port.
     *
     * When both sample rate and ptime of the port match the bridge's
     * settings, this buffer will not be created.
     *
     * This buffer contains samples at port's clock rate.
     * The size of this buffer is the sum between port's samples per frame
     * and bridge's samples per frame.
     */
    pj_int16_t          *tx_buf;        /**< Tx buffer.                     */
    unsigned             tx_buf_cap;    /**< Max size, in samples.          */
    unsigned             tx_buf_count;  /**< # of samples in the buffer.    */

    /* When the port is not receiving signal from any other ports (e.g. when
     * no other ports is transmitting to this port), the bridge periodically
     * transmit NULL frame to the port to keep the port "alive" (for example,
     * a stream port needs this heart-beat to periodically transmit silence
     * frame to keep NAT binding alive).
     *
     * This NULL frame should be sent to the port at the port's ptime rate.
     * So if the port's ptime is greater than the bridge's ptime, the bridge
     * needs to delay the NULL frame until it's the right time to do so.
     *
     * This variable keeps track of how many pending NULL samples are being
     * "held" for this port. Once this value reaches samples_per_frame
     * value of the port, a NULL frame is sent. The samples value on this
     * variable is clocked at the port's clock rate.
     */
    unsigned             tx_heart_beat;

    /* Delay buffer is a special buffer for sound device port (port 0, master
     * port) and other passive ports (sound device port is also passive port).
     *
     * We need the delay buffer because we can not expect the mic and speaker
     * thread to run equally after one another. In most systems, each thread
     * will run multiple times before the other thread gains execution time.
     * For example, in my system, mic thread is called three times, then
     * speaker thread is called three times, and so on. This we call burst.
     *
     * There is also possibility of drift, unbalanced rate between put_frame
     * and get_frame operation, in passive ports. If drift happens, snd_buf
     * needs to be expanded or shrinked.
     *
     * Burst and drift are handled by delay buffer.
     */
    pjmedia_delay_buf   *delay_buf;

    pj_bool_t            is_new;        /**< Newly added port, avoid read/write
                                             data from/to.                  */

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
    /* Cache-line alignment for multi-threading */
    pj_uint8_t          cache_pad1[PJMEDIA_CONF_CACHE_LINE_SIZE];

    /* Per-port processing statistics */
    pj_atomic_t        *processing_count;
    pj_timestamp        last_process_time;

    /* Thread affinity hint */
    unsigned            preferred_cpu;

    pj_uint8_t          cache_pad2[PJMEDIA_CONF_CACHE_LINE_SIZE];
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */
};


/* Forward declarations */
typedef struct op_entry op_entry;


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    pj_pool_t            *pool;         /**< Pool                           */
    unsigned              options;      /**< Bitmask options.               */
    unsigned              max_ports;    /**< Maximum ports.                 */
    unsigned              port_cnt;     /**< Current number of ports.       */
    unsigned              connect_cnt;  /**< Total number of connections    */
    pjmedia_snd_port     *snd_dev_port; /**< Sound device port.             */
    pjmedia_port         *master_port;  /**< Port zero's port.              */
    char                  master_name_buf[80]; /**< Port0 name buffer.      */
    pj_mutex_t           *mutex;        /**< Conference mutex.              */
    struct conf_port    **ports;        /**< Array of ports.                */
    unsigned              clock_rate;   /**< Sampling rate.                 */
    unsigned              channel_count;/**< Number of channels (1=mono).   */
    unsigned              samples_per_frame;    /**< Samples per frame.     */
    unsigned              bits_per_sample;      /**< Bits per sample.       */

    op_entry             *op_queue;     /**< Queue of operations.           */
    op_entry             *op_queue_free;/**< Queue of free entries.         */
    pjmedia_conf_op_cb    cb;           /**< OP callback.                   */

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
    /* Worker thread management */
    worker_context     *workers;
    unsigned            worker_count;
    pj_sem_t          **worker_sems;
    pj_atomic_t        *workers_ready;
    pj_atomic_t        *frame_generation;

    /* Load balancing */
    load_stats         *worker_loads;
    pj_timestamp        last_rebalance;
    unsigned            rebalance_interval_ms;
    pj_lock_t          *rebalance_lock;

    /* CPU topology */
    unsigned            total_cpus;
    unsigned           *cpu_to_numa;
    pj_bool_t          *cpu_available;

    /* Synchronization */
    pj_barrier_t       *phase_barrier;      /* PJSIP barrier for phase sync */
    pj_atomic_t        *processing_phase;   /* Keep this for state tracking */

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
    /* Work stealing statistics */
    pj_atomic_t        *total_steals;
    pj_atomic_t        *total_steal_attempts;
#endif /* PJMEDIA_CONF_USE_WORK_STEALING */
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */
};


/* Prototypes */
static pj_status_t put_frame(pjmedia_port *this_port,
                             pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port,
                             pjmedia_frame *frame);
static pj_status_t destroy_port(pjmedia_port *this_port);

#if !DEPRECATED_FOR_TICKET_2234
static pj_status_t get_frame_pasv(pjmedia_port *this_port,
                                  pjmedia_frame *frame);
static pj_status_t destroy_port_pasv(pjmedia_port *this_port);
#endif

/* Helper for aligned allocation */
#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
static void* pj_pool_alloc_aligned(pj_pool_t *pool, pj_size_t size,
                                   pj_size_t alignment)
{
    void *mem;
    pj_size_t aligned_size;

    /* Calculate aligned size */
    aligned_size = size + alignment - 1;

    /* Allocate memory */
    mem = pj_pool_alloc(pool, aligned_size);
    if (!mem)
        return NULL;

    /* Align the pointer */
    return (void*)(((uintptr_t)mem + alignment - 1) & ~(alignment - 1));
}

#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
static void* alloc_aligned_buffer(pj_pool_t *pool, size_t size, size_t alignment)
{
    void *ptr;
    ptr = pj_pool_alloc_aligned(pool, size, alignment);
    if (!ptr) return NULL;

    /* Zero-initialize for safety */
    pj_bzero(ptr, size);
    return ptr;
}
#endif /* PJMEDIA_CONF_USE_SIMD */

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
/* Initialize work queue */
static pj_status_t init_work_queue(pj_pool_t *pool, work_queue **queue)
{
    work_queue *q;
    unsigned i;

    /* Allocate aligned for cache performance */
    q = (work_queue*)pj_pool_alloc_aligned(pool, sizeof(work_queue),
                                           PJMEDIA_CONF_CACHE_LINE_SIZE);
    if (!q) return PJ_ENOMEM;

    q->capacity = PJMEDIA_CONF_WORK_QUEUE_SIZE;
    q->items = (work_item*)pj_pool_calloc(pool, q->capacity, sizeof(work_item));
    if (!q->items) return PJ_ENOMEM;

    /* Initialize atomic flags */
    for (i = 0; i < q->capacity; i++) {
        pj_atomic_create(pool, 0, &q->items[i].taken);
    }

    pj_atomic_create(pool, 0, &q->head);
    pj_atomic_create(pool, 0, &q->tail);
    pj_atomic_create(pool, 0, &q->size);

    *queue = q;
    return PJ_SUCCESS;
}

/* Push work to local queue (single producer) */
static pj_bool_t push_work(work_queue *queue, unsigned port_idx)
{
    pj_uint32_t head, next_head, tail;

    head = pj_atomic_get(queue->head);
    tail = pj_atomic_get(queue->tail);
    next_head = (head + 1) % queue->capacity;

    /* Check if queue is full */
    if (next_head == tail)
        return PJ_FALSE;

    queue->items[head].port_idx = port_idx;
    pj_atomic_set(queue->items[head].taken, 0);

    /* Memory barrier to ensure item is written before updating head */
    MEMORY_BARRIER();

    pj_atomic_set(queue->head, next_head);
    pj_atomic_inc(queue->size);

    return PJ_TRUE;
}

/* Pop work from local queue (owner thread) */
static pj_bool_t pop_local_work(work_queue *queue, unsigned *port_idx)
{
    pj_uint32_t tail, head;

    tail = pj_atomic_get(queue->tail);
    head = pj_atomic_get(queue->head);

    if (tail == head)
        return PJ_FALSE;

    *port_idx = queue->items[tail].port_idx;

    pj_atomic_set(queue->tail, (tail + 1) % queue->capacity);
    pj_atomic_dec(queue->size);

    return PJ_TRUE;
}

/* Steal work from another worker's queue */
static pj_bool_t steal_work(work_queue *victim_queue, unsigned *port_idx)
{
    pj_uint32_t tail;

    /* Quick check if worth trying */
    if (pj_atomic_get(victim_queue->size) == 0)
        return PJ_FALSE;

    /* Try to steal from tail - simpler approach without cmpxchg */
    tail = pj_atomic_get(victim_queue->tail);

    /* Check if queue is empty */
    if (tail == pj_atomic_get(victim_queue->head))
        return PJ_FALSE;

    /* Try to claim the item at tail */
    if (pj_atomic_get(victim_queue->items[tail].taken) == 0) {
        /* Mark as taken first */
        pj_atomic_set(victim_queue->items[tail].taken, 1);

        /* Double-check it's still available */
        if (tail == pj_atomic_get(victim_queue->tail)) {
            /* Successfully stolen */
            *port_idx = victim_queue->items[tail].port_idx;

            /* Advance tail */
            pj_atomic_set(victim_queue->tail, (tail + 1) % victim_queue->capacity);
            pj_atomic_dec(victim_queue->size);

            return PJ_TRUE;
        }
    }

    return PJ_FALSE;
}

/* Check if we should attempt work stealing */
static pj_bool_t should_steal_work(work_queue *queue)
{
    pj_uint32_t size = pj_atomic_get(queue->size);
    pj_uint32_t threshold = (queue->capacity * PJMEDIA_CONF_STEAL_THRESHOLD) / 100;
    return size < threshold;
}

#endif /* PJMEDIA_CONF_USE_WORK_STEALING */
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */

/* As we don't hold mutex in the clock/get_frame(), some conference operations
 * that change conference states need to be synchronized with the clock.
 * So some steps of the operations needs to be executed within the clock tick
 * context, especially the steps related to changing ports connection.
 */

/* Synchronized operation list entry. */
typedef struct op_entry {
    PJ_DECL_LIST_MEMBER(struct op_entry);
    pjmedia_conf_op_type          type;
    pjmedia_conf_op_param         param;
} op_entry;


/* Prototypes of synchronized operation */
static pj_status_t op_add_port(pjmedia_conf *conf,
                               const pjmedia_conf_op_param *prm);
static pj_status_t op_remove_port(pjmedia_conf *conf,
                                  const pjmedia_conf_op_param *prm);
static pj_status_t op_connect_ports(pjmedia_conf *conf,
                                    const pjmedia_conf_op_param *prm);
static pj_status_t op_disconnect_ports(pjmedia_conf *conf,
                                       const pjmedia_conf_op_param *prm);

/* Standard required prototypes */
static pj_status_t read_port(pjmedia_conf *conf,
                            struct conf_port *cport,
                            pj_int16_t *frame,
                            pj_size_t count,
                            pjmedia_frame_type *type);
static pj_status_t write_port(pjmedia_conf *conf,
                             struct conf_port *cport,
                             const pj_timestamp *timestamp,
                             pjmedia_frame_type *frm_type);

/* Multi-threading specific prototypes */
#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
static pj_status_t init_cpu_topology(pjmedia_conf *conf);
static pj_status_t pin_thread_to_cpu(unsigned cpu_id);
static int worker_thread_proc(void *arg);
static void process_ports_range(pjmedia_conf *conf, unsigned start_port,
                               unsigned end_port, worker_context *ctx);
static void rebalance_worker_loads(pjmedia_conf *conf);
static unsigned select_cpu_for_worker(pjmedia_conf *conf, unsigned worker_id);
static pj_status_t get_frame_mt(pjmedia_port *this_port, pjmedia_frame *frame);
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */

static op_entry* get_free_op_entry(pjmedia_conf *conf)
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

static void handle_op_queue(pjmedia_conf *conf)
{
    /* The queue may grow while mutex is released, better put a limit? */
    enum { MAX_PROCESSED_OP = 100 };
    int i = 0;

    while (i++ < MAX_PROCESSED_OP) {
        op_entry *op;
        pjmedia_conf_op_type type;
        pjmedia_conf_op_param param;
        pj_status_t status;

        pj_mutex_lock(conf->mutex);

        /* Stop when queue empty */
        if (pj_list_empty(conf->op_queue)) {
            pj_mutex_unlock(conf->mutex);
            break;
        }

        /* Copy op */
        op = conf->op_queue->next;
        type = op->type;
        param = op->param;

        /* Free op */
        pj_list_erase(op);
        op->type = PJMEDIA_CONF_OP_UNKNOWN;
        pj_list_push_back(conf->op_queue_free, op);

        pj_mutex_unlock(conf->mutex);

        /* Process op */
        switch(type) {
            case PJMEDIA_CONF_OP_ADD_PORT:
                status = op_add_port(conf, &param);
                break;
            case PJMEDIA_CONF_OP_REMOVE_PORT:
                status = op_remove_port(conf, &param);
                break;
            case PJMEDIA_CONF_OP_CONNECT_PORTS:
                status = op_connect_ports(conf, &param);
                break;
            case PJMEDIA_CONF_OP_DISCONNECT_PORTS:
                status = op_disconnect_ports(conf, &param);
                break;
            default:
                status = PJ_EINVALIDOP;
                pj_assert(!"Invalid sync-op in conference");
                break;
        }
        if (conf->cb) {
            pjmedia_conf_op_info info = { 0 };

            pj_log_push_indent();
            info.op_type = type;
            info.status = status;
            info.op_param = param;
            (*conf->cb)(&info);
            pj_log_pop_indent();
        }
    }
}


/* Group lock handler */
static void conf_port_on_destroy(void *arg)
{
    struct conf_port *conf_port = (struct conf_port*)arg;
    if (conf_port->pool)
        pj_pool_safe_release(&conf_port->pool);
}


/*
 * Create port.
 */
static pj_status_t create_conf_port( pj_pool_t *parent_pool,
                                     pjmedia_conf *conf,
                                     pjmedia_port *port,
                                     const pj_str_t *name,
                                     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port;
    pj_pool_t *pool = NULL;
    char pname[PJ_MAX_OBJ_NAME];
    pj_status_t status = PJ_SUCCESS;

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0 && defined(HAS_NUMA_SUPPORT) && HAS_NUMA_SUPPORT != 0
    /* Save current NUMA policy */
    int saved_numa_preferred = -1;
    pj_bool_t numa_policy_set = PJ_FALSE;

    if (numa_available()) {
        saved_numa_preferred = numa_preferred();

        /* Use round-robin across NUMA nodes for initial allocation */
        static pj_atomic_t *numa_rr_counter = NULL;
        if (!numa_rr_counter) {
            pj_atomic_create(parent_pool, 0, &numa_rr_counter);
        }

        int num_nodes = numa_max_node() + 1;
        if (num_nodes > 1) {
            int node = pj_atomic_inc_and_get(numa_rr_counter) % num_nodes;
            numa_set_preferred(node);
            numa_policy_set = PJ_TRUE;
        }
    }
#endif /* PJMEDIA_CONF_USE_NUMA */
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */

    /* Make sure pool name is NULL terminated */
    pj_assert(name);
    pj_ansi_strxcpy2(pname, name, sizeof(pname));

    /* Create own pool */
    pool = pj_pool_create(parent_pool->factory, pname, 500, 500, NULL);
    if (!pool) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Create port. */
    conf_port = PJ_POOL_ZALLOC_T(pool, struct conf_port);
    PJ_ASSERT_ON_FAIL(conf_port, {status = PJ_ENOMEM; goto on_return;});
    conf_port->pool = pool;

    /* Increment port ref count to avoid premature destroy due to
     * asynchronous port removal.
     */
    if (port) {
        if (!port->grp_lock) {
            /* Create group lock if it does not have one */
            pjmedia_port_init_grp_lock(port, pool, NULL);
        }

        pj_grp_lock_add_ref(port->grp_lock);

        /* Pool may be used for creating port's group lock and the group lock
         * may be used by app, so pool destroy must be done via handler.
         */
        status = pj_grp_lock_add_handler(port->grp_lock, NULL, conf_port,
                                         &conf_port_on_destroy);
    }

    /* Set name */
    pj_strdup_with_null(pool, &conf_port->name, name);

    /* Default has tx and rx enabled. */
    conf_port->rx_setting = PJMEDIA_PORT_ENABLE;
    conf_port->tx_setting = PJMEDIA_PORT_ENABLE;

    /* Default level adjustment is 128 (which means no adjustment) */
    conf_port->tx_adj_level = NORMAL_LEVEL;
    conf_port->rx_adj_level = NORMAL_LEVEL;

    /* Create transmit flag array */
    conf_port->listener_slots = (SLOT_TYPE*) pj_pool_zalloc(pool,
                                          conf->max_ports * sizeof(SLOT_TYPE));
    PJ_ASSERT_ON_FAIL(conf_port->listener_slots,
                      {status = PJ_ENOMEM; goto on_return;});

    /* Create adjustment level array */
    conf_port->listener_adj_level = (unsigned *) pj_pool_zalloc(pool,
                                       conf->max_ports * sizeof(unsigned));
    PJ_ASSERT_ON_FAIL(conf_port->listener_adj_level,
                      {status = PJ_ENOMEM; goto on_return;});

    /* Save some port's infos, for convenience. */
    if (port) {
        pjmedia_audio_format_detail *afd;

        afd = pjmedia_format_get_audio_format_detail(&port->info.fmt, 1);
        conf_port->port = port;
        conf_port->clock_rate = afd->clock_rate;
        conf_port->samples_per_frame = PJMEDIA_AFD_SPF(afd);
        conf_port->channel_count = afd->channel_count;
    } else {
        conf_port->port = NULL;
        conf_port->clock_rate = conf->clock_rate;
        conf_port->samples_per_frame = conf->samples_per_frame;
        conf_port->channel_count = conf->channel_count;
    }

    /* Create adjustment level buffer. */
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
    conf_port->adj_level_buf = (pj_int16_t*)
        alloc_aligned_buffer(pool,
                            conf->samples_per_frame * sizeof(pj_int16_t),
                            SIMD_ALIGNMENT);
#else
    conf_port->adj_level_buf = (pj_int16_t*) pj_pool_zalloc(pool,
                               conf->samples_per_frame * sizeof(pj_int16_t));
#endif
    PJ_ASSERT_ON_FAIL(conf_port->adj_level_buf,
                      {status = PJ_ENOMEM; goto on_return;});

    /* If port's clock rate is different than conference's clock rate,
     * create a resample sessions.
     */
    if (conf_port->clock_rate != conf->clock_rate) {

        pj_bool_t high_quality;
        pj_bool_t large_filter;

        high_quality = ((conf->options & PJMEDIA_CONF_USE_LINEAR)==0);
        large_filter = ((conf->options & PJMEDIA_CONF_SMALL_FILTER)==0);

        /* Create resample for rx buffer. */
        status = pjmedia_resample_create( pool,
                                          high_quality,
                                          large_filter,
                                          conf->channel_count,
                                          conf_port->clock_rate,/* Rate in */
                                          conf->clock_rate, /* Rate out */
                                          conf->samples_per_frame *
                                          conf_port->clock_rate /
                                          conf->clock_rate,
                                          &conf_port->rx_resample);
        if (status != PJ_SUCCESS)
            goto on_return;


        /* Create resample for tx buffer. */
        status = pjmedia_resample_create(pool,
                                         high_quality,
                                         large_filter,
                                         conf->channel_count,
                                         conf->clock_rate,  /* Rate in */
                                         conf_port->clock_rate, /* Rate out */
                                         conf->samples_per_frame,
                                         &conf_port->tx_resample);
        if (status != PJ_SUCCESS)
            goto on_return;
    }

    /*
     * Initialize rx and tx buffer, only when port's samples per frame or
     * port's clock rate or channel number is different then the conference
     * bridge settings.
     */
    if (conf_port->clock_rate != conf->clock_rate ||
        conf_port->channel_count != conf->channel_count ||
        conf_port->samples_per_frame != conf->samples_per_frame)
    {
        unsigned port_ptime, conf_ptime, buff_ptime;

        port_ptime = conf_port->samples_per_frame / conf_port->channel_count *
            1000 / conf_port->clock_rate;
        conf_ptime = conf->samples_per_frame / conf->channel_count *
            1000 / conf->clock_rate;

        /* Calculate the size (in ptime) for the port buffer according to
         * this formula:
         *   - if either ptime is an exact multiple of the other, then use
         *     the larger ptime (e.g. 20ms and 40ms, use 40ms).
         *   - if not, then the ptime is sum of both ptimes (e.g. 20ms
         *     and 30ms, use 50ms)
         */
        if (port_ptime > conf_ptime) {
            buff_ptime = port_ptime;
            if (port_ptime % conf_ptime)
                buff_ptime += conf_ptime;
        } else {
            buff_ptime = conf_ptime;
            if (conf_ptime % port_ptime)
                buff_ptime += port_ptime;
        }

        /* Create RX buffer. */
        conf_port->rx_buf_cap = conf_port->clock_rate * buff_ptime / 1000;
        if (conf_port->channel_count > conf->channel_count)
            conf_port->rx_buf_cap *= conf_port->channel_count;
        else
            conf_port->rx_buf_cap *= conf->channel_count;

        conf_port->rx_buf_count = 0;
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
        conf_port->rx_buf = (pj_int16_t*)
                            alloc_aligned_buffer(pool,
                                                conf_port->rx_buf_cap * sizeof(conf_port->rx_buf[0]),
                                                SIMD_ALIGNMENT);
#else
        conf_port->rx_buf = (pj_int16_t*)
                            pj_pool_alloc(pool, conf_port->rx_buf_cap *
                                                sizeof(conf_port->rx_buf[0]));
#endif
        PJ_ASSERT_ON_FAIL(conf_port->rx_buf,
                          {status = PJ_ENOMEM; goto on_return;});

        /* Create TX buffer. */
        conf_port->tx_buf_cap = conf_port->rx_buf_cap;
        conf_port->tx_buf_count = 0;
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
        conf_port->tx_buf = (pj_int16_t*)
                            alloc_aligned_buffer(pool,
                                                conf_port->tx_buf_cap * sizeof(conf_port->tx_buf[0]),
                                                SIMD_ALIGNMENT);
#else
        conf_port->tx_buf = (pj_int16_t*)
                            pj_pool_alloc(pool, conf_port->tx_buf_cap *
                                                sizeof(conf_port->tx_buf[0]));
#endif
        PJ_ASSERT_ON_FAIL(conf_port->tx_buf,
                          {status = PJ_ENOMEM; goto on_return;});
    }

    /* Create mix buffer. */
#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
    /* Use aligned allocation for better cache and SIMD performance */
    conf_port->mix_buf = (pj_int32_t*)
        alloc_aligned_buffer(pool,
                            conf->samples_per_frame * sizeof(pj_int32_t),
                            SIMD_ALIGNMENT);
#else
    /* Use aligned allocation for better cache performance */
    conf_port->mix_buf = (pj_int32_t*)
        pj_pool_alloc_aligned(pool,
                             conf->samples_per_frame * sizeof(pj_int32_t),
                             32);
#endif /* PJMEDIA_CONF_USE_SIMD */
    /* Initialize NUMA preference to none - will be set when port is assigned */
    conf_port->preferred_cpu = 0;
#else
    /* Standard allocation */
    conf_port->mix_buf = (pj_int32_t*)
                         pj_pool_zalloc(pool, conf->samples_per_frame *
                                              sizeof(conf_port->mix_buf[0]));
#endif

    PJ_ASSERT_ON_FAIL(conf_port->mix_buf,
                      {status = PJ_ENOMEM; goto on_return;});
    conf_port->last_mix_adj = NORMAL_LEVEL;

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0 && defined(HAS_NUMA_SUPPORT) && HAS_NUMA_SUPPORT != 0
    /* Restore NUMA policy */
    if (numa_policy_set && numa_available()) {
        numa_set_localalloc();
    }
#endif
#endif

    /* Done */
    *p_conf_port = conf_port;

on_return:
#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0 && defined(HAS_NUMA_SUPPORT) && HAS_NUMA_SUPPORT != 0
    /* Make sure we restore NUMA policy on error too */
    if (numa_policy_set && numa_available()) {
        numa_set_localalloc();
    }
#endif
#endif

    if (status != PJ_SUCCESS) {
        if (pool)
            pj_pool_release(pool);
    }

    return status;
}


/*
 * Add passive port.
 */
static pj_status_t create_pasv_port( pjmedia_conf *conf,
                                     pj_pool_t *pool,
                                     const pj_str_t *name,
                                     pjmedia_port *port,
                                     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port;
    pj_status_t status;
    unsigned ptime;

    /* Create port */
    status = create_conf_port(pool, conf, port, name, &conf_port);
    if (status != PJ_SUCCESS)
        return status;

    /* Passive port has delay buf. */
    ptime = conf->samples_per_frame * 1000 / conf->clock_rate /
            conf->channel_count;
    status = pjmedia_delay_buf_create(pool, name->ptr,
                                      conf->clock_rate,
                                      conf->samples_per_frame,
                                      conf->channel_count,
                                      RX_BUF_COUNT * ptime, /* max delay */
                                      0, /* options */
                                      &conf_port->delay_buf);
    if (status != PJ_SUCCESS)
        return status;

    *p_conf_port = conf_port;

    return PJ_SUCCESS;
}


/*
 * Create port zero for the sound device.
 */
static pj_status_t create_sound_port( pj_pool_t *pool,
                                      pjmedia_conf *conf )
{
    struct conf_port *conf_port;
    pj_str_t name = { "Master/sound", 12 };
    pj_status_t status;


    status = create_pasv_port(conf, pool, &name, NULL, &conf_port);
    if (status != PJ_SUCCESS)
        return status;


    /* Create sound device port: */

    if ((conf->options & PJMEDIA_CONF_NO_DEVICE) == 0) {
        pjmedia_aud_stream *strm;
        pjmedia_aud_param param;

        /*
         * If capture is disabled then create player only port.
         * Otherwise create bidirectional sound device port.
         */
        if (conf->options & PJMEDIA_CONF_NO_MIC)  {
            status = pjmedia_snd_port_create_player(pool, -1, conf->clock_rate,
                                                    conf->channel_count,
                                                    conf->samples_per_frame,
                                                    conf->bits_per_sample,
                                                    0,  /* options */
                                                    &conf->snd_dev_port);

        } else {
            status = pjmedia_snd_port_create( pool, -1, -1, conf->clock_rate,
                                              conf->channel_count,
                                              conf->samples_per_frame,
                                              conf->bits_per_sample,
                                              0,    /* Options */
                                              &conf->snd_dev_port);

        }

        if (status != PJ_SUCCESS)
            return status;

        strm = pjmedia_snd_port_get_snd_stream(conf->snd_dev_port);
        status = pjmedia_aud_stream_get_param(strm, &param);
        if (status == PJ_SUCCESS) {
            pjmedia_aud_dev_info snd_dev_info;
            if (conf->options & PJMEDIA_CONF_NO_MIC)
                pjmedia_aud_dev_get_info(param.play_id, &snd_dev_info);
            else
                pjmedia_aud_dev_get_info(param.rec_id, &snd_dev_info);
            pj_strdup2_with_null(pool, &conf_port->name, snd_dev_info.name);
        }

        PJ_LOG(5,(THIS_FILE, "Sound device successfully created for port 0"));
    }


     /* Add the port to the bridge */
    conf->ports[0] = conf_port;
    conf->port_cnt++;

    return PJ_SUCCESS;
}

/* Multi-threading specific functions */
#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0

static pj_status_t conf_thread_set_affinity(unsigned cpu_mask)
{
#if defined(PJ_HAS_THREAD_AFFINITY) && PJ_HAS_THREAD_AFFINITY != 0
    return pj_thread_set_affinity(pj_thread_this(), cpu_mask);
#elif defined(PJ_LINUX) || defined(PJ_DARWIN)
    /* Fallback to pthread for Linux/macOS */
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    int cpu_id = 0;

    CPU_ZERO(&cpuset);
    while (cpu_mask) {
        if (cpu_mask & 1) {
            CPU_SET(cpu_id, &cpuset);
        }
        cpu_mask >>= 1;
        cpu_id++;
    }

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return PJ_RETURN_OS_ERROR(errno);
    }
    return PJ_SUCCESS;
#else
    /* Platform doesn't support thread affinity */
    PJ_UNUSED_ARG(cpu_mask);
    return PJ_ENOTSUP;
#endif
}

/* Thread naming compatibility layer */
static void conf_thread_set_name(const char *name)
{
    /* Platform-specific naming */
#if defined(PJ_LINUX) || defined(PJ_ANDROID)
    prctl(PR_SET_NAME, name);
#elif defined(PJ_DARWIN)
    pthread_setname_np(name);
#endif
}

static int worker_thread_proc(void *arg)
{
    worker_context *ctx = (worker_context*)arg;
    pjmedia_conf *conf = ctx->conf;
    pj_status_t status;
    char thread_name[32];

    /* Pin to CPU */
    status = pin_thread_to_cpu(ctx->cpu_id);
    if (status != PJ_SUCCESS && status != PJ_ENOTSUP) {
        PJ_LOG(3,(THIS_FILE, "Worker %u: CPU pinning failed", ctx->worker_id));
    }

#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0 && defined(HAS_NUMA_SUPPORT) && HAS_NUMA_SUPPORT != 0
    /* Set NUMA allocation policy for this thread */
    if (numa_available()) {
        int node = numa_node_of_cpu(ctx->cpu_id);
        numa_set_preferred(node);
        PJ_LOG(5,(THIS_FILE, "Worker %u: NUMA node %d preferred",
                  ctx->worker_id, node));
    }
#endif

    /* Set thread name using compatibility layer */
    pj_ansi_snprintf(thread_name, sizeof(thread_name), "conf_work_%u",
                     ctx->worker_id);
    conf_thread_set_name(thread_name);

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
    PJ_LOG(4,(THIS_FILE, "Worker %u started on CPU %u (work stealing enabled)",
              ctx->worker_id, ctx->cpu_id));
#else
    PJ_LOG(4,(THIS_FILE, "Worker %u started on CPU %u",
              ctx->worker_id, ctx->cpu_id));
#endif

    while (ctx->active) {
        /* Wait for work signal */
        pj_sem_wait(conf->worker_sems[ctx->worker_id]);

        if (!ctx->active)
            break;

        /* Process assigned ports */
        process_ports_range(conf, ctx->port_start, ctx->port_end, ctx);

        /* Signal completion */
        pj_atomic_inc(conf->workers_ready);
    }

    return 0;
}

/* Replace the old pin_thread_to_cpu function */
static pj_status_t pin_thread_to_cpu(unsigned cpu_id)
{
    /* Convert CPU ID to mask */
    unsigned cpu_mask = 1U << cpu_id;
    pj_status_t status = conf_thread_set_affinity(cpu_mask);

    if (status != PJ_SUCCESS && status != PJ_ENOTSUP) {
        PJ_LOG(3,(THIS_FILE, "Failed to pin thread to CPU %u: %d",
                  cpu_id, status));
    } else if (status == PJ_SUCCESS) {
        PJ_LOG(5,(THIS_FILE, "Thread pinned to CPU %u", cpu_id));
    }

    return status;
}

static pj_status_t init_cpu_topology(pjmedia_conf *conf)
{
    int i;

#if defined(PJ_LINUX) || defined(PJ_ANDROID)
    conf->total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(PJ_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    conf->total_cpus = sysinfo.dwNumberOfProcessors;
#elif defined(PJ_DARWIN)
    size_t len = sizeof(conf->total_cpus);
    int mib[2] = { CTL_HW, HW_NCPU };
    sysctl(mib, 2, &conf->total_cpus, &len, NULL, 0);
#else
    /* Default to 1 CPU if we can't detect */
    conf->total_cpus = 1;
#endif

    if (conf->total_cpus < 1)
        conf->total_cpus = 1;

    conf->cpu_to_numa = pj_pool_calloc(conf->pool, conf->total_cpus,
                                       sizeof(unsigned));
    conf->cpu_available = pj_pool_calloc(conf->pool, conf->total_cpus,
                                         sizeof(pj_bool_t));

    /* Detect NUMA topology */
#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0 && defined(HAS_NUMA_SUPPORT) && HAS_NUMA_SUPPORT != 0
    if (numa_available()) {
        for (i = 0; i < conf->total_cpus; i++) {
            conf->cpu_to_numa[i] = numa_node_of_cpu(i);
            conf->cpu_available[i] = PJ_TRUE;
        }
        PJ_LOG(4,(THIS_FILE, "NUMA topology detected: %d CPUs across nodes",
                  conf->total_cpus));
    } else
#endif
    {
        /* Non-NUMA system */
        for (i = 0; i < conf->total_cpus; i++) {
            conf->cpu_to_numa[i] = 0;
            conf->cpu_available[i] = PJ_TRUE;
        }
        PJ_LOG(4,(THIS_FILE, "Non-NUMA system: %d CPUs detected",
                  conf->total_cpus));
    }

    return PJ_SUCCESS;
}


static unsigned select_cpu_for_worker(pjmedia_conf *conf, unsigned worker_id)
{
    unsigned cpu_id;
    unsigned cpus_per_worker;

    /* Distribute workers across CPUs */
    cpus_per_worker = conf->total_cpus / conf->worker_count;
    cpu_id = worker_id * cpus_per_worker;

    return cpu_id % conf->total_cpus;
}

#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0

/* Include cpuid header */
#ifdef __GNUC__
#include <cpuid.h>
#endif

/* Check if CPU supports AVX2 */
static pj_bool_t cpu_has_avx2(void)
{
    static int has_avx2 = -1;

    if (has_avx2 == -1) {
#ifdef __GNUC__
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
        if (__get_cpuid_max(0, NULL) >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            has_avx2 = (ebx & (1 << 5)) ? 1 : 0;
        } else {
            has_avx2 = 0;
        }
#else
        /* Assume AVX2 is available if SIMD is enabled */
        has_avx2 = 1;
#endif
    }

    return has_avx2 ? PJ_TRUE : PJ_FALSE;
}

/* Enable AVX2 for these functions */
#ifdef __GNUC__
#pragma GCC push_options
#pragma GCC target("avx2")
#elif defined(_MSC_VER)
/* MSVC doesn't need special attributes for intrinsics */
#endif

/* Helper to mix audio using AVX2 SIMD instructions */
static void mix_audio_simd(pj_int32_t *mix_buf, const pj_int16_t *input,
                          unsigned samples_per_frame)
{
    unsigned k = 0;

    /* Only use SIMD if CPU supports AVX2 */
    if (!cpu_has_avx2()) {
        /* Fallback to scalar implementation */
        for (k = 0; k < samples_per_frame; k++) {
            mix_buf[k] += input[k];
        }
        return;
    }

    /* Process 8 samples at a time with AVX2 */
    if (IS_ALIGNED(mix_buf, SIMD_ALIGNMENT)) {
        for (; k + 7 < samples_per_frame; k += 8) {
            /* Load 8 16-bit samples and convert to 32-bit */
            __m128i input_16 = _mm_loadu_si128((const __m128i*)&input[k]);
            __m256i input_32 = _mm256_cvtepi16_epi32(input_16);

            /* Load existing mix buffer values */
            __m256i mix = _mm256_load_si256((const __m256i*)&mix_buf[k]);

            /* Add input to mix buffer */
            mix = _mm256_add_epi32(mix, input_32);

            /* Store back to mix buffer */
            _mm256_store_si256((__m256i*)&mix_buf[k], mix);
        }
    }

    /* Handle remaining samples */
    for (; k < samples_per_frame; k++) {
        mix_buf[k] += input[k];
    }
}

/* Helper for level adjustment with SIMD */
static void adjust_level_simd(pj_int16_t *output, const pj_int16_t *input,
                             unsigned samples_per_frame, int adj_level)
{
    unsigned k = 0;

    /* Only use SIMD if CPU supports AVX2 */
    if (!cpu_has_avx2()) {
        /* Fallback to scalar implementation */
        for (k = 0; k < samples_per_frame; k++) {
            pj_int32_t itemp = input[k];
            itemp *= adj_level;
            itemp >>= 7;
            if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
            else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;
            output[k] = (pj_int16_t)itemp;
        }
        return;
    }

    __m256i adj_vector = _mm256_set1_epi32(adj_level);

    /* Process 8 samples at a time */
    for (; k + 7 < samples_per_frame; k += 8) {
        /* Load 8 16-bit samples and convert to 32-bit */
        __m128i input_16 = _mm_loadu_si128((const __m128i*)&input[k]);
        __m256i input_32 = _mm256_cvtepi16_epi32(input_16);

        /* Multiply by adjustment level */
        input_32 = _mm256_mullo_epi32(input_32, adj_vector);

        /* Shift right by 7 (divide by 128) */
        input_32 = _mm256_srai_epi32(input_32, 7);

        /* Clamp to 16-bit range */
        input_32 = _mm256_max_epi32(input_32, _mm256_set1_epi32(MIN_LEVEL));
        input_32 = _mm256_min_epi32(input_32, _mm256_set1_epi32(MAX_LEVEL));

        /* Convert back to 16-bit and store - use pack instead of cvt */
        /* Pack 32-bit to 16-bit with signed saturation */
        __m256i zero = _mm256_setzero_si256();
        __m256i packed = _mm256_packs_epi32(input_32, zero);
        __m256i result = _mm256_permute4x64_epi64(packed, 0xD8);
        _mm_storeu_si128((__m128i*)&output[k], _mm256_castsi256_si128(result));
    }

    /* Handle remaining samples */
    for (; k < samples_per_frame; k++) {
        pj_int32_t itemp = input[k];
        itemp *= adj_level;
        itemp >>= 7;
        if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
        else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;
        output[k] = (pj_int16_t)itemp;
    }
}

/* Helper to find min/max using SIMD */
static void find_minmax_simd(const pj_int32_t *mix_buf, unsigned samples_per_frame,
                            pj_int32_t *mix_buf_min, pj_int32_t *mix_buf_max)
{
    unsigned k,i;

    /* Only use SIMD if CPU supports AVX2 */
    if (!cpu_has_avx2()) {
        /* Fallback to scalar implementation */
        for (k = 0; k < samples_per_frame; k++) {
            if (mix_buf[k] < *mix_buf_min) *mix_buf_min = mix_buf[k];
            if (mix_buf[k] > *mix_buf_max) *mix_buf_max = mix_buf[k];
        }
        return;
    }

    __m256i vmin = _mm256_set1_epi32(0);
    __m256i vmax = _mm256_set1_epi32(0);

    for (k = 0; k + 7 < samples_per_frame; k += 8) {
        __m256i mix_vals = _mm256_load_si256((const __m256i*)&mix_buf[k]);
        vmin = _mm256_min_epi32(vmin, mix_vals);
        vmax = _mm256_max_epi32(vmax, mix_vals);
    }

    /* Extract min/max from vectors */
    int min_array[8], max_array[8];
    _mm256_storeu_si256((__m256i*)min_array, vmin);
    _mm256_storeu_si256((__m256i*)max_array, vmax);

    for (i = 0; i < 8; i++) {
        if (min_array[i] < *mix_buf_min) *mix_buf_min = min_array[i];
        if (max_array[i] > *mix_buf_max) *mix_buf_max = max_array[i];
    }

    /* Check remaining samples */
    for (; k < samples_per_frame; k++) {
        if (mix_buf[k] < *mix_buf_min) *mix_buf_min = mix_buf[k];
        if (mix_buf[k] > *mix_buf_max) *mix_buf_max = mix_buf[k];
    }
}

/* Helper to copy samples with SIMD */
static void copy_samples_simd(pj_int32_t *mix_buf, const pj_int16_t *input,
                             unsigned samples_per_frame)
{
    unsigned k = 0;

    /* Only use SIMD if CPU supports AVX2 */
    if (!cpu_has_avx2()) {
        /* Fallback to scalar implementation */
        for (k = 0; k < samples_per_frame; k++) {
            mix_buf[k] = input[k];
        }
        return;
    }

    if (IS_ALIGNED(mix_buf, SIMD_ALIGNMENT)) {
        for (; k + 7 < samples_per_frame; k += 8) {
            __m128i input_16 = _mm_loadu_si128((const __m128i*)&input[k]);
            __m256i input_32 = _mm256_cvtepi16_epi32(input_16);
            _mm256_store_si256((__m256i*)&mix_buf[k], input_32);
        }
    }
    /* Handle remaining samples */
    for (; k < samples_per_frame; k++) {
        mix_buf[k] = input[k];
    }
}

/* Restore compiler options */
#ifdef __GNUC__
#pragma GCC pop_options
#endif

#else /* !PJMEDIA_CONF_USE_SIMD */

/* Stub implementations when SIMD is disabled */
static inline void mix_audio_simd(pj_int32_t *mix_buf, const pj_int16_t *input,
                                 unsigned samples_per_frame)
{
    unsigned k;
    for (k = 0; k < samples_per_frame; k++) {
        mix_buf[k] += input[k];
    }
}

static inline void adjust_level_simd(pj_int16_t *output, const pj_int16_t *input,
                                    unsigned samples_per_frame, int adj_level)
{
    unsigned k;
    for (k = 0; k < samples_per_frame; k++) {
        pj_int32_t itemp = input[k];
        itemp *= adj_level;
        itemp >>= 7;
        if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
        else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;
        output[k] = (pj_int16_t)itemp;
    }
}

static inline void find_minmax_simd(const pj_int32_t *mix_buf, unsigned samples_per_frame,
                                   pj_int32_t *mix_buf_min, pj_int32_t *mix_buf_max)
{
    unsigned k;
    for (k = 0; k < samples_per_frame; k++) {
        if (mix_buf[k] < *mix_buf_min) *mix_buf_min = mix_buf[k];
        if (mix_buf[k] > *mix_buf_max) *mix_buf_max = mix_buf[k];
    }
}

static inline void copy_samples_simd(pj_int32_t *mix_buf, const pj_int16_t *input,
                                    unsigned samples_per_frame)
{
    unsigned k;
    for (k = 0; k < samples_per_frame; k++) {
        mix_buf[k] = input[k];
    }
}

#endif /* !PJMEDIA_CONF_USE_SIMD */

static void process_ports_range(pjmedia_conf *conf,
                               unsigned start_port,
                               unsigned end_port,
                               worker_context *ctx)
{
    unsigned i, j;
    pj_timestamp start_ts, end_ts;
    pj_uint32_t processed = 0;
    pj_int16_t *frame_buf;

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
    unsigned port_idx;
    pj_bool_t has_work;
    pj_bool_t use_work_stealing = PJ_TRUE;

    /* Populate work queue if work stealing is enabled */
    if (ctx && ctx->local_queue) {
        /* Clear queue first */
        pj_atomic_set(ctx->local_queue->head, 0);
        pj_atomic_set(ctx->local_queue->tail, 0);
        pj_atomic_set(ctx->local_queue->size, 0);

        /* Add ports to work queue */
        for (i = start_port; i < end_port && i < conf->max_ports; i++) {
            struct conf_port *conf_port = conf->ports[i];
            if (!conf_port || conf_port->is_new)
                continue;
            if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE)
                continue;
            if (conf_port->listener_cnt == 0)
                continue;

            if (!push_work(ctx->local_queue, i)) {
                /* Queue full, fall back to normal processing */
                use_work_stealing = PJ_FALSE;
                break;
            }
        }
    } else {
        use_work_stealing = PJ_FALSE;
    }
#endif

    /* Allocate frame buffer for this worker */
    frame_buf = alloca(conf->samples_per_frame * sizeof(pj_int16_t));

    pj_get_timestamp(&start_ts);

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
    if (use_work_stealing) {
        /* Work stealing mode */
        do {
            has_work = pop_local_work(ctx->local_queue, &port_idx);

            if (!has_work && should_steal_work(ctx->local_queue)) {
                /* Try stealing from other workers */
                unsigned attempts = 0;

                while (attempts < PJMEDIA_CONF_STEAL_ATTEMPTS) {
                    worker_context *victim;

                    /* Round-robin victim selection */
                    ctx->victim_idx = (ctx->victim_idx + 1) % conf->worker_count;
                    if (ctx->victim_idx == ctx->worker_id) {
                        ctx->victim_idx = (ctx->victim_idx + 1) % conf->worker_count;
                    }

                    victim = &conf->workers[ctx->victim_idx];

                    if (victim->local_queue &&
                        steal_work(victim->local_queue, &port_idx)) {
                        has_work = PJ_TRUE;
                        pj_atomic_inc(ctx->successful_steals);
                        pj_atomic_inc(conf->total_steals);
                        break;
                    }

                    attempts++;
                    pj_atomic_inc(ctx->steal_attempts);
                    pj_atomic_inc(conf->total_steal_attempts);
                }
            }

            if (has_work) {
                i = port_idx;
                /* Process single port */
                struct conf_port *conf_port = conf->ports[i];
                pj_int32_t level = 0;

                /* Skip empty or new ports */
                if (!conf_port || conf_port->is_new)
                    continue;

                processed++;

                /* Skip if we're not allowed to receive from this port */
                if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE) {
                    conf_port->rx_level = 0;
                    continue;
                }

                /* Also skip if this port doesn't have listeners */
                if (conf_port->listener_cnt == 0) {
                    conf_port->rx_level = 0;
                    continue;
                }

                /* Get frame from this port */
                if (conf_port->delay_buf != NULL) {
                    pj_status_t status;

                    status = pjmedia_delay_buf_get(conf_port->delay_buf,
                                                  (pj_int16_t*)frame_buf);
                    if (status != PJ_SUCCESS) {
                        conf_port->rx_level = 0;
                        continue;
                    }

                } else {
                    pj_status_t status;
                    pjmedia_frame_type frame_type;

                    status = read_port(conf, conf_port, (pj_int16_t*)frame_buf,
                                      conf->samples_per_frame, &frame_type);

                    if (status != PJ_SUCCESS) {
                        conf_port->rx_level = 0;
                        continue;
                    }

                    /* Check that the port is not removed when we call get_frame() */
                    if (conf->ports[i] == NULL) {
                        conf_port->rx_level = 0;
                        continue;
                    }

                    /* Ignore if we didn't get any frame */
                    if (frame_type != PJMEDIA_FRAME_TYPE_AUDIO) {
                        conf_port->rx_level = 0;
                        continue;
                    }
                }

                /* Calculate and adjust RX level */
                if (conf_port->rx_adj_level != NORMAL_LEVEL) {
                    for (j = 0; j < conf->samples_per_frame; ++j) {
                        pj_int32_t itemp = frame_buf[j];

                        /* Adjust the level */
                        itemp *= conf_port->rx_adj_level;
                        itemp >>= 7;

                        /* Clip the signal if it's too loud */
                        if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                        else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                        frame_buf[j] = (pj_int16_t) itemp;
                        level += (frame_buf[j] >= 0 ? frame_buf[j] : -frame_buf[j]);
                    }
                } else {
                    for (j = 0; j < conf->samples_per_frame; ++j) {
                        level += (frame_buf[j] >= 0 ? frame_buf[j] : -frame_buf[j]);
                    }
                }

                level /= conf->samples_per_frame;

                /* Convert level to 8bit complement ulaw */
                level = pjmedia_linear2ulaw(level) ^ 0xff;

                /* Put this level to port's last RX level */
                conf_port->rx_level = level;

                /* Mix the audio to all listeners */
                for (j = 0; j < conf_port->listener_cnt; ++j) {
                    struct conf_port *listener;
                    pj_int32_t *mix_buf;
                    pj_int16_t *p_in_conn_leveled;

                    listener = conf->ports[conf_port->listener_slots[j]];

                    /* Skip if this listener doesn't want to receive audio */
                    if (!listener || listener->tx_setting != PJMEDIA_PORT_ENABLE)
                        continue;

                    /* In work stealing mode, we can process any port */

                    mix_buf = listener->mix_buf;

                    /* apply connection level, if not normal */
                    if (conf_port->listener_adj_level[j] != NORMAL_LEVEL) {
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
                        /* Use SIMD for level adjustment */
                        adjust_level_simd(conf_port->adj_level_buf, frame_buf,
                                         conf->samples_per_frame,
                                         conf_port->listener_adj_level[j]);
#else
                        unsigned k;
                        /* Mix with level adjustment */
                        for (k = 0; k < conf->samples_per_frame; ++k) {
                            pj_int32_t itemp = frame_buf[k];

                            itemp *= conf_port->listener_adj_level[j];
                            itemp >>= 7;

                            /* Clip the signal if needed */
                            if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                            else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                            /* Accumulate to mix buffer */
                            conf_port->adj_level_buf[k] = (pj_int16_t)itemp;
                        }
#endif
                        /* take the leveled frame */
                        p_in_conn_leveled = conf_port->adj_level_buf;
                    } else {
                        /* take the frame as-is */
                        p_in_conn_leveled = frame_buf;
                    }

                    if (listener->transmitter_cnt > 1) {
                        /* Mixing signals */
                        pj_int32_t mix_buf_min = 0;
                        pj_int32_t mix_buf_max = 0;

#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
                        /* Use SIMD mixing */
                        mix_audio_simd(mix_buf, p_in_conn_leveled, conf->samples_per_frame);

                        /* Find min/max for overflow detection using SIMD */
                        find_minmax_simd(mix_buf, conf->samples_per_frame,
                                        &mix_buf_min, &mix_buf_max);
#else
                        unsigned k;
                        /* Mix without level adjustment */
                        for (k = 0; k < conf->samples_per_frame; ++k) {
                            mix_buf[k] += p_in_conn_leveled[k];
                        }

                        /* Check for overflow and adjust mix_adj if needed */
                        for (k = 0; k < conf->samples_per_frame; ++k) {
                            if (mix_buf[k] < mix_buf_min)
                                mix_buf_min = mix_buf[k];
                            if (mix_buf[k] > mix_buf_max)
                                mix_buf_max = mix_buf[k];
                        }
#endif

                        /* Check if normalization adjustment needed */
                        if (mix_buf_min < MIN_LEVEL || mix_buf_max > MAX_LEVEL) {
                            int tmp_adj;

                            if (-mix_buf_min > mix_buf_max)
                                mix_buf_max = -mix_buf_min;

                            /* NORMAL_LEVEL * MAX_LEVEL / mix_buf_max */
                            tmp_adj = (MAX_LEVEL << 7) / mix_buf_max;
                            if (tmp_adj < listener->mix_adj)
                                listener->mix_adj = tmp_adj;
                        }
                    } else {
                        /* Only 1 transmitter */
#if defined(PJMEDIA_CONF_USE_SIMD) && PJMEDIA_CONF_USE_SIMD != 0
                        /* Use SIMD copy for aligned buffers */
                        copy_samples_simd(mix_buf, p_in_conn_leveled, conf->samples_per_frame);
#else
                        unsigned k;
                        for (k = 0; k < conf->samples_per_frame; ++k) {
                            mix_buf[k] = p_in_conn_leveled[k];
                        }
#endif
                    }
                }
            }
        } while (has_work);
    } else {
        /* Work stealing disabled - fall back to traditional processing */
#endif
        /* Traditional range-based processing */
        for (i = start_port; i < end_port && i < conf->max_ports; i++) {
            struct conf_port *conf_port = conf->ports[i];
            pj_int32_t level = 0;
            unsigned k;

            /* Skip empty or new ports */
            if (!conf_port || conf_port->is_new)
                continue;

            processed++;

            /* Skip if we're not allowed to receive from this port */
            if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE) {
                conf_port->rx_level = 0;
                continue;
            }

            /* Also skip if this port doesn't have listeners */
            if (conf_port->listener_cnt == 0) {
                conf_port->rx_level = 0;
                continue;
            }

            /* Get frame from this port */
            if (conf_port->delay_buf != NULL) {
                pj_status_t status;

                status = pjmedia_delay_buf_get(conf_port->delay_buf,
                                              (pj_int16_t*)frame_buf);
                if (status != PJ_SUCCESS) {
                    conf_port->rx_level = 0;
                    continue;
                }

            } else {
                pj_status_t status;
                pjmedia_frame_type frame_type;

                status = read_port(conf, conf_port, (pj_int16_t*)frame_buf,
                                  conf->samples_per_frame, &frame_type);

                if (status != PJ_SUCCESS) {
                    conf_port->rx_level = 0;
                    continue;
                }

                /* Check that the port is not removed when we call get_frame() */
                if (conf->ports[i] == NULL) {
                    conf_port->rx_level = 0;
                    continue;
                }

                /* Ignore if we didn't get any frame */
                if (frame_type != PJMEDIA_FRAME_TYPE_AUDIO) {
                    conf_port->rx_level = 0;
                    continue;
                }
            }

            /* Calculate and adjust RX level */
            if (conf_port->rx_adj_level != NORMAL_LEVEL) {
                for (j = 0; j < conf->samples_per_frame; ++j) {
                    pj_int32_t itemp = frame_buf[j];

                    /* Adjust the level */
                    itemp *= conf_port->rx_adj_level;
                    itemp >>= 7;

                    /* Clip the signal if it's too loud */
                    if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                    else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                    frame_buf[j] = (pj_int16_t) itemp;
                    level += (frame_buf[j] >= 0 ? frame_buf[j] : -frame_buf[j]);
                }
            } else {
                for (j = 0; j < conf->samples_per_frame; ++j) {
                    level += (frame_buf[j] >= 0 ? frame_buf[j] : -frame_buf[j]);
                }
            }

            level /= conf->samples_per_frame;

            /* Convert level to 8bit complement ulaw */
            level = pjmedia_linear2ulaw(level) ^ 0xff;

            /* Put this level to port's last RX level */
            conf_port->rx_level = level;

            /* Add the signal to all listeners */
            for (j = 0; j < conf_port->listener_cnt; ++j) {
                struct conf_port *listener;
                pj_int32_t *mix_buf;
                pj_int16_t *p_in_conn_leveled;

                listener = conf->ports[conf_port->listener_slots[j]];

                /* Skip if this listener doesn't want to receive audio */
                if (!listener || listener->tx_setting != PJMEDIA_PORT_ENABLE)
                    continue;

                /* Skip if listener is not in our processing range */
                if (conf_port->listener_slots[j] < start_port ||
                    conf_port->listener_slots[j] >= end_port)
                    continue;

                mix_buf = listener->mix_buf;

                /* apply connection level, if not normal */
                if (conf_port->listener_adj_level[j] != NORMAL_LEVEL) {
                    for (k = 0; k < conf->samples_per_frame; ++k) {
                        pj_int32_t itemp = frame_buf[k];

                        itemp *= conf_port->listener_adj_level[j];
                        itemp >>= 7;

                        /* Clip the signal if it's too loud */
                        if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                        else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                        conf_port->adj_level_buf[k] = (pj_int16_t)itemp;
                    }

                    /* take the leveled frame */
                    p_in_conn_leveled = conf_port->adj_level_buf;
                } else {
                    /* take the frame as-is */
                    p_in_conn_leveled = frame_buf;
                }

                if (listener->transmitter_cnt > 1) {
                    /* Mixing signals */
                    pj_int32_t mix_buf_min = 0;
                    pj_int32_t mix_buf_max = 0;

                    /* Mix without level adjustment */
                    for (k = 0; k < conf->samples_per_frame; ++k) {
                        mix_buf[k] += p_in_conn_leveled[k];
                    }

                    /* Check for overflow and adjust mix_adj if needed */
                    for (k = 0; k < conf->samples_per_frame; ++k) {
                        if (mix_buf[k] < mix_buf_min)
                            mix_buf_min = mix_buf[k];
                        if (mix_buf[k] > mix_buf_max)
                            mix_buf_max = mix_buf[k];
                    }

                    /* Check if normalization adjustment needed */
                    if (mix_buf_min < MIN_LEVEL || mix_buf_max > MAX_LEVEL) {
                        int tmp_adj;

                        if (-mix_buf_min > mix_buf_max)
                            mix_buf_max = -mix_buf_min;

                        /* NORMAL_LEVEL * MAX_LEVEL / mix_buf_max */
                        tmp_adj = (MAX_LEVEL << 7) / mix_buf_max;
                        if (tmp_adj < listener->mix_adj)
                            listener->mix_adj = tmp_adj;
                    }
                } else {
                    /* Only 1 transmitter */
                    for (k = 0; k < conf->samples_per_frame; ++k) {
                        mix_buf[k] = p_in_conn_leveled[k];
                    }
                }
            }
        }
#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
    }
#endif

    /* Synchronization barrier */
    {
        pj_status_t barrier_status;

        barrier_status = pj_barrier_wait(conf->phase_barrier, 0);

        if (barrier_status == 1) {
            TRACE_((THIS_FILE, "Worker %u: barrier serial thread",
                    ctx ? ctx->worker_id : 999));
        } else if (barrier_status != PJ_SUCCESS) {
            PJ_LOG(3,(THIS_FILE, "Worker %u: barrier wait failed: %d",
                    ctx ? ctx->worker_id : 999, barrier_status));
        }
    }

    /* Write to all ports in this range */
    for (i = start_port; i < end_port && i < conf->max_ports; i++) {
        struct conf_port *conf_port = conf->ports[i];
        pjmedia_frame_type frm_type;
        pj_status_t status;

        if (!conf_port || conf_port->is_new)
            continue;

        pj_timestamp ts;
        ts.u64 = pj_atomic_get(conf->frame_generation) * conf->samples_per_frame;

        status = write_port(conf, conf_port, &ts, &frm_type);
        if (status != PJ_SUCCESS) {
            /* Log error but continue processing other ports */
            PJ_LOG(4,(THIS_FILE, "Worker %u: write_port failed for port %u",
                      ctx ? ctx->worker_id : 999, i));
        }
    }

    pj_get_timestamp(&end_ts);

    /* Update performance statistics */
    if (ctx) {
        pj_uint32_t elapsed_ns = pj_elapsed_usec(&start_ts, &end_ts) * 1000;
        pj_atomic_add(ctx->frames_processed, 1);
        pj_atomic_add(ctx->ports_processed, processed);

        /* Update load statistics for rebalancing */
        if (conf->worker_loads) {
            load_stats *stats = &conf->worker_loads[ctx->worker_id];
            /* Exponential moving average */
            stats->avg_processing_ns = (stats->avg_processing_ns * 7 + elapsed_ns) / 8;
            if (elapsed_ns > stats->peak_processing_ns)
                stats->peak_processing_ns = elapsed_ns;
            stats->ports_assigned = end_port - start_port;
        }
    }
}


static void rebalance_worker_loads(pjmedia_conf *conf)
{
    unsigned i;
    unsigned total_active_ports = 0;
    unsigned ports_per_worker;
    unsigned current_port = 0;
    pj_timestamp now;

    pj_get_timestamp(&now);

    /* Check if it's time to rebalance */
    if (pj_elapsed_msec(&conf->last_rebalance, &now) < conf->rebalance_interval_ms)
        return;

    pj_lock_acquire(conf->rebalance_lock);

    /* Count active ports */
    for (i = 0; i < conf->max_ports; i++) {
        if (conf->ports[i] && !conf->ports[i]->is_new &&
            conf->ports[i]->rx_setting != PJMEDIA_PORT_DISABLE) {
            total_active_ports++;
        }
    }

    if (total_active_ports == 0) {
        pj_lock_release(conf->rebalance_lock);
        return;
    }

    /* Equal distribution */
    ports_per_worker = (total_active_ports + conf->worker_count - 1) /
                      conf->worker_count;

    for (i = 0; i < conf->worker_count; i++) {
        conf->workers[i].port_start = current_port;
        current_port += ports_per_worker;
        if (current_port > conf->max_ports)
            current_port = conf->max_ports;
        conf->workers[i].port_end = current_port;
    }

    conf->last_rebalance = now;
    pj_lock_release(conf->rebalance_lock);
}

static void write_output_frame(pjmedia_conf *conf, pjmedia_frame *frame)
{
    pjmedia_frame_type speaker_frame_type = PJMEDIA_FRAME_TYPE_AUDIO;

    /* Return sound playback frame from port 0 */
    if (conf->ports[0] && conf->ports[0]->tx_level) {
        TRACE_((THIS_FILE, "write to audio, count=%d",
                           conf->samples_per_frame));
        pjmedia_copy_samples((pj_int16_t*)frame->buf,
                            (const pj_int16_t*)conf->ports[0]->mix_buf,
                            conf->samples_per_frame);
    } else {
        /* Force frame type NONE */
        speaker_frame_type = PJMEDIA_FRAME_TYPE_NONE;
        pjmedia_zero_samples((pj_int16_t*)frame->buf, conf->samples_per_frame);
    }

    /* Sset frame type */
    frame->type = speaker_frame_type;
}

static pj_status_t get_frame_mt(pjmedia_port *this_port,
                               pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    unsigned i;

    /* Handle queued operations synchronously */
    if (!pj_list_empty(conf->op_queue)) {
        handle_op_queue(conf);
    }

    /* Rebalance if needed */
    rebalance_worker_loads(conf);

    /* Increment frame generation */
    pj_atomic_inc(conf->frame_generation);

    /* Reset worker ready count */
    pj_atomic_set(conf->workers_ready, 0);

    /* Reset port buffers */
    for (i = 0; i < conf->max_ports; i++) {
        struct conf_port *conf_port = conf->ports[i];

        if (!conf_port || conf_port->is_new)
            continue;

        /* Skip if we're not allowed to transmit to this port */
        if (conf_port->tx_setting != PJMEDIA_PORT_ENABLE)
            continue;

        /* Reset buffer and auto adjustment level */
        conf_port->mix_adj = NORMAL_LEVEL;
        if (conf_port->transmitter_cnt) {
            pj_bzero(conf_port->mix_buf,
                     conf->samples_per_frame * sizeof(conf_port->mix_buf[0]));
        }
    }

    /* Signal all workers to start */
    for (i = 0; i < conf->worker_count; i++) {
        pj_sem_post(conf->worker_sems[i]);
    }

    {
        pj_status_t barrier_status;
        barrier_status = pj_barrier_wait(conf->phase_barrier, 0);
        if (barrier_status == 1) {
            TRACE_((THIS_FILE, "Main thread is barrier serial thread"));
        } else if (barrier_status < 0) {
            PJ_LOG(3,(THIS_FILE, "Main thread: barrier wait error: %d",
                    barrier_status));
        }
    }

    /* Wait for all workers to complete */
    while (pj_atomic_get(conf->workers_ready) < conf->worker_count) {
        pj_thread_sleep(0); /* Yield CPU */
    }

    /* Final mixing and output (single-threaded for now) */
    write_output_frame(conf, frame);

    return PJ_SUCCESS;
}

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0

PJ_DEF(pj_status_t) pjmedia_conf_get_work_stealing_stats(pjmedia_conf *conf,
                                                        pj_uint32_t *total_steals,
                                                        pj_uint32_t *total_attempts,
                                                        unsigned *per_worker_steals,
                                                        unsigned *per_worker_attempts)
{
    unsigned i;

    PJ_ASSERT_RETURN(conf, PJ_EINVAL);

    if (total_steals)
        *total_steals = pj_atomic_get(conf->total_steals);

    if (total_attempts)
        *total_attempts = pj_atomic_get(conf->total_steal_attempts);

    if (per_worker_steals && per_worker_attempts) {
        for (i = 0; i < conf->worker_count; i++) {
            per_worker_steals[i] = pj_atomic_get(conf->workers[i].successful_steals);
            per_worker_attempts[i] = pj_atomic_get(conf->workers[i].steal_attempts);
        }
    }

    return PJ_SUCCESS;
}

#endif /* PJMEDIA_CONF_USE_WORK_STEALING */
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */


/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool_,
                                         unsigned max_ports,
                                         unsigned clock_rate,
                                         unsigned channel_count,
                                         unsigned samples_per_frame,
                                         unsigned bits_per_sample,
                                         unsigned options,
                                         pjmedia_conf **p_conf )
{
    pj_pool_t *pool;
    pjmedia_conf *conf;
    const pj_str_t name = { "Conf", 4 };
    pj_status_t status;

    PJ_ASSERT_RETURN(samples_per_frame > 0, PJ_EINVAL);
    /* Can only accept 16bits per sample, for now.. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Creating conference bridge with %d ports",
              max_ports));

    /* Create own pool */
    pool = pj_pool_create(pool_->factory, name.ptr, 512, 512, NULL);
    if (!pool) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in alloc"));
        return PJ_ENOMEM;
    }

    /* Create and init conf structure. */
    conf = PJ_POOL_ZALLOC_T(pool, pjmedia_conf);
    PJ_ASSERT_RETURN(conf, PJ_ENOMEM);
    conf->pool = pool;

    conf->ports = (struct conf_port**)
                  pj_pool_zalloc(pool, max_ports*sizeof(void*));
    PJ_ASSERT_RETURN(conf->ports, PJ_ENOMEM);

    conf->options = options;
    conf->max_ports = max_ports;
    conf->clock_rate = clock_rate;
    conf->channel_count = channel_count;
    conf->samples_per_frame = samples_per_frame;
    conf->bits_per_sample = bits_per_sample;


    /* Create and initialize the master port interface. */
    conf->master_port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(conf->master_port, PJ_ENOMEM);

    pjmedia_port_info_init(&conf->master_port->info, &name, SIGNATURE,
                           clock_rate, channel_count, bits_per_sample,
                           samples_per_frame);

    conf->master_port->port_data.pdata = conf;
    conf->master_port->port_data.ldata = 0;

    conf->master_port->get_frame = &get_frame;
    conf->master_port->put_frame = &put_frame;
    conf->master_port->on_destroy = &destroy_port;

    /* Create port zero for sound device. */
    status = create_sound_port(pool, conf);
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy(conf);
        return status;
    }

    /* Create mutex. */
    status = pj_mutex_create_recursive(pool, "conf", &conf->mutex);
    if (status != PJ_SUCCESS) {
        pjmedia_conf_destroy(conf);
        return status;
    }

    /* If sound device was created, connect sound device to the
     * master port.
     */
    if (conf->snd_dev_port) {
        status = pjmedia_snd_port_connect( conf->snd_dev_port,
                                           conf->master_port );
        if (status != PJ_SUCCESS) {
            pjmedia_conf_destroy(conf);
            return status;
        }
    }

    /* Allocate synchronized operation queues */
    conf->op_queue = PJ_POOL_ZALLOC_T(pool, op_entry);
    conf->op_queue_free = PJ_POOL_ZALLOC_T(pool, op_entry);
    if (!conf->op_queue || !conf->op_queue_free) {
        PJ_PERROR(1, (THIS_FILE, PJ_ENOMEM, "Create failed in create queues"));
        pjmedia_conf_destroy(conf);
        return PJ_ENOMEM;
    }
    pj_list_init(conf->op_queue);
    pj_list_init(conf->op_queue_free);

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
        unsigned i;

        /* Initialize CPU topology */
        status = init_cpu_topology(conf);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Determine worker count */
        conf->worker_count = PJMEDIA_CONF_THREAD_POOL_SIZE;
        if (conf->worker_count > conf->total_cpus)
            conf->worker_count = conf->total_cpus;

        /* Allocate worker contexts */
        conf->workers = pj_pool_calloc(pool, conf->worker_count,
                                      sizeof(worker_context));
        conf->worker_sems = pj_pool_calloc(pool, conf->worker_count,
                                          sizeof(pj_sem_t*));
        conf->worker_loads = pj_pool_calloc(pool, conf->worker_count,
                                           sizeof(load_stats));

        /* Create synchronization primitives */
        status = pj_atomic_create(pool, 0, &conf->workers_ready);
        if (status != PJ_SUCCESS)
            goto on_error;

        status = pj_atomic_create(pool, 0, &conf->frame_generation);
        if (status != PJ_SUCCESS)
            goto on_error;

        status = pj_barrier_create(pool, conf->worker_count + 1,
                                  &conf->phase_barrier);
        if (status != PJ_SUCCESS) {
            PJ_LOG(3,(THIS_FILE, "Failed to create phase barrier"));
            goto on_error;
        }

        status = pj_atomic_create(pool, 0, &conf->processing_phase);
        if (status != PJ_SUCCESS)
            goto on_error;


        status = pj_lock_create_recursive_mutex(pool, "rebalance",
                                               &conf->rebalance_lock);
        if (status != PJ_SUCCESS)
            goto on_error;

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
        /* Initialize work stealing statistics */
        status = pj_atomic_create(pool, 0, &conf->total_steals);
        if (status != PJ_SUCCESS)
            goto on_error;

        status = pj_atomic_create(pool, 0, &conf->total_steal_attempts);
        if (status != PJ_SUCCESS)
            goto on_error;
#endif /* PJMEDIA_CONF_USE_WORK_STEALING */

        /* Initialize workers */
        for (i = 0; i < conf->worker_count; i++) {
            worker_context *ctx = &conf->workers[i];

            ctx->conf = conf;
            ctx->worker_id = i;
            ctx->cpu_id = select_cpu_for_worker(conf, i);
            ctx->active = PJ_TRUE;

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
            /* Initialize work stealing for this worker */
            status = init_work_queue(pool, &ctx->local_queue);
            if (status != PJ_SUCCESS) {
                PJ_LOG(3,(THIS_FILE, "Failed to init work queue for worker %u", i));
                goto on_error;
            }

            pj_atomic_create(pool, 0, &ctx->steal_attempts);
            pj_atomic_create(pool, 0, &ctx->successful_steals);
            ctx->victim_idx = i;  /* Start with different victims */
#endif /* PJMEDIA_CONF_USE_WORK_STEALING */

            /* Create worker semaphore */
            status = pj_sem_create(pool, NULL, 0, 1, &conf->worker_sems[i]);
            if (status != PJ_SUCCESS)
                goto on_error;

            /* Create performance counters */
            pj_atomic_create(pool, 0, &ctx->frames_processed);
            pj_atomic_create(pool, 0, &ctx->ports_processed);

            /* Start worker thread */
            status = pj_thread_create(pool, "conf_worker",
                                     &worker_thread_proc,
                                     ctx, 0, 0, &ctx->thread);
            if (status != PJ_SUCCESS) {
                unsigned j;
                for (j = 0; j < i; j++) {
                    conf->workers[j].active = PJ_FALSE;
                    pj_sem_post(conf->worker_sems[j]);
                    pj_thread_join(conf->workers[j].thread);
                }
                PJ_LOG(3,(THIS_FILE, "Failed to create worker thread %u", i));
                goto on_error;
            }
        }

        /* Set rebalance interval */
        conf->rebalance_interval_ms = 50; /* Rebalance every 50ms */
        pj_get_timestamp(&conf->last_rebalance);

        /* Override get_frame function for MT */
        conf->master_port->get_frame = &get_frame_mt;

#if defined(PJMEDIA_CONF_USE_WORK_STEALING) && PJMEDIA_CONF_USE_WORK_STEALING != 0
        PJ_LOG(4,(THIS_FILE, "Conference bridge created with %u worker threads "
                  "and work stealing enabled", conf->worker_count));
#else
        PJ_LOG(4,(THIS_FILE, "Conference bridge created with %u worker threads",
                  conf->worker_count));
#endif
#endif /* PJMEDIA_CONF_USE_MULTI_THREADING */

    /* Done */
    *p_conf = conf;

    return PJ_SUCCESS;

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
on_error:
    pjmedia_conf_destroy(conf);
    return status;
#endif
}


/*
 * Pause sound device.
 */
static pj_status_t pause_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}

/*
 * Resume sound device.
 */
static pj_status_t resume_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}


/**
 * Destroy conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf )
{
    unsigned i;

    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    pj_log_push_indent();

    /* Destroy sound device port. */
    if (conf->snd_dev_port) {
        pjmedia_snd_port_destroy(conf->snd_dev_port);
        conf->snd_dev_port = NULL;
    }

    /* Flush any pending operation (connect, disconnect, etc) */
    handle_op_queue(conf);

    /* Remove all ports (may destroy them too). */
    for (i=0; i<conf->max_ports; ++i) {
        if (conf->ports[i]) {
            pj_status_t status;
            pjmedia_conf_op_param oprm = {0};
            oprm.remove_port.port = i;
            status = op_remove_port(conf, &oprm);
            if (conf->cb) {
                pjmedia_conf_op_info op_info = { 0 };

                pj_log_push_indent();
                op_info.op_type = PJMEDIA_CONF_OP_REMOVE_PORT;
                op_info.status = status;
                op_info.op_param = oprm;

                (*conf->cb)(&op_info);
                pj_log_pop_indent();
            }
        }
    }

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
        /* Stop all worker threads */
        for (i = 0; i < conf->worker_count; i++) {
            conf->workers[i].active = PJ_FALSE;
            pj_sem_post(conf->worker_sems[i]);
        }

        /* Wait for workers to exit */
        for (i = 0; i < conf->worker_count; i++) {
            if (conf->workers[i].thread) {
                pj_thread_join(conf->workers[i].thread);
                pj_thread_destroy(conf->workers[i].thread);
            }
        }

        /* Cleanup synchronization objects */
        if (conf->phase_barrier)
            pj_barrier_destroy(conf->phase_barrier);

        if (conf->rebalance_lock)
            pj_lock_destroy(conf->rebalance_lock);

        /* Destroy semaphores */
        for (i = 0; i < conf->worker_count; i++) {
            if (conf->worker_sems && conf->worker_sems[i])
                pj_sem_destroy(conf->worker_sems[i]);
        }

#endif

    if (conf->cb) {
        conf->cb = NULL;
    }

    /* Destroy mutex */
    if (conf->mutex)
        pj_mutex_destroy(conf->mutex);

    /* Destroy pool */
    if (conf->pool)
        pj_pool_safe_release(&conf->pool);

    pj_log_pop_indent();

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_conf_set_op_cb(pjmedia_conf *conf,
                                           pjmedia_conf_op_cb cb)
{
    PJ_ASSERT_RETURN(conf, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);
    conf->cb = cb;
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Destroy the master port (will destroy the conference)
 */
static pj_status_t destroy_port(pjmedia_port *this_port)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    return pjmedia_conf_destroy(conf);
}

#if !DEPRECATED_FOR_TICKET_2234
static pj_status_t destroy_port_pasv(pjmedia_port *this_port) {
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    struct conf_port *port = conf->ports[this_port->port_data.ldata];
    pj_status_t status;

    status = pjmedia_delay_buf_destroy(port->delay_buf);
    if (status == PJ_SUCCESS)
        port->delay_buf = NULL;

    return status;
}
#endif

/*
 * Get port zero interface.
 */
PJ_DEF(pjmedia_port*) pjmedia_conf_get_master_port(pjmedia_conf *conf)
{
    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL, NULL);

    /* Can only return port interface when PJMEDIA_CONF_NO_DEVICE was
     * present in the option.
     */
    PJ_ASSERT_RETURN((conf->options & PJMEDIA_CONF_NO_DEVICE) != 0, NULL);

    return conf->master_port;
}


/*
 * Set master port name.
 */
PJ_DEF(pj_status_t) pjmedia_conf_set_port0_name(pjmedia_conf *conf,
                                                const pj_str_t *name)
{
    pj_size_t len;

    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL && name != NULL, PJ_EINVAL);

    len = name->slen;
    if (len > sizeof(conf->master_name_buf))
        len = sizeof(conf->master_name_buf);

    if (len > 0) pj_memcpy(conf->master_name_buf, name->ptr, len);

    conf->ports[0]->name.ptr = conf->master_name_buf;
    conf->ports[0]->name.slen = len;

    if (conf->master_port)
        conf->master_port->info.name = conf->ports[0]->name;

    return PJ_SUCCESS;
}

/*
 * Add stream port to the conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
                                           pj_pool_t *pool,
                                           pjmedia_port *strm_port,
                                           const pj_str_t *port_name,
                                           unsigned *p_port )
{
    struct conf_port *conf_port;
    unsigned index;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(conf && pool && strm_port, PJ_EINVAL);

    pj_log_push_indent();

    /* If port_name is not specified, use the port's name */
    if (!port_name)
        port_name = &strm_port->info.name;

    /* For this version of PJMEDIA, channel(s) number MUST be:
     * - same between port & conference bridge.
     * - monochannel on port or conference bridge.
     */
    if (PJMEDIA_PIA_CCNT(&strm_port->info) != conf->channel_count &&
        (PJMEDIA_PIA_CCNT(&strm_port->info) != 1 &&
         conf->channel_count != 1))
    {
        pj_assert(!"Number of channels mismatch");
        status = PJMEDIA_ENCCHANNEL;
        goto on_return;
    }

    pj_mutex_lock(conf->mutex);

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
        if (conf->ports[index] == NULL)
            break;
    }

    if (index == conf->max_ports) {
        PJ_PERROR(3,(THIS_FILE, PJ_ETOOMANY, "Add port %s failed",
                     port_name->ptr));
        status = PJ_ETOOMANY;
        goto on_return;
    }

    /* Create conf port structure. */
    status = create_conf_port(pool, conf, strm_port, port_name, &conf_port);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Audio data flow is not protected, avoid processing this newly
     * added port.
     */
    conf_port->is_new = PJ_TRUE;

    /* Put the port, but don't add port counter yet */
    conf->ports[index] = conf_port;
    //conf->port_cnt++;

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
#if defined(PJMEDIA_CONF_USE_NUMA) && PJMEDIA_CONF_USE_NUMA != 0 && defined(HAS_NUMA_SUPPORT) && HAS_NUMA_SUPPORT != 0
    if (conf->cpu_to_numa && numa_available()) {
        int num_nodes = numa_max_node() + 1;
        if (num_nodes > 1) {
            int numa_node = index % num_nodes;
            conf_port->preferred_cpu = numa_node * (conf->total_cpus / num_nodes);

            PJ_LOG(5,(THIS_FILE, "Port %d assigned NUMA node %d preference",
                      index, numa_node));
        }
    }
#endif
#endif

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_ADD_PORT;
        ope->param.add_port.port = index;
        pj_list_push_back(conf->op_queue, ope);
        PJ_LOG(4,(THIS_FILE, "Add port %d (%.*s) queued",
                             index, (int)port_name->slen, port_name->ptr));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* Done. */
    if (p_port) {
        *p_port = index;
    }

on_return:
    pj_mutex_unlock(conf->mutex);
    pj_log_pop_indent();

    return status;
}

static pj_status_t op_add_port(pjmedia_conf *conf,
                               const pjmedia_conf_op_param *prm)
{
    unsigned port = prm->add_port.port;
    struct conf_port *cport = conf->ports[port];

    /* Port must be valid and flagged as new. */
    if (!cport || !cport->is_new)
        return PJ_EINVAL;

    /* Activate newly added port */
    cport->is_new = PJ_FALSE;
    ++conf->port_cnt;

    PJ_LOG(4,(THIS_FILE, "Added port %d (%.*s), port count=%d",
              port, (int)cport->name.slen, cport->name.ptr, conf->port_cnt));

    return PJ_SUCCESS;
}


#if !DEPRECATED_FOR_TICKET_2234
/*
 * Add passive port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_passive_port( pjmedia_conf *conf,
                                                   pj_pool_t *pool,
                                                   const pj_str_t *name,
                                                   unsigned clock_rate,
                                                   unsigned channel_count,
                                                   unsigned samples_per_frame,
                                                   unsigned bits_per_sample,
                                                   unsigned options,
                                                   unsigned *p_slot,
                                                   pjmedia_port **p_port )
{
    struct conf_port *conf_port;
    pjmedia_port *port;
    unsigned index;
    pj_str_t tmp;
    pj_status_t status;

    PJ_LOG(1, (THIS_FILE, "This API has been deprecated since 1.3 and will "
                          "be removed in the future release!"));

    PJ_ASSERT_RETURN(conf && pool, PJ_EINVAL);

    /* For this version of PJMEDIA, channel(s) number MUST be:
     * - same between port & conference bridge.
     * - monochannel on port or conference bridge.
     */
    if (channel_count != conf->channel_count &&
        (channel_count != 1 && conf->channel_count != 1))
    {
        pj_assert(!"Number of channels mismatch");
        return PJMEDIA_ENCCHANNEL;
    }

    /* For this version, options must be zero */
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);
    PJ_UNUSED_ARG(options);

    pj_mutex_lock(conf->mutex);

    if (conf->port_cnt >= conf->max_ports) {
        pj_assert(!"Too many ports");
        pj_mutex_unlock(conf->mutex);
        return PJ_ETOOMANY;
    }

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
        if (conf->ports[index] == NULL)
            break;
    }

    pj_assert(index != conf->max_ports);

    if (name == NULL) {
        name = &tmp;

        tmp.ptr = (char*) pj_pool_alloc(pool, 32);
        tmp.slen = pj_ansi_snprintf(tmp.ptr, 32, "ConfPort#%d", index);
    }

    /* Create and initialize the media port structure. */
    port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(port, PJ_ENOMEM);

    pjmedia_port_info_init(&port->info, name, SIGNATURE_PORT,
                           clock_rate, channel_count, bits_per_sample,
                           samples_per_frame);

    port->port_data.pdata = conf;
    port->port_data.ldata = index;

    port->get_frame = &get_frame_pasv;
    port->put_frame = &put_frame;
    port->on_destroy = &destroy_port_pasv;


    /* Create conf port structure. */
    status = create_pasv_port(conf, pool, name, port, &conf_port);
    if (status != PJ_SUCCESS) {
        pj_mutex_unlock(conf->mutex);
        return status;
    }


    /* Put the port. */
    conf->ports[index] = conf_port;
    conf->port_cnt++;

    /* Done. */
    if (p_slot)
        *p_slot = index;
    if (p_port)
        *p_port = port;

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}
#endif /* DEPRECATED_FOR_TICKET_2234 */



/*
 * Change TX and RX settings for the port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_configure_port( pjmedia_conf *conf,
                                                  unsigned slot,
                                                  pjmedia_port_op tx,
                                                  pjmedia_port_op rx)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    conf_port = conf->ports[slot];

    if (tx != PJMEDIA_PORT_NO_CHANGE)
        conf_port->tx_setting = tx;

    if (rx != PJMEDIA_PORT_NO_CHANGE)
        conf_port->rx_setting = rx;

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Connect port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_connect_port( pjmedia_conf *conf,
                                               unsigned src_slot,
                                               unsigned sink_slot,
                                               int adj_level )
{
    struct conf_port *src_port, *dst_port;
    pj_bool_t start_sound = PJ_FALSE;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports &&
                     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Connect ports %d->%d requested",
                         src_slot, sink_slot));

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_CONNECT_PORTS;
        ope->param.connect_ports.src = src_slot;
        ope->param.connect_ports.sink = sink_slot;
        ope->param.connect_ports.adj_level = adj_level;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Connect ports %d->%d queued",
                             src_slot, sink_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

    /* This is first connection, start clock */
    if (conf->connect_cnt == 0)
        start_sound = 1;

on_return:
    pj_mutex_unlock(conf->mutex);

    /* Sound device must be started without mutex, otherwise the
     * sound thread will deadlock (?)
     */
    if (start_sound)
        resume_sound(conf);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Connect ports %d->%d failed",
                     src_slot, sink_slot));
    }

    pj_log_pop_indent();

    return status;
}

static pj_status_t op_connect_ports(pjmedia_conf *conf,
                                    const pjmedia_conf_op_param *prm)
{
    unsigned src_slot, sink_slot;
    struct conf_port *src_port, *dst_port;
    unsigned i;

    /* Ports must be valid. */
    src_slot = prm->connect_ports.src;
    sink_slot = prm->connect_ports.sink;
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];

    if (!src_port || !dst_port) {
        PJ_PERROR(3,(THIS_FILE, PJ_EINVAL,
                     "Failed connecting %d->%d, make sure ports are valid",
                     src_slot, sink_slot));
        return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d already exists",
                      src_slot, sink_slot));
            return PJ_EEXISTS;
        }
    }

    src_port->listener_slots[src_port->listener_cnt] = sink_slot;

    /* Set normalized adjustment level. */
    src_port->listener_adj_level[src_port->listener_cnt] =
                                prm->connect_ports.adj_level + NORMAL_LEVEL;

    ++conf->connect_cnt;
    ++src_port->listener_cnt;
    ++dst_port->transmitter_cnt;

    PJ_LOG(4,(THIS_FILE,"Port %d (%.*s) transmitting to port %d (%.*s)",
              src_slot,
              (int)src_port->name.slen,
              src_port->name.ptr,
              sink_slot,
              (int)dst_port->name.slen,
              dst_port->name.ptr));

    return PJ_SUCCESS;
}

/*
 * Disconnect port
 */
PJ_DEF(pj_status_t) pjmedia_conf_disconnect_port( pjmedia_conf *conf,
                                                  unsigned src_slot,
                                                  unsigned sink_slot )
{
    struct conf_port *src_port, *dst_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports &&
                     sink_slot<conf->max_ports, PJ_EINVAL);

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Disconnect ports %d->%d requested",
                         src_slot, sink_slot));

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = src_slot;
        ope->param.disconnect_ports.sink = sink_slot;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Disconnect ports %d->%d queued",
                             src_slot, sink_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Disconnect ports %d->%d failed",
                     src_slot, sink_slot));
    }

    pj_log_pop_indent();

    return status;
}

static pj_status_t op_disconnect_ports(pjmedia_conf *conf,
                                       const pjmedia_conf_op_param *prm)
{
    unsigned src_slot, sink_slot;
    struct conf_port *src_port = NULL, *dst_port = NULL;
    int i;

    /* Ports must be valid. */
    src_slot = prm->disconnect_ports.src;
    sink_slot = prm->disconnect_ports.sink;

    if (src_slot != INVALID_SLOT)
        src_port = conf->ports[src_slot];
    if (sink_slot != INVALID_SLOT)
        dst_port = conf->ports[sink_slot];

    /* Disconnect source -> sink */
    if (src_port && dst_port) {
        /* Check if connection has been made */
        for (i=0; i<(int)src_port->listener_cnt; ++i) {
            if (src_port->listener_slots[i] == sink_slot)
                break;
        }
        if (i == (int)src_port->listener_cnt) {
            PJ_LOG(3,(THIS_FILE, "Ports connection %d->%d does not exist",
                      src_slot, sink_slot));
            return PJ_EINVAL;
        }

        pj_assert(src_port->listener_cnt > 0 &&
                  src_port->listener_cnt < conf->max_ports);
        pj_assert(dst_port->transmitter_cnt > 0 &&
                  dst_port->transmitter_cnt < conf->max_ports);
        pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE),
                       src_port->listener_cnt, i);
        pj_array_erase(src_port->listener_adj_level, sizeof(unsigned),
                       src_port->listener_cnt, i);
        --conf->connect_cnt;
        --src_port->listener_cnt;
        --dst_port->transmitter_cnt;

        PJ_LOG(4,(THIS_FILE,
                  "Port %d (%.*s) stop transmitting to port %d (%.*s)",
                  src_slot,
                  (int)src_port->name.slen,
                  src_port->name.ptr,
                  sink_slot,
                  (int)dst_port->name.slen,
                  dst_port->name.ptr));

        /* if source port is passive port and has no listener,
         * reset delaybuf.
         */
        if (src_port->delay_buf && src_port->listener_cnt == 0)
            pjmedia_delay_buf_reset(src_port->delay_buf);

    /* Disconnect multiple conn: any -> sink */
    } else if (dst_port) {
        PJ_LOG(4,(THIS_FILE,
                  "Stop any transmission to port %d (%.*s)",
                  sink_slot,
                  (int)dst_port->name.slen,
                  dst_port->name.ptr));

        for (i=0; i<(int)conf->max_ports; ++i) {
            int j;

            src_port = conf->ports[i];
            if (!src_port || src_port->listener_cnt == 0)
                continue;

            /* We need to iterate backwards since the listener count
             * can potentially decrease.
             */
            for (j=src_port->listener_cnt-1; j>=0; --j) {
                if (src_port->listener_slots[j] == sink_slot) {
                    pj_status_t status;
                    pjmedia_conf_op_param op_prm = {0};

                    op_prm.disconnect_ports.src = i;
                    op_prm.disconnect_ports.sink = sink_slot;
                    status = op_disconnect_ports(conf, &op_prm);
                    if (status != PJ_SUCCESS) {
                        PJ_PERROR(4, (THIS_FILE, status,
                                      "Fail to stop transmission from port "
                                      "%d to port %d",
                                      i, sink_slot));
                    }
                    break;
                }
            }
        }

    /* Disconnect multiple conn: source -> any */
    } else if (src_port) {
        PJ_LOG(4,(THIS_FILE,
                  "Stop any transmission from port %d (%.*s)",
                  src_slot,
                  (int)src_port->name.slen,
                  src_port->name.ptr));

        /* We need to iterate backwards since the listener count
         * will keep decreasing.
         */
        for (i=src_port->listener_cnt-1; i>=0; --i) {
            pj_status_t status;
            pjmedia_conf_op_param op_prm = {0};

            op_prm.disconnect_ports.src = src_slot;
            op_prm.disconnect_ports.sink = src_port->listener_slots[i];
            status = op_disconnect_ports(conf, &op_prm);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4, (THIS_FILE, status,
                              "Fail to stop transmission from port "
                              "%d to port %d",
                              src_slot, src_port->listener_slots[i]));
            }
        }

    /* Invalid ports */
    } else {
        PJ_ASSERT_RETURN(!"Invalid ports specified in conf disconnect",
                         PJ_EINVAL);
    }

    /* Pause sound dev when there is no connection, the pause should be done
     * outside mutex to avoid possible deadlock.
     * Note that currently this is done with mutex, it is safe because
     * pause_sound() is a no-op (just maintaining old code).
     */
    if (conf->connect_cnt == 0) {
        pause_sound(conf);
    }
    return PJ_SUCCESS;
}

/*
 * Disconnect port from all sources
 */
PJ_DEF(pj_status_t)
pjmedia_conf_disconnect_port_from_sources( pjmedia_conf *conf,
                                           unsigned sink_slot)
{
    struct conf_port *dst_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && sink_slot<conf->max_ports, PJ_EINVAL);

    pj_log_push_indent();
    PJ_LOG(5,(THIS_FILE, "Disconnect ports any->%d requested",
                         sink_slot));

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    dst_port = conf->ports[sink_slot];
    if (!dst_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = INVALID_SLOT;
        ope->param.disconnect_ports.sink = sink_slot;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Disconnect ports any->%d queued", sink_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Disconnect ports any->%d failed",
                     sink_slot));
    }

    pj_log_pop_indent();

    return status;
}


/*
 * Disconnect port from all sinks
 */
PJ_DEF(pj_status_t)
pjmedia_conf_disconnect_port_from_sinks( pjmedia_conf *conf,
                                         unsigned src_slot)
{
    struct conf_port *src_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports, PJ_EINVAL);

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Disconnect ports %d->any requested",
                         src_slot));

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    src_port = conf->ports[src_slot];
    if (!src_port) {
        status = PJ_EINVAL;
        goto on_return;
    }

    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_DISCONNECT_PORTS;
        ope->param.disconnect_ports.src = src_slot;
        ope->param.disconnect_ports.sink = INVALID_SLOT;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Disconnect ports %d->any queued", src_slot));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Disconnect ports %d->any failed",
                     src_slot));
    }

    pj_log_pop_indent();

    return status;
}


/*
 * Get number of ports currently registered to the conference bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_port_count(pjmedia_conf *conf)
{
    return conf->port_cnt;
}

/*
 * Get total number of ports connections currently set up in the bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_connect_count(pjmedia_conf *conf)
{
    return conf->connect_cnt;
}


/*
 * Remove the specified port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
                                              unsigned port )
{
    struct conf_port *conf_port;
    op_entry *ope;
    pj_status_t status = PJ_SUCCESS;

    pj_log_push_indent();

    PJ_LOG(5,(THIS_FILE, "Remove port %d requested", port));

    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[port];
    if (conf_port == NULL) {
        status = PJ_EINVAL;
        goto on_return;
    }

    /* If port is new, remove it synchronously */
    if (conf_port->is_new) {
        pj_bool_t found = PJ_FALSE;

        /* Find & cancel the add-op.
         * Also cancel all following ops involving the slot.
         * Note that after removed, the slot may be reused by another port
         * so if not cancelled, those following ops may be applied to the
         * wrong port.
         */
        ope = conf->op_queue->next;
        while (ope != conf->op_queue) {
            op_entry* cancel_op;

            cancel_op = NULL;
            if (ope->type == PJMEDIA_CONF_OP_ADD_PORT &&
                ope->param.add_port.port == port)
            {
                found = PJ_TRUE;
                cancel_op = ope;
            } else if (found && ope->type == PJMEDIA_CONF_OP_CONNECT_PORTS &&
                       (ope->param.connect_ports.src == port ||
                        ope->param.connect_ports.sink == port))
            {
                cancel_op = ope;
            } else if (found && ope->type == PJMEDIA_CONF_OP_DISCONNECT_PORTS &&
                       (ope->param.disconnect_ports.src == port ||
                        ope->param.disconnect_ports.sink == port))
            {
                cancel_op = ope;
            }

            ope = ope->next;

            /* Cancel op */
            if (cancel_op) {
                pjmedia_conf_op_info op_info = { 0 };

                op_info.op_type = cancel_op->type;
                op_info.op_param = cancel_op->param;

                pj_list_erase(cancel_op);
                cancel_op->type = PJMEDIA_CONF_OP_UNKNOWN;
                pj_list_push_back(conf->op_queue_free, cancel_op);

                if (conf->cb) {
                    pj_log_push_indent();
                    op_info.status = PJ_ECANCELLED;
                    (*conf->cb)(&op_info);
                    pj_log_pop_indent();
                }
            }
        }

        /* If the add-op is not found, it may be being executed,
         * do not remove it synchronously to avoid race condition.
         */
        if (found) {
            pjmedia_conf_op_param prm;

            /* Release mutex to avoid deadlock */
            pj_mutex_unlock(conf->mutex);

            /* Remove it */
            prm.remove_port.port = port;
            status = op_remove_port(conf, &prm);

            if (conf->cb) {
                pjmedia_conf_op_info op_info = { 0 };

                pj_log_push_indent();
                op_info.op_type = PJMEDIA_CONF_OP_REMOVE_PORT;
                op_info.status = status;
                op_info.op_param = prm;

                (*conf->cb)(&op_info);
                pj_log_pop_indent();
            }

            pj_log_pop_indent();
            return PJ_SUCCESS;
        }
    }

    /* Queue the operation */
    ope = get_free_op_entry(conf);
    if (ope) {
        ope->type = PJMEDIA_CONF_OP_REMOVE_PORT;
        ope->param.remove_port.port = port;
        pj_list_push_back(conf->op_queue, ope);

        PJ_LOG(4,(THIS_FILE, "Remove port %d queued", port));
    } else {
        status = PJ_ENOMEM;
        goto on_return;
    }

on_return:
    pj_mutex_unlock(conf->mutex);

    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(THIS_FILE, status, "Remove port %d failed", port));
    }

    pj_log_pop_indent();

    return status;
}


static pj_status_t op_remove_port(pjmedia_conf *conf,
                                  const pjmedia_conf_op_param *prm)
{
    unsigned port = prm->remove_port.port;
    struct conf_port *conf_port;
    pjmedia_conf_op_param op_prm;
    pj_status_t status;

    /* Port must be valid. */
    conf_port = conf->ports[port];
    if (conf_port == NULL) {
        PJ_PERROR(3, (THIS_FILE, PJ_ENOTFOUND, "Remove port failed"));
        return PJ_EINVAL;
    }

    conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;

    /* disconnect port from all sources which are transmitting to it */
    pj_bzero(&op_prm, sizeof(op_prm));
    op_prm.disconnect_ports.src = INVALID_SLOT;
    op_prm.disconnect_ports.sink = port;
    status = op_disconnect_ports(conf, &op_prm);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4, (THIS_FILE, status,
                      "Fail to stop transmission from any->%d", port));
    }

    /* disconnect port from all sinks to which it is transmitting to */
    pj_bzero(&op_prm, sizeof(op_prm));
    op_prm.disconnect_ports.src = port;
    op_prm.disconnect_ports.sink = INVALID_SLOT;
    status = op_disconnect_ports(conf, &op_prm);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4, (THIS_FILE, status,
                      "Fail to stop transmission from %d->any", port));
    }

    /* Destroy resample if this conf port has it. */
    if (conf_port->rx_resample) {
        pjmedia_resample_destroy(conf_port->rx_resample);
        conf_port->rx_resample = NULL;
    }
    if (conf_port->tx_resample) {
        pjmedia_resample_destroy(conf_port->tx_resample);
        conf_port->tx_resample = NULL;
    }

    /* Destroy pjmedia port if this conf port is passive port,
     * i.e: has delay buf.
     */
    if (conf_port->delay_buf) {
        pjmedia_delay_buf_destroy(conf_port->delay_buf);
        conf_port->delay_buf = NULL;

        if (conf_port->port)
            pjmedia_port_destroy(conf_port->port);
        conf_port->port = NULL;
    }

    /* Remove the port. */
    pj_mutex_lock(conf->mutex);
    conf->ports[port] = NULL;
    pj_mutex_unlock(conf->mutex);

    if (!conf_port->is_new)
        --conf->port_cnt;

    PJ_LOG(4,(THIS_FILE,"Removed port %d (%.*s), port count=%d",
              port, (int)conf_port->name.slen, conf_port->name.ptr,
              conf->port_cnt));

    /* Decrease conf port ref count */
    if (conf_port->port && conf_port->port->grp_lock)
        pj_grp_lock_dec_ref(conf_port->port->grp_lock);
    else
        conf_port_on_destroy(conf_port);

    return PJ_SUCCESS;
}


/*
 * Enum ports.
 */
PJ_DEF(pj_status_t) pjmedia_conf_enum_ports( pjmedia_conf *conf,
                                             unsigned ports[],
                                             unsigned *p_count )
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && p_count && ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    for (i=0; i<conf->max_ports && count<*p_count; ++i) {
        if (!conf->ports[i])
            continue;

        ports[count++] = i;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    *p_count = count;
    return PJ_SUCCESS;
}

/*
 * Get port info
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_port_info( pjmedia_conf *conf,
                                                unsigned slot,
                                                pjmedia_conf_port_info *info)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    info->slot = slot;
    info->name = conf_port->name;
    if (conf_port->port) {
        pjmedia_format_copy(&info->format, &conf_port->port->info.fmt);
    } else {
        pj_bzero(&info->format, sizeof(info->format));
        info->format.id = (pj_uint32_t)PJMEDIA_FORMAT_INVALID;
    }
    info->tx_setting = conf_port->tx_setting;
    info->rx_setting = conf_port->rx_setting;
    info->listener_cnt = conf_port->listener_cnt;
    info->listener_slots = conf_port->listener_slots;
    info->transmitter_cnt = conf_port->transmitter_cnt;
    info->clock_rate = conf_port->clock_rate;
    info->channel_count = conf_port->channel_count;
    info->samples_per_frame = conf_port->samples_per_frame;
    info->bits_per_sample = conf->bits_per_sample;
    info->tx_adj_level = conf_port->tx_adj_level - NORMAL_LEVEL;
    info->rx_adj_level = conf_port->rx_adj_level - NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_conf_get_ports_info(pjmedia_conf *conf,
                                                unsigned *size,
                                                pjmedia_conf_port_info info[])
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && size && info, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    for (i=0; i<conf->max_ports && count<*size; ++i) {
        if (!conf->ports[i])
            continue;

        pjmedia_conf_get_port_info(conf, i, &info[count]);
        ++count;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    *size = count;
    return PJ_SUCCESS;
}


/*
 * Get signal level.
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_signal_level( pjmedia_conf *conf,
                                                   unsigned slot,
                                                   unsigned *tx_level,
                                                   unsigned *rx_level)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    if (tx_level != NULL) {
        *tx_level = conf_port->tx_level;
    }

    if (rx_level != NULL)
        *rx_level = conf_port->rx_level;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Adjust RX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_rx_level( pjmedia_conf *conf,
                                                  unsigned slot,
                                                  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    /* Set normalized adjustment level. */
    conf_port->rx_adj_level = adj_level + NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Adjust TX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_tx_level( pjmedia_conf *conf,
                                                  unsigned slot,
                                                  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127,, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    /* Set normalized adjustment level. */
    conf_port->tx_adj_level = adj_level + NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Adjust level of individual connection.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_conn_level( pjmedia_conf *conf,
                                                    unsigned src_slot,
                                                    unsigned sink_slot,
                                                    int adj_level )
{
    struct conf_port *src_port, *dst_port;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports &&
                     sink_slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
        if (src_port->listener_slots[i] == sink_slot)
            break;
    }

    if (i == src_port->listener_cnt) {
        /* connection hasn't been made */
        pj_mutex_unlock(conf->mutex);
        return PJ_EINVAL;
    }
    /* Set normalized adjustment level. */
    src_port->listener_adj_level[i] = adj_level + NORMAL_LEVEL;

    pj_mutex_unlock(conf->mutex);
    return PJ_SUCCESS;
}


/*
 * Read from port.
 */
static pj_status_t read_port( pjmedia_conf *conf,
                              struct conf_port *cport, pj_int16_t *frame,
                              pj_size_t count, pjmedia_frame_type *type )
{

    pj_assert(count == conf->samples_per_frame);

    TRACE_((THIS_FILE, "read_port %.*s: count=%d",
                       (int)cport->name.slen, cport->name.ptr,
                       count));

    /*
     * If port's samples per frame and sampling rate and channel count
     * matche conference bridge's settings, get the frame directly from
     * the port.
     */
    if (cport->rx_buf_cap == 0) {
        pjmedia_frame f;
        pj_status_t status;

        f.buf = frame;
        f.size = count * BYTES_PER_SAMPLE;

        TRACE_((THIS_FILE, "  get_frame %.*s: count=%d",
                   (int)cport->name.slen, cport->name.ptr,
                   count));

        status = pjmedia_port_get_frame(cport->port, &f);

        *type = f.type;

        return status;

    } else {
        unsigned samples_req;

        /* Initialize frame type */
        if (cport->rx_buf_count == 0) {
            *type = PJMEDIA_FRAME_TYPE_NONE;
        } else {
            /* we got some samples in the buffer */
            *type = PJMEDIA_FRAME_TYPE_AUDIO;
        }

        /*
         * If we don't have enough samples in rx_buf, read from the port
         * first. Remember that rx_buf may be in different clock rate and
         * channel count...
         */

        samples_req = (unsigned) (count * 1.0 *
                      cport->clock_rate / conf->clock_rate + 0.5);

        while (cport->rx_buf_count < samples_req) {

            pjmedia_frame f;
            pj_status_t status;

            f.buf = cport->rx_buf + cport->rx_buf_count;
            f.size = cport->samples_per_frame * BYTES_PER_SAMPLE;

            TRACE_((THIS_FILE, "  get_frame, count=%d",
                       cport->samples_per_frame));

            status = pjmedia_port_get_frame(cport->port, &f);

            if (status != PJ_SUCCESS) {
                /* Fatal error! */
                return status;
            }

            if (f.type != PJMEDIA_FRAME_TYPE_AUDIO) {
                TRACE_((THIS_FILE, "  get_frame returned non-audio"));
                pjmedia_zero_samples( cport->rx_buf + cport->rx_buf_count,
                                      cport->samples_per_frame);
            } else {
                /* We've got at least one frame */
                *type = PJMEDIA_FRAME_TYPE_AUDIO;
            }

            /* Adjust channels */
            if (cport->channel_count != conf->channel_count) {
                if (cport->channel_count == 1) {
                    pjmedia_convert_channel_1ton((pj_int16_t*)f.buf,
                                                 (const pj_int16_t*)f.buf,
                                                 conf->channel_count,
                                                 cport->samples_per_frame,
                                                 0);
                    cport->rx_buf_count += (cport->samples_per_frame *
                                            conf->channel_count);
                } else { /* conf->channel_count == 1 */
                    pjmedia_convert_channel_nto1((pj_int16_t*)f.buf,
                                                 (const pj_int16_t*)f.buf,
                                                 cport->channel_count,
                                                 cport->samples_per_frame,
                                                 PJMEDIA_STEREO_MIX, 0);
                    cport->rx_buf_count += (cport->samples_per_frame /
                                            cport->channel_count);
                }
            } else {
                cport->rx_buf_count += cport->samples_per_frame;
            }

            TRACE_((THIS_FILE, "  rx buffer size is now %d",
                    cport->rx_buf_count));

            pj_assert(cport->rx_buf_count <= cport->rx_buf_cap);
        }

        /*
         * If port's clock_rate is different, resample.
         * Otherwise just copy.
         */
        if (cport->clock_rate != conf->clock_rate) {

            unsigned src_count;

            TRACE_((THIS_FILE, "  resample, input count=%d",
                    pjmedia_resample_get_input_size(cport->rx_resample)));

            pjmedia_resample_run( cport->rx_resample,cport->rx_buf, frame);

            src_count = (unsigned)(count * 1.0 * cport->clock_rate /
                                   conf->clock_rate + 0.5);
            cport->rx_buf_count -= src_count;
            if (cport->rx_buf_count) {
                pjmedia_move_samples(cport->rx_buf, cport->rx_buf+src_count,
                                     cport->rx_buf_count);
            }

            TRACE_((THIS_FILE, "  rx buffer size is now %d",
                    cport->rx_buf_count));

        } else {

            pjmedia_copy_samples(frame, cport->rx_buf, (unsigned)count);
            cport->rx_buf_count -= (unsigned)count;
            if (cport->rx_buf_count) {
                pjmedia_move_samples(cport->rx_buf, cport->rx_buf+count,
                                     cport->rx_buf_count);
            }
        }
    }

    return PJ_SUCCESS;
}

/*
 * Write the mixed signal to the port.
 */
static pj_status_t write_port(pjmedia_conf *conf, struct conf_port *cport,
                              const pj_timestamp *timestamp,
                              pjmedia_frame_type *frm_type)
{
    pj_int16_t *buf;
    unsigned j, ts;
    pj_status_t status;
    pj_int32_t adj_level;
    pj_int32_t tx_level;
    unsigned dst_count;

    *frm_type = PJMEDIA_FRAME_TYPE_AUDIO;

    /* Skip port if it is disabled */
    if (cport->tx_setting == PJMEDIA_PORT_DISABLE) {
        cport->tx_level = 0;
        *frm_type = PJMEDIA_FRAME_TYPE_NONE;
        return PJ_SUCCESS;
    }
    /* If port is muted or nobody is transmitting to this port,
     * transmit NULL frame.
     */
    else if ((cport->tx_setting == PJMEDIA_PORT_MUTE) ||
             (cport->transmitter_cnt == 0)) {
        pjmedia_frame frame;

        /* Clear left-over samples in tx_buffer, if any, so that it won't
         * be transmitted next time we have audio signal.
         */
        cport->tx_buf_count = 0;

        /* Add sample counts to heart-beat samples */
        cport->tx_heart_beat += conf->samples_per_frame * cport->clock_rate /
                                conf->clock_rate *
                                cport->channel_count / conf->channel_count;

        /* Set frame timestamp */
        frame.timestamp.u64 = timestamp->u64 * cport->clock_rate /
                                conf->clock_rate;
        frame.type = PJMEDIA_FRAME_TYPE_NONE;
        frame.buf = NULL;
        frame.size = 0;

        /* Transmit heart-beat frames (may transmit more than one NULL frame
         * if port's ptime is less than bridge's ptime.
         */
        if (cport->port && cport->port->put_frame) {
            while (cport->tx_heart_beat >= cport->samples_per_frame) {

                pjmedia_port_put_frame(cport->port, &frame);

                cport->tx_heart_beat -= cport->samples_per_frame;
                frame.timestamp.u64 += cport->samples_per_frame;
            }
        }

        cport->tx_level = 0;
        *frm_type = PJMEDIA_FRAME_TYPE_NONE;
        return PJ_SUCCESS;
    }

    /* Reset heart-beat sample count */
    cport->tx_heart_beat = 0;

    buf = (pj_int16_t*) cport->mix_buf;

    /* If there are sources in the mix buffer, convert the mixed samples
     * from 32bit to 16bit in the mixed samples itself. This is possible
     * because mixed sample is 32bit.
     *
     * In addition to this process, if we need to change the level of
     * TX signal, we adjust is here too.
     */

    /* Calculate signal level and adjust the signal when needed.
     * Two adjustments performed at once:
     * 1. user setting adjustment (tx_adj_level).
     * 2. automatic adjustment of overflowed mixed buffer (mix_adj).
     */

    /* Apply simple AGC to the mix_adj, the automatic adjust, to avoid
     * dramatic change in the level thus causing noise because the signal
     * is now not aligned with the signal from the previous frame.
     */
    SIMPLE_AGC(cport->last_mix_adj, cport->mix_adj);
    cport->last_mix_adj = cport->mix_adj;

    /* adj_level = cport->tx_adj_level * cport->mix_adj / NORMAL_LEVEL;*/
    adj_level = cport->tx_adj_level * cport->mix_adj;
    adj_level >>= 7;

    tx_level = 0;

    if (adj_level != NORMAL_LEVEL) {
        for (j=0; j<conf->samples_per_frame; ++j) {
            pj_int32_t itemp = cport->mix_buf[j];

            /* Adjust the level */
            /*itemp = itemp * adj_level / NORMAL_LEVEL;*/
            itemp = (itemp * adj_level) >> 7;

            /* Clip the signal if it's too loud */
            if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
            else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

            /* Put back in the buffer. */
            buf[j] = (pj_int16_t) itemp;

            tx_level += (buf[j]>=0? buf[j] : -buf[j]);
        }
    } else {
        for (j=0; j<conf->samples_per_frame; ++j) {
            buf[j] = (pj_int16_t) cport->mix_buf[j];
            tx_level += (buf[j]>=0? buf[j] : -buf[j]);
        }
    }

    tx_level /= conf->samples_per_frame;

    /* Convert level to 8bit complement ulaw */
    tx_level = pjmedia_linear2ulaw(tx_level) ^ 0xff;

    cport->tx_level = tx_level;

    /* If port has the same clock_rate and samples_per_frame and
     * number of channels as the conference bridge, transmit the
     * frame as is.
     */
    if (cport->clock_rate == conf->clock_rate &&
        cport->samples_per_frame == conf->samples_per_frame &&
        cport->channel_count == conf->channel_count)
    {
        if (cport->port != NULL) {
            pjmedia_frame frame;

            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame.buf = buf;
            frame.size = conf->samples_per_frame * BYTES_PER_SAMPLE;
            /* No need to adjust timestamp, port has the same
             * clock rate as conference bridge
             */
            frame.timestamp = *timestamp;

            TRACE_((THIS_FILE, "put_frame %.*s, count=%d",
                               (int)cport->name.slen, cport->name.ptr,
                               frame.size / BYTES_PER_SAMPLE));

            return pjmedia_port_put_frame(cport->port, &frame);
        } else
            return PJ_SUCCESS;
    }

    /* If it has different clock_rate, must resample. */
    if (cport->clock_rate != conf->clock_rate) {
        pjmedia_resample_run( cport->tx_resample, buf,
                              cport->tx_buf + cport->tx_buf_count );
        dst_count = (unsigned)(conf->samples_per_frame * 1.0 *
                               cport->clock_rate / conf->clock_rate + 0.5);
    } else {
        /* Same clock rate.
         * Just copy the samples to tx_buffer.
         */
        pjmedia_copy_samples( cport->tx_buf + cport->tx_buf_count,
                              buf, conf->samples_per_frame );
        dst_count = conf->samples_per_frame;
    }

    /* Adjust channels */
    if (cport->channel_count != conf->channel_count) {
        pj_int16_t *tx_buf = cport->tx_buf + cport->tx_buf_count;
        if (conf->channel_count == 1) {
            pjmedia_convert_channel_1ton(tx_buf, tx_buf,
                                         cport->channel_count,
                                         dst_count, 0);
            dst_count *= cport->channel_count;
        } else { /* cport->channel_count == 1 */
            pjmedia_convert_channel_nto1(tx_buf, tx_buf,
                                         conf->channel_count,
                                         dst_count, PJMEDIA_STEREO_MIX, 0);
            dst_count /= conf->channel_count;
        }
    }

    cport->tx_buf_count += dst_count;

    pj_assert(cport->tx_buf_count <= cport->tx_buf_cap);

    /* Transmit while we have enough frame in the tx_buf. */
    status = PJ_SUCCESS;
    ts = 0;
    while (cport->tx_buf_count >= cport->samples_per_frame &&
           status == PJ_SUCCESS)
    {

        TRACE_((THIS_FILE, "write_port %.*s: count=%d",
                           (int)cport->name.slen, cport->name.ptr,
                           cport->samples_per_frame));

        if (cport->port) {
            pjmedia_frame frame;

            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
            frame.buf = cport->tx_buf;
            frame.size = cport->samples_per_frame * BYTES_PER_SAMPLE;
            /* Adjust timestamp as port may have different clock rate
             * than the bridge.
             */
            frame.timestamp.u64 = timestamp->u64 * cport->clock_rate /
                                  conf->clock_rate;

            /* Add timestamp for individual frame */
            frame.timestamp.u64 += ts;
            ts += cport->samples_per_frame;

            TRACE_((THIS_FILE, "put_frame %.*s, count=%d",
                               (int)cport->name.slen, cport->name.ptr,
                               frame.size / BYTES_PER_SAMPLE));

            status = pjmedia_port_put_frame(cport->port, &frame);

        } else
            status = PJ_SUCCESS;

        cport->tx_buf_count -= cport->samples_per_frame;
        if (cport->tx_buf_count) {
            pjmedia_move_samples(cport->tx_buf,
                                 cport->tx_buf + cport->samples_per_frame,
                                 cport->tx_buf_count);
        }

        TRACE_((THIS_FILE, " tx_buf count now is %d",
                           cport->tx_buf_count));
    }

    return status;
}


/*
 * Player callback.
 */
static pj_status_t get_frame(pjmedia_port *this_port,
                             pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;

#if defined(PJMEDIA_CONF_USE_MULTI_THREADING) && PJMEDIA_CONF_USE_MULTI_THREADING != 0
    return get_frame_mt(this_port, frame);
#endif

    pjmedia_frame_type speaker_frame_type = PJMEDIA_FRAME_TYPE_NONE;
    unsigned ci, cj, i, j;
    pj_int16_t *p_in;

    TRACE_((THIS_FILE, "- clock -"));

    /* Check that correct size is specified. */
    pj_assert(frame->size == conf->samples_per_frame *
                             conf->bits_per_sample / 8);

    /* Perform any queued operations that need to be synchronized with
     * the clock such as connect, disonnect, remove.
     */
    if (!pj_list_empty(conf->op_queue)) {
        pj_log_push_indent();
        handle_op_queue(conf);
        pj_log_pop_indent();
    }

    /* No mutex from this point! Otherwise it may cause deadlock as
     * put_frame()/get_frame() may invoke callback.
     *
     * Note that any changes on the conference connections must be
     * synchronized.
     */

    /* Reset port source count. We will only reset port's mix
     * buffer when we have someone transmitting to it.
     */
    for (i=0, ci=0; i<conf->max_ports && ci < conf->port_cnt; ++i) {
        struct conf_port *conf_port = conf->ports[i];

        /* Skip empty or new port. */
        if (!conf_port || conf_port->is_new)
            continue;

        /* Var "ci" is to count how many ports have been visited so far. */
        ++ci;

        /* Skip if we're not allowed to transmit to this port. */
        if (conf_port->tx_setting != PJMEDIA_PORT_ENABLE)
            continue;

        /* Reset buffer (only necessary if the port has transmitter) and
         * reset auto adjustment level for mixed signal.
         */
        conf_port->mix_adj = NORMAL_LEVEL;
        if (conf_port->transmitter_cnt) {
            pj_bzero(conf_port->mix_buf,
                     conf->samples_per_frame*sizeof(conf_port->mix_buf[0]));
        }
    }

    /* Get frames from all ports, and "mix" the signal
     * to mix_buf of all listeners of the port.
     */
    for (i=0, ci=0; i < conf->max_ports && ci < conf->port_cnt; ++i) {
        struct conf_port *conf_port = conf->ports[i];
        pj_int32_t level = 0;

        /* Skip empty or new port. */
        if (!conf_port || conf_port->is_new)
            continue;

        /* Var "ci" is to count how many ports have been visited so far. */
        ++ci;

        /* Skip if we're not allowed to receive from this port. */
        if (conf_port->rx_setting == PJMEDIA_PORT_DISABLE) {
            conf_port->rx_level = 0;
            continue;
        }

        /* Also skip if this port doesn't have listeners. */
        if (conf_port->listener_cnt == 0) {
            conf_port->rx_level = 0;
            continue;
        }

        /* Get frame from this port.
         * For passive ports, get the frame from the delay_buf.
         * For other ports, get the frame from the port.
         */
        if (conf_port->delay_buf != NULL) {
            pj_status_t status;

            status = pjmedia_delay_buf_get(conf_port->delay_buf,
                                  (pj_int16_t*)frame->buf);
            if (status != PJ_SUCCESS) {
                conf_port->rx_level = 0;
                continue;
            }

        } else {

            pj_status_t status;
            pjmedia_frame_type frame_type;

            status = read_port(conf, conf_port, (pj_int16_t*)frame->buf,
                               conf->samples_per_frame, &frame_type);

            if (status != PJ_SUCCESS) {
                /* bennylp: why do we need this????
                 * Also see comments on similar issue with write_port().
                PJ_LOG(4,(THIS_FILE, "Port %.*s get_frame() returned %d. "
                                     "Port is now disabled",
                                     (int)conf_port->name.slen,
                                     conf_port->name.ptr,
                                     status));
                conf_port->rx_setting = PJMEDIA_PORT_DISABLE;
                 */
                conf_port->rx_level = 0;
                continue;
            }

            /* Check that the port is not removed when we call get_frame() */
            if (conf->ports[i] == NULL) {
                conf_port->rx_level = 0;
                continue;
            }


            /* Ignore if we didn't get any frame */
            if (frame_type != PJMEDIA_FRAME_TYPE_AUDIO) {
                conf_port->rx_level = 0;
                continue;
            }
        }

        p_in = (pj_int16_t*) frame->buf;

        /* Adjust the RX level from this port
         * and calculate the average level at the same time.
         */
        if (conf_port->rx_adj_level != NORMAL_LEVEL) {
            for (j=0; j<conf->samples_per_frame; ++j) {
                /* For the level adjustment, we need to store the sample to
                 * a temporary 32bit integer value to avoid overflowing the
                 * 16bit sample storage.
                 */
                pj_int32_t itemp;

                itemp = p_in[j];
                /*itemp = itemp * adj / NORMAL_LEVEL;*/
                /* bad code (signed/unsigned badness):
                 *  itemp = (itemp * conf_port->rx_adj_level) >> 7;
                 */
                itemp *= conf_port->rx_adj_level;
                itemp >>= 7;

                /* Clip the signal if it's too loud */
                if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                p_in[j] = (pj_int16_t) itemp;
                level += (p_in[j]>=0? p_in[j] : -p_in[j]);
            }
        } else {
            for (j=0; j<conf->samples_per_frame; ++j) {
                level += (p_in[j]>=0? p_in[j] : -p_in[j]);
            }
        }

        level /= conf->samples_per_frame;

        /* Convert level to 8bit complement ulaw */
        level = pjmedia_linear2ulaw(level) ^ 0xff;

        /* Put this level to port's last RX level. */
        conf_port->rx_level = level;

        // Ticket #671: Skipping very low audio signal may cause noise
        // to be generated in the remote end by some hardphones.
        /* Skip processing frame if level is zero */
        //if (level == 0)
        //    continue;

        /* Add the signal to all listeners. */
        for (cj=0; cj < conf_port->listener_cnt; ++cj)
        {
            struct conf_port *listener;
            pj_int32_t *mix_buf;
            pj_int16_t *p_in_conn_leveled;

            listener = conf->ports[conf_port->listener_slots[cj]];

            /* Skip if this listener doesn't want to receive audio */
            if (listener->tx_setting != PJMEDIA_PORT_ENABLE)
                continue;

            mix_buf = listener->mix_buf;

            /* apply connection level, if not normal */
            if (conf_port->listener_adj_level[cj] != NORMAL_LEVEL) {
                unsigned k = 0;
                for (; k < conf->samples_per_frame; ++k) {
                    /* For the level adjustment, we need to store the sample to
                     * a temporary 32bit integer value to avoid overflowing the
                     * 16bit sample storage.
                     */
                    pj_int32_t itemp;

                    itemp = p_in[k];
                    /*itemp = itemp * adj / NORMAL_LEVEL;*/
                    /* bad code (signed/unsigned badness):
                     *  itemp = (itemp * conf_port->listsener_adj_level) >> 7;
                     */
                    itemp *= conf_port->listener_adj_level[cj];
                    itemp >>= 7;

                    /* Clip the signal if it's too loud */
                    if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
                    else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

                    conf_port->adj_level_buf[k] = (pj_int16_t)itemp;
                }

                /* take the leveled frame */
                p_in_conn_leveled = conf_port->adj_level_buf;
            } else {
                /* take the frame as-is */
                p_in_conn_leveled = p_in;
            }

            if (listener->transmitter_cnt > 1) {
                /* Mixing signals,
                 * and calculate appropriate level adjustment if there is
                 * any overflowed level in the mixed signal.
                 */
                unsigned k, samples_per_frame = conf->samples_per_frame;
                pj_int32_t mix_buf_min = 0;
                pj_int32_t mix_buf_max = 0;

                for (k = 0; k < samples_per_frame; ++k) {
                    mix_buf[k] += p_in_conn_leveled[k];
                    if (mix_buf[k] < mix_buf_min)
                        mix_buf_min = mix_buf[k];
                    if (mix_buf[k] > mix_buf_max)
                        mix_buf_max = mix_buf[k];
                }

                /* Check if normalization adjustment needed. */
                if (mix_buf_min < MIN_LEVEL || mix_buf_max > MAX_LEVEL) {
                    int tmp_adj;

                    if (-mix_buf_min > mix_buf_max)
                        mix_buf_max = -mix_buf_min;

                    /* NORMAL_LEVEL * MAX_LEVEL / mix_buf_max; */
                    tmp_adj = (MAX_LEVEL<<7) / mix_buf_max;
                    if (tmp_adj < listener->mix_adj)
                        listener->mix_adj = tmp_adj;
                }
            } else {
                /* Only 1 transmitter:
                 * just copy the samples to the mix buffer
                 * no mixing and level adjustment needed
                 */
                unsigned k, samples_per_frame = conf->samples_per_frame;

                for (k = 0; k < samples_per_frame; ++k) {
                    mix_buf[k] = p_in_conn_leveled[k];
                }
            }
        } /* loop the listeners of conf port */
    } /* loop of all conf ports */

    /* Time for all ports to transmit whetever they have in their
     * buffer.
     */
    for (i=0, ci=0; i<conf->max_ports && ci<conf->port_cnt; ++i) {
        struct conf_port *conf_port = conf->ports[i];
        pjmedia_frame_type frm_type;
        pj_status_t status;

        if (!conf_port || conf_port->is_new)
            continue;

        /* Var "ci" is to count how many ports have been visited. */
        ++ci;

        status = write_port( conf, conf_port, &frame->timestamp,
                             &frm_type);
        if (status != PJ_SUCCESS) {
            /* bennylp: why do we need this????
               One thing for sure, put_frame()/write_port() may return
               non-successfull status on Win32 if there's temporary glitch
               on network interface, so disabling the port here does not
               sound like a good idea.

            PJ_LOG(4,(THIS_FILE, "Port %.*s put_frame() returned %d. "
                                 "Port is now disabled",
                                 (int)conf_port->name.slen,
                                 conf_port->name.ptr,
                                 status));
            conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
            */
            continue;
        }

        /* Set the type of frame to be returned to sound playback
         * device.
         */
        if (i == 0)
            speaker_frame_type = frm_type;
    }

    /* Return sound playback frame. */
    if (conf->ports[0]->tx_level) {
        TRACE_((THIS_FILE, "write to audio, count=%d",
                           conf->samples_per_frame));
        pjmedia_copy_samples( (pj_int16_t*)frame->buf,
                              (const pj_int16_t*)conf->ports[0]->mix_buf,
                              conf->samples_per_frame);
    } else {
        /* Force frame type NONE */
        speaker_frame_type = PJMEDIA_FRAME_TYPE_NONE;
    }

    /* MUST set frame type */
    frame->type = speaker_frame_type;

#ifdef REC_FILE
    if (fhnd_rec == NULL)
        fhnd_rec = fopen(REC_FILE, "wb");
    if (fhnd_rec)
        fwrite(frame->buf, frame->size, 1, fhnd_rec);
#endif

    return PJ_SUCCESS;
}


#if !DEPRECATED_FOR_TICKET_2234
/*
 * get_frame() for passive port
 */
static pj_status_t get_frame_pasv(pjmedia_port *this_port,
                                  pjmedia_frame *frame)
{
    pj_assert(0);
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return -1;
}
#endif


/*
 * Recorder (or passive port) callback.
 */
static pj_status_t put_frame(pjmedia_port *this_port,
                             pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    struct conf_port *port = conf->ports[this_port->port_data.ldata];
    pj_status_t status;

    /* Check for correct size. */
    PJ_ASSERT_RETURN( frame->size == conf->samples_per_frame *
                                     conf->bits_per_sample / 8,
                      PJMEDIA_ENCSAMPLESPFRAME);

    /* Check existance of delay_buf instance */
    PJ_ASSERT_RETURN( port->delay_buf, PJ_EBUG );

    /* Skip if this port is muted/disabled. */
    if (port->rx_setting != PJMEDIA_PORT_ENABLE) {
        return PJ_SUCCESS;
    }

    /* Skip if no port is listening to the microphone */
    if (port->listener_cnt == 0) {
        return PJ_SUCCESS;
    }

    status = pjmedia_delay_buf_put(port->delay_buf, (pj_int16_t*)frame->buf);

    return status;
}


/*
 * Add destructor handler.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_destroy_handler(
                                            pjmedia_conf* conf,
                                            unsigned slot,
                                            void* member,
                                            pj_grp_lock_handler handler)
{
    struct conf_port *cport;
    pj_grp_lock_t *grp_lock;

    PJ_ASSERT_RETURN(conf && handler && slot < conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid and has group lock. */
    cport = conf->ports[slot];
    if (!cport || !cport->port || !cport->port->grp_lock) {
        pj_mutex_unlock(conf->mutex);
        return cport? PJ_EINVALIDOP : PJ_EINVAL;
    }
    grp_lock = cport->port->grp_lock;

    pj_mutex_unlock(conf->mutex);

    return pj_grp_lock_add_handler(grp_lock, NULL, member, handler);
}


/*
 * Remove previously registered destructor handler.
 */
PJ_DEF(pj_status_t) pjmedia_conf_del_destroy_handler(
                                            pjmedia_conf* conf,
                                            unsigned slot,
                                            void* member,
                                            pj_grp_lock_handler handler)
{
    struct conf_port* cport;
    pj_grp_lock_t* grp_lock;

    PJ_ASSERT_RETURN(conf && handler && slot < conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid and has group lock. */
    cport = conf->ports[slot];
    if (!cport || !cport->port || !cport->port->grp_lock) {
        pj_mutex_unlock(conf->mutex);
        return cport ? PJ_EINVALIDOP : PJ_EINVAL;
    }
    grp_lock = cport->port->grp_lock;

    pj_mutex_unlock(conf->mutex);

    return pj_grp_lock_del_handler(grp_lock, member, handler);
}

#endif