/* 
 * Copyright (C) 2022 Teluu Inc. (http://www.teluu.com)
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
#include <pjnath/upnp.h>
#include <pjnath/config.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJNATH_HAS_UPNP) && (PJNATH_HAS_UPNP != 0)

#include <upnp/upnp.h>
#include <upnp/upnptools.h>

#define THIS_FILE       "upnp.c"

#define TRACE_(...) // PJ_LOG(6, (THIS_FILE, ##__VA_ARGS__))

/* Set to 1 to enable UPnP native logging */
#define ENABLE_LOG 0

/* Maximum number of devices. */
#define MAX_DEVS 16

#if ENABLE_LOG
#   include <upnp/upnpdebug.h>
#endif


/* UPnP device descriptions. */
static const char* UPNP_ROOT_DEVICE = "upnp:rootdevice";
static const char* UPNP_IGD_DEVICE =
    "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
static const char* UPNP_WANIP_SERVICE =
    "urn:schemas-upnp-org:service:WANIPConnection:1";
static const char* UPNP_WANPPP_SERVICE =
    "urn:schemas-upnp-org:service:WANPPPConnection:1";


/* Structure for IGD device. */
struct igd
{
    pj_str_t    dev_id;
    pj_str_t    url;
    pj_str_t    service_type;
    pj_str_t    control_url;
    pj_str_t    public_ip;
    pj_sockaddr public_ip_addr;
    
    pj_bool_t   valid;
    pj_bool_t   alive;
};

/* UPnP manager. */
static struct upnp
{
    unsigned            initialized;
    pj_pool_t          *pool;
    pj_thread_desc      thread_desc;
    pj_thread_t        *thread;
    pj_mutex_t         *mutex;
    int                 search_cnt;
    pj_status_t         status;

    unsigned            igd_cnt;
    struct igd          igd_devs[20];
    int                 primary_igd_idx;

    UpnpClient_Handle   client_hnd;
    void                (*upnp_cb)(pj_status_t status);
} upnp_mgr;


/* Get the value of the node. */
static const char * get_node_value(IXML_Node *node)
{
    const char *ret = NULL;
    if (node) {
        IXML_Node* child = ixmlNode_getFirstChild(node);
        if (child)
            ret = ixmlNode_getNodeValue(child);
    }
    return ret;
}

/* Get the value of the first element in the doc with the specified name. */
static const char * doc_get_elmt_value(IXML_Document *doc, const char *name)
{
    const char *ret = NULL;
    IXML_NodeList *node_list = ixmlDocument_getElementsByTagName(doc, name);
    if (node_list) {
        ret = get_node_value(ixmlNodeList_item(node_list, 0));
        ixmlNodeList_free(node_list);
    }
    return ret;
}

/* Get the value of the first element with the specified name. */
static const char * elmt_get_elmt_value(IXML_Element *elmt, const char *name)
{
    const char *ret = NULL;
    IXML_NodeList *node_list = ixmlElement_getElementsByTagName(elmt, name);
    if (node_list) {
        ret = get_node_value(ixmlNodeList_item(node_list, 0));
        ixmlNodeList_free(node_list);
    }
    return ret;
}

/* Check if response contains errorCode. */
static const char * check_error_response(IXML_Document *doc)
{
    const char *error_code = doc_get_elmt_value(doc, "errorCode");

    if (error_code) {
        const char *error_desc = doc_get_elmt_value(doc, "errorDescription");

        PJ_LOG(3, (THIS_FILE, "Response error code: %s (%s)",
                              error_code, error_desc));
    }

    return error_code;
}

/* Query the external IP of the IGD. */
static const char *action_get_external_ip(struct igd *igd)
{
    static const char* action_name = "GetExternalIPAddress";
    IXML_Document *action = NULL;
    IXML_Document *response = NULL;
    const char *public_ip = NULL;
    int upnp_err;
    pj_status_t status;

    /* Create action XML. */
    action = UpnpMakeAction(action_name, igd->service_type.ptr, 0, NULL);
    if (!action) {
        PJ_LOG(3, (THIS_FILE, "Failed to make GetExternalIPAddress action"));
        return NULL;
    }

    /* Send the action XML. */
    upnp_err = UpnpSendAction(upnp_mgr.client_hnd, igd->control_url.ptr,
                              igd->service_type.ptr, NULL, action, &response);
    if (upnp_err != UPNP_E_SUCCESS || !response) {
        PJ_LOG(3, (THIS_FILE, "Failed to send GetExternalIPAddress action: %s",
                              UpnpGetErrorMessage(upnp_err)));
        goto on_error;
    }

    if (check_error_response(response))
        goto on_error;
    
    /* Get the external IP address from the response. */
    public_ip = doc_get_elmt_value(response, "NewExternalIPAddress");
    if (!public_ip) {
        PJ_LOG(3, (THIS_FILE, "IGD %s has no external IP", igd->dev_id.ptr));
        goto on_error;
    }
    pj_strdup2_with_null(upnp_mgr.pool, &igd->public_ip, public_ip);
    status = pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &igd->public_ip,
                               &igd->public_ip_addr);
    if (status != PJ_SUCCESS)
        goto on_error;
    public_ip = igd->public_ip.ptr;

on_error:
    ixmlDocument_free(action);
    if (response) ixmlDocument_free(response);
    
    return public_ip;
}

/* Download the XML document of the IGD. */
static void download_igd_xml(unsigned dev_idx)
{
    struct igd *igd_dev = &upnp_mgr.igd_devs[dev_idx];
    const char *url = igd_dev->url.ptr;
    IXML_Document *doc = NULL;
    int upnp_err;
    const char *dev_type;
    const char *friendly_name;
    const char *base_url;
    const char *control_url;
    const char *public_ip;
    char *abs_control_url = NULL;
    IXML_NodeList *service_list = NULL;
    unsigned i, n;

    upnp_err = UpnpDownloadXmlDoc(url, &doc);
    if (upnp_err != UPNP_E_SUCCESS || !doc) {
        PJ_LOG(3, (THIS_FILE, "Error downloading device XML doc from %s: %s",
                              url, UpnpGetErrorMessage(upnp_err)));
        goto on_error;
    }

    /* Check device type. */
    dev_type = doc_get_elmt_value(doc, "deviceType");
    if (!dev_type) return;
    if (pj_ansi_strcmp(dev_type, UPNP_IGD_DEVICE) != 0) {
        /* Device type is not IGD. */
        goto on_error;
    }

    /* Get friendly name. */
    friendly_name = doc_get_elmt_value(doc, "friendlyName");
    if (!friendly_name)
        friendly_name = "";
    
    /* Get base URL. */
    base_url = doc_get_elmt_value(doc, "URLBase");
    if (!base_url)
        base_url = url;

    /* Get list of services defined by serviceType. */
    service_list = ixmlDocument_getElementsByTagName(doc, "serviceType");
    n = ixmlNodeList_length(service_list);

    for (i = 0; i < n; i++) {
        IXML_Node *service_type_node = ixmlNodeList_item(service_list, i);
        IXML_Node *service_node = ixmlNode_getParentNode(service_type_node);
        IXML_Element* service_element = (IXML_Element*) service_node;
        const char *service_type;
        pj_bool_t call_cb = PJ_FALSE;

        /* Check if parent node is "service". */
        if (!service_node ||
            (pj_ansi_strcmp(ixmlNode_getNodeName(service_node), "service")))
        {
            continue;
        }
        
        /* We only want serviceType of WANIPConnection or WANPPPConnection. */
        service_type = get_node_value(service_type_node);
        if (pj_ansi_strcmp(service_type, UPNP_WANIP_SERVICE) &&
            pj_ansi_strcmp(service_type, UPNP_WANPPP_SERVICE))
        {
            continue;
        }

        /* Get the controlURL. */
        control_url = elmt_get_elmt_value(service_element, "controlURL");
        if (!control_url)
            continue;

        /* Resolve the absolute address of controlURL. */
        upnp_err = UpnpResolveURL2(base_url, control_url, &abs_control_url);
        if (upnp_err == UPNP_E_SUCCESS) {
            pj_strdup2_with_null(upnp_mgr.pool, &igd_dev->control_url,
                                 abs_control_url);
            free(abs_control_url);
        } else {
            PJ_LOG(4, (THIS_FILE, "Error resolving absolute controlURL: %s",
                                  UpnpGetErrorMessage(upnp_err)));
            pj_strdup2_with_null(upnp_mgr.pool, &igd_dev->control_url,
                                 control_url);
        }

        pj_strdup2_with_null(upnp_mgr.pool, &igd_dev->service_type, service_type);

        /* Get the public IP of the IGD. */
        public_ip = action_get_external_ip(igd_dev);
        if (!public_ip)
            break;

        /* We find a valid IGD. */
        igd_dev->valid = PJ_TRUE;
        igd_dev->alive = PJ_TRUE;
        
        PJ_LOG(4, (THIS_FILE, "Valid IGD:\n"
                              "\tUDN          : %s\n"
                              "\tName         : %s\n"
                              "\tService Type : %s\n"
                              "\tControl URL  : %s\n"
                              "\tPublic IP    : %s",
                              igd_dev->dev_id.ptr,
                              friendly_name,
                              igd_dev->service_type.ptr,
                              igd_dev->control_url.ptr,
                              public_ip));

        /* Use this as primary IGD if we haven't had one. */
        pj_mutex_lock(upnp_mgr.mutex);
        if (upnp_mgr.primary_igd_idx < 0) {
            upnp_mgr.primary_igd_idx = dev_idx;
            call_cb = PJ_TRUE;
            upnp_mgr.status = PJ_SUCCESS;
        }
        pj_mutex_unlock(upnp_mgr.mutex);

        if (call_cb && upnp_mgr.upnp_cb) {
            (*upnp_mgr.upnp_cb)(upnp_mgr.status);
        }

        break;
    }

on_error:
    if (service_list)
        ixmlNodeList_free(service_list);
    if (doc)
        ixmlDocument_free(doc);
}

/* Add a newly discovered IGD. */
static void add_device(const char *dev_id, const char *url)
{
    unsigned i;

    if (upnp_mgr.igd_cnt >= MAX_DEVS) {
        PJ_LOG(3, (THIS_FILE, "Warning: Too many UPnP devices discovered"));
        return;
    }

    pj_mutex_lock(upnp_mgr.mutex);
    for (i = 0; i < upnp_mgr.igd_cnt; i++) {
        if (!pj_strcmp2(&upnp_mgr.igd_devs[i].dev_id, dev_id) &&
            !pj_strcmp2(&upnp_mgr.igd_devs[i].url, url))
        {
            /* Device exists. */
            pj_mutex_unlock(upnp_mgr.mutex);
            return;
        }
    }

    pj_strdup2_with_null(upnp_mgr.pool,
                         &upnp_mgr.igd_devs[upnp_mgr.igd_cnt].dev_id, dev_id);
    pj_strdup2_with_null(upnp_mgr.pool,
                         &upnp_mgr.igd_devs[upnp_mgr.igd_cnt++].url, url);
    pj_mutex_unlock(upnp_mgr.mutex);

    PJ_LOG(4, (THIS_FILE, "Discovered a new IGD %s, url: %s", dev_id, url));

    /* Download the IGD's XML doc. */
    download_igd_xml(upnp_mgr.igd_cnt-1);
}

/* Update online status of an IGD. */
static void set_device_online(const char *dev_id)
{
    unsigned i;

    for (i = 0; i < upnp_mgr.igd_cnt; i++) {
        struct igd *igd = &upnp_mgr.igd_devs[i];
        
        /* We are only interested in valid IGDs that we can use. */
        if (!pj_strcmp2(&igd->dev_id, dev_id) && igd->valid) {
            igd->alive = PJ_TRUE;

            if (upnp_mgr.primary_igd_idx < 0) {
                /* If we don't have a primary IGD, use this. */
                pj_mutex_lock(upnp_mgr.mutex);
                upnp_mgr.primary_igd_idx = i;
                pj_mutex_unlock(upnp_mgr.mutex);

                PJ_LOG(4, (THIS_FILE, "Using primary IGD %s",
                                      upnp_mgr.igd_devs[i].dev_id.ptr));
            }
        }
    }
}

/* Update IGD status to offline. */
static void set_device_offline(const char *dev_id)
{
    int i;

    for (i = 0; i < (int)upnp_mgr.igd_cnt; i++) {
        struct igd *igd = &upnp_mgr.igd_devs[i];

        /* We are only interested in valid IGDs that we can use. */
        if (!pj_strcmp2(&igd->dev_id, dev_id) && igd->valid) {
            igd->alive = PJ_FALSE;
 
            pj_mutex_lock(upnp_mgr.mutex);
            if (i == upnp_mgr.primary_igd_idx) {
                unsigned j;

                /* The primary IGD is offline, try to find another one. */
                upnp_mgr.primary_igd_idx = -1;
                for (j = 0; j < upnp_mgr.igd_cnt; j++) {
                    igd = &upnp_mgr.igd_devs[j];
                    if (igd->valid && igd->alive) {
                        upnp_mgr.primary_igd_idx = j;
                        break;
                    }
                }

                PJ_LOG(4, (THIS_FILE, "Device %s offline, now using IGD %s",
                                      upnp_mgr.igd_devs[i].dev_id.ptr,
                                      (upnp_mgr.primary_igd_idx < 0? "(none)":
                                      igd->dev_id.ptr)));
            }
            pj_mutex_unlock(upnp_mgr.mutex);
        }
    }
}

/* UPnP client callback. */
static int client_cb(Upnp_EventType event_type, const void *event,
                     void * user_data)
{
    /* Ignore if already uninitialized or incorrect user data. */
    if (!upnp_mgr.initialized || user_data != &upnp_mgr)
        return UPNP_E_SUCCESS;

    if (!pj_thread_is_registered()) {
        pj_bzero(upnp_mgr.thread_desc, sizeof(pj_thread_desc));
        pj_thread_register("upnp_cb", upnp_mgr.thread_desc,
                           &upnp_mgr.thread);
    }

    switch (event_type) {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    {
        const UpnpDiscovery *d_event = (const UpnpDiscovery *) event;
        int upnp_status = UpnpDiscovery_get_ErrCode(d_event);
        const char *dev_id, *location;

        if (upnp_status != UPNP_E_SUCCESS) {
            PJ_LOG(4, (THIS_FILE, "UPnP discovery error: %s",
                                  UpnpGetErrorMessage(upnp_status)));
            break;
        }

        dev_id = UpnpDiscovery_get_DeviceID_cstr(d_event);
        location = UpnpDiscovery_get_Location_cstr(d_event);

        add_device(dev_id, location);
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*) event;
        set_device_online(UpnpDiscovery_get_DeviceID_cstr(d_event));
        break;
    }

    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        const UpnpDiscovery* d_event = (const UpnpDiscovery*) event;        
        set_device_offline(UpnpDiscovery_get_DeviceID_cstr(d_event));
        break;
    }

    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
    {
        pj_bool_t call_cb = PJ_FALSE;

        pj_mutex_lock(upnp_mgr.mutex);
        if (upnp_mgr.search_cnt > 0) {
            --upnp_mgr.search_cnt;
            if (upnp_mgr.search_cnt == 0 && upnp_mgr.primary_igd_idx < 0) {
                PJ_LOG(4,(THIS_FILE, "Search timed out, no valid IGD found"));
                call_cb = PJ_TRUE;
                upnp_mgr.status = PJ_ENOTFOUND;
            }
        }
        pj_mutex_unlock(upnp_mgr.mutex);

        if (call_cb && upnp_mgr.upnp_cb) {
            (*upnp_mgr.upnp_cb)(upnp_mgr.status);
        }

        break;
    }
    case UPNP_CONTROL_ACTION_COMPLETE:
    {
        int err_code;
        IXML_Document *response = NULL;
        const UpnpActionComplete* a_event = (const UpnpActionComplete *) event;
        if (!a_event)
            break;

        /* The only action complete event we're supposed to receive is
         * from port mapping deletion action.
         */
        err_code = UpnpActionComplete_get_ErrCode(a_event);
        if (err_code != UPNP_E_SUCCESS) {
            PJ_LOG(4, (THIS_FILE, "Port mapping deletion action complete "
                                  "error: %d (%s)", err_code,
                                  UpnpGetErrorMessage(err_code)));
            break;
        }
        
        response = UpnpActionComplete_get_ActionResult(a_event);
        if (!response) {
            PJ_LOG(4, (THIS_FILE, "Failed to get response to delete port "
                                  "mapping"));
        } else {
            if (!check_error_response(response)) {
                PJ_LOG(4, (THIS_FILE, "Successfully deleted port mapping"));
            }
            ixmlDocument_free(response);
        }

        break;
    }
    default:
        TRACE_("Unhandled UPnP client callback %d", event_type);
        break;
    }
    
    return UPNP_E_SUCCESS;
}

/* Initiate search for Internet Gateway Devices. */
static void search_igd(int search_time)
{
    int err;

    upnp_mgr.search_cnt = 4;

    err = UpnpSearchAsync(upnp_mgr.client_hnd, search_time,
                          UPNP_ROOT_DEVICE, &upnp_mgr);
    if (err != UPNP_E_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Searching for UPNP_ROOT_DEVICE failed: %s",
                              UpnpGetErrorMessage(err)));
    }

    err = UpnpSearchAsync(upnp_mgr.client_hnd, search_time,
                          UPNP_IGD_DEVICE, &upnp_mgr);
    if (err != UPNP_E_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Searching for UPNP_IGD_DEVICE failed: %s",
                              UpnpGetErrorMessage(err)));
    }

    err = UpnpSearchAsync(upnp_mgr.client_hnd, search_time,
                          UPNP_WANIP_SERVICE, &upnp_mgr);
    if (err != UPNP_E_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Searching for UPNP_WANIP_SERVICE failed: %s",
                              UpnpGetErrorMessage(err)));
    }

    err = UpnpSearchAsync(upnp_mgr.client_hnd, search_time,
                          UPNP_WANPPP_SERVICE, &upnp_mgr);
    if (err != UPNP_E_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Searching for UPNP_WANPPP_SERVICE failed: %s",
                              UpnpGetErrorMessage(err)));
    }
}

/* Initialize UPnP. */
PJ_DEF(pj_status_t) pj_upnp_init(const pj_upnp_init_param *param)
{
    int upnp_err;
    const char *ip_address;
    unsigned short port;
    const char *ip_address6 = NULL;
    unsigned short port6 = 0;
    pj_status_t status;

    if (upnp_mgr.initialized)
        return PJ_SUCCESS;

#if ENABLE_LOG
    UpnpSetLogLevel(UPNP_ALL);
    UpnpSetLogFileNames("upnp.log", NULL);
    upnp_err = UpnpInitLog();
    if (upnp_err != UPNP_E_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Failed to initialize UPnP log: %s",
                              UpnpGetErrorMessage(upnp_err)));
    }
#endif

    pj_bzero(&upnp_mgr, sizeof(upnp_mgr));
    upnp_err = UpnpInit2(param->if_name, (unsigned short)param->port);
    if (upnp_err != UPNP_E_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to initialize libupnp with "
                              "interface %s: %s",
                              (param->if_name? param->if_name: "NULL"),
                              UpnpGetErrorMessage(upnp_err)));
        return PJ_EUNKNOWN;
    }

    /* Register client. */
    upnp_err = UpnpRegisterClient(client_cb, &upnp_mgr, &upnp_mgr.client_hnd);
    if (upnp_err != UPNP_E_SUCCESS) {
        PJ_LOG(1, (THIS_FILE, "Failed to register client: %s",
                              UpnpGetErrorMessage(upnp_err)));
        UpnpFinish();
        return PJ_EINVALIDOP;
    }

    /* Try to disable web server. */
    if (UpnpIsWebserverEnabled()) {
        UpnpEnableWebserver(0);
        if (UpnpIsWebserverEnabled()) {
            PJ_LOG(4, (THIS_FILE, "Failed to disable web server"));
        }
    }

    /* Makes the XML parser more tolerant to malformed text. */
    ixmlRelaxParser(1);

    upnp_mgr.initialized = 1;
    upnp_mgr.primary_igd_idx = -1;
    upnp_mgr.upnp_cb = param->upnp_cb;
    upnp_mgr.pool = pj_pool_create(param->factory, "upnp", 512, 512, NULL);
    if (!upnp_mgr.pool) {
        pj_upnp_deinit();
        return PJ_ENOMEM;
    }
    status = pj_mutex_create_recursive(upnp_mgr.pool, "upnp", &upnp_mgr.mutex);
    if (status != PJ_SUCCESS) {
        pj_upnp_deinit();
        return status;
    }

    ip_address = UpnpGetServerIpAddress();
    port = UpnpGetServerPort();
#if PJ_HAS_IPV6
    ip_address6 = UpnpGetServerIp6Address();
    port6 = UpnpGetServerPort6();
#endif
    if (param->if_name) {
        PJ_LOG(4, (THIS_FILE, "UPnP initialized with interface %s",
                              param->if_name));
    }
    if (ip_address6 && port6) {
        PJ_LOG(4, (THIS_FILE, "UPnP initialized on %s:%u (IPv4) and "
                              "%s:%u (IPv6)", ip_address, port,
                              ip_address6, port6));
    } else {
        PJ_LOG(4, (THIS_FILE, "UPnP initialized on %s:%u", ip_address, port));
    }

    /* Search for Internet Gateway Devices. */
    upnp_mgr.status = PJ_EPENDING;
    search_igd(param->search_time > 0? param->search_time:
               PJ_UPNP_DEFAULT_SEARCH_TIME);

    return PJ_SUCCESS;
}

/* Deinitialize UPnP. */
PJ_DEF(pj_status_t) pj_upnp_deinit(void)
{
    PJ_LOG(4, (THIS_FILE, "UPnP deinitializing..."));

    /* Note that this function will wait until all its worker threads
     * complete.
     */
    UpnpFinish();

    if (upnp_mgr.mutex)
        pj_mutex_destroy(upnp_mgr.mutex);

    if (upnp_mgr.pool)
        pj_pool_release(upnp_mgr.pool);
    
    pj_bzero(&upnp_mgr, sizeof(upnp_mgr));
    upnp_mgr.primary_igd_idx = -1;

    PJ_LOG(4, (THIS_FILE, "UPnP deinitialized"));
    
    return PJ_SUCCESS;
}


/* Send request to add port mapping. */
PJ_DEF(pj_status_t)pj_upnp_add_port_mapping(unsigned sock_cnt,
                                            const pj_sock_t sock[],
                                            unsigned ext_port[],
                                            pj_sockaddr mapped_addr[])
{
    unsigned max_wait = 20;
    unsigned i;
    struct igd *igd = NULL;
    pj_status_t status = PJ_SUCCESS;

    if (!upnp_mgr.initialized) {
        PJ_LOG(3, (THIS_FILE, "UPnP not initialized yet"));
        return PJ_EINVALIDOP;
    }

    /* If IGD search hasn't completed, wait momentarily. */
    while (upnp_mgr.status == PJ_EPENDING && max_wait > 0) {
        pj_thread_sleep(100);
        max_wait--;
    }

    /* Need to lock in case the device becomes offline at the same time. */
    pj_mutex_lock(upnp_mgr.mutex);
    if (upnp_mgr.primary_igd_idx < 0) {
        PJ_LOG(3, (THIS_FILE, "No valid IGD"));
        pj_mutex_unlock(upnp_mgr.mutex);
        return PJ_ENOTFOUND;
    }

    igd = &upnp_mgr.igd_devs[upnp_mgr.primary_igd_idx];
    pj_mutex_unlock(upnp_mgr.mutex);

    for (i = 0; i < sock_cnt; i++) {
        static const char *ACTION_ADD_PORT_MAPPING = "AddPortMapping";
        static const char *PORT_MAPPING_DESCRIPTION = "pjsip-upnp";
        int upnp_err;
        IXML_Document *action = NULL;
        IXML_Document *response = NULL;
        char int_port_buf[10], ext_port_buf[10];
        char addr_buf[PJ_INET6_ADDRSTRLEN];
        unsigned int_port;
        pj_sockaddr bound_addr;
        int namelen = sizeof(pj_sockaddr);
        const char *pext_port = (ext_port? ext_port_buf: int_port_buf);

        /* Get socket's bound address. */
        status = pj_sock_getsockname(sock[i], &bound_addr, &namelen);
        if (status != PJ_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "getsockname() error"));
            goto on_error;
        }

        if (!pj_sockaddr_has_addr(&bound_addr)) {
            pj_sockaddr addr;

            /* Get local IP address. */
            status = pj_gethostip(bound_addr.addr.sa_family, &addr);
            if (status != PJ_SUCCESS)
                goto on_error;

            pj_sockaddr_copy_addr(&bound_addr, &addr);
        }

        pj_sockaddr_print(&bound_addr, addr_buf, sizeof(addr_buf), 0);
        int_port = pj_sockaddr_get_port(&bound_addr);
        pj_utoa(int_port, int_port_buf);
        if (ext_port)
            pj_utoa(ext_port[i], ext_port_buf);

        /* Create action XML. */
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewRemoteHost", "");
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewExternalPort", pext_port);
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewProtocol", "UDP");
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewInternalPort",int_port_buf);
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewInternalClient", addr_buf);
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewEnabled", "1");
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr,
                        "NewPortMappingDescription", PORT_MAPPING_DESCRIPTION);
        UpnpAddToAction(&action, ACTION_ADD_PORT_MAPPING,
                        igd->service_type.ptr, "NewLeaseDuration","0");

        /* Send the action XML. */
        upnp_err = UpnpSendAction(upnp_mgr.client_hnd, igd->control_url.ptr,
                                  igd->service_type.ptr, NULL, action,
                                  &response);
        if (upnp_err != UPNP_E_SUCCESS || !response) {
            PJ_LOG(3, (THIS_FILE, "Failed to %s IGD %s to add port mapping "
                                  "for %s:%s -> %s:%s: %d (%s)",
                                  response? "send action to":
                                  "get response from",
                                  igd->dev_id.ptr,
                                  igd->public_ip.ptr, pext_port,
                                  addr_buf, int_port_buf, upnp_err,           
                                  UpnpGetErrorMessage(upnp_err)));
            status = PJ_ETIMEDOUT;
        }

        TRACE_("Add port mapping XML action:\n%s",
               ixmlPrintDocument(action));
        TRACE_("Add port mapping XML response:\n%s",
               (response? ixmlPrintDocument(response): "empty"));

        if (response && check_error_response(response)) {
            /* The error detail will be printed by check_error_response(). */
            status = PJ_EINVALIDOP;
        }

        ixmlDocument_free(action);
        if (response) ixmlDocument_free(response);
        
        if (status != PJ_SUCCESS)
            goto on_error;

        pj_sockaddr_cp(&mapped_addr[i], &bound_addr);
        status = pj_sockaddr_set_str_addr(bound_addr.addr.sa_family,
                                          &mapped_addr[i], &igd->public_ip);
        if (status != PJ_SUCCESS)
            goto on_error;
        pj_sockaddr_set_port(&mapped_addr[i],
                             (pj_uint16_t)(ext_port? ext_port[i]: int_port));

        PJ_LOG(4, (THIS_FILE, "Successfully add port mapping to IGD %s: "
                              "%s:%s -> %s:%s", igd->dev_id.ptr,
                              igd->public_ip.ptr, pext_port,
                              addr_buf, int_port_buf));
    }
    
    return PJ_SUCCESS;

on_error:
    /* Port mapping was unsuccessful, so we need to delete all
     * the previous port mappings.
     */
    while (i > 0) {
        pj_upnp_del_port_mapping(&mapped_addr[--i]);
    }

    return status;
}


/* Send request to delete port mapping. */
PJ_DEF(pj_status_t)pj_upnp_del_port_mapping(const pj_sockaddr *mapped_addr)
{
    static const char* ACTION_DELETE_PORT_MAPPING = "DeletePortMapping";
    int upnp_err;
    struct igd *igd = NULL;
    IXML_Document *action = NULL;
    pj_status_t status = PJ_SUCCESS;
    pj_sockaddr host_addr;
    unsigned ext_port;
    char ext_port_buf[10];

    if (!upnp_mgr.initialized)
        return PJ_EINVALIDOP;
    
    /* Need to lock in case the device becomes offline at the same time. */
    pj_mutex_lock(upnp_mgr.mutex);
    if (upnp_mgr.primary_igd_idx < 0) {
        PJ_LOG(3, (THIS_FILE, "No valid IGD"));
        pj_mutex_unlock(upnp_mgr.mutex);
        return PJ_ENOTFOUND;
    }

    igd = &upnp_mgr.igd_devs[upnp_mgr.primary_igd_idx];
    pj_mutex_unlock(upnp_mgr.mutex);

    /* Compare IGD's public IP to the mapped public address. */
    pj_sockaddr_cp(&host_addr, mapped_addr);
    pj_sockaddr_set_port(&host_addr, 0);
    if (pj_sockaddr_cmp(&igd->public_ip_addr, &host_addr)) {
        unsigned i;

        /* The primary IGD's public IP is different. Find the IGD
         * that matches the mapped address.
         */
        igd = NULL;
        for (i = 0; i < upnp_mgr.igd_cnt; i++, igd = NULL) {
            igd = &upnp_mgr.igd_devs[i];
            if (igd->valid && igd->alive &&
                !pj_sockaddr_cmp(&igd->public_ip_addr, &host_addr))
            {
                break;
            }
        }
    }
    
    if (!igd) {
        /* Either the IGD we previously requested to add port mapping has become
         * offline, or the address is actually not a valid.
         */
        PJ_LOG(3, (THIS_FILE, "The IGD is offline or invalid mapped address"));
        return PJ_EGONE;
    } 

    ext_port = pj_sockaddr_get_port(mapped_addr);
    if (ext_port == 0) {
        /* Deleting port zero should be harmless, but it's a waste of time. */
        PJ_LOG(3, (THIS_FILE, "Invalid port number to be deleted"));
        return PJ_EINVALIDOP;
    }
    pj_utoa(ext_port, ext_port_buf);

    /* Create action XML. */
    UpnpAddToAction(&action, ACTION_DELETE_PORT_MAPPING, igd->service_type.ptr,
                    "NewRemoteHost", "");
    UpnpAddToAction(&action, ACTION_DELETE_PORT_MAPPING, igd->service_type.ptr,
                    "NewExternalPort", ext_port_buf);
    UpnpAddToAction(&action, ACTION_DELETE_PORT_MAPPING, igd->service_type.ptr,
                    "NewProtocol", "UDP");

    /* For mapping deletion, send the action XML async, to avoid long
     * wait in network disconnection scenario.
     */
    upnp_err = UpnpSendActionAsync(upnp_mgr.client_hnd, igd->control_url.ptr,
                                   igd->service_type.ptr, NULL, action,
                                   client_cb, &upnp_mgr);
    if (upnp_err == UPNP_E_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Successfully sending async action to "
                              "delete port mapping to IGD %s for "
                              "%s:%s", igd->dev_id.ptr,
                              igd->public_ip.ptr, ext_port_buf));
    } else {
        PJ_LOG(3, (THIS_FILE, "Failed to send action to IGD %s to delete "
                              "port mapping for %s:%s: %d (%s)",
                              igd->dev_id.ptr, igd->public_ip.ptr,
                              ext_port_buf, upnp_err,
                              UpnpGetErrorMessage(upnp_err)));
        status = PJ_EINVALIDOP;
    }
    
    ixmlDocument_free(action);
    
    return status;
}

#if defined(_MSC_VER)
#   pragma comment(lib, "libupnp")
#   pragma comment(lib, "libixml")
#   pragma comment(lib, "libpthread")
#endif

#endif  /* PJNATH_HAS_UPNP */
