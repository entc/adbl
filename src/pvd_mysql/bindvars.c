#include "adbl_types.h"
#include "bindvars.h"

// cape includes
#include <fmt/cape_json.h>
#include <sys/cape_log.h>

#define ADBL_BIND_BUFFER_SIZE 4000

//-----------------------------------------------------------------------------

struct AdblBindVars_s
{  
  MYSQL_BIND* binds;
  
  int size;
  
  int pos;
  
};

//------------------------------------------------------------------------------------------------------

AdblBindVars adbl_bindvars_new (int size)
{
  number_t bindSize;
  
  AdblBindVars self = CAPE_NEW (struct AdblBindVars_s);
  
  self->size = size;
  self->pos = 0;
  
  bindSize = sizeof(MYSQL_BIND) * size;
  if (bindSize > 0)
  {
    // create a new array of binds
    self->binds = CAPE_ALLOC(bindSize);    
    memset (self->binds, 0, bindSize);
  }
  else
  {
    self->binds = NULL;
  }
  
  return self;
}

//------------------------------------------------------------------------------------------------------

void adbl_bindvars_del (AdblBindVars* p_self)
{
  AdblBindVars self = *p_self;
  
  {
    int i;
    
    for (i = 0; i < self->size; i++)
    {
      MYSQL_BIND* bind = &(self->binds[i]);
      
      if (bind->buffer && bind->pack_length)
      {
        if (bind->buffer)
        {
          CAPE_FREE (bind->buffer);
        }
        
        if (bind->is_null)
        {
          CAPE_FREE (bind->is_null);
        }        
      }
    }
  }
  
  // release the array of the bind variables
  CAPE_FREE (self->binds);
  
  CAPE_DEL (p_self, struct AdblBindVars_s);
}

//------------------------------------------------------------------------------------------------------

MYSQL_BIND* adbl_bindvars_binds (AdblBindVars self)
{
  return self->binds;
}

//------------------------------------------------------------------------------------------------------

void adbl_bindvars_set (AdblBindVars self, CapeUdc item, int check_for_specials)
{
  switch (cape_udc_type (item))
  {
    case CAPE_UDC_STRING:
    {
      if (self->pos < self->size)
      {
        MYSQL_BIND* bind = &(self->binds[self->pos]);
        
        const CapeString h = cape_udc_s (item, NULL);
        
        bind->buffer_type = MYSQL_TYPE_STRING;
        bind->buffer = (char*)h;
        bind->buffer_length = strlen (h);
        bind->is_null = 0;
        bind->length = 0;
        bind->error = 0;

        self->pos++;
      }
            
      break;
    }
    case CAPE_UDC_NUMBER:
    {
      if (self->pos < self->size)
      {
        MYSQL_BIND* bind = &(self->binds[self->pos]);
        
        bind->buffer_type = MYSQL_TYPE_LONG;
        bind->buffer = cape_udc_data (item);
        bind->buffer_length = 0;
        bind->is_null = 0;
        bind->length = 0;
        bind->error = 0; 
        bind->is_unsigned = 0;
        
        self->pos++;
      }
      
      break;
    }
    case CAPE_UDC_FLOAT:
    {
      if (self->pos < self->size)
      {
        MYSQL_BIND* bind = &(self->binds[self->pos]);
        
        bind->buffer_type = MYSQL_TYPE_DOUBLE;
        bind->buffer = cape_udc_data (item);
        bind->buffer_length = 0;
        bind->is_null = 0;
        bind->length = 0;
        bind->error = 0; 
        bind->is_unsigned = 0;
        
        self->pos++;
      }
      
      break;
    }
    case CAPE_UDC_DATETIME:
    {
      if (self->pos < self->size)
      {
        MYSQL_BIND* bind = &(self->binds[self->pos]);
        
        const CapeDatetime* cape_dt = cape_udc_d (item, NULL);
        
        if (cape_dt)
        {
          MYSQL_TIME* dt = CAPE_NEW(MYSQL_TIME);
          
          dt->year = cape_dt->year;
          dt->month = cape_dt->month;
          dt->day = cape_dt->day;
          
          dt->hour = cape_dt->hour;
          dt->minute = cape_dt->minute;
          dt->second = cape_dt->sec;
          
          //dt->second_part = cape_dt->msec * 1000;
          dt->second_part = cape_dt->usec;
          
          bind->buffer_type = MYSQL_TYPE_DATETIME;
          bind->buffer = dt;
          bind->buffer_length = 0;
          bind->is_null = 0;
          bind->length = 0;
          bind->error = 0; 
          bind->is_unsigned = 0;
          
          bind->pack_length = 1;
        }
        else
        {
          bind->buffer_type = MYSQL_TYPE_DATETIME;
          bind->buffer = NULL;
          bind->buffer_length = 0;
          bind->is_null = 0;
          bind->length = 0;
          bind->error = 0; 
          bind->is_unsigned = 0;
        }
        
        self->pos++;
      }
      
      break;
    }
    case CAPE_UDC_BOOL:
    {
      if (self->pos < self->size)
      {
        MYSQL_BIND* bind = &(self->binds[self->pos]);
        
        bind->buffer_type = MYSQL_TYPE_LONG;    // use long because original is int
        bind->buffer = cape_udc_data (item);
        bind->buffer_length = 0;
        bind->is_null = 0;
        bind->length = 0;
        bind->error = 0; 
        bind->is_unsigned = 0;
        
        self->pos++;
      }
      
      break;
    }
    case CAPE_UDC_NULL:
    {
      if (self->pos < self->size)
      {
        MYSQL_BIND* bind = &(self->binds[self->pos]);
        
        bind->buffer_type = MYSQL_TYPE_NULL;
        bind->buffer = NULL;
        bind->buffer_length = 0;
        bind->is_null = 0;
        bind->length = 0;
        bind->error = 0; 
        bind->is_unsigned = 0;

        self->pos++;
      }

      break;
    }
    case CAPE_UDC_LIST:
    case CAPE_UDC_NODE:
    {
      if (check_for_specials)
      {
        switch (cape_udc_get_n (item, ADBL_SPECIAL__TYPE, 0))
        {
          case ADBL_TYPE__BETWEEN:
          {
            // check what kind of special param we have
            CapeUdc from_node = cape_udc_get (item, ADBL_SPECIAL__BETWEEN_FROM);
            CapeUdc to_node = cape_udc_get (item, ADBL_SPECIAL__BETWEEN_TO);
            
            if (from_node && to_node)
            {
              adbl_bindvars_set (self, from_node, FALSE);
              adbl_bindvars_set (self, to_node, FALSE);
            }
            
            break;
          }
          case ADBL_TYPE__GREATER_THAN:
          {
            CapeUdc greater = cape_udc_get (item, ADBL_SPECIAL__GREATER);
           
            if (greater)
            {
              adbl_bindvars_set (self, greater, FALSE);
            }
           
            break;
          }
          case ADBL_TYPE__LESS_THAN:
          {
            CapeUdc less = cape_udc_get (item, ADBL_SPECIAL__LESS);
            
            if (less)
            {
              adbl_bindvars_set (self, less, FALSE);
            }
            
            break;
          }
        }
      }
      else
      {
        if (self->pos < self->size)
        {
          MYSQL_BIND* bind = &(self->binds[self->pos]);
          
          // convert into string
          CapeString h = cape_json_to_s (item);
          
          bind->buffer_type = MYSQL_TYPE_STRING;
          bind->buffer = h;
          bind->buffer_length = strlen (h);
          bind->is_null = NULL;
          bind->length = 0;
          bind->error = 0;
          
          bind->pack_length = 1;
          
          self->pos++;
        }
        
        break;
      }
    }
  }
}

//------------------------------------------------------------------------------------------------------

void adbl_bind_add__string_options (MYSQL_BIND* bind, CapeUdc item)
{
  // options might be set, in case they are JSON encoded
  const CapeString options_text = cape_udc_s (item, NULL);
  if (options_text)
  {
    // decode
    CapeUdc options = cape_json_from_s (options_text);
    if (options)
    {
      number_t size = cape_udc_get_n (options, "size", 0);
      if (size)
      {
        bind->buffer = CAPE_ALLOC(size);
        memset (bind->buffer, 0, size);
        
        bind->buffer_length = size;
        
        return;
      }
    }
  }

  // default
  
  bind->buffer = CAPE_ALLOC(ADBL_BIND_BUFFER_SIZE);
  memset (bind->buffer, 0, ADBL_BIND_BUFFER_SIZE);
  
  bind->buffer_length = ADBL_BIND_BUFFER_SIZE;
}

//------------------------------------------------------------------------------------------------------

void adbl_bind_add (MYSQL_BIND* bind, CapeUdc item)
{
  switch (cape_udc_type (item))
  {
    case CAPE_UDC_STRING:
    {
      bind->buffer_type = MYSQL_TYPE_STRING;
      
      adbl_bind_add__string_options (bind, item);
      
      bind->is_null = CAPE_ALLOC(sizeof(my_bool));
      bind->length = 0;
      bind->error = 0;
      
      bind->pack_length = 1;
      
      break;
    }
    case CAPE_UDC_NUMBER:
    {
      bind->buffer_type = MYSQL_TYPE_LONG;
      
      bind->buffer = CAPE_ALLOC(8);
      memset (bind->buffer, 0, 8);
      
      bind->buffer_length = 0;
      bind->is_null = CAPE_ALLOC(sizeof(my_bool));
      bind->length = 0;
      bind->error = 0; 
      bind->is_unsigned = 0;
      
      bind->pack_length = 1;
      
      break;
    }
    case CAPE_UDC_FLOAT:
    {
      bind->buffer_type = MYSQL_TYPE_DOUBLE;
      bind->buffer = CAPE_ALLOC(8);
      bind->buffer_length = 0;
      bind->is_null = CAPE_ALLOC(sizeof(my_bool));
      bind->length = 0;
      bind->error = 0; 
      bind->is_unsigned = 0;
      
      bind->pack_length = 1;
      
      break;
    }
    case CAPE_UDC_BOOL:
    {
      bind->buffer_type = MYSQL_TYPE_LONG;    // use long because original is int
      bind->buffer = CAPE_ALLOC(8);
      bind->buffer_length = 0;
      bind->is_null = CAPE_ALLOC(sizeof(my_bool));
      bind->length = 0;
      bind->error = 0; 
      bind->is_unsigned = 0;
      
      bind->pack_length = 1;
      
      break;
    }
    case CAPE_UDC_DATETIME:
    {
      bind->buffer_type = MYSQL_TYPE_DATETIME;
      bind->buffer = CAPE_ALLOC (sizeof(MYSQL_TIME));
      bind->buffer_length = 0;
      bind->is_null = CAPE_ALLOC (sizeof(MYSQL_TIME));
      bind->length = 0;
      bind->error = 0; 
      bind->is_unsigned = 0;
      
      bind->pack_length = 1;
      
      break;
    }
    case CAPE_UDC_NULL:
    {
      bind->buffer_type = MYSQL_TYPE_NULL;
      bind->buffer = NULL;
      bind->buffer_length = 0;
      bind->is_null = NULL;
      bind->length = 0;
      bind->error = 0; 
      bind->is_unsigned = 0;
      
      bind->pack_length = 1;
      
      break;
    }
    case CAPE_UDC_LIST:
    case CAPE_UDC_NODE:
    {
      bind->buffer_type = MYSQL_TYPE_STRING;
      
      bind->buffer = CAPE_ALLOC(ADBL_BIND_BUFFER_SIZE);
      memset (bind->buffer, 0, ADBL_BIND_BUFFER_SIZE);
      
      bind->buffer_length = ADBL_BIND_BUFFER_SIZE;
      bind->is_null = CAPE_ALLOC(sizeof(my_bool));
      bind->length = 0;
      bind->error = 0;
      
      bind->pack_length = 1;
      
      break;
    }
  }  
}

//------------------------------------------------------------------------------------------------------

int adbl_bind_get (MYSQL_BIND* bind, CapeUdc item)
{
  my_bool isnull = *(bind->is_null);
  
  if (isnull)
  {
    return FALSE; 
  }
  
  switch (cape_udc_type (item))
  {
    case CAPE_UDC_STRING:
    {
      // copy from buffer
      cape_udc_set_s_cp (item, bind->buffer);
      
      break;
    }
    case CAPE_UDC_NUMBER:
    {
      number_t* h = bind->buffer;      
      cape_udc_set_n (item, *h);
      
      break;
    }
    case CAPE_UDC_FLOAT:
    {
      double* h = bind->buffer;      
      cape_udc_set_f (item, *h);
      
      break;
    }
    case CAPE_UDC_BOOL:
    {
      number_t* h = bind->buffer;      
      cape_udc_set_b (item, *h);
      
      break;
    }
    case CAPE_UDC_DATETIME:
    {
      MYSQL_TIME* h = bind->buffer;
     
      CapeDatetime dt;
      
      dt.year = h->year;
      dt.month = h->month;
      dt.day = h->day;
      
      dt.hour = h->hour;
      dt.minute = h->minute;
      dt.sec = h->second;
      
      dt.usec = h->second_part;
      dt.msec = dt.usec / 1000;
      
      cape_udc_set_d (item, &dt);

      break;
    }
    case CAPE_UDC_LIST:
    case CAPE_UDC_NODE:
    {
      // convert into node
      CapeUdc h = cape_json_from_s (bind->buffer);

      if (h)
      {
        cape_udc_merge_mv (item, &h);
      }
      else
      {
        cape_log_fmt (CAPE_LL_ERROR, "ADBL", "bind get", "can't convert result into node / list");
      }
      
      break;
    }
  }
  
  return TRUE;
}

//------------------------------------------------------------------------------------------------------

int adbl_bindvars_add (AdblBindVars self, CapeUdc item, CapeErr err)
{
  if (self->pos < self->size)
  {
    MYSQL_BIND* bind = &(self->binds[self->pos]);
    
    adbl_bind_add (bind, item);
    
    self->pos++;
  }
  
  return CAPE_ERR_NONE;
}

//------------------------------------------------------------------------------------------------------

int adbl_bindvars_get (AdblBindVars self, number_t index, CapeUdc item)
{
  int res = FALSE;
  
  if (index < self->size)
  {
    MYSQL_BIND* bind = &(self->binds[index]);
    
    res = adbl_bind_get (bind, item);
  }
  
  return res;
}

//------------------------------------------------------------------------------------------------------

int adbl_bindvars_set_from_node (AdblBindVars self, CapeUdc node, int check_for_specials, CapeErr err)
{
  int res;
  CapeUdcCursor* cursor = cape_udc_cursor_new (node, CAPE_DIRECTION_FORW);
  
  while (cape_udc_cursor_next (cursor))
  {
    const CapeString param_name = cape_udc_name (cursor->item);
    if (param_name)
    {
      adbl_bindvars_set (self, cursor->item, check_for_specials);
    }
  }
  
  cape_udc_cursor_del (&cursor);
  return CAPE_ERR_NONE;
}

//------------------------------------------------------------------------------------------------------

int adbl_bindvars_add_from_node (AdblBindVars self, CapeUdc node, CapeErr err)
{
  int res;
  CapeUdcCursor* cursor = cape_udc_cursor_new (node, CAPE_DIRECTION_FORW);
  
  while (cape_udc_cursor_next (cursor))
  {
    const CapeString column_name = cape_udc_name (cursor->item);
    if (column_name)
    {
      res = adbl_bindvars_add (self, cursor->item, err);
      if (res)
      {
        goto exit;
      }
    }
  }  
  
  res = CAPE_ERR_NONE;
  
exit:
  
  cape_udc_cursor_del (&cursor);
  return res;
}

//------------------------------------------------------------------------------------------------------
