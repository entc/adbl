#include "prepare.h"
#include "adbl.h"

// cape includes
#include "stc/cape_stream.h"
#include "fmt/cape_json.h"
#include "sys/cape_log.h"

//-----------------------------------------------------------------------------

struct AdblPrepare_s
{
  CapeUdc params;                // owned
  
  CapeUdc values;                // will be transfered
  
  CapeStream stream;
};

//-----------------------------------------------------------------------------

AdblPrepare adbl_prepare_new (CapeUdc* p_params, CapeUdc* p_values)
{
  AdblPrepare self = CAPE_NEW(struct AdblPrepare_s);
  
  self->values = NULL;
  self->params = NULL;
  
  // check all values
  if (p_values)
  {
    CapeUdc values = *p_values;
    
    self->values = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    CapeUdcCursor* cursor = cape_udc_cursor_new (values, CAPE_DIRECTION_FORW);
    
    while (cape_udc_cursor_next (cursor))
    {
      CapeUdc item = cape_udc_cursor_ext (values, cursor);
      
      switch (cape_udc_type(item))
      {
        case CAPE_UDC_STRING:
        case CAPE_UDC_BOOL:
        case CAPE_UDC_FLOAT:
        case CAPE_UDC_NULL:
        case CAPE_UDC_NODE:
        case CAPE_UDC_LIST:
        {
          cape_udc_add (self->values, &item);
          break;
        }
        case CAPE_UDC_NUMBER:
        {
          // check the value
          number_t val = cape_udc_n (item, ADBL_AUTO_INCREMENT);
          
          if (val == ADBL_AUTO_INCREMENT)
          {
            cape_udc_del (&item);
          }
          else if (val == ADBL_AUTO_SEQUENCE_ID)
          {
            cape_udc_del (&item);
          }
          else
          {
            cape_udc_add (self->values, &item);
          }
          
          break;
        }
        default:
        {
          cape_udc_del (&item);
          break;
        }
      }
      
    }
    
    cape_udc_cursor_del (&cursor);
    
    cape_udc_del (p_values);
  }
  
  // params are optional
  if (p_params)
  {
    self->params = *p_params;
    *p_params = NULL;
  }

  self->stream = cape_stream_new ();
  
  return self;
}

//-----------------------------------------------------------------------------

void adbl_prepare_del (AdblPrepare* p_self)
{
  AdblPrepare self = *p_self;
  
  cape_udc_del (&(self->params));
  cape_udc_del (&(self->values));

  cape_stream_del (&(self->stream));
  
  CAPE_DEL(p_self, struct AdblPrepare_s);
}

//-----------------------------------------------------------------------------

int adbl_prepare_execute (const CapeString statement, sqlite3* handle, CapeErr err)
{
  int res;
  char* errmsg;
  
  res = sqlite3_exec (handle, statement, 0, 0, &errmsg);
  
  if( res == SQLITE_OK )
  {
    return CAPE_ERR_NONE;
  }
  else
  {    
    // set the error
    res = cape_err_set (err, CAPE_ERR_3RDPARTY_LIB, errmsg);
    
    sqlite3_free (errmsg);

    return res;
  }
}

//-----------------------------------------------------------------------------

int adbl_prepare_run (AdblPrepare self, sqlite3* handle, CapeErr err)
{
  return adbl_prepare_execute (cape_stream_get (self->stream), handle, err);
}

//-----------------------------------------------------------------------------

number_t adbl_prepare_lastid (AdblPrepare self, sqlite3* handle, CapeErr err)
{
  // TODO: fetch last inserted id
  // select seq from sqlite_sequence where name="table_name"
  

  return 0;
}

//-----------------------------------------------------------------------------

void adbl_pvd_append_table (CapeStream stream, const char* schema, const char* table)
{
  // schema and table name
  cape_stream_append_str (stream, schema );
  cape_stream_append_str (stream, "." );
  cape_stream_append_str (stream, table );
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_constraints (CapeStream stream, CapeUdc params, const char* table)
{
  CapeUdcCursor* cursor = cape_udc_cursor_new (params, CAPE_DIRECTION_FORW);
  
  while (cape_udc_cursor_next (cursor))
  {
    const CapeString param_name = cape_udc_name (cursor->item);
    if (param_name)
    {
      if (cursor->position > 0)
      {
        cape_stream_append_str (stream, " AND ");
      }
      
      cape_stream_append_str (stream, table);
      cape_stream_append_str (stream, ".");
      cape_stream_append_str (stream, param_name);   
      cape_stream_append_str (stream, " = ?");
    }
  }
  
  cape_udc_cursor_del (&cursor);
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_where_clause (CapeStream stream, CapeUdc params, const char* table)
{
  if (params == NULL)
  {
    return;
  }
  
  cape_stream_append_str (stream, " WHERE ");
  
  adbl_prepare_append_constraints (stream, params, table);
}

//-----------------------------------------------------------------------------

void adbl_pvd_append_columns (CapeStream stream, CapeUdc values, const char* table)
{
  CapeUdcCursor* cursor = cape_udc_cursor_new (values, CAPE_DIRECTION_FORW);
  
  while (cape_udc_cursor_next (cursor))
  {
    const CapeString column_name = cape_udc_name (cursor->item);
    
    if (column_name)
    {
      if (cursor->position > 0)
      {
        cape_stream_append_str (stream, ", ");
      }
      
      cape_stream_append_str (stream, table);
      cape_stream_append_str (stream, ".");
      cape_stream_append_str (stream, column_name);
    }
  }
  
  cape_udc_cursor_del (&cursor);
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_val (CapeStream stream, CapeUdc item)
{
  switch (cape_udc_type (item))
  {
    case CAPE_UDC_STRING:
    {
      cape_stream_append_c (stream, '"');
      cape_stream_append_str (stream, cape_udc_s (item, ""));
      cape_stream_append_c (stream, '"');
      break;
    }
    case CAPE_UDC_NUMBER:
    {
      cape_stream_append_n (stream, cape_udc_n (item, 0));
      break;
    }
    case CAPE_UDC_FLOAT:
    {
      cape_stream_append_f (stream, cape_udc_f (item, 0));
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_values (CapeStream stream, CapeUdc values)
{
  CapeUdcCursor* cursor = cape_udc_cursor_new (values, CAPE_DIRECTION_FORW);
  
  while (cape_udc_cursor_next (cursor))
  {
    const CapeString param_name = cape_udc_name (cursor->item);
    
    if (param_name)
    {
      if (cursor->position > 0)
      {
        cape_stream_append_str (stream, ", ");
      }

      adbl_prepare_append_val (stream, cursor->item);
    }
  }
  
  cape_udc_cursor_del (&cursor);
}

//-----------------------------------------------------------------------------

int adbl_prepare_statement_select (AdblPrepare self, const char* schema, const char* table, CapeErr err)
{
  
  
}

//-----------------------------------------------------------------------------

int adbl_prepare_statement_insert (AdblPrepare self, const char* schema, const char* table, CapeErr err)
{
  cape_stream_append_str (self->stream, "INSERT INTO ");
  
  adbl_pvd_append_table (self->stream, schema, table);
  
  cape_stream_append_str (self->stream, " (");
  
  adbl_pvd_append_columns (self->stream, self->values, table);
  
  cape_stream_append_str (self->stream, ") VALUES (");
  
  adbl_prepare_append_values (self->stream, self->values);
  
  cape_stream_append_str (self->stream, ")");
  
  cape_log_msg (CAPE_LL_TRACE, "ADBL", "sqlite3 **SQL**", cape_stream_get (self->stream));
  
  return CAPE_ERR_NONE;
}

//-----------------------------------------------------------------------------

int adbl_prepare_statement_delete (AdblPrepare self, const char* schema, const char* table, CapeErr err)
{
  cape_stream_append_str (self->stream, "DELETE FROM ");
  
  adbl_pvd_append_table (self->stream, schema, table);
  
  adbl_prepare_append_where_clause (self->stream, self->params, table);
}

//-----------------------------------------------------------------------------

int adbl_prepare_statement_update (AdblPrepare self, const char* schema, const char* table, CapeErr err)
{
  
}

//-----------------------------------------------------------------------------

int adbl_prepare_statement_setins (AdblPrepare self, const char* schema, const char* table, CapeErr err)
{
  
}

//-----------------------------------------------------------------------------
