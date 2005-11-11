/* $Id$
 */
#ifndef __PJSIP_SIP_MODULE_H__
#define __PJSIP_SIP_MODULE_H__

/**
 * @file sip_module.h
 * @brief Module helpers
 */
#include <pjsip/sip_types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MOD SIP Modules
 * @ingroup PJSIP
 * @{
 */

/**
 * Module registration structure, which is passed by the module to the
 * endpoint during the module registration process. This structure enables
 * the endpoint to query the module capability and to further communicate
 * with the module.
 */
struct pjsip_module
{
    /**
     * Module name.
     */
    pj_str_t name;

    /**
     * Flag to indicate the type of interfaces supported by the module.
     */
    pj_uint32_t flag;

    /**
     * Integer number to identify module initialization and start order with
     * regard to other modules. Higher number will make the module gets
     * initialized later.
     */
    pj_uint32_t priority;

    /**
     * Opaque data which can be used by a module to identify a resource within
     * the module itself.
     */
    void *mod_data;

    /**
     * Number of methods supported by this module.
     */
    int method_cnt;

    /**
     * Array of methods supported by this module.
     */
    const pjsip_method *methods[8];

    /**
     * Pointer to function to be called to initialize the module.
     *
     * @param endpt	The endpoint instance.
     * @param mod	The module.
     * @param id	The unique module ID assigned to this module.
     *
     * @return		Module should return zero when initialization succeed.
     */
    pj_status_t (*init_module)(pjsip_endpoint *endpt,
			       struct pjsip_module *mod, pj_uint32_t id);

    /**
     * Pointer to function to be called to start the module.
     *
     * @param mod	The module.
     *
     * @return		Module should return zero to indicate success.
     */
    pj_status_t (*start_module)(struct pjsip_module *mod);

    /**
     * Pointer to function to be called to deinitialize the module before
     * it is unloaded.
     *
     * @param mod	The module.
     *
     * @return		Module should return zero to indicate success.
     */
    pj_status_t (*deinit_module)(struct pjsip_module *mod);

    /**
     * Pointer to function to receive transaction related events.
     * If the module doesn't wish to receive such notification, this member
     * must be set to NULL.
     *
     * @param mod	The module.
     * @param event	The transaction event.
     */
    void (*tsx_handler)(struct pjsip_module *mod, pjsip_event *event);
};


/**
 * Prototype of function to register static modules (eg modules that are
 * linked staticly with the application). This function must be implemented 
 * by any applications that use PJSIP library.
 *
 * @param count	    [input/output] On input, it contains the maximum number of
 *		    elements in the array. On output, the function fills with 
 *		    the number of modules to be registered.
 * @param modules   [output] array of pointer to modules to be registered.
 */
pj_status_t register_static_modules( pj_size_t *count,
				     pjsip_module **modules );

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_MODULE_H__ */

