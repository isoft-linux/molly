#add_definitions(-std=c++11)

set(testudisksclientgui_SRC
    test-udisksclient-gui.cpp
)

qt5_add_dbus_interface(testudisksclientgui_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.isoftlinux.OSProber.xml osprober)

add_executable(test-udisksclient-gui
    ${testudisksclientgui_SRC})
target_include_directories(test-udisksclient-gui PRIVATE
    ${LIBPARTCLONE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(test-udisksclient-gui
    Qt5::Test 
    Qt5::Widgets
    Qt5::DBus
    udisks2-qt5
    ${LIBPARTCLONE_LIBRARIES})
add_test(test-udisksclient-gui test-udisksclient-gui)
