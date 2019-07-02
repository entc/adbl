#include "prepare.h"
#include "adbl.h"

// cape includes
#include "stc/cape_stream.h"
#include "stc/cape_list.h"
#include "sys/cape_log.h"
#include "fmt/cape_json.h"

//-----------------------------------------------------------------------------

struct AdblPrepare_s
{
  CapeUdc params;                // owned
  
  CapeUdc values;                // will be transfered
  
  CapeStream stream;
  
  CapeList binds;
  
  sqlite3_stmt* stmt;
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
  self->stmt = NULL;
  self->binds = cape_list_new (NULL);
  
  return self;
}

//-----------------------------------------------------------------------------

void adbl_prepare_del (AdblPrepare* p_self)
{
  AdblPrepare self = *p_self;
  
  cape_udc_del (&(self->params));
  cape_udc_del (&(self->values));

  cape_stream_del (&(self->stream));
  
  cape_list_del (&(self->binds));
  
  if (self->stmt)
  {
    // free resources
    sqlite3_finalize (self->stmt);
  }
  
  CAPE_DEL(p_self, struct AdblPrepare_s);
}

//-----------------------------------------------------------------------------

void adbl_pvd_append_table (CapeStream stream, const char* schema, const char* table)
{
  // schema and table name
  //cape_stream_append_str (stream, schema );
  //cape_stream_append_str (stream, "." );
  cape_stream_append_str (stream, table );
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

int adbl_prepare_prepare (AdblPrepare self, sqlite3* handle, CapeErr err)
{
  if (sqlite3_prepare_v2 (handle, cape_stream_get (self->stream), cape_stream_size (self->stream), &(self->stmt), NULL) == SQLITE_OK)
  {
    return CAPE_ERR_NONE;
  }
  else
  {    
    // set the error
    return cape_err_set (err, CAPE_ERR_3RDPARTY_LIB, sqlite3_errmsg (handle));
  }
}

//-----------------------------------------------------------------------------

int adbl_prepare_run (AdblPrepare self, sqlite3* handle, CapeErr err)
{
  int res = sqlite3_step (self->stmt);
 
  switch (res)
  {
    // No more data
    case SQLITE_DONE:
    {
      return CAPE_ERR_NONE;
    }
    // New data
    case SQLITE_ROW:
    {
      return CAPE_ERR_NONE;
    }      
    default:
    {
      return CAPE_ERR_NONE;
    }
  }
}

//-----------------------------------------------------------------------------

void adbl_prepare_next__get_value (AdblPrepare self, CapeUdc item, number_t pos)
{
  switch (cape_udc_type (item))
  {
    case CAPE_UDC_NULL:
    {
      break;      
    }
    case CAPE_UDC_STRING:
    {
      const unsigned char* h = sqlite3_column_text (self->stmt, pos);
      cape_udc_set_s_cp (item, (CapeString)h);
      break;
    }
    case CAPE_UDC_NUMBER:
    {
      cape_udc_set_n (item, sqlite3_column_int64 (self->stmt, pos));
      break;
    }
    case CAPE_UDC_FLOAT:
    {
      cape_udc_set_f (item, sqlite3_column_double (self->stmt, pos));
      break;      
    }
    case CAPE_UDC_BOOL:
    {
      cape_udc_set_b (item, sqlite3_column_int (self->stmt, pos));
      break;
    }
    case CAPE_UDC_NODE:
    case CAPE_UDC_LIST:
    {

      break;
    }
    default:
    {

      break;
    }   
  }
}

//-----------------------------------------------------------------------------

int adbl_prepare_next (AdblPrepare self)
{
  int res = sqlite3_step (self->stmt);
  
  switch (res)
  {
    // No more data
    case SQLITE_DONE:
    {
      return FALSE;
    }
    // New data
    case SQLITE_ROW:
    {
      CapeUdcCursor* cursor = cape_udc_cursor_new (self->values, CAPE_DIRECTION_FORW);
      
      while (cape_udc_cursor_next (cursor))
      {
        adbl_prepare_next__get_value (self, cursor->item, cursor->position);
      }
      
      cape_udc_cursor_del (&cursor);
      
      return TRUE;
    }      
    default:
    {
      return FALSE;
    }
  }
}

//-----------------------------------------------------------------------------

CapeUdc adbl_prepare_values (AdblPrepare self)
{
  return self->values;
}

//-----------------------------------------------------------------------------

number_t adbl_prepare_lastid (AdblPrepare self, sqlite3* handle, const char* schema, const char* table, CapeErr err)
{
  // free resources
  sqlite3_finalize (self->stmt);
  
  cape_stream_clr (self->stream);
  
  cape_udc_del (&(self->values));
  
  self->values = cape_udc_new (CAPE_UDC_NODE, NULL);
  
  cape_udc_add_n (self->values, "seq", 0);
  
  cape_stream_append_str (self->stream, "SELECT seq FROM sqlite_sequence WHERE name = \"");

  adbl_pvd_append_table (self->stream, schema, table);
  
  cape_stream_append_c (self->stream, '"');
  
  cape_log_msg (CAPE_LL_TRACE, "ADBL", "sqlite3 **SQL**", cape_stream_get (self->stream));
  
  if (sqlite3_prepare_v2 (handle, cape_stream_get (self->stream), cape_stream_size (self->stream), &(self->stmt), NULL) != SQLITE_OK)
  {
    return 0;
  }
  
  if (!adbl_prepare_next (self))
  {
    return 0;
  }

  return cape_udc_get_n (self->values, "seq", 0);
}

//-----------------------------------------------------------------------------

void adbl_prepare_binds__clear_string (void* ptr)
{
  CapeString h = ptr; cape_str_del (&h);
}

//-----------------------------------------------------------------------------

int adbl_prepare_binds_val (AdblPrepare self, CapeUdc item, number_t pos, CapeErr err)
{
  int res;
  
  switch (cape_udc_type(item))
  {
    case CAPE_UDC_NULL:
    {
      res = sqlite3_bind_null (self->stmt, pos);
      break;      
    }
    case CAPE_UDC_STRING:
    {
      res = sqlite3_bind_text (self->stmt, pos, cape_udc_s (item, NULL), -1, SQLITE_STATIC);  // don't free the string
      break;
    }
    case CAPE_UDC_NUMBER:
    {
      res = sqlite3_bind_int64 (self->stmt, pos, cape_udc_n (item, 0));
      break;
    }
    case CAPE_UDC_FLOAT:
    {
      res = sqlite3_bind_double (self->stmt, pos, cape_udc_f (item, .0));
      break;      
    }
    case CAPE_UDC_BOOL:
    {
      res = sqlite3_bind_int (self->stmt, pos, cape_udc_b (item, FALSE));     
      break;
    }
    case CAPE_UDC_NODE:
    case CAPE_UDC_LIST:
    {
      res = sqlite3_bind_text (self->stmt, pos, cape_json_to_s (item), -1, adbl_prepare_binds__clear_string);
      break;
    }
    default:
    {
      res = cape_err_set (err, CAPE_ERR_WRONG_VALUE, "UDC type not supported");
      break;
    }
  }
  
  return res;
}

//-----------------------------------------------------------------------------

int adbl_prepare_bind (AdblPrepare self, CapeErr err)
{
  int res;
  
  CapeListCursor* cursor = cape_list_cursor_create (self->binds, CAPE_DIRECTION_FORW);

  while (cape_list_cursor_next (cursor))
  {
    res = adbl_prepare_binds_val (self, cape_list_node_data (cursor->node), cursor->position + 1, err);
    if (res)
    {
      goto exit_and_cleanup;
    }
  }

  res = CAPE_ERR_NONE;
  
exit_and_cleanup:
  
  cape_list_cursor_destroy (&cursor);
  
  return res;
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_values (AdblPrepare self, CapeStream stream, CapeUdc values)
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

      cape_stream_append_c (stream, '?');
      
      cape_list_push_back (self->binds, cursor->item);
    }
  }
  
  cape_udc_cursor_del (&cursor);
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
      
      cape_stream_append_str (stream, column_name);
    }
  }
  
  cape_udc_cursor_del (&cursor);
}

//-----------------------------------------------------------------------------

void adbl_pvd_append_update (AdblPrepare self, CapeStream stream, CapeUdc values, const char* table)
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
      
      cape_stream_append_str (stream, table );
      cape_stream_append_str (stream, "." );
      cape_stream_append_str (stream, column_name);
      cape_stream_append_str (stream, " = ?" );
      
      cape_list_push_back (self->binds, cursor->item);
    }
  }
  
  cape_udc_cursor_del (&cursor);
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_constraints (AdblPrepare self, CapeStream stream, CapeUdc params, const char* table)
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
      
      cape_list_push_back (self->binds, cursor->item);
    }
  }
  
  cape_udc_cursor_del (&cursor);
}

//-----------------------------------------------------------------------------

void adbl_prepare_append_where_clause (AdblPrepare self, CapeStream stream, CapeUdc params, const char* table)
{
  if (params == NULL)
  {
    return;
  }
  
  cape_stream_append_str (stream, " WHERE ");
  
  adbl_prepare_append_constraints (self, stream, params, table);
}

//-----------------------------------------------------------------------------

void adbl_prepare_statement_select (AdblPrepare self, const char* schema, const char* table)
{
  cape_stream_append_str (self->stream, "SELECT ");
  
  adbl_pvd_append_columns (self->stream, self->values, table);
  
  cape_stream_append_str (self->stream, " FROM ");

  adbl_pvd_append_table (self->stream, schema, table);
  
  adbl_prepare_append_where_clause (self, self->stream, self->params, table);

  cape_log_msg (CAPE_LL_TRACE, "ADBL", "sqlite3 **SQL**", cape_stream_get (self->stream));
}

//-----------------------------------------------------------------------------

void adbl_prepare_statement_insert (AdblPrepare self, const char* schema, const char* table)
{
  cape_stream_append_str (self->stream, "INSERT INTO ");
  
  adbl_pvd_append_table (self->stream, schema, table);
  
  cape_stream_append_str (self->stream, " (");
  
  adbl_pvd_append_columns (self->stream, self->values, table);
  
  cape_stream_append_str (self->stream, ") VALUES (");
  
  adbl_prepare_append_values (self, self->stream, self->values);
  
  cape_stream_append_str (self->stream, ")");
  
  cape_log_msg (CAPE_LL_TRACE, "ADBL", "sqlite3 **SQL**", cape_stream_get (self->stream));
}

//-----------------------------------------------------------------------------

void adbl_prepare_statement_delete (AdblPrepare self, const char* schema, const char* table)
{
  cape_stream_append_str (self->stream, "DELETE FROM ");
  
  adbl_pvd_append_table (self->stream, schema, table);
  
  adbl_prepare_append_where_clause (self, self->stream, self->params, table);
}

//-----------------------------------------------------------------------------

void adbl_prepare_statement_update (AdblPrepare self, const char* schema, const char* table)
{
  cape_stream_append_str (self->stream, "UPDATE ");
  
  adbl_pvd_append_table (self->stream, schema, table);
  
  cape_stream_append_str (self->stream, " SET ");
  
  adbl_pvd_append_update (self, self->stream, self->values, table);
  
  adbl_prepare_append_where_clause (self, self->stream, self->params, table);
}

//-----------------------------------------------------------------------------

void adbl_prepare_statement_setins (AdblPrepare self, const char* schema, const char* table)
{
  
}

//-----------------------------------------------------------------------------
