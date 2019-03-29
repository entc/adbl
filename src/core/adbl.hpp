#ifndef __ADBL_HPP__H
#define __ADBL_HPP__H 1

#include "adbl.h"
#include "hpp/cape_sys.hpp"

#include <stdexcept>

namespace adbl {
  
  class TransactionScope
  {
    
  public:
    
    TransactionScope (AdblSession session)
    : m_session (session)
    , m_trx (NULL)
    {
    }
    
    virtual ~TransactionScope ()
    {
      if (m_trx)
      {
        cape::ErrHolder errh;
        
        adbl_trx_rollback (&m_trx, errh.err);
      }
    }
    
    void start ()
    {
      cape::ErrHolder errh;
      
      m_trx = adbl_trx_new (m_session, errh.err);
      
      if (m_trx == NULL)
      {
        throw std::runtime_error (errh.text());
      }
    }
    
    void commit ()
    {
      int res;
      cape::ErrHolder errh;
      
      res = adbl_trx_commit (&m_trx, errh.err);
      if (res)
      {
        throw std::runtime_error (errh.text());
      }
    }
    
    void trx_update (const char* table, CapeUdc* p_params, CapeUdc* p_values)
    {
      int res;
      cape::ErrHolder errh;

      // execute database statement
      res = adbl_trx_update (m_trx, table, p_params, p_values, errh.err);
      if (res)
      {
        throw std::runtime_error (errh.text());
      }      
    }
    
    void trx_delete (const char* table, CapeUdc* p_params)
    {
      int res;
      cape::ErrHolder errh;

      res = adbl_trx_delete (m_trx, table, p_params, errh.err);
      if (res)
      {
        throw std::runtime_error (errh.text());
      }      
    }
    
    number_t trx_insert (const char* table, CapeUdc* p_values)
    {
      number_t inserted_id = 0;
      cape::ErrHolder errh;
      
      // execute database statement
      inserted_id = adbl_trx_insert (m_trx, table, p_values, errh.err);
      if (inserted_id == 0)
      {
        throw std::runtime_error (errh.text());
      }
      
      return inserted_id;
    }
    
  protected:
    
    AdblSession m_session;    // reference
    
    AdblTrx m_trx;
    
  };  
}

#endif

