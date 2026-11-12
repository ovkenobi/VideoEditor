cmake_minimum_required(VERSION 3.21)

project(VideoEditor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(VideoEditor WIN32
    src/main.cpp
    src/appsettings.cpp
    src/appsettings.h
    src/util/timeformat.cpp
    src/util/timeformat.h
    src/ffmpeg/mediainfo.h
    src/ffmpeg/ffprobe.cpp
    src/ffmpeg/ffprobe.h
    src/ffmpeg/ffmpegrunner.cpp
    src/ffmpeg/ffmpegrunner.h
    src/media/framesequence.cpp
    src/media/framesequence.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/concattab.cpp
    src/ui/concattab.h
    src/ui/extracttab.cpp
    src/ui/extracttab.h
    src/ui/imagestovideotab.cpp
    src/ui/imagestovideotab.h
    src/ui/overlaytab.cpp
    src/ui/overlaytab.h
    src/ui/settingsdialog.cpp
    src/ui/settingsdialog.h
)

target_include_directories(VideoEditor PRIVATE src)
target_link_libraries(VideoEditor PRIVATE Qt6::Widgets)

# FFmpeg/ffprobe must sit next to the application executable.
add_custom_command(TARGET VideoEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg.exe"
        "${CMAKE_CURRENT_SOURCE_DIR}/ffprobe.exe"
        "$<TARGET_FILE_DIR:VideoEditor>"
    COMMENT "Copying FFmpeg binaries to the build directory"
)
