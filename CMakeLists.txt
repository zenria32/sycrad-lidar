cmake_minimum_required(VERSION 3.16)

project(Sycrad VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/MP)
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(THIRD_PARTY_DIR "${PROJECT_ROOT_DIR}/third_party")

function(set_first_existing_path output_var)
    foreach(candidate ${ARGN})
        if(EXISTS "${candidate}")
            set(${output_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    list(GET ARGN 0 first_candidate)
    set(${output_var} "${first_candidate}" PARENT_SCOPE)
endfunction()

set(APP_ICON_SOURCES "")
if(APPLE)
    set(APP_ICON_MAC "${PROJECT_ROOT_DIR}/icons/mac.icns")
    list(APPEND APP_ICON_SOURCES "${APP_ICON_MAC}")
elseif(WIN32)
    set(APP_ICON_WIN "${PROJECT_ROOT_DIR}/icons/win.ico")
    set(APP_ICON_RESOURCE "${PROJECT_ROOT_DIR}/win_ico.rc")
    list(APPEND APP_ICON_SOURCES "${APP_ICON_RESOURCE}")
endif()

set(QT_BASE_DIR "${THIRD_PARTY_DIR}/QtBase")
set(QT_SHADER_TOOLS_DIR "${THIRD_PARTY_DIR}/QtShaderTools")
set(QT_SVG_DIR "${THIRD_PARTY_DIR}/QtSvg")

set(QT_INCLUDE_DIR "${QT_BASE_DIR}/include")
set(QT_PLATFORM_INCLUDE_DIR "")
if(APPLE)
    set(QT_PLATFORM_INCLUDE_DIR "${QT_INCLUDE_DIR}/macos")
elseif(WIN32)
    set(QT_PLATFORM_INCLUDE_DIR "${QT_INCLUDE_DIR}/windows")
endif()

set(QT_BASE_INCLUDE_DIRS "${QT_INCLUDE_DIR}")
if(QT_PLATFORM_INCLUDE_DIR AND EXISTS "${QT_PLATFORM_INCLUDE_DIR}")
    list(PREPEND QT_BASE_INCLUDE_DIRS "${QT_PLATFORM_INCLUDE_DIR}")
endif()

set(QT_CORE_DIR "${QT_INCLUDE_DIR}/QtCore")
set(QT_GUI_DIR "${QT_INCLUDE_DIR}/QtGui")
set(QT_WIDGETS_DIR "${QT_INCLUDE_DIR}/QtWidgets")
set(QT_CONCURRENT_DIR "${QT_INCLUDE_DIR}/QtConcurrent")
set(QT_DBUS_DIR "${QT_INCLUDE_DIR}/QtDBus")
set(QT_GUI_UPDATED_DIR "${QT_GUI_DIR}/6.12.0/QtGui")
set(QT_GUI_RHI_DIR "${QT_GUI_UPDATED_DIR}/rhi")
set(QT_GUI_RHI_P_DIR "${QT_GUI_UPDATED_DIR}/private")

if(QT_PLATFORM_INCLUDE_DIR)
    set_first_existing_path(QT_CORE_DIR
            "${QT_PLATFORM_INCLUDE_DIR}/QtCore"
            "${QT_INCLUDE_DIR}/QtCore"
    )
    set_first_existing_path(QT_GUI_DIR
            "${QT_PLATFORM_INCLUDE_DIR}/QtGui"
            "${QT_INCLUDE_DIR}/QtGui"
    )
    set_first_existing_path(QT_WIDGETS_DIR
            "${QT_PLATFORM_INCLUDE_DIR}/QtWidgets"
            "${QT_INCLUDE_DIR}/QtWidgets"
    )
    set_first_existing_path(QT_CONCURRENT_DIR
            "${QT_PLATFORM_INCLUDE_DIR}/QtConcurrent"
            "${QT_INCLUDE_DIR}/QtConcurrent"
    )
    set_first_existing_path(QT_DBUS_DIR
            "${QT_PLATFORM_INCLUDE_DIR}/QtDBus"
            "${QT_INCLUDE_DIR}/QtDBus"
    )
    set_first_existing_path(QT_GUI_UPDATED_DIR
            "${QT_PLATFORM_INCLUDE_DIR}/QtGui/6.12.0/QtGui"
            "${QT_INCLUDE_DIR}/QtGui/6.12.0/QtGui"
    )
    set(QT_GUI_RHI_DIR "${QT_GUI_UPDATED_DIR}/rhi")
    set(QT_GUI_RHI_P_DIR "${QT_GUI_UPDATED_DIR}/private")
endif()

set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(RENDER_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/render_engine")
set(UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(SPATIAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spatial")
set(CALIBRATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/calibration")

set(LAS_TOOLS_DIR "${THIRD_PARTY_DIR}/LAStools")
set(KISS_ICP_DIR "${THIRD_PARTY_DIR}/kiss_icp")
set(ANKERL_DIR "${THIRD_PARTY_DIR}")

set(DATA_SOURCE_CODE
        ${DATA_DIR}/data.h
        ${DATA_DIR}/data_loader.h
        ${DATA_DIR}/data_loader.cpp
        ${DATA_DIR}/pcd_reader.h
        ${DATA_DIR}/pcd_reader.cpp
        ${DATA_DIR}/cuboid.h
        ${DATA_DIR}/cuboid_commands.h
        ${DATA_DIR}/cuboid_manager.h
        ${DATA_DIR}/cuboid_manager.cpp
)

set(RENDER_ENGINE_SOURCE_CODE
        ${RENDER_ENGINE_DIR}/abstract_renderer.h
        ${RENDER_ENGINE_DIR}/abstract_renderer.cpp
        ${RENDER_ENGINE_DIR}/grid_renderer.h
        ${RENDER_ENGINE_DIR}/grid_renderer.cpp
        ${RENDER_ENGINE_DIR}/point_cloud_renderer.h
        ${RENDER_ENGINE_DIR}/point_cloud_renderer.cpp
        ${RENDER_ENGINE_DIR}/orbital_camera.h
        ${RENDER_ENGINE_DIR}/orbital_camera.cpp
        ${RENDER_ENGINE_DIR}/camera_controller.h
        ${RENDER_ENGINE_DIR}/camera_controller.cpp
        ${RENDER_ENGINE_DIR}/input_handler.h
        ${RENDER_ENGINE_DIR}/input_handler.cpp
        ${RENDER_ENGINE_DIR}/lidar_viewport.h
        ${RENDER_ENGINE_DIR}/lidar_viewport.cpp
        ${RENDER_ENGINE_DIR}/prepare_cmd_buffer.h
        ${RENDER_ENGINE_DIR}/prepare_cmd_buffer.cpp
        ${RENDER_ENGINE_DIR}/ray.h
        ${RENDER_ENGINE_DIR}/cuboid_renderer.h
        ${RENDER_ENGINE_DIR}/cuboid_renderer.cpp
        ${RENDER_ENGINE_DIR}/gizmo_renderer.h
        ${RENDER_ENGINE_DIR}/gizmo_renderer.cpp
)

set(UI_SOURCE_CODE
        ${UI_DIR}/main_window.h
        ${UI_DIR}/main_window.cpp
        ${UI_DIR}/splash_screen.h
        ${UI_DIR}/splash_screen.cpp
        ${UI_DIR}/camera_frame.h
        ${UI_DIR}/camera_frame.cpp
        ${UI_DIR}/notification_widget.h
        ${UI_DIR}/notification_widget.cpp
        ${UI_DIR}/project_manager.h
        ${UI_DIR}/project_manager.cpp
        ${UI_DIR}/import_widget.h
        ${UI_DIR}/import_widget.cpp
        ${UI_DIR}/statistics_widget.h
        ${UI_DIR}/statistics_widget.cpp
        ${UI_DIR}/loading_bar.h
        ${UI_DIR}/loading_bar.cpp
)

set(SPATIAL_SOURCE_CODE
        ${SPATIAL_DIR}/octree_node.h
        ${SPATIAL_DIR}/octree_builder.h
        ${SPATIAL_DIR}/octree_builder.cpp
)

set(CALIBRATION_SOURCE_CODE
        ${CALIBRATION_DIR}/calibration_data.h
        ${CALIBRATION_DIR}/calibration_parser.h
        ${CALIBRATION_DIR}/calibration_parser.cpp
        ${CALIBRATION_DIR}/calibration_store.h
        ${CALIBRATION_DIR}/calibration_store.cpp
)

set(PROJECT_SOURCE_CODE
        main.cpp
        resources.qrc
)

if(APPLE)
    set(QT_CORE_LIB_DIR "${QT_BASE_DIR}/lib/libQt6Core.6.dylib")
    set(QT_GUI_LIB_DIR "${QT_BASE_DIR}/lib/libQt6Gui.6.dylib")
    set(QT_WIDGETS_LIB_DIR "${QT_BASE_DIR}/lib/libQt6Widgets.6.dylib")
    set(QT_CONCURRENT_LIB_DIR "${QT_BASE_DIR}/lib/libQt6Concurrent.6.dylib")
    set(QT_SHADER_TOOLS_LIB_DIR "${QT_SHADER_TOOLS_DIR}/lib/libQt6ShaderTools.6.dylib")
    set(QT_DBUS_LIB_DIR "${QT_BASE_DIR}/lib/libQt6DBus.6.dylib")
    set(QT_SVG_RUNTIME_LIB "${QT_SVG_DIR}/lib/libQt6Svg.6.dylib")

    set(QT_SVG_ICON_PLUGIN "${QT_SVG_DIR}/plugins/iconengines/libqsvgicon.dylib")
    set(QT_SVG_IMAGE_PLUGIN "${QT_SVG_DIR}/plugins/imageformats/libqsvg.dylib")

    set(QSB_EXECUTABLE "${QT_SHADER_TOOLS_DIR}/bin/qsb")
    set(QT_MOC_EXECUTABLE "${QT_BASE_DIR}/libexec/moc")
    set(QT_RCC_EXECUTABLE "${QT_BASE_DIR}/libexec/rcc")
    set(QSB_ENV "DYLD_LIBRARY_PATH=${QT_BASE_DIR}/lib:${QT_SHADER_TOOLS_DIR}/lib")

    set(QT_PLATFORM_PLUGINS "${QT_BASE_DIR}/plugins/platforms/libqcocoa.dylib")
    set(QT_STYLE_PLUGINS "${QT_BASE_DIR}/plugins/styles/libqmacstyle.dylib")
    set(QT_IMAGEFORMAT_PLUGINS "${QT_BASE_DIR}/plugins/imageformats/libqico.dylib" "${QT_BASE_DIR}/plugins/imageformats/libqjpeg.dylib" "${QT_SVG_IMAGE_PLUGIN}")
    set(QT_ICONENGINE_PLUGINS "${QT_SVG_ICON_PLUGIN}")

    set(LAS_LIB_DIR "${LAS_TOOLS_DIR}/LASlib/lib/libLASlib.dylib")
elseif(WIN32)
    set(QT_CORE_LIB_DIR "${QT_BASE_DIR}/lib/Qt6Core.lib")
    set(QT_GUI_LIB_DIR "${QT_BASE_DIR}/lib/Qt6Gui.lib")
    set(QT_WIDGETS_LIB_DIR "${QT_BASE_DIR}/lib/Qt6Widgets.lib")
    set(QT_CONCURRENT_LIB_DIR "${QT_BASE_DIR}/lib/Qt6Concurrent.lib")
    set(QT_SHADER_TOOLS_LIB_DIR "${QT_SHADER_TOOLS_DIR}/lib/Qt6ShaderTools.lib")
    set(QT_DBUS_LIB_DIR "${QT_BASE_DIR}/lib/Qt6DBus.lib")
    set(QT_ENTRYPOINT_LIB "")
    if(EXISTS "${QT_BASE_DIR}/lib/Qt6EntryPoint.lib")
        set(QT_ENTRYPOINT_LIB "${QT_BASE_DIR}/lib/Qt6EntryPoint.lib")
    elseif(EXISTS "${QT_BASE_DIR}/lib/qtmain.lib")
        set(QT_ENTRYPOINT_LIB "${QT_BASE_DIR}/lib/qtmain.lib")
    endif()

    set_first_existing_path(QT_CORE_DLL
            "${QT_BASE_DIR}/libexec/Qt6Core.dll"
            "${QT_BASE_DIR}/bin/Qt6Core.dll"
    )
    set_first_existing_path(QT_GUI_DLL
            "${QT_BASE_DIR}/libexec/Qt6Gui.dll"
            "${QT_BASE_DIR}/bin/Qt6Gui.dll"
    )
    set_first_existing_path(QT_WIDGETS_DLL
            "${QT_BASE_DIR}/libexec/Qt6Widgets.dll"
            "${QT_BASE_DIR}/bin/Qt6Widgets.dll"
    )
    set_first_existing_path(QT_CONCURRENT_DLL
            "${QT_BASE_DIR}/libexec/Qt6Concurrent.dll"
            "${QT_BASE_DIR}/bin/Qt6Concurrent.dll"
    )
    set_first_existing_path(QT_DBUS_DLL
            "${QT_BASE_DIR}/libexec/Qt6DBus.dll"
            "${QT_BASE_DIR}/bin/Qt6DBus.dll"
    )
    set_first_existing_path(QT_SHADER_TOOLS_DLL
            "${QT_SHADER_TOOLS_DIR}/libexec/Qt6ShaderTools.dll"
            "${QT_SHADER_TOOLS_DIR}/bin/Qt6ShaderTools.dll"
    )

    set(QT_SVG_LIB_DIR "${QT_SVG_DIR}/lib/Qt6Svg.lib")
    set_first_existing_path(QT_SVG_RUNTIME_LIB
            "${QT_SVG_DIR}/libexec/Qt6Svg.dll"
            "${QT_SVG_DIR}/bin/Qt6Svg.dll"
    )
    set_first_existing_path(QT_SVG_ICON_PLUGIN
            "${QT_SVG_DIR}/libexec/qsvgicon.dll"
            "${QT_SVG_DIR}/plugins/iconengines/qsvgicon.dll"
    )
    set_first_existing_path(QT_SVG_IMAGE_PLUGIN
            "${QT_SVG_DIR}/libexec/qsvg.dll"
            "${QT_SVG_DIR}/plugins/imageformats/qsvg.dll"
    )

    set(QSB_EXECUTABLE "${QT_SHADER_TOOLS_DIR}/bin/qsb.exe")
    set_first_existing_path(QT_MOC_EXECUTABLE
            "${QT_BASE_DIR}/libexec/moc.exe"
            "${QT_BASE_DIR}/bin/moc.exe"
    )
    set_first_existing_path(QT_RCC_EXECUTABLE
            "${QT_BASE_DIR}/libexec/rcc.exe"
            "${QT_BASE_DIR}/bin/rcc.exe"
    )
    set(QSB_PATH "${QT_BASE_DIR}/libexec;${QT_SHADER_TOOLS_DIR}/libexec;${QT_SHADER_TOOLS_DIR}/bin;$ENV{PATH}")
    string(REPLACE ";" "\\;" QSB_PATH_ESCAPED "${QSB_PATH}")
    set(QSB_ENV "PATH=${QSB_PATH_ESCAPED}")

    set(LAS_LIB_DIR "${LAS_TOOLS_DIR}/LASlib/lib/LASlib.lib")
    set_first_existing_path(LAS_DLL
            "${LAS_TOOLS_DIR}/LASlib/libexec/LASlib.dll"
            "${LAS_TOOLS_DIR}/LASlib/lib/LASlib.dll"
    )

    set(QT_PLATFORM_PLUGINS "${QT_BASE_DIR}/plugins/platforms/qwindows.dll")
    set(QT_STYLE_PLUGINS "")
    if(EXISTS "${QT_BASE_DIR}/plugins/styles/qmodernwindowsstyle.dll")
        list(APPEND QT_STYLE_PLUGINS "${QT_BASE_DIR}/plugins/styles/qmodernwindowsstyle.dll")
    endif()

    set(QT_IMAGEFORMAT_PLUGINS "")
    if(EXISTS "${QT_BASE_DIR}/plugins/imageformats/qjpeg.dll")
        list(APPEND QT_IMAGEFORMAT_PLUGINS "${QT_BASE_DIR}/plugins/imageformats/qjpeg.dll")
    endif()
    if(EXISTS "${QT_BASE_DIR}/plugins/imageformats/qico.dll")
        list(APPEND QT_IMAGEFORMAT_PLUGINS "${QT_BASE_DIR}/plugins/imageformats/qico.dll")
    endif()
    if(EXISTS "${QT_BASE_DIR}/plugins/imageformats/qico.dll")
        list(APPEND QT_IMAGEFORMAT_PLUGINS "${QT_BASE_DIR}/plugins/imageformats/qico.dll")
    endif()
    list(APPEND QT_IMAGEFORMAT_PLUGINS "${QT_SVG_IMAGE_PLUGIN}")
    set(QT_ICONENGINE_PLUGINS "${QT_SVG_ICON_PLUGIN}")
    set(QT_GUI_PLATFORM_HEADER "${QT_PLATFORM_INCLUDE_DIR}/QtGui/qwindowdefs_win.h")
else()
    message(FATAL_ERROR "Linux is not currently supported.")
endif()

if(WIN32)
    set(KISS_ICP_CORE_LIB "${KISS_ICP_DIR}/lib/kiss_icp_core.lib")
    set(KISS_ICP_PIPELINE_LIB "${KISS_ICP_DIR}/lib/kiss_icp_pipeline.lib")
    set_first_existing_path(TBB_LIB "${KISS_ICP_DIR}/lib/tbb12.lib")
else()
    set(KISS_ICP_CORE_LIB "${KISS_ICP_DIR}/lib/libkiss_icp_core.a")
    set(KISS_ICP_PIPELINE_LIB "${KISS_ICP_DIR}/lib/libkiss_icp_pipeline.a")
    set(TBB_LIB "${KISS_ICP_DIR}/lib/libtbb.a")
endif()

set(REQUIRED_PATHS
        ${QT_CORE_LIB_DIR}
        ${QT_GUI_LIB_DIR}
        ${QT_WIDGETS_LIB_DIR}
        ${QT_CONCURRENT_LIB_DIR}
        ${QT_SHADER_TOOLS_LIB_DIR}
        ${QT_DBUS_LIB_DIR}
        ${QT_SVG_RUNTIME_LIB}
        ${QT_SVG_ICON_PLUGIN}
        ${QT_SVG_IMAGE_PLUGIN}
        ${QSB_EXECUTABLE}
        ${QT_MOC_EXECUTABLE}
        ${QT_RCC_EXECUTABLE}
        ${LAS_LIB_DIR}
        ${KISS_ICP_CORE_LIB}
        ${KISS_ICP_PIPELINE_LIB}
        ${TBB_LIB}
)
if(WIN32)
    list(APPEND REQUIRED_PATHS
            ${QT_GUI_PLATFORM_HEADER}
            ${QT_CORE_DLL}
            ${QT_GUI_DLL}
            ${QT_WIDGETS_DLL}
            ${QT_CONCURRENT_DLL}
            ${QT_DBUS_DLL}
            ${QT_SHADER_TOOLS_DLL}
            ${LAS_DLL}
    )
endif()
foreach(required_path ${REQUIRED_PATHS})
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Missing dependency: ${required_path}")
    endif()
endforeach()

set(QT_VERSION_MAJOR 6)
set_property(DIRECTORY PROPERTY QT_VERSION_MAJOR "${QT_VERSION_MAJOR}")

function(add_shader output_variable shader_path)
    set(options "")
    set(one_value_args GLSL HLSL MSL)
    cmake_parse_arguments(SHADER "${options}" "${one_value_args}" "" ${ARGN})

    set(shader_input "${PROJECT_ROOT_DIR}/${shader_path}")
    set(shader_output "${CMAKE_CURRENT_BINARY_DIR}/.qsb/${shader_path}.qsb")
    get_filename_component(shader_output_dir "${shader_output}" DIRECTORY)

    add_custom_command(
            OUTPUT "${shader_output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}"
            COMMAND "${CMAKE_COMMAND}" -E env "${QSB_ENV}"
            "${QSB_EXECUTABLE}"
            --glsl "${SHADER_GLSL}"
            --hlsl "${SHADER_HLSL}"
            --msl "${SHADER_MSL}"
            -o "${shader_output}"
            --depfile "${shader_output}.d"
            "${shader_input}"
            DEPFILE "${shader_output}.d"
            DEPENDS "${shader_input}" "${QSB_EXECUTABLE}" "${QT_SHADER_TOOLS_LIB_DIR}" "${QT_GUI_LIB_DIR}" "${QT_CORE_LIB_DIR}"
            COMMENT "Generating ${shader_path}.qsb"
            VERBATIM
    )

    set(${output_variable} "${shader_output}" PARENT_SCOPE)
endfunction()

add_shader(GRID_VERT_QSB shaders/grid.vert GLSL "100es,120,150" HLSL "50" MSL "12")
add_shader(GRID_FRAG_QSB shaders/grid.frag GLSL "100es,120,150" HLSL "50" MSL "12")
add_shader(POINT_CLOUD_VERT_QSB shaders/point_cloud.vert GLSL "300es,120,150" HLSL "50" MSL "12")
add_shader(POINT_CLOUD_FRAG_QSB shaders/point_cloud.frag GLSL "300es,120,150" HLSL "50" MSL "12")
add_shader(CUBOID_VERT_QSB shaders/cuboid.vert GLSL "300es,120,150" HLSL "50" MSL "12")
add_shader(CUBOID_FRAG_QSB shaders/cuboid.frag GLSL "300es,120,150" HLSL "50" MSL "12")
add_shader(GIZMO_VERT_QSB shaders/gizmo.vert GLSL "300es,120,150" HLSL "50" MSL "12")
add_shader(GIZMO_FRAG_QSB shaders/gizmo.frag GLSL "300es,120,150" HLSL "50" MSL "12")

set(GENERATED_SHADER_FILES
        "${GRID_VERT_QSB}"
        "${GRID_FRAG_QSB}"
        "${POINT_CLOUD_VERT_QSB}"
        "${POINT_CLOUD_FRAG_QSB}"
        "${CUBOID_VERT_QSB}"
        "${CUBOID_FRAG_QSB}"
        "${GIZMO_VERT_QSB}"
        "${GIZMO_FRAG_QSB}"
)

set(SHADER_OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/.qsb")
set(GENERATED_SHADER_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders.qrc")

file(WRITE "${GENERATED_SHADER_RESOURCE}" "<RCC>\n <qresource prefix=\"/\">\n")

foreach(shader_output IN LISTS GENERATED_SHADER_FILES)
    file(RELATIVE_PATH shader_alias "${SHADER_OUTPUT_ROOT}" "${shader_output}")
    file(APPEND "${GENERATED_SHADER_RESOURCE}" " <file alias=\"${shader_alias}\">${shader_output}</file>\n")
endforeach()

file(APPEND "${GENERATED_SHADER_RESOURCE}" " </qresource>\n</RCC>\n")

add_custom_target(Shaders DEPENDS ${GENERATED_SHADER_FILES})

add_library(QtCore SHARED IMPORTED GLOBAL)
set_target_properties(QtCore PROPERTIES
        COMPATIBLE_INTERFACE_STRING "QT_MAJOR_VERSION"
        INTERFACE_COMPILE_DEFINITIONS "QT_CORE_LIB"
        INTERFACE_INCLUDE_DIRECTORIES "${QT_BASE_INCLUDE_DIRS};${QT_CORE_DIR}"
)
if(WIN32)
    set_target_properties(QtCore PROPERTIES
            IMPORTED_IMPLIB "${QT_CORE_LIB_DIR}"
            IMPORTED_LOCATION "${QT_CORE_DLL}"
    )
else()
    set_target_properties(QtCore PROPERTIES
            IMPORTED_LOCATION "${QT_CORE_LIB_DIR}"
    )
endif()

add_library(QtGui SHARED IMPORTED GLOBAL)
set_target_properties(QtGui PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "QT_GUI_LIB"
        INTERFACE_INCLUDE_DIRECTORIES "${QT_BASE_INCLUDE_DIRS};${QT_GUI_DIR}"
        INTERFACE_LINK_LIBRARIES QtCore
)
if(WIN32)
    set_target_properties(QtGui PROPERTIES
            IMPORTED_IMPLIB "${QT_GUI_LIB_DIR}"
            IMPORTED_LOCATION "${QT_GUI_DLL}"
    )
else()
    set_target_properties(QtGui PROPERTIES
            IMPORTED_LOCATION "${QT_GUI_LIB_DIR}"
    )
endif()

add_library(QtWidgets SHARED IMPORTED GLOBAL)
set_target_properties(QtWidgets PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "QT_WIDGETS_LIB"
        INTERFACE_INCLUDE_DIRECTORIES "${QT_BASE_INCLUDE_DIRS};${QT_WIDGETS_DIR}"
        INTERFACE_LINK_LIBRARIES "QtGui;QtCore"
)
if(WIN32)
    set_target_properties(QtWidgets PROPERTIES
            IMPORTED_IMPLIB "${QT_WIDGETS_LIB_DIR}"
            IMPORTED_LOCATION "${QT_WIDGETS_DLL}"
    )
else()
    set_target_properties(QtWidgets PROPERTIES
            IMPORTED_LOCATION "${QT_WIDGETS_LIB_DIR}"
    )
endif()

add_library(QtConcurrent SHARED IMPORTED GLOBAL)
set_target_properties(QtConcurrent PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "QT_CONCURRENT_LIB"
        INTERFACE_INCLUDE_DIRECTORIES "${QT_BASE_INCLUDE_DIRS};${QT_CONCURRENT_DIR}"
        INTERFACE_LINK_LIBRARIES QtCore
)
if(WIN32)
    set_target_properties(QtConcurrent PROPERTIES
            IMPORTED_IMPLIB "${QT_CONCURRENT_LIB_DIR}"
            IMPORTED_LOCATION "${QT_CONCURRENT_DLL}"
    )
else()
    set_target_properties(QtConcurrent PROPERTIES
            IMPORTED_LOCATION "${QT_CONCURRENT_LIB_DIR}"
    )
endif()

add_library(LASlib SHARED IMPORTED GLOBAL)
set_target_properties(LASlib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LAS_TOOLS_DIR}/LASlib/inc;${LAS_TOOLS_DIR}/LASlib/src;${LAS_TOOLS_DIR}/LASzip/include/laszip;${LAS_TOOLS_DIR}/LASzip/src"
)
if(WIN32)
    set_target_properties(LASlib PROPERTIES
            IMPORTED_IMPLIB "${LAS_LIB_DIR}"
            IMPORTED_LOCATION "${LAS_DLL}"
    )
else()
    set_target_properties(LASlib PROPERTIES
            IMPORTED_LOCATION "${LAS_LIB_DIR}"
    )
endif()

add_library(tbb STATIC IMPORTED GLOBAL)
set_target_properties(tbb PROPERTIES
        IMPORTED_LOCATION "${TBB_LIB}"
)

add_library(K_C STATIC IMPORTED GLOBAL)
set_target_properties(K_C PROPERTIES
        IMPORTED_LOCATION "${KISS_ICP_CORE_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${KISS_ICP_DIR}/include"
        INTERFACE_LINK_LIBRARIES tbb
)

add_library(K_P STATIC IMPORTED GLOBAL)
set_target_properties(K_P PROPERTIES
        IMPORTED_LOCATION "${KISS_ICP_PIPELINE_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${KISS_ICP_DIR}/include"
        INTERFACE_LINK_LIBRARIES "K_C"
)

if(APPLE)
    add_executable(Sycrad MACOSX_BUNDLE
            ${PROJECT_SOURCE_CODE}
            ${GENERATED_SHADER_RESOURCE}
            ${DATA_SOURCE_CODE}
            ${RENDER_ENGINE_SOURCE_CODE}
            ${UI_SOURCE_CODE}
            ${SPATIAL_SOURCE_CODE}
            ${CALIBRATION_SOURCE_CODE}
            ${APP_ICON_SOURCES}
    )
elseif(WIN32)
    add_executable(Sycrad WIN32
            ${PROJECT_SOURCE_CODE}
            ${GENERATED_SHADER_RESOURCE}
            ${DATA_SOURCE_CODE}
            ${RENDER_ENGINE_SOURCE_CODE}
            ${UI_SOURCE_CODE}
            ${SPATIAL_SOURCE_CODE}
            ${CALIBRATION_SOURCE_CODE}
            ${APP_ICON_SOURCES}
    )
else()
    message(FATAL_ERROR "Linux is not currently supported.")
endif()

add_dependencies(Sycrad Shaders)

if(APPLE)
    set_source_files_properties("${APP_ICON_MAC}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

target_include_directories(Sycrad PRIVATE
        ${PROJECT_ROOT_DIR}
        ${DATA_DIR}
        ${RENDER_ENGINE_DIR}
        ${UI_DIR}
        ${SPATIAL_DIR}
        ${CALIBRATION_DIR}
        ${QT_BASE_INCLUDE_DIRS}
        ${QT_CORE_DIR}
        ${QT_GUI_DIR}
        ${QT_WIDGETS_DIR}
        ${QT_CONCURRENT_DIR}
        ${QT_GUI_UPDATED_DIR}
        ${QT_GUI_RHI_DIR}
        ${QT_GUI_RHI_P_DIR}
        ${LAS_TOOLS_DIR}/LASlib/inc
        ${LAS_TOOLS_DIR}/LASlib/src
        ${LAS_TOOLS_DIR}/LASzip/include/laszip
        ${LAS_TOOLS_DIR}/LASzip/src
        ${KISS_ICP_DIR}/include
        ${ANKERL_DIR}
)

target_link_libraries(Sycrad PRIVATE
        QtCore
        QtGui
        QtWidgets
        QtConcurrent
        LASlib
        K_P
)

if(APPLE)
    target_link_libraries(Sycrad PRIVATE
            "-framework Metal"
            "-framework Foundation"
            "-framework QuartzCore"
    )
endif()

if(WIN32 AND QT_ENTRYPOINT_LIB)
    target_link_libraries(Sycrad PRIVATE "${QT_ENTRYPOINT_LIB}")
elseif(WIN32 AND MSVC)
    target_link_options(Sycrad PRIVATE "/ENTRY:mainCRTStartup")
endif()

set_target_properties(Sycrad PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.Sycrad"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Sycrad LiDAR"
        AUTOMOC_EXECUTABLE "${QT_MOC_EXECUTABLE}"
        AUTORCC_EXECUTABLE "${QT_RCC_EXECUTABLE}"
        WIN32_EXECUTABLE TRUE
)

if(APPLE)
    set_target_properties(Sycrad PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks;${QT_BASE_DIR}/lib;${QT_SHADER_TOOLS_DIR}/lib;${QT_SVG_DIR}/lib;${LAS_TOOLS_DIR}/LASlib/lib"
            INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

if(APPLE)
    set(QT_RUNTIME_LIBS
            "${QT_CORE_LIB_DIR}"
            "${QT_GUI_LIB_DIR}"
            "${QT_WIDGETS_LIB_DIR}"
            "${QT_CONCURRENT_LIB_DIR}"
            "${QT_DBUS_LIB_DIR}"
            "${QT_SVG_RUNTIME_LIB}"
    )
elseif(WIN32)
    set(QT_RUNTIME_LIBS
            "${QT_CORE_DLL}"
            "${QT_GUI_DLL}"
            "${QT_WIDGETS_DLL}"
            "${QT_CONCURRENT_DLL}"
            "${QT_DBUS_DLL}"
            "${QT_SVG_RUNTIME_LIB}"
    )
endif()

set(QT_CONF_FILE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf")
if(APPLE)
    file(WRITE "${QT_CONF_FILE}" "[Paths]\nPlugins = PlugIns\n")
elseif(WIN32)
    file(WRITE "${QT_CONF_FILE}" "[Paths]\nPlugins = plugins\n")
endif()

if(APPLE)
    add_custom_command(TARGET Sycrad POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/../Frameworks"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/platforms"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/styles"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/imageformats"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/iconengines"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/../Resources"
            COMMENT "Copying Qt runtime files into the app bundle"
    )

    foreach(runtime_lib IN LISTS QT_RUNTIME_LIBS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_lib}"
                "$<TARGET_FILE_DIR:Sycrad>/../Frameworks/"
        )
    endforeach()

    add_custom_command(TARGET Sycrad POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${LAS_LIB_DIR}"
            "$<TARGET_FILE_DIR:Sycrad>/../Frameworks/"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${QT_CONF_FILE}"
            "$<TARGET_FILE_DIR:Sycrad>/../Resources/qt.conf"
    )
    foreach(plugin_file IN LISTS QT_PLATFORM_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/platforms/"
        )
    endforeach()
    foreach(plugin_file IN LISTS QT_STYLE_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/styles/"
        )
    endforeach()
    foreach(plugin_file IN LISTS QT_IMAGEFORMAT_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/imageformats/"
        )
    endforeach()
    foreach(plugin_file IN LISTS QT_ICONENGINE_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/../PlugIns/iconengines/"
        )
    endforeach()
elseif(WIN32)
    add_custom_command(TARGET Sycrad POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/plugins/platforms"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/plugins/styles"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/plugins/imageformats"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Sycrad>/plugins/iconengines"
            COMMENT "Copying Qt runtime files next to the Windows executable"
    )
    foreach(runtime_lib IN LISTS QT_RUNTIME_LIBS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_lib}"
                "$<TARGET_FILE_DIR:Sycrad>/"
        )
    endforeach()
    add_custom_command(TARGET Sycrad POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${LAS_DLL}"
            "$<TARGET_FILE_DIR:Sycrad>/"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${QT_CONF_FILE}"
            "$<TARGET_FILE_DIR:Sycrad>/qt.conf"
    )
    foreach(plugin_file IN LISTS QT_PLATFORM_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/plugins/platforms/"
        )
    endforeach()
    foreach(plugin_file IN LISTS QT_STYLE_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/plugins/styles/"
        )
    endforeach()
    foreach(plugin_file IN LISTS QT_IMAGEFORMAT_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/plugins/imageformats/"
        )
    endforeach()
    foreach(plugin_file IN LISTS QT_ICONENGINE_PLUGINS)
        add_custom_command(TARGET Sycrad POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${plugin_file}"
                "$<TARGET_FILE_DIR:Sycrad>/plugins/iconengines/"
        )
    endforeach()
endif()

include(GNUInstallDirs)
install(TARGETS Sycrad
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(APPLE)
    install(FILES ${QT_RUNTIME_LIBS} DESTINATION "Sycrad.app/Contents/Frameworks")
    install(FILES "${LAS_LIB_DIR}" DESTINATION "Sycrad.app/Contents/Frameworks")
    install(FILES "${QT_CONF_FILE}" DESTINATION "Sycrad.app/Contents/Resources")
    install(FILES ${QT_PLATFORM_PLUGINS} DESTINATION "Sycrad.app/Contents/PlugIns/platforms")
    install(FILES ${QT_STYLE_PLUGINS} DESTINATION "Sycrad.app/Contents/PlugIns/styles")
    install(FILES ${QT_IMAGEFORMAT_PLUGINS} DESTINATION "Sycrad.app/Contents/PlugIns/imageformats")
    if(QT_ICONENGINE_PLUGINS)
        install(FILES ${QT_ICONENGINE_PLUGINS} DESTINATION "Sycrad.app/Contents/PlugIns/iconengines")
    endif()
elseif(WIN32)
    install(FILES ${QT_RUNTIME_LIBS} DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${LAS_DLL}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES ${QT_PLATFORM_PLUGINS} DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/platforms")
    if(QT_STYLE_PLUGINS)
        install(FILES ${QT_STYLE_PLUGINS} DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/styles")
    endif()
    install(FILES ${QT_IMAGEFORMAT_PLUGINS} DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/imageformats")
    if(QT_ICONENGINE_PLUGINS)
        install(FILES ${QT_ICONENGINE_PLUGINS} DESTINATION "${CMAKE_INSTALL_BINDIR}/plugins/iconengines")
    endif()
    install(FILES "${QT_CONF_FILE}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
