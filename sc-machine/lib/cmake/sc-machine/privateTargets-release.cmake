#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sc-kpm-common" for configuration "Release"
set_property(TARGET sc-kpm-common APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-kpm-common PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-common.so"
  IMPORTED_SONAME_RELEASE "libsc-kpm-common.so"
  )

list(APPEND _cmake_import_check_targets sc-kpm-common )
list(APPEND _cmake_import_check_files_for_sc-kpm-common "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-common.so" )

# Import target "sc-kpm-search" for configuration "Release"
set_property(TARGET sc-kpm-search APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-kpm-search PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-search.so"
  IMPORTED_SONAME_RELEASE "libsc-kpm-search.so"
  )

list(APPEND _cmake_import_check_targets sc-kpm-search )
list(APPEND _cmake_import_check_files_for_sc-kpm-search "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-search.so" )

# Import target "sc-kpm-utils" for configuration "Release"
set_property(TARGET sc-kpm-utils APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-kpm-utils PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-utils.so"
  IMPORTED_SONAME_RELEASE "libsc-kpm-utils.so"
  )

list(APPEND _cmake_import_check_targets sc-kpm-utils )
list(APPEND _cmake_import_check_files_for_sc-kpm-utils "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-utils.so" )

# Import target "sc-kpm-ui" for configuration "Release"
set_property(TARGET sc-kpm-ui APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-kpm-ui PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-ui.so"
  IMPORTED_SONAME_RELEASE "libsc-kpm-ui.so"
  )

list(APPEND _cmake_import_check_targets sc-kpm-ui )
list(APPEND _cmake_import_check_files_for_sc-kpm-ui "${_IMPORT_PREFIX}/lib/extensions/libsc-kpm-ui.so" )

# Import target "sc-server-lib" for configuration "Release"
set_property(TARGET sc-server-lib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-server-lib PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/extensions/libsc-server-lib.so"
  IMPORTED_SONAME_RELEASE "libsc-server-lib.so"
  )

list(APPEND _cmake_import_check_targets sc-server-lib )
list(APPEND _cmake_import_check_files_for_sc-server-lib "${_IMPORT_PREFIX}/lib/extensions/libsc-server-lib.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
