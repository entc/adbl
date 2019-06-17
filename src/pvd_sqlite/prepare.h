#ifndef __ADBL_SQLITE3__PREPARE_H
#define __ADBL_SQLITE3__PREPARE_H 1

#include "adbl_sqlite.h"

//-----------------------------------------------------------------------------

// sqlite3 includes
#include <sqlite3.h>

// cape includes
#include "sys/cape_export.h"
#include "sys/cape_err.h"
#include "stc/cape_udc.h"

//=============================================================================

struct AdblPrepare_s; typedef struct AdblPrepare_s* AdblPrepare;

//-----------------------------------------------------------------------------

__CAPE_LIBEX   AdblPrepare     adbl_prepare_new                (CapeUdc* p_params, CapeUdc* p_values);

__CAPE_LIBEX   void            adbl_prepare_del                (AdblPrepare*);

__CAPE_LIBEX   int             adbl_prepare_execute            (const CapeString statement, sqlite3* handle, CapeErr err);

//-----------------------------------------------------------------------------

__CAPE_LIBEX   int             adbl_prepare_prepare            (AdblPrepare, sqlite3* handle, CapeErr err);

__CAPE_LIBEX   int             adbl_prepare_bind               (AdblPrepare, CapeErr err);

__CAPE_LIBEX   int             adbl_prepare_run                (AdblPrepare, sqlite3* handle, CapeErr err);

__CAPE_LIBEX   number_t        adbl_prepare_lastid             (AdblPrepare, sqlite3* handle, const char* schema, const char* table, CapeErr err);

//-----------------------------------------------------------------------------

__CAPE_LIBEX   int             adbl_prepare_next               (AdblPrepare);

__CAPE_LIBEX   CapeUdc         adbl_prepare_values             (AdblPrepare);

//-----------------------------------------------------------------------------

__CAPE_LIBEX   void            adbl_prepare_statement_select   (AdblPrepare, const char* schema, const char* table);

__CAPE_LIBEX   void            adbl_prepare_statement_insert   (AdblPrepare, const char* schema, const char* table);

__CAPE_LIBEX   void            adbl_prepare_statement_delete   (AdblPrepare, const char* schema, const char* table);

__CAPE_LIBEX   void            adbl_prepare_statement_update   (AdblPrepare, const char* schema, const char* table);

__CAPE_LIBEX   void            adbl_prepare_statement_setins   (AdblPrepare, const char* schema, const char* table);

//-----------------------------------------------------------------------------

#endif
