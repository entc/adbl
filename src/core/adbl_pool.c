#include "adbl_pool.h"

// cape includes
#include <sys/cape_types.h>
#include <sys/cape_mutex.h>

//-----------------------------------------------------------------------------

typedef struct
{
  const AdblPvd* pvd;
  
  void* handle;
  
  int used;
  
} AdblPoolItem;

//-----------------------------------------------------------------------------

struct AdblPool_s
{
  const AdblPvd* pvd;
  
  CapeMutex mutex;
  
  CapeList connections;
  
};

//-----------------------------------------------------------------------------

static void __STDCALL adbl_pool__connections__on_del (void* ptr)
{
  AdblPoolItem* item = ptr;
  
  item->pvd->pvd_close (&(item->handle));
  
  CAPE_DEL (&item, struct AdblPool_s);
}

//-----------------------------------------------------------------------------

AdblPool adbl_pool_new (const AdblPvd* pvd)
{
  AdblPool self = CAPE_NEW (struct AdblPool_s);
  
  self->pvd = pvd;
  
  self->mutex = cape_mutex_new ();
  self->connections = cape_list_new (adbl_pool__connections__on_del);
  
  return self;  
}

//-----------------------------------------------------------------------------

void adbl_pool_del (AdblPool* p_self)
{
  if (*p_self)
  {
    AdblPool self = *p_self;
    
    cape_mutex_del (&(self->mutex));
    cape_list_del (&(self->connections));
    
    CAPE_DEL(p_self, struct AdblPool_s);
  } 
}

//-----------------------------------------------------------------------------

const AdblPvd* adbl_pool_pvd (AdblPool self)
{
  return self->pvd;
}

//-----------------------------------------------------------------------------

CapeListNode adbl_pool_get (AdblPool self)
{
  CapeListNode n = NULL;
  
  cape_mutex_lock (self->mutex);

  {
    CapeListCursor* cursor = cape_list_cursor_create (self->connections, CAPE_DIRECTION_FORW);
    
    while (cape_list_cursor_next (cursor))
    {
      AdblPoolItem* item = cape_list_node_data (cursor->node);
      
      if (item->used == FALSE)
      {
        n = cursor->node;
        item->used = TRUE;
        
        break;
      }
    }
    
    cape_list_cursor_destroy (&cursor);
  }

  cape_mutex_unlock (self->mutex);
  
  return n;
}

//-----------------------------------------------------------------------------

CapeListNode adbl_pool_add (AdblPool self, void* pvd_handle)
{
  CapeListNode n = NULL;
  
  AdblPoolItem* item = CAPE_NEW (AdblPoolItem);
  
  item->handle = pvd_handle;
  item->pvd = self->pvd;
  item->used = TRUE;
  
  cape_mutex_lock (self->mutex);

  n = cape_list_push_back (self->connections, item);
  
  cape_mutex_unlock (self->mutex);
  
  return n;
}

//-----------------------------------------------------------------------------

void adbl_pool_rel (AdblPool self, CapeListNode n)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  printf ("RELEASE CONNECTION\n");
  
  cape_mutex_lock (self->mutex);
  
  item->used = FALSE;
  
  cape_mutex_unlock (self->mutex);
}

//-----------------------------------------------------------------------------

int adbl_pool_trx_begin (AdblPool self, CapeListNode n, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);

  return self->pvd->pvd_begin (item->handle, err);
}

//-----------------------------------------------------------------------------

int adbl_pool_trx_commit (AdblPool self, CapeListNode n, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_commit (item->handle, err);
}

//-----------------------------------------------------------------------------

int adbl_pool_trx_rollback (AdblPool self, CapeListNode n, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_rollback (item->handle, err);
}

//-----------------------------------------------------------------------------

CapeUdc adbl_pool_trx_query (AdblPool self, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_get (item->handle, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

number_t adbl_pool_trx_insert (AdblPool self, CapeListNode n, const char* table, CapeUdc* p_values, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_ins (item->handle, table, p_values, err);
}

//-----------------------------------------------------------------------------

int adbl_pool_trx_update (AdblPool self, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_set (item->handle, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

int adbl_pool_trx_delete (AdblPool self, CapeListNode n, const char* table, CapeUdc* p_params, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_del (item->handle, table, p_params, err);
}

//-----------------------------------------------------------------------------

number_t adbl_pool_trx_inorup (AdblPool self, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);
  
  return self->pvd->pvd_ins_or_set (item->handle, table, p_params, p_values, err);
}

//-----------------------------------------------------------------------------

void* adbl_pool_trx_cursor_new (AdblPool self, CapeListNode n, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  AdblPoolItem* item = cape_list_node_data (n);

  return self->pvd->pvd_cursor_new (item->handle, table, p_params, p_values, err);
}
