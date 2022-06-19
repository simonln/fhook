aux_source_directory(. SCRS)

add_executable(example ${SCRS})

target_link_libraries(example foo fhook)
target_include_directories(example PUBLIC ../lib ../../src)
if(MSVC)
else()
target_compile_options(example PUBLIC -fpermissive)
endif()