#include "adbl.h" 
#include "adbl_types.h"
#include "adbl_pool.h"

// cape includes
#include "sys/cape_dl.h"
#include "sys/cape_types.h"
#include "sys/cape_log.h"
#include "fmt/cape_json.h"

//-----------------------------------------------------------------------------

struct AdblCtx_s
{
  CapeDl hlib;  
  
  AdblPvd pvd;
};

//-----------------------------------------------------------------------------

AdblCtx adbl_ctx_new (const char* path, const char* backend, CapeErr err)
{
  int res;
  AdblPvd pvd;
  
  CapeDl hlib = cape_dl_new ();

  // try to load the module
  res = cape_dl_load (hlib, path, backend, err);
  if (res)
  {
    goto exit;
  }
  
  pvd.pvd_open = cape_dl_funct (hlib, "adbl_pvd_open", err);
  if (pvd.pvd_open == NULL)
  {
    goto exit;    
  }

  pvd.pvd_clone = cape_dl_funct (hlib, "adbl_pvd_clone", err);
  if (pvd.pvd_clone == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_close = cape_dl_funct (hlib, "adbl_pvd_close", err);
  if (pvd.pvd_close == NULL)
  {
    goto exit;    
  }

  pvd.pvd_begin = cape_dl_funct (hlib, "adbl_pvd_begin", err);
  if (pvd.pvd_begin == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_commit = cape_dl_funct (hlib, "adbl_pvd_commit", err);
  if (pvd.pvd_commit == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_rollback = cape_dl_funct (hlib, "adbl_pvd_rollback", err);
  if (pvd.pvd_rollback == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_get = cape_dl_funct (hlib, "adbl_pvd_get", err);
  if (pvd.pvd_get == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_ins = cape_dl_funct (hlib, "adbl_pvd_ins", err);
  if (pvd.pvd_ins == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_set = cape_dl_funct (hlib, "adbl_pvd_set", err);
  if (pvd.pvd_set == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_del = cape_dl_funct (hlib, "adbl_pvd_del", err);
  if (pvd.pvd_del == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_ins_or_set = cape_dl_funct (hlib, "adbl_pvd_ins_or_set", err);
  if (pvd.pvd_ins_or_set == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_cursor_new = cape_dl_funct (hlib, "adbl_pvd_cursor_new", err);
  if (pvd.pvd_cursor_new == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_cursor_del = cape_dl_funct (hlib, "adbl_pvd_cursor_del", err);
  if (pvd.pvd_cursor_del == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_cursor_next = cape_dl_funct (hlib, "adbl_pvd_cursor_next", err);
  if (pvd.pvd_cursor_next == NULL)
  {
    goto exit;    
  }
  
  pvd.pvd_cursor_get = cape_dl_funct (hlib, "adbl_pvd_cursor_get", err);
  if (pvd.pvd_cursor_get == NULL)
  {
    goto exit;    
  }

  pvd.pvd_atomic_dec = cape_dl_funct (hlib, "adbl_pvd_atomic_dec", err);
  if (pvd.pvd_atomic_dec == NULL)
  {
    goto exit;
  }

  pvd.pvd_atomic_inc = cape_dl_funct (hlib, "adbl_pvd_atomic_inc", err);
  if (pvd.pvd_atomic_inc == NULL)
  {
    goto exit;
  }

  {
    AdblCtx self = CAPE_NEW(struct AdblCtx_s);
    
    self->hlib = hlib;
    
    memcpy(&(self->pvd), &pvd, sizeof(AdblPvd));
    
    return self;  
  }
  
exit:

  cape_dl_del (&hlib);

  return NULL;
}

//-----------------------------------------------------------------------------

void adbl_ctx_del (AdblCtx* p_self)
{
  if (*p_self)
  {
    AdblCtx self = *p_self;
    
    cape_dl_del (&(self->hlib));    
    
    CAPE_DEL(p_self, struct AdblCtx_s);
  }  
}

//-----------------------------------------------------------------------------

struct AdblSession_s
{
  const AdblPvd* pvd;
  
  void* session;
  
  AdblPool pool;
};

//=============================================================================

AdblSession adbl_session_open (AdblCtx ctx, CapeUdc connection_properties, CapeErr err)
{
  const AdblPvd* pvd = &(ctx->pvd);
  
  void* session = pvd->pvd_open (connection_properties, err);
  if (session == NULL)
  {
    return NULL;
  }
  
  {
    AdblSession self = CAPE_NEW(struct AdblSession_s);
    
    self->pvd = pvd;
    self->session = session;
    
    self->pool = adbl_pool_new (&(ctx->pvd));
    
    return self;
  }
}

//-----------------------------------------------------------------------------

AdblSession adbl_session_open_file (AdblCtx ctx, const char* config_file, CapeErr err)
{
  AdblSession res = NULL;
  
  CapeUdc connection_properties = cape_json_from_file (config_file, err);
  
  // something went wrong?
  if (connection_properties == NULL)
  {
    return NULL;
  }
  
  res = adbl_session_open (ctx, connection_properties, err);
  
  cape_udc_del (&connection_properties);
  
  return res;
}

//-----------------------------------------------------------------------------

void adbl_session_close (AdblSession* p_self)
{
  if (*p_self)
  {
    AdblSession self = *p_self;
    
    self->pvd->pvd_close (&(self->session));
    
    adbl_pool_del (&(self->pool));
    
    CAPE_DEL(p_self, struct AdblSession_s);
  }  
}

//-----------------------------------------------------------------------------

CapeUdc adbl_session_query (AdblSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  return self->pvd->pvd_get (self->session, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

number_t adbl_session_atomic_dec (AdblSession self, const char* table, CapeUdc* p_params, const CapeString atomic_value, CapeErr err)
{
  return self->pvd->pvd_atomic_dec (self->session, table, p_params, atomic_value, err);
}

//-----------------------------------------------------------------------------

number_t adbl_session_atomic_inc (AdblSession self, const char* table, CapeUdc* p_params, const CapeString atomic_value, CapeErr err)
{
  return self->pvd->pvd_atomic_inc (self->session, table, p_params, atomic_value, err);
}

//=============================================================================

struct AdblTrx_s
{
  AdblPool pool;  
  CapeListNode pool_node;
  
  int in_trx;
};

//-----------------------------------------------------------------------------

AdblTrx adbl_trx_new  (AdblSession session, CapeErr err)
{
  CapeListNode pool_node = adbl_pool_get (session->pool);

  if (NULL == pool_node)
  {
    // clone the current pvd handle
    void* pvd_handle = session->pvd->pvd_clone (session->session, err);
    
    if (NULL == pvd_handle)
    {
      // some error happened
      return NULL;
    }
    
    pool_node = adbl_pool_add (session->pool, pvd_handle);
  }
  
  {
    AdblTrx self = CAPE_NEW (struct AdblTrx_s);
    
    self->pool = session->pool;
    self->pool_node = pool_node;
    
    // don't start with a transaction  
    self->in_trx = FALSE;
    
    return self;
  }  
}

//-----------------------------------------------------------------------------

int adbl_trx_commit (AdblTrx* p_self, CapeErr err)
{
  int res = CAPE_ERR_NONE;

  if (*p_self)
  {
    AdblTrx self = *p_self;
    
    if (self->in_trx)
    {
      cape_log_msg (CAPE_LL_TRACE, "ADBL", "trx commit", "COMMIT TRANSACTION");
      
      res = adbl_pool_trx_commit (self->pool, self->pool_node, err);
    }
    
    adbl_pool_rel (self->pool, self->pool_node);
    
    CAPE_DEL(p_self, struct AdblTrx_s);
  }
  
  return res;
}

//-----------------------------------------------------------------------------

int adbl_trx_rollback (AdblTrx* p_self, CapeErr err)
{
  int res = CAPE_ERR_NONE;

  if (*p_self)
  {
    AdblTrx self = *p_self;
    
    if (self->in_trx)
    {
      cape_log_msg (CAPE_LL_TRACE, "ADBL", "trx rollback", "ROLLBACK TRANSACTION");
      
      res = adbl_pool_trx_rollback (self->pool, self->pool_node, err);
    }
    
    adbl_pool_rel (self->pool, self->pool_node);
    
    CAPE_DEL(p_self, struct AdblTrx_s);    
  }

  return res;
}

//-----------------------------------------------------------------------------

int adbl_trx_start (AdblTrx self, CapeErr err)
{
  if (self == NULL)
  {
    return cape_err_set (err, CAPE_ERR_NO_OBJECT, "transaction is not an object");
  }
  
  if (self->in_trx == FALSE)
  {
    int res;

    cape_log_msg (CAPE_LL_TRACE, "ADBL", "trx start", "START TRANSACTION");
    
    res = adbl_pool_trx_begin (self->pool, self->pool_node, err);
    if (res)
    {
      return res;
    }

    self->in_trx = TRUE;
  }
    
  return CAPE_ERR_NONE;
}

//-----------------------------------------------------------------------------

CapeUdc adbl_trx_query (AdblTrx self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res = adbl_trx_start (self, err);
  if (res)
  {
    return NULL;
  }
  
  return adbl_pool_trx_query (self->pool, self->pool_node, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

number_t adbl_trx_insert (AdblTrx self, const char* table, CapeUdc* p_values, CapeErr err)
{
  int res = adbl_trx_start (self, err);
  if (res)
  {
    return -1;
  }
  
  return adbl_pool_trx_insert (self->pool, self->pool_node, table, p_values, err);
}

//-----------------------------------------------------------------------------

int adbl_trx_update (AdblTrx self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res = adbl_trx_start (self, err);
  if (res)
  {
    return res;
  }
  
  return adbl_pool_trx_update (self->pool, self->pool_node, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

int adbl_trx_delete (AdblTrx self, const char* table, CapeUdc* p_params, CapeErr err)
{
  int res = adbl_trx_start (self, err);
  if (res)
  {
    return res;
  }
  
  return adbl_pool_trx_delete (self->pool, self->pool_node, table, p_params, err);
}

//-----------------------------------------------------------------------------

number_t adbl_trx_inorup (AdblTrx self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res = adbl_trx_start (self, err);
  if (res)
  {
    return -1;
  }
  
  return adbl_pool_trx_inorup (self->pool, self->pool_node, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

struct AdblCursor_s
{
  const AdblPvd* pvd;
  
  void* handle;
};

//-----------------------------------------------------------------------------

AdblCursor adbl_trx_cursor_new (AdblTrx trx, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  void* handle;
  
  int res = adbl_trx_start (trx, err);
  if (res)
  {
    return NULL;
  }
  
  handle = adbl_pool_trx_cursor_new (trx->pool, trx->pool_node, table, p_params, p_values, err);

  if (handle == NULL)
  {
    return NULL;
  }

  {
    AdblCursor self = CAPE_NEW(struct AdblCursor_s);
    
    self->pvd = adbl_pool_pvd (trx->pool);
    self->handle = handle;
    
    return self;
  }
}

//-----------------------------------------------------------------------------

void adbl_trx_cursor_del (AdblCursor* p_self)
{
  AdblCursor self = *p_self;
  
  self->pvd->pvd_cursor_del (&(self->handle));
  
  CAPE_DEL(p_self, struct AdblCursor_s);
}

//-----------------------------------------------------------------------------

int adbl_trx_cursor_next (AdblCursor self)
{
  return self->pvd->pvd_cursor_next (self->handle);
}

//-----------------------------------------------------------------------------

CapeUdc adbl_trx_cursor_get (AdblCursor self)
{
  return self->pvd->pvd_cursor_get (self->handle);
}

//-----------------------------------------------------------------------------

void adbl_param_add__greater_than_n (CapeUdc params, const CapeString name, number_t value)
{
  CapeUdc h = cape_udc_new (CAPE_UDC_NODE, name);
  
  cape_udc_add_n (h, ADBL_SPECIAL__TYPE, ADBL_TYPE__GREATER_THAN);
  cape_udc_add_n (h, ADBL_SPECIAL__GREATER, value);
  
  cape_udc_add (params, &h);
}

//-----------------------------------------------------------------------------

void adbl_param_add__greater_than_d (CapeUdc params, const CapeString name, const CapeDatetime* value)
{
  CapeUdc h = cape_udc_new (CAPE_UDC_NODE, name);
  
  cape_udc_add_n (h, ADBL_SPECIAL__TYPE, ADBL_TYPE__GREATER_THAN);
  cape_udc_add_d (h, ADBL_SPECIAL__GREATER, value);
  
  cape_udc_add (params, &h);
}

//-----------------------------------------------------------------------------

void adbl_param_add__less_than_n (CapeUdc params, const CapeString name, number_t value)
{
  CapeUdc h = cape_udc_new (CAPE_UDC_NODE, name);
  
  cape_udc_add_n (h, ADBL_SPECIAL__TYPE, ADBL_TYPE__LESS_THAN);
  cape_udc_add_n (h, ADBL_SPECIAL__LESS, value);
  
  cape_udc_add (params, &h);
}

//-----------------------------------------------------------------------------

void adbl_param_add__less_than_d (CapeUdc params, const CapeString name, const CapeDatetime* value)
{
  CapeUdc h = cape_udc_new (CAPE_UDC_NODE, name);
  
  cape_udc_add_n (h, ADBL_SPECIAL__TYPE, ADBL_TYPE__LESS_THAN);
  cape_udc_add_d (h, ADBL_SPECIAL__LESS, value);
  
  cape_udc_add (params, &h);
}

//-----------------------------------------------------------------------------

void adbl_param_add__between_n (CapeUdc params, const CapeString name, number_t from, number_t until)
{
  CapeUdc h = cape_udc_new (CAPE_UDC_NODE, name);

  cape_udc_add_n (h, ADBL_SPECIAL__TYPE, ADBL_TYPE__BETWEEN);

  cape_udc_add_n (h, ADBL_SPECIAL__BETWEEN_FROM, from);
  cape_udc_add_n (h, ADBL_SPECIAL__BETWEEN_TO, until);
  
  cape_udc_add (params, &h);
}

//-----------------------------------------------------------------------------

void adbl_param_add__between_d (CapeUdc params, const CapeString name, const CapeDatetime* from, const CapeDatetime* until)
{
  CapeUdc h = cape_udc_new (CAPE_UDC_NODE, name);
  
  cape_udc_add_n (h, ADBL_SPECIAL__TYPE, ADBL_TYPE__BETWEEN);
  
  cape_udc_add_d (h, ADBL_SPECIAL__BETWEEN_FROM, from);
  cape_udc_add_d (h, ADBL_SPECIAL__BETWEEN_TO, until);
  
  cape_udc_add (params, &h);
}

//-----------------------------------------------------------------------------
