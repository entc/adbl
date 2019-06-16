cmake_minimum_required(VERSION 2.4)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
SET(CMAKE_MACOSX_RPATH 0)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
SET(CMAKE_BUILD_TYPE Debug)

find_package(AdblSqlite3)

IF(SQLITE_FOUND)

  INCLUDE_DIRECTORIES( "../core" ${SQLITE_INCLUDES})

  SET (CORE_SOURCES
    adbl_sqlite.c
    prepare.c
  )

  SET (CORE_HEADERS
    prepare.h
  )

  ADD_LIBRARY(adbl2_sqlite3 SHARED ${CORE_SOURCES} ${CORE_HEADERS})
  TARGET_LINK_LIBRARIES( adbl2_sqlite3 adbl2 cape ${SQLITE_LIBRARIES})

  install (TARGETS adbl2_sqlite3 DESTINATION adbl)
  
  # for docker
  INSTALL (TARGETS adbl2_sqlite3 DESTINATION docker/qore/mods/adbl)
  INSTALL (FILES ${SQLITE_LIBRARIES} DESTINATION docker/qore/lib)

ELSE()

  MESSAGE(WARNING "sqlite3 was not found on your system" )

ENDIF()
