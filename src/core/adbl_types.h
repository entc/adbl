#ifndef __ADBL_TYPES__H
#define __ADBL_TYPES__H 1

// cape includes
#include <sys/cape_err.h>
#include <stc/cape_udc.h>

//-----------------------------------------------------------------------------

#define ADBL_TYPE__BETWEEN             1
#define ADBL_TYPE__GREATER_THAN        2
#define ADBL_TYPE__LESS_THAN           3

#define ADBL_SPECIAL__TYPE             "__type"
#define ADBL_SPECIAL__BETWEEN_FROM     "__from"
#define ADBL_SPECIAL__BETWEEN_TO       "__to"
#define ADBL_SPECIAL__GREATER          "__greater"
#define ADBL_SPECIAL__LESS             "__less"

//-----------------------------------------------------------------------------

typedef void*     (__STDCALL *fct_adbl_pvd_open)          (CapeUdc, CapeErr);
typedef void*     (__STDCALL *fct_adbl_pvd_clone)         (void*, CapeErr);
typedef void      (__STDCALL *fct_adbl_pvd_close)         (void**);
typedef int       (__STDCALL *fct_adbl_pvd_begin)         (void*, CapeErr);
typedef int       (__STDCALL *fct_adbl_pvd_commit)        (void*, CapeErr);
typedef int       (__STDCALL *fct_adbl_pvd_rollback)      (void*, CapeErr);

typedef CapeUdc   (__STDCALL *fct_adbl_pvd_get)           (void*, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr);
typedef number_t  (__STDCALL *fct_adbl_pvd_ins)           (void*, const char* table, CapeUdc* p_values, CapeErr);
typedef int       (__STDCALL *fct_adbl_pvd_set)           (void*, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr);
typedef int       (__STDCALL *fct_adbl_pvd_del)           (void*, const char* table, CapeUdc* p_params, CapeErr);
typedef number_t  (__STDCALL *fct_adbl_pvd_ins_or_set)    (void*, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr);

typedef void*     (__STDCALL *fct_adbl_pvd_cursor_new)    (void*, const char* table, CapeUdc* p_params, CapeUdc* p_values, CapeErr);
typedef void      (__STDCALL *fct_adbl_pvd_cursor_del)    (void**);
typedef int       (__STDCALL *fct_adbl_pvd_cursor_next)   (void*);
typedef CapeUdc   (__STDCALL *fct_adbl_pvd_cursor_get)    (void*);

typedef int       (__STDCALL *fct_adbl_pvd_atomic_dec)    (void*, const char* table, CapeUdc* p_params, const CapeString atomic_value, CapeErr);
typedef int       (__STDCALL *fct_adbl_pvd_atomic_inc)    (void*, const char* table, CapeUdc* p_params, const CapeString atomic_value, CapeErr);

//-----------------------------------------------------------------------------

typedef struct
{
  fct_adbl_pvd_open           pvd_open;
  fct_adbl_pvd_clone          pvd_clone;
  fct_adbl_pvd_close          pvd_close;
  fct_adbl_pvd_begin          pvd_begin;
  fct_adbl_pvd_commit         pvd_commit;
  fct_adbl_pvd_rollback       pvd_rollback;
  fct_adbl_pvd_get            pvd_get;
  fct_adbl_pvd_ins            pvd_ins;
  fct_adbl_pvd_set            pvd_set;
  fct_adbl_pvd_del            pvd_del;
  fct_adbl_pvd_ins_or_set     pvd_ins_or_set;
  fct_adbl_pvd_cursor_new     pvd_cursor_new;
  fct_adbl_pvd_cursor_del     pvd_cursor_del;
  fct_adbl_pvd_cursor_next    pvd_cursor_next;
  fct_adbl_pvd_cursor_get     pvd_cursor_get;
  fct_adbl_pvd_atomic_dec     pvd_atomic_dec;
  fct_adbl_pvd_atomic_inc     pvd_atomic_inc;

} AdblPvd;

//-----------------------------------------------------------------------------

#endif
