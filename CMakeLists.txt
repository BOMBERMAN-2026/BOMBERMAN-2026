###########################################################################################################

# cmake_minimum_required(VERSION 3.10)
# project( Bomberman )

# set(CMAKE_CXX_STANDARD 11)
# set(OpenGL_GL_PREFERENCE GLVND)

# # Add source files
# file(GLOB SOURCES 
#     src/*.cpp
# )
# file(GLOB LIBS
#     lib/*.a
# )

# include_directories(include 
#     lib/include
# )

# # Find OpenGL
# find_package(OpenGL REQUIRED)

# # Find GLEW
# find_package(GLEW REQUIRED)

# # Find GLFW
# find_package(glfw3 REQUIRED)

# add_executable(Bomberman ${SOURCES})

# target_link_libraries(Bomberman ${LIBS})

# target_link_libraries(Bomberman -lGL -lX11 -lXxf86vm -lXrandr -lpthread -lXi -ldl -lXinerama -lXcursor)
###########################################################################################################

cmake_minimum_required(VERSION 3.10)
project(Bomberman)

set(CMAKE_CXX_STANDARD 11)
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Configuración de archivos
file(GLOB SOURCES "src/*.cpp" "src/enemies/*.cpp")

# 2. Directorios de inclusión
include_directories(include lib/include)

# 3. Buscar paquetes
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

# FFmpeg local (headers en lib/include/, import libs y DLLs en lib/)
set(FFMPEG_LIB_DIR "${CMAKE_SOURCE_DIR}/lib")

# 4. Crear el ejecutable
add_executable(Bomberman ${SOURCES})

# 5. Vinculación inteligente (Target Link Libraries)
# Usamos condiciones para las librerías específicas de cada sistema
if(WIN32)
    # Localizar las import libraries de FFmpeg (.dll.a)
    file(GLOB FFMPEG_IMPORT_LIBS "${FFMPEG_LIB_DIR}/*.dll.a")

    # En Windows, las dependencias de sistema suelen venir con el compilador o la SDK
    target_link_libraries(Bomberman PRIVATE 
        GLEW::GLEW 
        glfw 
        OpenGL::GL
        ${FFMPEG_IMPORT_LIBS}
    )

    # Copiar DLLs de runtime junto al .exe para poder ejecutarlo desde build/
    # sin depender de PATH del sistema/terminal.
    set(MSYS2_UCRT_BIN "C:/msys64/mingw64/bin")
    set(BOMBERMAN_RUNTIME_DLLS
        glfw3.dll
        glew32.dll
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
    )

    foreach(_dll ${BOMBERMAN_RUNTIME_DLLS})
        if(EXISTS "${MSYS2_UCRT_BIN}/${_dll}")
            add_custom_command(TARGET Bomberman POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MSYS2_UCRT_BIN}/${_dll}"
                    "$<TARGET_FILE_DIR:Bomberman>/${_dll}"
                VERBATIM
            )
        endif()
    endforeach()

    # Copiar DLLs de FFmpeg locales al directorio del ejecutable
    file(GLOB FFMPEG_DLLS "${FFMPEG_LIB_DIR}/*.dll")
    foreach(_dll ${FFMPEG_DLLS})
        get_filename_component(_dll_name "${_dll}" NAME)
        add_custom_command(TARGET Bomberman POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dll}"
                "$<TARGET_FILE_DIR:Bomberman>/${_dll_name}"
            VERBATIM
        )
    endforeach()
else()
    # En Linux, necesitamos las librerías de X11 que mencionaste antes
    find_package(Threads REQUIRED) # Reemplaza a -lpthread

    # En Linux usar FFmpeg del sistema via pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswscale
    )
    
    target_link_libraries(Bomberman PRIVATE 
        GLEW::GLEW 
        glfw 
        OpenGL::GL
        PkgConfig::FFMPEG
        X11
        Xrandr
        Xinerama
        Xcursor
        Xi
        Xxf86vm
        ${CMAKE_DL_LIBS} # Reemplaza a -ldl
        Threads::Threads
    )
endif()

