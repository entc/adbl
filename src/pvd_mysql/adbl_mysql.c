#include "adbl_mysql.h"

#include "bindvars.h"
#include "prepare.h"

// cape includes
#include "sys/cape_types.h"
#include "stc/cape_stream.h"
#include "sys/cape_log.h"
#include "sys/cape_mutex.h"
#include "fmt/cape_json.h"

#include <stdio.h>

//-----------------------------------------------------------------------------

#include <mysql.h>

static int init_status = 0;

#if defined __WINDOWS_OS

#include <windows.h>

// fix for linking mariadb library
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")

//------------------------------------------------------------------------------------------------------

// this might work?
/*
extern "C" BOOL WINAPI DllMain (HINSTANCE const instance, DWORD const reason, LPVOID const reserved)
{
  // Perform actions based on the reason for calling.
  switch (reason)
  {
    case DLL_PROCESS_ATTACH:
    {
      cape_log_msg (CAPE_LL_DEBUG, "ADBL", "load library", "MYSQL INIT");
      
      mysql_library_init (0, NULL, NULL);  
      
      break;
    }      
    case DLL_THREAD_ATTACH:
    {
      
      break;
    }      
    case DLL_THREAD_DETACH:
    {
      
      break;
    }      
    case DLL_PROCESS_DETACH:
    {
      cape_log_msg (CAPE_LL_DEBUG, "ADBL", "unload library", "MYSQL DONE");
      
      mysql_thread_end ();
      
      mysql_library_end ();
      
      break;
    }
  }
  
  return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
*/
//------------------------------------------------------------------------------------------------------

#else

//------------------------------------------------------------------------------------------------------

void __attribute__ ((constructor)) library_init (void)
{
  unsigned long vv = mysql_get_client_version ();
  
  number_t major = vv / 10000;
  number_t release = (vv - (major * 10000)) / 100;
  number_t sub = (vv - (major * 10000) - (release * 100));
  
  
  cape_log_fmt (CAPE_LL_DEBUG, "ADBL", "load library", "MYSQL INIT: Version (%i.%02i.%02i)", major, release, sub);
  
  // TODO: this might hang in various mariadb client versions
  //    -> mariadb version 5.5.33 - 1.161 - x86_64
  mysql_library_init (0, NULL, NULL);
}

//------------------------------------------------------------------------------------------------------

void __attribute__ ((destructor)) library_fini (void)
{
  cape_log_msg (CAPE_LL_DEBUG, "ADBL", "unload library", "MYSQL DONE");

  mysql_thread_end ();
  
  mysql_library_end ();
}

#endif

//------------------------------------------------------------------------------------------------------

void adbl_mysql_init ()
{
  if (init_status == 0)
  {
    mysql_library_init (0, NULL, NULL);
  }
  
  init_status++;
}

//------------------------------------------------------------------------------------------------------

void adbl_mysql_done ()
{
  init_status--;
  
  if (init_status == 0)
  {
    mysql_library_end ();    
  }
}

//-----------------------------------------------------------------------------

struct AdblPvdSession_s
{
  MYSQL* mysql;      // mysql clientlibrary handle
  
  char* schema;      // the schema used for statements
  
  int ansi_quotes;   // defines if the staments must be quoted
  
  CapeUdc cp;
  
  int max_retries;
  
  CapeMutex mutex;
};

//-----------------------------------------------------------------------------

int adbl_pvd__error (AdblPvdSession self, CapeErr err)
{
  unsigned int error_code = mysql_errno (self->mysql);
  if (error_code)
  {
    return cape_err_set_fmt (err, CAPE_ERR_3RDPARTY_LIB, "%i (%s): %s", error_code, mysql_sqlstate (self->mysql), mysql_error (self->mysql));
  }
  else
  {
    return CAPE_ERR_NONE;
  }
}

//-----------------------------------------------------------------------------

int adbl_pvd_connect (AdblPvdSession self, CapeErr err)
{
  int res;
  int timeout;
  my_bool reconnect;
  
  // settings
  mysql_options (self->mysql, MYSQL_OPT_COMPRESS, 0);
  
  reconnect = FALSE;
  mysql_options (self->mysql, MYSQL_OPT_RECONNECT, &reconnect);
  
  // set propper read timeout
  timeout = 1;
  mysql_options (self->mysql, MYSQL_OPT_READ_TIMEOUT, &timeout);
  
  cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [opts]: ", "timeout read:  %is", timeout);
  
  // set propper write timeout
  timeout = 1;
  mysql_options (self->mysql, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
  
  cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [opts]: ", "timeout write: %is", timeout);
  
  // we start with no transaction -> activate autocommit
  mysql_options (self->mysql, MYSQL_INIT_COMMAND, "SET autocommit=1");
  
  // important, otherwise UTF8 is not handled correctly
  mysql_options (self->mysql, MYSQL_INIT_COMMAND, "SET NAMES UTF8");
  
  {
    const CapeString host = cape_udc_get_s (self->cp, "host", "127.0.0.1");
    const CapeString user = cape_udc_get_s (self->cp, "user", "admin");
    const CapeString pass = cape_udc_get_s (self->cp, "pass", "admin");
    number_t port = cape_udc_get_n (self->cp, "port", 3306);
    
    cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [host]: ", "'%s'", host);
    cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [port]: ", "'%i'", port);
    cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [user]: ", "'%s'", user);
    cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [pass]: ", "'%s'", pass);
    cape_log_fmt (CAPE_LL_TRACE, "ADBL", "connect [dbas]: ", "'%s'", self->schema);

    // connect
    if (mysql_real_connect (self->mysql, host, user, pass, self->schema, port, 0, CLIENT_MULTI_RESULTS) != self->mysql)
    {
      res = adbl_pvd__error (self, err);
      goto exit_and_cleanup;
    }
  }
  
  // find out the ansi variables
  {
    MYSQL_RES* mr;
    
    mysql_query (self->mysql, "SELECT @@global.sql_mode");
    mr = mysql_use_result (self->mysql);
    if(mr)
    {
      MYSQL_ROW row;
      row = mysql_fetch_row (mr);
      
      self->ansi_quotes = strstr (row[0], "ANSI_QUOTES" ) != 0;
      
      mysql_free_result(mr);
    }
  }
  
  res = CAPE_ERR_NONE;
  
exit_and_cleanup:

  return res;
}

//-----------------------------------------------------------------------------

int adbl_check_error (AdblPvdSession self, unsigned int error_code, CapeErr err)
{
  // log the error code
  cape_log_fmt (CAPE_LL_ERROR, "ADBL", "mysql error", "ERROR: %i", error_code);
  
  switch (error_code)
  {
    case 1021:   // HY000: ER_DISK_FULL
    {
      // add to fatal system: send some sms or so the responsible person
      cape_log_msg (CAPE_LL_FATAL, "ADBL", "adbl check error", "disk is full");
      break;
    }
    case 1152:   // 08S01: ER_ABORTING_CONNECTION
    case 2006:   // HY000: MySQL server has gone away
    case 2013:   // HY000: Lost connection to MySQL server during query
    {
      int res;
      
      cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql error", "server went away -> try to reconnect");

      // disconnect
      mysql_close (self->mysql);

      // re-initialize the mysql handle
      self->mysql = mysql_init (NULL);

      // clear error log
      cape_err_clr (err);
      
      // try to connect
      res = adbl_pvd_connect (self, err);
      if (res)
      {
        cape_log_fmt (CAPE_LL_ERROR, "ADBL", "mysql error", "can't reconnect: %s", cape_err_text (err));
        return res;
      }
      else
      {        
        cape_log_fmt (CAPE_LL_DEBUG, "ADBL", "mysql error", "successful reconnected");
        return CAPE_ERR_CONTINUE;
      }
    }
  }
  
  return CAPE_ERR_3RDPARTY_LIB;
}

//-----------------------------------------------------------------------------

AdblPvdSession __STDCALL adbl_pvd_open (CapeUdc cp, CapeErr err)
{
  int res;
  AdblPvdSession self = CAPE_NEW (struct AdblPvdSession_s);
  
  self->max_retries = 5;
  self->ansi_quotes = FALSE;
  
  self->schema = cape_str_cp (cape_udc_get_s (cp, "schema", NULL));
  self->cp = cape_udc_cp (cp);
  
  self->mutex = cape_mutex_new ();
  
  // init mysql
  self->mysql = mysql_init (NULL);
  
  // the initial connect should work
  res = adbl_pvd_connect (self, err);
  if (res)
  {
    adbl_pvd_close (&self);
    return NULL;
  }
  
  return self;
}

//-----------------------------------------------------------------------------

AdblPvdSession __STDCALL adbl_pvd_clone (AdblPvdSession rhs, CapeErr err)
{
  int res;
  AdblPvdSession self = CAPE_NEW (struct AdblPvdSession_s);
  
  self->max_retries = 5;
  self->ansi_quotes = FALSE;
    
  self->schema = cape_str_cp (rhs->schema);
  self->cp = cape_udc_cp (rhs->cp);
  
  self->mutex = cape_mutex_new ();

  // init mysql
  self->mysql = mysql_init (NULL);
  
  // the initial connect should work
  res = adbl_pvd_connect (self, err);
  if (res)
  {
    adbl_pvd_close (&self);
    return NULL;
  }
  
  // to be on the safe side
  mysql_thread_end ();
  
  return self;
}

//-----------------------------------------------------------------------------

void __STDCALL adbl_pvd_close (AdblPvdSession* p_self)
{
  AdblPvdSession self = *p_self;
  
  cape_log_msg (CAPE_LL_DEBUG, "ADBL", "mysql", "session closed");
  
  cape_udc_del (&(self->cp));
  cape_str_del (&(self->schema));
  
  mysql_close (self->mysql);
  
  cape_mutex_del (&(self->mutex));
  
  CAPE_DEL(p_self, struct AdblPvdSession_s);
}

//-----------------------------------------------------------------------------

CapeUdc __STDCALL adbl_pvd_get (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  AdblPvdCursor cursor = adbl_pvd_cursor_new (self, table, p_params, p_values, err);
  
  if (cursor == NULL)
  {
    cape_log_msg (CAPE_LL_ERROR, "ADBL", "mysql get", cape_err_text(err));
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

  // local objects
  AdblPrepare pre = NULL;
  
  // some prechecks
  if (NULL == p_values)
  {
    res = cape_err_set (err, CAPE_ERR_MISSING_PARAM, "values was not provided");
    goto exit_and_cleanup;
  }
  
  // some prechecks
  if (0 == cape_udc_size (*p_values))
  {
    res = CAPE_ERR_NONE;
    goto exit_and_cleanup;
  }
  
  pre = adbl_prepare_new (NULL, p_values);

  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql insert", "enter new cycle #1 -> [%i]", i);
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql insert", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_statement_insert (pre, self, self->schema, table, self->ansi_quotes, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql insert", "enter new cycle #2 -> [%i]", i);
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql insert", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_binds_values (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql insert", "enter new cycle #3 -> [%i]", i);
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql insert", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql insert", "enter new cycle #4 -> [%i]", i);
          continue;
        }
        
        goto exit_and_cleanup;
      }

      // get last inserted id
      last_insert_id = (number_t)mysql_insert_id (self->mysql);

      // done
      break;
    }
  }
    
exit_and_cleanup:
  
  adbl_prepare_del (&pre);

  cape_mutex_unlock (self->mutex);

  return last_insert_id;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_del (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeErr err)
{
  int res;
  
  AdblPrepare pre = adbl_prepare_new (p_params, NULL);
  
  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql delete", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_statement_delete (pre, self, self->schema, table, self->ansi_quotes, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql delete", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_binds_params (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql delete", cape_err_text(err));    
        goto exit_and_cleanup;    
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        goto exit_and_cleanup;
      }

      // done
      break;
    }
  }
  
  res = CAPE_ERR_NONE;
  
exit_and_cleanup:
  
  adbl_prepare_del (&pre);

  cape_mutex_unlock (self->mutex);

  return res;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_set (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res;
    
  // local objects
  AdblPrepare pre = NULL;
  
  // some prechecks
  if (NULL == p_params)
  {
    res = cape_err_set (err, CAPE_ERR_MISSING_PARAM, "params was not provided");
    goto exit_and_cleanup;
  }
  
  // some prechecks
  if (0 == cape_udc_size (*p_params))
  {
    res = cape_err_set (err, CAPE_ERR_MISSING_PARAM, "params was empty");
    goto exit_and_cleanup;
  }

  // some prechecks
  if (NULL == p_values)
  {
    res = cape_err_set (err, CAPE_ERR_MISSING_PARAM, "values was not provided");
    goto exit_and_cleanup;
  }
  
  // some prechecks
  if (0 == cape_udc_size (*p_values))
  {
    res = CAPE_ERR_NONE;
    goto exit_and_cleanup;
  }
  
  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  pre = adbl_prepare_new (p_params, p_values);

  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql insert", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_statement_update (pre, self, self->schema, table, self->ansi_quotes, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql set", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      // all binds are done as parameter
      res = adbl_prepare_binds_all (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql set", cape_err_text(err));    
        goto exit_and_cleanup;    
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        goto exit_and_cleanup;
      }

      // done
      break;
    }
  }
    
  res = CAPE_ERR_NONE;
  
exit_and_cleanup:
  
  adbl_prepare_del (&pre);

  cape_mutex_unlock (self->mutex);

  return res;
}

//-----------------------------------------------------------------------------

number_t __STDCALL adbl_pvd_ins_or_set (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res;
  number_t last_insert_id = 0;

  // local objects
  AdblPrepare pre = NULL;
  
  // some prechecks
  if (NULL == p_params)
  {
    res = cape_err_set (err, CAPE_ERR_MISSING_PARAM, "params was not provided");
    goto exit_and_cleanup;
  }
  
  // some prechecks
  if (0 == cape_udc_size (*p_params))
  {
    res = CAPE_ERR_NONE;
    goto exit_and_cleanup;
  }
  
  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  pre = adbl_prepare_new (p_params, p_values);

  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql insert", cape_err_text(err));    
        goto exit_and_cleanup;
      }

      res = adbl_prepare_statement_setins (pre, self, self->schema, table, self->ansi_quotes, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql ins_or_set", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      // all binds are done as parameter
      res = adbl_prepare_binds_all (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql ins_or_set", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        goto exit_and_cleanup;
      }

      // done
      break;
    }
  }

  // get last inserted id
  last_insert_id = (number_t)mysql_insert_id (self->mysql);

exit_and_cleanup:
  
  adbl_prepare_del (&pre);

  cape_mutex_unlock (self->mutex);

  return last_insert_id;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_begin (AdblPvdSession self, CapeErr err)
{
  int i;
  for (i = 0; i < self->max_retries; i++)
  {
    // mysqlclient is not thread safe, so we need to protect the resource with mutex
    cape_mutex_lock (self->mutex);

    if (mysql_query (self->mysql, "START TRANSACTION") != 0)
    {
      unsigned int error_code = mysql_errno (self->mysql);
      
      cape_log_fmt (CAPE_LL_ERROR, "ADBL", "begin", "error seen: %i", error_code);    
      
      // try to figure out if the error was serious
      int res = adbl_check_error (self, error_code, err);

      cape_mutex_unlock (self->mutex);

      if (res == CAPE_ERR_CONTINUE)
      {
        cape_log_fmt (CAPE_LL_TRACE, "ADBL", "begin", "trigger next cycle");    
        continue;
      }
      
      return cape_err_set_fmt (err, CAPE_ERR_3RDPARTY_LIB, "%i (%s): %s", error_code, mysql_sqlstate (self->mysql), mysql_error (self->mysql));
    }
    
    cape_mutex_unlock (self->mutex);
    break;
  }
    
  return CAPE_ERR_NONE;
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_commit (AdblPvdSession self, CapeErr err)
{
  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  mysql_query (self->mysql, "COMMIT");
  
  cape_mutex_unlock (self->mutex);

  return adbl_pvd__error (self, err);
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_rollback (AdblPvdSession self, CapeErr err)
{
  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  mysql_query (self->mysql, "ROLLBACK");

  cape_mutex_unlock (self->mutex);

  return adbl_pvd__error (self, err);
}

//-----------------------------------------------------------------------------

AdblPvdCursor __STDCALL adbl_pvd_cursor_new (AdblPvdSession self, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr err)
{
  int res;
  AdblPrepare pre = adbl_prepare_new (p_params, p_values);
  
  cape_mutex_lock (self->mutex);
  
  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql cursor", "enter new cycle #1 -> [%i]", i);    
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql cursor", cape_err_text(err));    
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_statement_select (pre, self, self->schema, table, self->ansi_quotes, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql cursor", "enter new cycle #2 -> [%i]", i);    
          continue;
        }
        
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_binds_params (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql cursor", "enter new cycle #3 -> [%i]", i);    
          continue;
        }
        
        goto exit_and_cleanup;    
      }
      
      res = adbl_prepare_binds_result (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql cursor", "enter new cycle #4 -> [%i]", i);
          continue;
        }
        
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          cape_log_fmt (CAPE_LL_TRACE, "ADBL", "mysql cursor", "enter new cycle #5 -> [%i]", i);
          continue;
        }
        
        goto exit_and_cleanup;    
      }

      // done
      break;
    }
  }
  
  {
    AdblPvdCursor ret = adbl_prepare_to_cursor (&pre);
    
    cape_mutex_unlock (self->mutex);

    return ret;
  }
  
// --------------
exit_and_cleanup:
  
  cape_mutex_unlock (self->mutex);

  adbl_prepare_del (&pre);
  return NULL;
}

//-----------------------------------------------------------------------------

void __STDCALL adbl_pvd_cursor_del (AdblPvdCursor* p_self)
{
  AdblPvdCursor self = *p_self;

  mysql_stmt_free_result (self->stmt);
  
  mysql_stmt_close (self->stmt);
  
  // clean up the array
  adbl_bindvars_del (&(self->binds));   
  
  cape_udc_del (&(self->values));
  
  CAPE_DEL (p_self, struct AdblPvdCursor_s);
}

//-----------------------------------------------------------------------------

int __STDCALL adbl_pvd_cursor_next (AdblPvdCursor self)
{
  switch (mysql_stmt_fetch (self->stmt))
  {
    case 0:
    {
      return TRUE;
    }
    case MYSQL_NO_DATA:
    {
      return FALSE;
    }
    case 1:   // some kind of error happened
    {
      cape_log_msg (CAPE_LL_ERROR, "ADBL", "cursor next", mysql_stmt_error(self->stmt));
      return FALSE;
    }
    case MYSQL_DATA_TRUNCATED:  // the data in the column don't fits into the binded buffer (1024)
    {
      cape_log_msg (CAPE_LL_WARN, "ADBL", "cursor next", "data truncated");      
      return TRUE;
    }
  }
  
  return TRUE;
}

//-----------------------------------------------------------------------------

CapeUdc __STDCALL adbl_pvd_cursor_get (AdblPvdCursor self)
{
  CapeUdc result_row = cape_udc_new (CAPE_UDC_NODE, NULL);
  
  {
    int i = 0;
    CapeUdcCursor* cursor = cape_udc_cursor_new (self->values, CAPE_DIRECTION_FORW);
    
    while (cape_udc_cursor_next (cursor))
    {
      const CapeString column_name = cape_udc_name (cursor->item);
      
      if (column_name)
      {
        CapeUdc item = cape_udc_new (cape_udc_type (cursor->item), column_name);
        
        if (adbl_bindvars_get (self->binds, i, item))
        {
          cape_udc_add (result_row, &item);
        }
        else
        {
          cape_udc_del (&item);
        }
        
        i++;
      }
    }
    
    cape_udc_cursor_del (&cursor);
  }
  
  return result_row;
}

//-----------------------------------------------------------------------------

number_t __STDCALL adbl_pvd_atomic_dec (AdblPvdSession self, const char* table, CapeUdc* p_params, const CapeString atomic_value, CapeErr err)
{
  number_t ret = -1;
  
  int res;
  AdblPrepare pre = adbl_prepare_new (p_params, NULL);

  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql atomic dec", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_statement_atodec (pre, self, self->schema, table, self->ansi_quotes, atomic_value, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql atomic dec", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_binds_params (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql atomic dec", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        goto exit_and_cleanup;
      }

      // done
      break;
    }
  }
  
  // get last inserted id
  ret = (number_t)mysql_insert_id (self->mysql);

  // to be on the safe side, clear the error aswell
  cape_err_clr (err);
  
exit_and_cleanup:
  
  adbl_prepare_del (&pre);

  cape_mutex_unlock (self->mutex);

  return ret;
}

//-----------------------------------------------------------------------------

number_t __STDCALL adbl_pvd_atomic_inc (AdblPvdSession self, const char* table, CapeUdc* p_params, const CapeString atomic_value, CapeErr err)
{
  number_t ret = -1;
  
  int res;
  AdblPrepare pre = adbl_prepare_new (p_params, NULL);

  // mysqlclient is not thread safe, so we need to protect the resource with mutex
  cape_mutex_lock (self->mutex);

  // run the procedure
  {
    int i;
    for (i = 0; i < self->max_retries; i++)
    {
      res = adbl_prepare_init (pre, self, self->mysql, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql atomic dec", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_statement_atoinc (pre, self, self->schema, table, self->ansi_quotes, atomic_value, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql atomic dec", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_binds_params (pre, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        cape_log_msg (CAPE_LL_WARN, "ADBL", "mysql atomic dec", cape_err_text(err));
        goto exit_and_cleanup;
      }
      
      res = adbl_prepare_execute (pre, self, err);
      if (res)
      {
        if (res == CAPE_ERR_CONTINUE)
        {
          continue;
        }
        
        goto exit_and_cleanup;
      }

      // done
      break;
    }
  }
  
  // get last inserted id
  ret = (number_t)mysql_insert_id (self->mysql);

  // to be on the safe side, clear the error aswell
  cape_err_clr (err);
  
exit_and_cleanup:
  
  adbl_prepare_del (&pre);

  cape_mutex_unlock (self->mutex);

  return ret;
}

//-----------------------------------------------------------------------------
