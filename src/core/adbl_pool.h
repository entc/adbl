#ifndef __ADBL_POOL__H
#define __ADBL_POOL__H 1

#include "adbl_types.h"

#include "sys/cape_export.h"
#include "sys/cape_err.h"
#include "stc/cape_udc.h"

//-----------------------------------------------------------------------------

struct AdblPool_s; typedef struct AdblPool_s* AdblPool;

//-----------------------------------------------------------------------------

__CAPE_LIBEX   AdblPool           adbl_pool_new              (const AdblPvd* pvd);

__CAPE_LIBEX   void               adbl_pool_del              (AdblPool*);

__CAPE_LIBEX   const AdblPvd*     adbl_pool_pvd              (AdblPool);

//-----------------------------------------------------------------------------

                                  /* get the next free connection */
__CAPE_LIBEX   CapeListNode       adbl_pool_get              (AdblPool);

                                  /* add another connection to the pool */
__CAPE_LIBEX   CapeListNode       adbl_pool_add              (AdblPool, void* session);

                                  /* release a connection */
__CAPE_LIBEX   void               adbl_pool_rel              (AdblPool, CapeListNode);

//-----------------------------------------------------------------------------

__CAPE_LIBEX   int                adbl_pool_trx_begin        (AdblPool, CapeListNode, CapeErr err);

__CAPE_LIBEX   int                adbl_pool_trx_commit       (AdblPool, CapeListNode, CapeErr err);

__CAPE_LIBEX   int                adbl_pool_trx_rollback     (AdblPool, CapeListNode, CapeErr err);

__CAPE_LIBEX   CapeUdc            adbl_pool_trx_query        (AdblPool, CapeListNode, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err);

__CAPE_LIBEX   number_t           adbl_pool_trx_insert       (AdblPool, CapeListNode n, const char* table, CapeUdc* p_values, CapeErr err);

__CAPE_LIBEX   int                adbl_pool_trx_update       (AdblPool, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err);

__CAPE_LIBEX   int                adbl_pool_trx_delete       (AdblPool, CapeListNode n, const char* table, CapeUdc* p_params, CapeErr err);

__CAPE_LIBEX   number_t           adbl_pool_trx_inorup       (AdblPool, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err);

__CAPE_LIBEX   void*              adbl_pool_trx_cursor_new   (AdblPool, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err);

//-----------------------------------------------------------------------------

#endif
