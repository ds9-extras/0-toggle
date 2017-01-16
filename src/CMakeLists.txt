add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.zerocontrol\")

set(zerocontrol_SRCS
    zerocontrol.cpp
)

add_library(plasma_applet_zerocontrol MODULE ${zerocontrol_SRCS})

kcoreaddons_desktop_to_json(plasma_applet_zerocontrol package/metadata.desktop SERVICE_TYPES plasma-applet.desktop)

target_link_libraries(plasma_applet_zerocontrol
                      Qt5::Gui
                      KF5::Plasma
                      KF5::I18n)


install(TARGETS plasma_applet_zerocontrol DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/applets)
install(FILES org.kde.zerocontrol.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

plasma_install_package(package org.kde.zerocontrol)
ecm_install_icons(ICONS 512-apps-zeronet.png DESTINATION ${ICON_INSTALL_DIR})
