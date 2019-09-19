#ifndef __ADBL_HPP__H
#define __ADBL_HPP__H 1

#include "adbl.h"
#include "hpp/cape_sys.hpp"
#include "hpp/cape_stc.hpp"

#include <stdexcept>

namespace adbl {
  
  class Session
  {
    
  public:  
  
    Session ()
    {
    }
    
    ~Session ()
    {
    }

  private:
    
    AdblCtx m_ctx;
    
    AdblSession m_session;
    
  };
  
  class TransactionScope
  {
    
  public:
    
    TransactionScope (AdblSession session)
    : m_session (session)
    , m_trx (NULL)
    {
    }
    
    //-----------------------------------------------------------------------------
    
    virtual ~TransactionScope ()
    {
      if (m_trx)
      {
        cape::ErrHolder errh;
        
        adbl_trx_rollback (&m_trx, errh.err);
      }
    }
    
    //-----------------------------------------------------------------------------
    
    void start ()
    {
      cape::ErrHolder errh;
      
      m_trx = adbl_trx_new (m_session, errh.err);
      
      if (m_trx == NULL)
      {
        throw std::runtime_error (errh.text());
      }
    }
    
    //-----------------------------------------------------------------------------
    
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
    
    //-----------------------------------------------------------------------------
    
    void update (const char* table, cape::Udc&& params, cape::Udc&& values)
    {
      int res;
      cape::ErrHolder errh;
      
      // transfer ownership
      CapeUdc c_params = params.release ();
      CapeUdc c_values = values.release ();

      // execute database statement
      if (adbl_trx_update (m_trx, table, &c_params, &c_values, errh.err))
      {
        throw std::runtime_error (errh.text());
      }
    }
    
    //-----------------------------------------------------------------------------
    
    void del (const char* table, cape::Udc&& params)
    {
      int res;
      cape::ErrHolder errh;

      // transfer ownership
      CapeUdc c_params = params.release ();
      
      if (adbl_trx_delete (m_trx, table, &c_params, errh.err))
      {
        throw std::runtime_error (errh.text());
      }      
    }
    
    //-----------------------------------------------------------------------------
    
    number_t ins (const char* table, cape::Udc&& values)
    {
      number_t inserted_id = 0;
      cape::ErrHolder errh;
      
      // transfer ownership
      CapeUdc c_values = values.release ();
      
      // execute database statement
      inserted_id = adbl_trx_insert (m_trx, table, &c_values, errh.err);
      if (inserted_id == 0)
      {
        throw std::runtime_error (errh.text());
      }
      
      return inserted_id;
    }
    
    //-----------------------------------------------------------------------------
    
  protected:
    
    AdblSession m_session;    // reference
    
    AdblTrx m_trx;
    
  };  
}

#endif

