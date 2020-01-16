cmake_minimum_required(VERSION 2.4)

SET(CMAKE_BUILD_TYPE Debug)

#----------------------------------------------------------------------------------

SET(ADBL_CORE_SOURCES
  core/adbl.c
  core/adbl_pool.c
)

SET(ADBL_CORE_HEADERS
  core/adbl.h
)

#----------------------------------------------------------------------------------

INCLUDE_DIRECTORIES("." "../../cape/src")

add_library             (adbl2 SHARED ${ADBL_CORE_SOURCES} ${ADBL_CORE_HEADERS})
target_link_libraries   (adbl2 cape)
set_target_properties   (adbl2 PROPERTIES VERSION 2.0.1 SOVERSION 2.0.1)

install (TARGETS adbl2 DESTINATION lib)

# for docker
INSTALL (TARGETS adbl2 DESTINATION docker/qore/lib)

#----------------------------------------------------------------------------------

INCLUDE_DIRECTORIES("." "core")

SET(ADBL_APP_SOURCES
  app/adbl_app.c
)

add_executable          (adbl_app ${ADBL_APP_SOURCES})
target_link_libraries   (adbl_app adbl2)
set_target_properties   (adbl_app PROPERTIES VERSION 1.0.1 SOVERSION 1.0.1)

#----------------------------------------------------------------------------------

SUBDIRS(pvd_mysql)
SUBDIRS(pvd_sqlite)

