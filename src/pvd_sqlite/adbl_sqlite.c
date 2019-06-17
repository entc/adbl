#include "adbl_sqlite.h"
#include "prepare.h"

// cape includes
#include "sys/cape_types.h"
#include "stc/cape_stream.h"
#include "sys/cape_log.h"
#include "sys/cape_file.h"

//-----------------------------------------------------------------------------

#include "sqlite3.h"

//-----------------------------------------------------------------------------

struct AdblPvdSession_s
{
  sqlite3* handle;   // mysql clientlibrary handle
  
  char* schema;      // the schema used for statements
  
  CapeString file;
  
  sqlite3_mutex* mutex;
  
};

//-----------------------------------------------------------------------------

AdblPvdSession __STDCALL adbl_pvd_open (CapeUdc cp, CapeErr err)
{
  AdblPvdSession self = CAPE_NEW (struct AdblPvdSession_s);

  self->schema = cape_str_cp (cape_udc_get_s (cp, "schema", NULL));
  self->handle = NULL;
  self->file = NULL;

  // get the file
  {
    const CapeString database_file = cape_udc_get_s (cp, "dbfile", NULL);
    if (database_file == NULL)
    {
      cape_err_set (err, CAPE_ERR_NOT_FOUND, "database file not found");
      goto exit_and_cleanup;
    }
    
    self->file = cape_fs_path_absolute (database_file);
  }

  // open the file
  {
    int res = sqlite3_open (self->file, &(self->handle));
    if( res == SQLITE_OK )
    {

      
    }
    else
    {
      cape_err_set (err, CAPE_ERR_3RDPARTY_LIB, "database can't be opened");
      goto exit_and_cleanup;
    }
  }
  
  self->mutex = sqlite3_mutex_alloc (SQLITE_MUTEX_FAST);
  
  return self;
  
exit_and_cleanup:

  adbl_pvd_close (&self);
  
  return self;
}

//-----------------------------------------------------------------------------

void __STDCALL adbl_pvd_close (AdblPvdSession* p_self)
{
  AdblPvdSession self = *p_self;
  
  cape_log_msg (CAPE_LL_DEBUG, "ADBL", "sqlite3", "session closed");
  
  cape_str_del (&(self->schema));
  cape_str_del (&(self->file));
  
  if (self->handle)
  {
    sqlite3_close (self->handle);    
  }
  
  sqlite3_mutex_free (self->mutex);
  
  CAPE_DEL(p_self, struct AdblPvdSession_s);
}

//-----------------------------------------------------------------------------

CapeUdc __STDCALL adbl_pvd_get (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  AdblPvdCursor cursor = adbl_pvd_cursor_new (self, table, p_params, p_values, err);
  
  if (cursor == NULL)
  {
    cape_log_msg (CAPE_LL_ERROR, "ADBL", "sqlite get", cape_err_text(err));
    return NULL;
  }
  
  {
    CapeUdc results = cape_udc_new (CAPE_UDC_LIST, NULL);

    {
      // fetch all rows and merge them into one result set  
      while (adbl_pvd_cursor_next (cursor))
      {
        // this will transfer the ownership
        CapeUdc result_row = adbl_pvd_cursor_get (cursor);
        
        // add this rowto the list
        cape_udc_add (results, &result_row);
      }
      
      adbl_pvd_cursor_del (&cursor);
    }
        
    return results;
  }
}

//-----------------------------------------------------------------------------

number_t __STDCALL adbl_pvd_ins (AdblPvdSession self, const char* table, CapeUdc* p_values, CapeErr err)
{
  int res;
  number_t last_insert_id = 0;
  
  AdblPrepare pre = adbl_prepare_new (NULL, p_values);
  
  adbl_prepare_statement_insert (pre, self->schema, table);
  
  res = adbl_prepare_prepare (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_bind (pre, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_run (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  last_insert_id = adbl_prepare_lastid (pre, self->handle, self->schema, table, err);
  
exit_and_cleanup:
  
  if (cape_err_code (err))
  {
    cape_log_msg (CAPE_LL_WARN, "ADBL", "sqlite3 insert", cape_err_text(err));    
  }
  
  adbl_prepare_del (&pre);  
  return last_insert_id;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_del (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeErr err)
{
  int res;
  AdblPrepare pre = adbl_prepare_new (p_params, NULL);
  
  adbl_prepare_statement_delete (pre, self->schema, table);
  
  res = adbl_prepare_prepare (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_bind (pre, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_run (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = CAPE_ERR_NONE;
  
exit_and_cleanup:
  
  adbl_prepare_del (&pre);  
  return res;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_set (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res;
  AdblPrepare pre = adbl_prepare_new (p_params, p_values);
  
  adbl_prepare_statement_update (pre, self->schema, table);
  
  res = adbl_prepare_prepare (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_bind (pre, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_run (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = CAPE_ERR_NONE;
  
exit_and_cleanup:
  
  adbl_prepare_del (&pre);  
  return res;
}

//-----------------------------------------------------------------------------

number_t __STDCALL adbl_pvd_ins_or_set (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res;
  number_t last_insert_id = 0;
  
  AdblPrepare pre = adbl_prepare_new (p_params, p_values);
  
  adbl_prepare_statement_setins (pre, self->schema, table);
  
  res = adbl_prepare_prepare (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_bind (pre, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_run (pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  last_insert_id = adbl_prepare_lastid (pre, self->handle, self->schema, table, err);
  
  exit_and_cleanup:
  
  adbl_prepare_del (&pre);  
  return last_insert_id;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_begin (AdblPvdSession self, CapeErr err)
{
  int res = adbl_prepare_execute ("BEGIN", self->handle, err);
  if (res)
  {
    cape_log_msg (CAPE_LL_WARN, "ADBL", "sqlite3 begin", cape_err_text(err));    
    return res;
  }
  else
  {
    return CAPE_ERR_NONE;
  }
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_commit (AdblPvdSession self, CapeErr err)
{
  int res = adbl_prepare_execute ("COMMIT", self->handle, err);
  if (res)
  {
    cape_log_msg (CAPE_LL_WARN, "ADBL", "sqlite3 commit", cape_err_text(err));    
    return res;
  }
  else
  {
    return CAPE_ERR_NONE;
  }
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_rollback (AdblPvdSession self, CapeErr err)
{
  int res = adbl_prepare_execute ("ROLLBACK", self->handle, err);
  if (res)
  {
    cape_log_msg (CAPE_LL_WARN, "ADBL", "sqlite3 rollback", cape_err_text(err));    
    return res;
  }
  else
  {
    return CAPE_ERR_NONE;
  }
}

//-----------------------------------------------------------------------------

struct AdblPvdCursor_s
{
  AdblPrepare pre;
};

//-----------------------------------------------------------------------------

AdblPvdCursor __STDCALL adbl_pvd_cursor_new (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res;
  
  AdblPvdCursor cursor = CAPE_NEW (struct AdblPvdCursor_s);
  
  cursor->pre = adbl_prepare_new (p_params, p_values);
  
  adbl_prepare_statement_select (cursor->pre, self->schema, table);
  
  res = adbl_prepare_prepare (cursor->pre, self->handle, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  res = adbl_prepare_bind (cursor->pre, err);
  if (res)
  {
    goto exit_and_cleanup;
  }
  
  return cursor;
  
// --------------
exit_and_cleanup:
  
  adbl_pvd_cursor_del (&cursor);
  return NULL;
}

//-----------------------------------------------------------------------------

void __STDCALL adbl_pvd_cursor_del (AdblPvdCursor* p_self)
{
  int i;
  AdblPvdCursor self = *p_self;

  adbl_prepare_del (&(self->pre)); 
  
  CAPE_DEL (p_self, struct AdblPvdCursor_s);
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_cursor_next (AdblPvdCursor self)
{
  return adbl_prepare_next (self->pre);
}

//-----------------------------------------------------------------------------

CapeUdc __STDCALL adbl_pvd_cursor_get (AdblPvdCursor self)
{
  return cape_udc_cp (adbl_prepare_values (self->pre));
}

//-----------------------------------------------------------------------------
