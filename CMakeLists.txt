set(source_files
    model/dlarp.cc
    helper/dlarp-helper.cc
)

set(header_files
    model/dlarp.h
    helper/dlarp-helper.h
)

set(libraries_to_link
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libmobility}
)

set(test_sources)

# Ensure the library is properly built without ALIAS
add_library(dlarp SHARED ${source_files})
target_include_directories(dlarp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dlarp PUBLIC ${libraries_to_link})

# Examples
set(example_sources examples/dlarp-example.cc)
add_executable(dlarp-example ${example_sources})
target_link_libraries(dlarp-example PRIVATE dlarp)
