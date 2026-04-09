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
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/enemies/*.cpp")

# 2. Directorios de inclusión
include_directories(include lib/include)

# 3. Buscar paquetes
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

# 4. Crear el ejecutable
add_executable(Bomberman ${SOURCES})

# 5. Vinculación inteligente (Target Link Libraries)
# Usamos condiciones para las librerías específicas de cada sistema
if(WIN32)
    # En Windows, las dependencias de sistema suelen venir con el compilador o la SDK
    target_link_libraries(Bomberman PRIVATE 
        GLEW::GLEW 
        glfw 
        OpenGL::GL
    )

    # Copiar DLLs de runtime junto al .exe para poder ejecutarlo desde build/
    # sin depender de PATH del sistema/terminal.
    set(MSYS2_UCRT_BIN "C:/msys64/ucrt64/bin")
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
else()
    # En Linux, necesitamos las librerías de X11 que mencionaste antes
    find_package(Threads REQUIRED) # Reemplaza a -lpthread
    
    target_link_libraries(Bomberman PRIVATE 
        GLEW::GLEW 
        glfw 
        OpenGL::GL
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

