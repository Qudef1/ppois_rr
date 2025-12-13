#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sc-machine::sc-machine" for configuration "Release"
set_property(TARGET sc-machine::sc-machine APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-machine PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/sc-machine"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-machine )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-machine "${_IMPORT_PREFIX}/bin/sc-machine" )

# Import target "sc-machine::sc-builder" for configuration "Release"
set_property(TARGET sc-machine::sc-builder APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-builder PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/sc-builder"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-builder )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-builder "${_IMPORT_PREFIX}/bin/sc-builder" )

# Import target "sc-machine::sc-machine-runner" for configuration "Release"
set_property(TARGET sc-machine::sc-machine-runner APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-machine-runner PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsc-machine-runner.so"
  IMPORTED_SONAME_RELEASE "libsc-machine-runner.so"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-machine-runner )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-machine-runner "${_IMPORT_PREFIX}/lib/libsc-machine-runner.so" )

# Import target "sc-machine::sc-core" for configuration "Release"
set_property(TARGET sc-machine::sc-core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-core PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsc-core.so"
  IMPORTED_SONAME_RELEASE "libsc-core.so"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-core )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-core "${_IMPORT_PREFIX}/lib/libsc-core.so" )

# Import target "sc-machine::sc-memory" for configuration "Release"
set_property(TARGET sc-machine::sc-memory APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-memory PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsc-memory.so"
  IMPORTED_SONAME_RELEASE "libsc-memory.so"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-memory )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-memory "${_IMPORT_PREFIX}/lib/libsc-memory.so" )

# Import target "sc-machine::sc-agents-common" for configuration "Release"
set_property(TARGET sc-machine::sc-agents-common APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-agents-common PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsc-agents-common.so"
  IMPORTED_SONAME_RELEASE "libsc-agents-common.so"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-agents-common )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-agents-common "${_IMPORT_PREFIX}/lib/libsc-agents-common.so" )

# Import target "sc-machine::sc-builder-lib" for configuration "Release"
set_property(TARGET sc-machine::sc-builder-lib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-builder-lib PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsc-builder-lib.so"
  IMPORTED_SONAME_RELEASE "libsc-builder-lib.so"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-builder-lib )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-builder-lib "${_IMPORT_PREFIX}/lib/libsc-builder-lib.so" )

# Import target "sc-machine::sc-config" for configuration "Release"
set_property(TARGET sc-machine::sc-config APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sc-machine::sc-config PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsc-config.so"
  IMPORTED_SONAME_RELEASE "libsc-config.so"
  )

list(APPEND _cmake_import_check_targets sc-machine::sc-config )
list(APPEND _cmake_import_check_files_for_sc-machine::sc-config "${_IMPORT_PREFIX}/lib/libsc-config.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
