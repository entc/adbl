#include "adbl.h"

#include <stc/cape_udc.h>
#include <sys/cape_queue.h>

//-----------------------------------------------------------------------------

#include <stdio.h>

//-----------------------------------------------------------------------------

static void __STDCALL worker_part (void* ptr, number_t pos)
{
  AdblTrx trx = NULL;
  AdblSession session = ptr;
  number_t last_inserted_row = 0;
  int i;
  
  CapeErr err = cape_err_new ();
  
  trx = adbl_trx_new (session, err);
  if (trx == NULL)
  {
    goto exit;
  }
  
  // fetch
  {
    CapeUdc results;
    
    //CapeUdc params = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    
    //cape_udc_add_n       (params, "id", 1);
    
    CapeUdc columns = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    
    // define the columns we want to fetch
    cape_udc_add_n       (columns, "fk01", 0);
    cape_udc_add_s_cp    (columns, "col01", NULL);
    cape_udc_add_s_cp    (columns, "col02", NULL);
    
    results = adbl_trx_query (trx, "test_table01", NULL, &columns, err);
    
    if (results)
    {
      printf ("amount of result: %li\n", cape_udc_size (results));
      
      cape_udc_del (&results);
    }
  }
  
  // insert
  for (i = 0; i < 2; i++)
  {
    CapeUdc values = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    // define the columns we want to insert
    cape_udc_add_n       (values, "id", ADBL_AUTO_INCREMENT);   // this column is an auto increment column
    cape_udc_add_n       (values, "fk01", 42);
    
    CapeString h = cape_str_uuid ();
    cape_udc_add_s_mv    (values, "col01", &h);
    
    cape_udc_add_s_cp    (values, "col02", "xxxx");
    
    
    CapeDatetime dt;    
    cape_datetime_utc (&dt);
    
    cape_udc_add_d       (values, "d01",  &dt);
    
    last_inserted_row = adbl_trx_insert (trx, "test_table01", &values, err);
    
    if (last_inserted_row <= 0)
    {
      printf ("ERROR: %s\n", cape_err_text(err));
      
      
    }
  }
  
  adbl_trx_commit (&trx, err);
  
exit:

  cape_err_del (&err);
}

//-----------------------------------------------------------------------------

int main (int argc, char *argv[])
{
  int i, res;
  number_t last_inserted_row = 0;

  CapeErr err = cape_err_new ();
  
  CapeQueue queue = cape_queue_new ();
  CapeSync sync = cape_sync_new ();
  
  AdblCtx ctx = NULL;
  AdblSession session = NULL;
  AdblTrx trx = NULL;
  
  CapeDatetime dt_start;
  
  cape_datetime_utc (&dt_start);
  
  res = cape_queue_start (queue, 10, err);
  if (res)
  {
    goto exit;
  }
  
  ctx = adbl_ctx_new ("pvd_mysql", "adbl2_mysql", err);
  if (ctx == NULL)
  {
    goto exit;
  }
  
  {
    CapeUdc properties = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    cape_udc_add_s_cp (properties, "host", "localhost");
    cape_udc_add_s_cp (properties, "schema", "test");
    
    cape_udc_add_s_cp (properties, "user", "test");
    cape_udc_add_s_cp (properties, "pass", "test");
    
    session = adbl_session_open (ctx, properties, err);
    
    cape_udc_del (&properties);
    
    if (session == NULL)
    {
      printf ("can't open session\n");
      
      goto exit;
    }
  }
  
  
  for (i = 0; i < 100; i++)
  {
    cape_queue_add (queue, sync, worker_part, NULL, session, 0);
  }
  
  cape_sync_wait (sync);
  
  // fetch again with params
  {
    CapeUdc results;
        
    CapeUdc params = cape_udc_new (CAPE_UDC_NODE, NULL);
    CapeUdc columns = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    adbl_param_add__between_n   (params, "fk01", 0, 20);
    //cape_udc_add_n              (params, "fk01", 10);

    // define the columns we want to fetch
    cape_udc_add_n              (columns, "fk01", 0);
    cape_udc_add_s_cp           (columns, "col01", NULL);
    cape_udc_add_s_cp           (columns, "col02", NULL);
    
    results = adbl_session_query (session, "test_table01", &params, &columns, err);
    
    if (results)
    {
      printf ("amount of result: %li\n", cape_udc_size (results));
      
      cape_udc_del (&results);
    }
  }

  
  
  // fetch again with params
  {
    CapeUdc results;
    
    CapeUdc params = cape_udc_new (CAPE_UDC_NODE, NULL);
    CapeUdc columns = cape_udc_new (CAPE_UDC_NODE, NULL);
    
    
    adbl_param_add__greater_than_d (params, "d01", &dt_start);
    
    // define the columns we want to fetch
    cape_udc_add_n              (columns, "fk01", 0);
    cape_udc_add_s_cp           (columns, "col01", NULL);
    cape_udc_add_s_cp           (columns, "col02", NULL);
    cape_udc_add_d              (columns, "d01", NULL);
    
    results = adbl_session_query (session, "test_table01", &params, &columns, err);
    
    if (results)
    {
      CapeUdcCursor* cursor = cape_udc_cursor_new (results, CAPE_DIRECTION_FORW);

      printf ("amount of result: %li\n", cape_udc_size (results));
      
      while (cape_udc_cursor_next (cursor))
      {
        cape_udc_print (cursor->item);        
      }
      
      cape_udc_cursor_del (&cursor);
      
      cape_udc_del (&results);
    }
  }
  
exit:

  if (trx)
  {
    adbl_trx_rollback (&trx, err);
  }

  if (cape_err_code(err))
  {
    printf ("ERROR: %s\n", cape_err_text(err));
  }
  
  if (session)
  {
    adbl_session_close (&session);
  }
  
  if (ctx)
  {
    adbl_ctx_del (&ctx);
  }
  
  cape_err_del (&err);
  
  cape_queue_del (&queue);
  cape_sync_del (&sync);
  
  return 0;
}

//-----------------------------------------------------------------------------

