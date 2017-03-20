qt5_add_resources(molly_RC_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../resources.qrc)

include_directories(
    ${LIBPARTCLONE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(molly_PART_SRCS
    main.cpp
    stepwidget.cpp
    wizardwidget.cpp
    partclonewidget.cpp
    parttofilewidget.cpp
    imgdialog.cpp
    parttopartwidget.cpp
    diskclonewidget.cpp
    disktofilewidget.cpp
    disktodiskwidget.cpp
    restorewidget.cpp
    filetopartwidget.cpp
    partrestorewidget.cpp
    filetodiskwidget.cpp
    utils.cpp
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
    ${LIBPARTCLONE_LIBRARIES}
)

install(TARGETS molly RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/molly.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
