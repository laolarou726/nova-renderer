find_package(SWIG 4 REQUIRED)
find_package(JNI)

include(UseSWIG)

set(NOVA_INCLUDE_DIRECTORY ${PROJECT_SOURCE_DIR}/include)
configure_file(nova_renderer.i.in ${CMAKE_CURRENT_BINARY_DIR}/nova_renderer.i @ONLY)

set(NOVA_SWIG_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/nova_renderer.i")
foreach(file ${NOVA_SWIG_SOURCE})
    set_property(SOURCE "${file}" PROPERTY CPLUSPLUS ON)
endforeach()

swig_add_library(nova_renderer_java TYPE SHARED LANGUAGE java SOURCES ${NOVA_SWIG_SOURCE} OUTPUT_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/swig/java)
swig_link_libraries(nova_renderer_java nova_renderer)
set_property(TARGET nova_renderer_java PROPERTY SWIG_INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/include/utils)
set_property(TARGET nova_renderer_java PROPERTY SWIG_COMPILE_OPTIONS -E)
target_include_directories(nova_renderer_java PRIVATE ${JNI_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/include)

if(NOVA_STD_FILESYSTEM)
    target_compile_definitions(nova_renderer_java PUBLIC NOVA_STD_FILESYSTEM=1)
elseif(NOVA_STD_FILESYSTEM_EXPERIMENTAL)
    target_compile_definitions(nova_renderer_java PUBLIC NOVA_STD_FILESYSTEM_EXPERIMENTAL=1)
endif()
