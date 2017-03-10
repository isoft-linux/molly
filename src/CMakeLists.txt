qt5_add_resources(molly_RC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../resources.qrc)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(molly_PART_SRCS
    main.cpp
    stepwidget.cpp
    wizardwidget.cpp
    partclonewidget.cpp
    diskclonewidget.cpp
    partrestorewidget.cpp
    diskrestorewidget.cpp
    ${molly_RC_SRCS}
)

qt5_add_dbus_interface(molly_PART_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.isoftlinux.OSProber.xml
    osprober
)

add_executable(molly ${molly_PART_SRCS})

target_link_libraries(molly
    Qt5::Gui
    Qt5::Widgets
    Qt5::DBus
    udisks2-qt5
)

install(TARGETS molly RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/molly.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
