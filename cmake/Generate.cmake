find_program(CTP2_BYACC NAMES byacc REQUIRED)
find_program(CTP2_FLEX NAMES flex REQUIRED)
set(CTP2_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
# Keep each parser's conventional filenames in a separate binary directory.
function(ctp2_parser name grammar scanner prefix basename)
  set(dir "${CTP2_GENERATED_DIR}/${name}")
  file(MAKE_DIRECTORY "${dir}")
  add_custom_command(OUTPUT "${dir}/${basename}.tab.c" "${dir}/${basename}.tab.h"
    COMMAND "${CTP2_BYACC}" -d -p "${prefix}" -b "${basename}" "${PROJECT_SOURCE_DIR}/ctp2_code/${grammar}"
    DEPENDS "${PROJECT_SOURCE_DIR}/ctp2_code/${grammar}"
    WORKING_DIRECTORY "${dir}" VERBATIM)
  add_custom_command(OUTPUT "${dir}/lex.${prefix}.c"
    COMMAND "${CTP2_FLEX}" -i "-P${prefix}" "-olex.${prefix}.c" "${PROJECT_SOURCE_DIR}/ctp2_code/${scanner}"
    DEPENDS "${PROJECT_SOURCE_DIR}/ctp2_code/${scanner}" "${dir}/${basename}.tab.h"
    WORKING_DIRECTORY "${dir}" VERBATIM)
  set(${name}_SOURCES "${dir}/${basename}.tab.c;${dir}/lex.${prefix}.c" PARENT_SCOPE)
endfunction()
ctp2_parser(dbgen gs/dbgen/ctpdb.y gs/dbgen/ctpdb.l yy y)
if(CMAKE_CROSSCOMPILING)
  if(NOT EXISTS "${CTP2_HOST_DBGEN}")
    message(FATAL_ERROR "Cross builds require CTP2_HOST_DBGEN pointing to a host-native ctpdb")
  endif()
  add_executable(ctpdb IMPORTED GLOBAL)
  set_target_properties(ctpdb PROPERTIES IMPORTED_LOCATION "${CTP2_HOST_DBGEN}")
else()
  add_executable(ctpdb ctp2_code/gs/dbgen/Datum.cpp ctp2_code/gs/dbgen/MemberClass.cpp
    ctp2_code/gs/dbgen/RecordDescription.cpp ctp2_code/gs/dbgen/ctpdb.cpp ${dbgen_SOURCES})
  target_link_libraries(ctpdb PRIVATE ctp2_options)
  target_include_directories(ctpdb PRIVATE ctp2_code/gs/dbgen ctp2_code/ctp/ctp2_utils "${CTP2_GENERATED_DIR}/dbgen")
  if(NOT WIN32)
    target_include_directories(ctpdb PRIVATE ctp2_code/os/nowin32)
  endif()
endif()
file(MAKE_DIRECTORY "${CTP2_GENERATED_DIR}/newdb")
function(ctp2_schema schema)
  set(outputs)
  foreach(record IN LISTS ARGN)
    list(APPEND outputs "${CTP2_GENERATED_DIR}/newdb/${record}.cpp" "${CTP2_GENERATED_DIR}/newdb/${record}.h")
  endforeach()
  set(stamp "${CTP2_GENERATED_DIR}/newdb/${schema}.complete")
  add_custom_command(OUTPUT "${stamp}" BYPRODUCTS ${outputs}
    COMMAND ctpdb -i "${PROJECT_SOURCE_DIR}/ctp2_code/gs/newdb/${schema}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
    DEPENDS ctpdb "${PROJECT_SOURCE_DIR}/ctp2_code/gs/newdb/${schema}"
    WORKING_DIRECTORY "${CTP2_GENERATED_DIR}/newdb" VERBATIM)
  set(CTP2_SCHEMA_STAMPS ${CTP2_SCHEMA_STAMPS} "${stamp}" PARENT_SCOPE)
  set(CTP2_SCHEMA_OUTPUTS ${CTP2_SCHEMA_OUTPUTS} ${outputs} PARENT_SCOPE)
endfunction()
include("${CMAKE_CURRENT_LIST_DIR}/SchemaOutputs.cmake")
add_custom_target(ctp2_database DEPENDS ${CTP2_SCHEMA_STAMPS})
