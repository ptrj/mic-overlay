if(NOT CPACK_INSTALL_CMAKE_PROJECTS)
    # Source archives must contain the project at the root, without usr/.
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
elseif(CPACK_GENERATOR MATCHES "^(DEB|RPM)$" AND NOT CPACK_INSTALL_PREFIX STREQUAL "/usr")
    message(FATAL_ERROR "Configure with -DCMAKE_INSTALL_PREFIX=/usr before generating DEB/RPM; the desktop launcher embeds this path.")
endif()
