add_executable(test-partclone
    test-partclone.cpp
    ${CMAKE_SOURCE_DIR}/partclone/src/progress.c
    ${CMAKE_SOURCE_DIR}/partclone/src/partclone.c
    ${CMAKE_SOURCE_DIR}/partclone/src/checksum.c
    ${CMAKE_SOURCE_DIR}/partclone/src/extfsclone.c)
target_include_directories(test-partclone PRIVATE
    ${CMAKE_SOURCE_DIR}/partclone/src
    ${CMAKE_SOURCE_DIR}/partclone)
target_link_libraries(test-partclone
    Qt5::Core
    ${LIBNCURSESW_LIBRARIES}
    ${LIBE2FSPROGS_LIBRARIES})
add_test(test-partclone test-partclone)
