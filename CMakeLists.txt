cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # mGBA 0.10.5 declara CMake antigo
project(gba-frontend C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Compilar testes unitários" ON)

# libmgba como subprojeto: frontends e dependências opcionais ficam desligados e a lib é estática
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(M_CORE_GB OFF CACHE BOOL "" FORCE)
set(USE_DEBUGGERS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/mgba EXCLUDE_FROM_ALL)

find_package(SDL2 REQUIRED)

# Fonte Inter embutida como array C (gerado uma vez; apague build/inter_data.c para regerar).
set(INTER_TTF ${CMAKE_CURRENT_SOURCE_DIR}/third_party/inter/Inter-Regular.ttf)
if(NOT EXISTS ${CMAKE_BINARY_DIR}/inter_data.c)
  file(READ ${INTER_TTF} INTER_HEX HEX)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," INTER_ARRAY "${INTER_HEX}")
  file(WRITE ${CMAKE_BINARY_DIR}/inter_data.c
    "const unsigned char inter_regular_ttf[] = {${INTER_ARRAY}};\nconst unsigned long inter_regular_ttf_size = sizeof(inter_regular_ttf);\n")
endif()

add_library(frontend_core STATIC src/input.c src/save.c src/utf8.c src/romscan.c src/bindings.c src/config.c src/menu.c src/gfx.c src/ttf.c src/icons.c src/library.c src/layout.c src/textedit.c src/settings.c src/hotkeys.c src/volume.c src/scale.c src/pacer.c src/view.c src/ui_draw.c src/turbo.c src/canvas.c src/ui_metrics.c src/stick.c src/nav.c src/overlay.c src/macro.c src/macro_store.c src/editor.c ${CMAKE_BINARY_DIR}/inter_data.c)
target_include_directories(frontend_core PUBLIC src third_party/mgba/include)
target_include_directories(frontend_core PRIVATE third_party/stb)
target_link_libraries(frontend_core PUBLIC SDL2::SDL2)

add_executable(gba-frontend src/main.c src/game.c src/video.c src/audio.c src/folderpick.c)
target_compile_definitions(gba-frontend PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(gba-frontend PRIVATE frontend_core mgba)
set_target_properties(gba-frontend PROPERTIES OUTPUT_NAME "gameboy front")
if(WIN32)
  target_link_libraries(gba-frontend PRIVATE shell32 ole32 uuid)
  # Ícone do .exe (assets/icon.ico, embutido pelo windres)
  enable_language(RC)
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/assets")
  target_sources(gba-frontend PRIVATE assets/app.rc)
  set_source_files_properties(assets/app.rc PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico)
  # Aplicativo de janela: sem o terminal que o Windows abre junto com um programa de console.
  # Para depurar com as mensagens no terminal, configure com -DGBA_CONSOLE=ON.
  option(GBA_CONSOLE "Compilar o gba-frontend como aplicativo de console (mostra o terminal)" OFF)
  if(NOT GBA_CONSOLE)
    target_link_options(gba-frontend PRIVATE -mwindows)
  endif()
endif()

add_executable(snapshot tools/snapshot.c)
target_link_libraries(snapshot PRIVATE frontend_core)
add_executable(ui_bench tools/ui_bench.c)
target_link_libraries(ui_bench PRIVATE frontend_core)
add_executable(crop_compare tools/crop_compare.c)
target_link_libraries(crop_compare PRIVATE frontend_core)
add_executable(dpi_probe tools/dpi_probe.c)
target_link_libraries(dpi_probe PRIVATE SDL2::SDL2)
add_executable(video_check tools/video_check.c src/video.c)
target_link_libraries(video_check PRIVATE frontend_core)

if(ENABLE_TESTS)
  enable_testing()
  add_executable(test_game tests/test_game.c src/game.c src/audio.c)
  target_link_libraries(test_game PRIVATE frontend_core mgba)
  add_test(NAME game COMMAND test_game)
  foreach(t input save utf8 romscan bindings config menu gfx ttf icons library layout textedit settings volume hotkeys scale pacer view ui_render thumb turbo canvas sharpness ui_metrics stick nav overlay macro macro_store editor)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${t}.c)
      add_executable(test_${t} tests/test_${t}.c)
      target_link_libraries(test_${t} PRIVATE frontend_core)
      add_test(NAME ${t} COMMAND test_${t})
    endif()
  endforeach()
endif()
