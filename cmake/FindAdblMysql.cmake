if (NOT MYSQL_FOUND)

  ## try the default way
  find_package(MySQL QUIET)
  
  if (NOT MYSQL_FOUND)

    ##____________________________________________________________________________
    ## Check for the header files

    find_path (MYSQL_INCLUDES
      NAMES mysql.h
      HINTS "/opt/local/include/mariadb/mysql/" "C:\\Program Files\\MariaDB 10.4\\include\\mysql"
      PATH_SUFFIXES mariadb
    )
    
    if (MYSQL_INCLUDES)
    
    else ()

      find_path (MYSQL_INCLUDES
        NAMES mysql.h
        HINTS "/opt/local/include/mariadb/mysql/"
        PATH_SUFFIXES mysql
      )
    
    endif ()

    ##____________________________________________________________________________
    ## Check for the library

    find_library (MYSQL_LIBRARY_01 mysqlclient 
      HINTS "/opt/local/lib/mariadb/mysql/" "/usr/lib/"
    )

    if (MYSQL_LIBRARY_01)
    
    else ()
    
    find_library (MYSQL_LIBRARY_01 mariadbclient 
      HINTS "/opt/local/lib/mariadb/mysql/" "/usr/lib/" "C:\\Program Files\\MariaDB 10.4\\lib"
    )

    endif ()

    # Correct Symlinks
    if(IS_SYMLINK ${MYSQL_LIBRARY_01})
      get_filename_component(TMP ${MYSQL_LIBRARY_01} REALPATH)
      list (APPEND MYSQL_LIBRARIES ${TMP})
    else()
      list (APPEND MYSQL_LIBRARIES ${MYSQL_LIBRARY_01})
    endif()


    ##____________________________________________________________________________
    ## Actions taken when all components have been found

    #find_package_handle_standard_args (MYSQL DEFAULT_MSG MYSQL_LIBRARIES MYSQL_INCLUDES)

    if (MYSQL_INCLUDES AND MYSQL_LIBRARIES)
      SET(MYSQL_FOUND TRUE)
    endif (MYSQL_INCLUDES AND MYSQL_LIBRARIES)

    if (MYSQL_FOUND)
      if (NOT MYSQL_FIND_QUIETLY)
        message (STATUS "MYSQL_INCLUDES  = ${MYSQL_INCLUDES}")
        message (STATUS "MYSQL_LIBRARIES = ${MYSQL_LIBRARIES}")
      endif (NOT MYSQL_FIND_QUIETLY)
    else (MYSQL_FOUND)
      if (MYSQL_FIND_REQUIRED)
        message (FATAL_ERROR "Could not find MYSQL!")
      endif (MYSQL_FIND_REQUIRED)
    endif (MYSQL_FOUND)

    ##____________________________________________________________________________
    ## Mark advanced variables

    mark_as_advanced (MYSQL_ROOT_DIR MYSQL_INCLUDES MYSQL_LIBRARIES)

  endif (NOT MYSQL_FOUND)
    
endif (NOT MYSQL_FOUND)

