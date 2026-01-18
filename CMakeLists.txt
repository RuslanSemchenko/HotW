cmake_minimum_required(VERSION 3.21)

# Сообщаем vcpkg, что мы управляем языком ASM_MASM вручную
set(VCPKG_LOAD_MASM_PROPS FALSE)

project(HeartOfTheWest VERSION 1.0 LANGUAGES C CXX ASM_MASM)

# ============================================================
# vcpkg toolchain
# ============================================================
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    endif()
endif()

# ============================================================
# Установка стандартов языка
# ============================================================
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# ============================================================
# Build options
# ============================================================
option(BUILD_CLIENT "Build client executable" ON)
option(BUILD_SERVER "Build dedicated server" ON)
option(BUILD_GAME_MODULES "Build game DLL modules" ON)
option(BUILD_RENDERER_OPENGL2 "Build OpenGL2 renderer" ON)

option(USE_OPENAL "Use OpenAL" ON)
option(USE_CURL "Use CURL" ON)
option(USE_FREETYPE "Use Freetype" ON)
option(USE_CODEC_VORBIS "Use Vorbis codec" ON)
option(USE_CODEC_OPUS "Use Opus codec" ON)
option(USE_RENDERER_DLOPEN "Use renderer dlopen" ON)
option(USE_MUMBLE "Use Mumble support" OFF)
option(USE_VOIP "Use VOIP (Voice Over IP - requires speex library)" OFF)

# ============================================================
# Game identity
# ============================================================
set(BASEGAME "HotW")
set(CLIENTBIN "HotW")
set(SERVERBIN "HotW_dedicated")
set(PRODUCT_VERSION "1.0")
string(TIMESTAMP PRODUCT_RELEASE "%Y%m%d")

# ============================================================
# Global compile definitions
# ============================================================
add_compile_definitions(
    HotW
    STANDALONE
    USE_ICON
    NO_VM_COMPILED
    PRODUCT_VERSION=\"${PRODUCT_VERSION}\"
    PRODUCT_RELEASE=\"${PRODUCT_RELEASE}\"
    _USE_MATH_DEFINES
)

if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(__WIN64__)
endif()

if(MSVC)
    add_compile_definitions(
        inline=__inline
        restrict=__restrict
    )
endif()

# ============================================================
# Output layout
# ============================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================
# Include directories
# ============================================================
include_directories(
    code
    code/qcommon
    code/game
    code/cgame
    code/ui
    code/client
    code/server
    code/sys
    code/botlib
    code/null
    code/sdl
    code/asm
)

# ============================================================
# Common sources
# ============================================================
set(COMMON_SOURCES
    code/qcommon/cmd.c
    code/qcommon/common.c
    code/qcommon/cvar.c
    code/qcommon/files.c
    code/qcommon/md4.c
    code/qcommon/md5.c
    code/qcommon/msg.c
    code/qcommon/net_chan.c
    code/qcommon/net_ip.c
    code/qcommon/huffman.c
    code/qcommon/q_math.c
    code/qcommon/q_shared.c
    code/qcommon/unzip.c
    code/qcommon/ioapi.c
    code/qcommon/vm.c
    code/qcommon/vm_interpreted.c
    code/qcommon/vm_x86.c
    code/qcommon/cm_load.c
    code/qcommon/cm_patch.c
    code/qcommon/cm_polylib.c
    code/qcommon/cm_test.c
    code/qcommon/cm_trace.c

    code/botlib/be_aas_bspq3.c
    code/botlib/be_aas_cluster.c
    code/botlib/be_aas_debug.c
    code/botlib/be_aas_entity.c
    code/botlib/be_aas_file.c
    code/botlib/be_aas_main.c
    code/botlib/be_aas_move.c
    code/botlib/be_aas_optimize.c
    code/botlib/be_aas_reach.c
    code/botlib/be_aas_route.c
    code/botlib/be_aas_routealt.c
    code/botlib/be_aas_sample.c
    code/botlib/be_ai_char.c
    code/botlib/be_ai_chat.c
    code/botlib/be_ai_gen.c
    code/botlib/be_ai_goal.c
    code/botlib/be_ai_move.c
    code/botlib/be_ai_weap.c
    code/botlib/be_ai_weight.c
    code/botlib/be_ea.c
    code/botlib/be_interface.c
    code/botlib/l_crc.c
    code/botlib/l_libvar.c
    code/botlib/l_log.c
    code/botlib/l_memory.c
    code/botlib/l_precomp.c
    code/botlib/l_script.c
    code/botlib/l_struct.c
)

# ============================================================
# External libraries (vcpkg)
# ============================================================
find_package(ZLIB REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED) 
find_package(assimp CONFIG REQUIRED)
if(USE_CURL)
    find_package(CURL REQUIRED)
endif()
if(USE_OPENAL)
    find_package(OpenAL CONFIG REQUIRED)
endif()
if(USE_CODEC_VORBIS)
    find_package(Vorbis REQUIRED)
    set(VORBIS_LIBRARIES Vorbis::vorbisfile)
endif()
if(USE_FREETYPE)
    find_package(Freetype REQUIRED)
endif()

# Image libraries for renderer
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

# =================================================================
# КОДЕКИ (OGG, VORBIS, OPUS)
# =================================================================

# Opus codec support (for audio files)
if(USE_CODEC_OPUS)
    find_package(OpusFile REQUIRED)
    set(OPUS_LIBRARIES OpusFile::opusfile)
endif()

# ============================================================
# CLIENT
# ============================================================
if(WIN32)
    set(CLIENT_SYS_SOURCES
        code/sys/con_passive.c
        code/sys/sys_win32.c
    )
else()
    set(CLIENT_SYS_SOURCES
        code/sys/con_passive.c
        code/sys/sys_unix.c
        code/sys/con_tty.c
    )
endif()

if(BUILD_CLIENT)
    add_executable(${CLIENTBIN}
        ${COMMON_SOURCES}

        code/client/cl_main.c
        code/client/cl_console.c
        code/client/cl_input.c
        code/client/cl_keys.c
        code/client/cl_parse.c
        code/client/cl_scrn.c
        code/client/cl_ui.c
        code/client/cl_cgame.c
        code/client/cl_net_chan.c
        code/client/cl_avi.c
        code/client/cl_curl.c
        code/client/cl_cin.c

        # Sound system
        code/client/snd_main.c
        code/client/snd_codec.c
        code/client/snd_codec_wav.c
        code/client/snd_codec_ogg.c
        code/client/snd_codec_opus.c
        code/client/snd_dma.c
        code/client/snd_mem.c
        code/client/snd_mix.c
        code/client/snd_adpcm.c
        code/client/snd_wavelet.c

        # OpenAL
        code/client/qal.c
        code/client/snd_openal.c

        # SDL
        code/sdl/sdl_input.c
        code/sdl/sdl_snd.c

        # Compression
        code/qcommon/puff.c

        # Server code (for local server)
        code/server/sv_main.c
        code/server/sv_init.c
        code/server/sv_client.c
        code/server/sv_ccmds.c
        code/server/sv_snapshot.c
        code/server/sv_world.c
        code/server/sv_game.c
        code/server/sv_bot.c
        code/server/sv_net_chan.c

        # System
        code/sys/sys_main.c
        code/sys/con_log.c
        ${CLIENT_SYS_SOURCES}
        code/sys/win_resource.rc
    )

    # Add mumble support
    if(USE_MUMBLE)
        target_sources(${CLIENTBIN} PRIVATE
            code/client/libmumblelink.c
        )
        message(STATUS "Mumble support enabled - libmumblelink.c will be compiled")
    else()
        message(STATUS "Mumble support disabled")
    endif()

    # Add x86_64 specific files
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(MSVC)
            target_sources(${CLIENTBIN} PRIVATE
                code/asm/snapvector.c
                code/asm/ftola.c
                code/asm/vm_x86_64.asm
            )
        else()
            target_sources(${CLIENTBIN} PRIVATE
                code/asm/snapvector.c
                code/asm/ftola.c
            )
        endif()
    endif()

    # Build compile definitions list conditionally
    set(CLIENT_COMPILE_DEFS BUILD_CLIENT BOTLIB USE_CODEC_OPUS USE_CODEC_VORBIS USE_RENDERER_DLOPEN)
    if(USE_MUMBLE)
        list(APPEND CLIENT_COMPILE_DEFS USE_MUMBLE)
    endif()
    
    target_compile_definitions(${CLIENTBIN} PRIVATE ${CLIENT_COMPILE_DEFS})
    target_include_directories(${CLIENTBIN} PRIVATE ${ZLIB_INCLUDE_DIRS} ${VORBIS_PKG_INCLUDE_DIRS} ${OPUS_PKG_INCLUDE_DIRS})
    if(USE_FREETYPE)
        target_include_directories(${CLIENTBIN} PRIVATE ${FREETYPE_INCLUDE_DIRS})
    endif()

    target_link_libraries(${CLIENTBIN} PRIVATE
        # Общие библиотеки
        ZLIB::ZLIB
        SDL3::SDL3
        ${OPENGL_LIBRARIES}
    )
    if(NOT WIN32)
        target_link_libraries(${CLIENTBIN} PRIVATE m)
    endif()
    
    if(WIN32)
        target_link_libraries(${CLIENTBIN} PRIVATE
            ws2_32   # Обязательно для сетевого кода (net_ip.c)
            winmm
            gdi32
            ole32
        )
    endif()
    
    if(USE_OPENAL)
        target_compile_definitions(${CLIENTBIN} PRIVATE USE_OPENAL)
        target_link_libraries(${CLIENTBIN} PRIVATE OpenAL::OpenAL)
    endif()

    if(USE_CURL)
        target_compile_definitions(${CLIENTBIN} PRIVATE USE_CURL)
        target_link_libraries(${CLIENTBIN} PRIVATE CURL::libcurl)
    endif()

    if(USE_CODEC_VORBIS)
        target_link_libraries(${CLIENTBIN} PRIVATE ${VORBIS_LIBRARIES})
    endif()

    if(USE_CODEC_OPUS)
        target_link_libraries(${CLIENTBIN} PRIVATE ${OPUS_LIBRARIES})
    endif()

    if(USE_FREETYPE)
        target_compile_definitions(${CLIENTBIN} PRIVATE BUILD_FREETYPE)
        target_link_libraries(${CLIENTBIN} PRIVATE Freetype::Freetype)
    endif()
endif()

if(WIN32)
    set(SERVER_SYS_SOURCES
        code/sys/sys_win32.c
        code/sys/con_passive.c
    )
else()
    set(SERVER_SYS_SOURCES
        code/sys/sys_unix.c
        code/sys/con_tty.c
    )
endif()

# ============================================================
# SERVER
# ============================================================
if(BUILD_SERVER)
    add_executable(${SERVERBIN}
        # Пустой исходный файл
    )

    target_sources(${SERVERBIN} PRIVATE
        ${COMMON_SOURCES}

        code/server/sv_main.c
        code/server/sv_init.c
        code/server/sv_client.c
        code/server/sv_ccmds.c
        code/server/sv_snapshot.c
        code/server/sv_world.c
        code/server/sv_game.c
        code/server/sv_bot.c
        code/server/sv_net_chan.c

        code/null/null_client.c
        code/null/null_input.c
        code/null/null_snddma.c

        # Compression
        code/qcommon/puff.c

        code/sys/sys_main.c
        ${SERVER_SYS_SOURCES}
        code/sys/con_log.c
        code/sys/win_resource.rc
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(MSVC)
            target_sources(${SERVERBIN} PRIVATE
                code/asm/snapvector.c
                code/asm/ftola.c
                code/asm/vm_x86_64.asm
            )
        else()
            target_sources(${SERVERBIN} PRIVATE
                code/asm/snapvector.c
                code/asm/ftola.c
            )
        endif()
    endif()

    set(SERVER_COMPILE_DEFS DEDICATED BOTLIB USE_CODEC_OPUS USE_CODEC_VORBIS USE_RENDERER_DLOPEN)
    if(USE_MUMBLE)
        list(APPEND SERVER_COMPILE_DEFS USE_MUMBLE)
    endif()
    
    target_compile_definitions(${SERVERBIN} PRIVATE ${SERVER_COMPILE_DEFS})
    target_include_directories(${SERVERBIN} PRIVATE ${ZLIB_INCLUDE_DIRS} ${VORBIS_PKG_INCLUDE_DIRS} ${OPUS_PKG_INCLUDE_DIRS})

    target_link_libraries(${SERVERBIN} PRIVATE
        ZLIB::ZLIB
    )
    if(NOT WIN32)
        target_link_libraries(${SERVERBIN} PRIVATE m)
    endif()
    
    if(WIN32)
        target_link_libraries(${SERVERBIN} PRIVATE
            ws2_32
            winmm
            psapi
        )
    endif()

    if(USE_CODEC_VORBIS)
        target_link_libraries(${SERVERBIN} PRIVATE ${VORBIS_LIBRARIES})
    endif()

    if(USE_CODEC_OPUS)
        target_link_libraries(${SERVERBIN} PRIVATE ${OPUS_LIBRARIES})
    endif()

    if(USE_OPENAL)
        target_link_libraries(${SERVERBIN} PRIVATE OpenAL::OpenAL)
    endif()

    if(USE_CURL)
        target_link_libraries(${SERVERBIN} PRIVATE CURL::libcurl)
    endif()
endif()

# ============================================================
# GAME MODULES
# ============================================================
if(BUILD_GAME_MODULES)
    add_library(cgame SHARED
        code/cgame/cg_main.c
        code/cgame/cg_consolecmds.c
        code/cgame/cg_draw.c
        code/cgame/cg_drawtools.c
        code/cgame/cg_effects.c
        code/cgame/cg_ents.c
        code/cgame/cg_event.c
        code/cgame/cg_info.c
        code/cgame/cg_localents.c
        code/cgame/cg_marks.c
        code/cgame/cg_newdraw.c
        code/cgame/cg_players.c
        code/cgame/cg_playerstate.c
        code/cgame/cg_predict.c
        code/cgame/cg_servercmds.c
        code/cgame/cg_snapshot.c
        code/cgame/cg_view.c
        code/cgame/cg_weapons.c
        code/cgame/cg_syscalls.c
        code/cgame/cg_unlagged.c
        code/cgame/cg_sg_utils.c
        code/ui/ui_shared.c
        code/game/bg_misc.c
        code/game/bg_pmove.c
        code/game/bg_slidemove.c
        code/game/bg_lib.c
        code/qcommon/q_math.c
        code/qcommon/q_shared.c
    )
    
    # ИСПРАВЛЕНИЕ: Автоматическое имя для x64 (cgame_x86_64.dll)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(cgame PROPERTIES PREFIX "" OUTPUT_NAME "cgame_x86_64")
    else()
        set_target_properties(cgame PROPERTIES PREFIX "")
    endif()

    target_compile_definitions(cgame PRIVATE CGAME)
    target_include_directories(cgame PRIVATE ${ZLIB_INCLUDE_DIRS})

    add_library(qagame SHARED
        code/game/g_main.c
        code/game/g_active.c
        code/game/g_bot.c
        code/game/g_client.c
        code/game/g_cmds.c
        code/game/g_combat.c
        code/game/g_items.c
        code/game/g_mem.c
        code/game/g_misc.c
        code/game/g_missile.c
        code/game/g_mover.c
        code/game/g_session.c
        code/game/g_spawn.c
        code/game/g_svcmds.c
        code/game/g_target.c
        code/game/g_team.c
        code/game/g_trigger.c
        code/game/g_utils.c
        code/game/g_weapon.c
        code/game/g_unlagged.c
        code/game/g_sg_utils.c
        code/game/g_hit.c
        code/game/ai_chat.c
        code/game/ai_cmd.c
        code/game/ai_dmnet.c
        code/game/ai_dmq3.c
        code/game/ai_main.c
        code/game/ai_team.c
        code/game/ai_vcmd.c
        code/game/bg_misc.c
        code/game/bg_pmove.c
        code/game/bg_slidemove.c
        code/game/bg_lib.c
        code/game/g_syscalls.c
        code/qcommon/q_math.c
        code/qcommon/q_shared.c
    )
    
    # ИСПРАВЛЕНИЕ: Автоматическое имя для x64 (qagame_x86_64.dll)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(qagame PROPERTIES PREFIX "" OUTPUT_NAME "qagame_x86_64")
    else()
        set_target_properties(qagame PROPERTIES PREFIX "")
    endif()

    target_compile_definitions(qagame PRIVATE QAGAME)
    target_include_directories(qagame PRIVATE ${ZLIB_INCLUDE_DIRS})

    add_library(ui SHARED
        code/ui/ui_main.c
        code/ui/ui_atoms.c
        code/ui/ui_gameinfo.c
        code/ui/ui_players.c
        code/ui/ui_shared.c
        code/ui/ui_syscalls.c
        code/game/bg_misc.c
        code/game/bg_lib.c
        code/qcommon/q_math.c
        code/qcommon/q_shared.c
    )
    
    # ИСПРАВЛЕНИЕ: Автоматическое имя для x64 (ui_x86_64.dll)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(ui PROPERTIES PREFIX "" OUTPUT_NAME "ui_x86_64")
    else()
        set_target_properties(ui PROPERTIES PREFIX "")
    endif()

    target_compile_definitions(ui PRIVATE UI)
    target_include_directories(ui PRIVATE ${ZLIB_INCLUDE_DIRS})
endif()

# ============================================================
# RENDERERS
# ============================================================
if(BUILD_CLIENT)
    # Common renderer sources
    set(RENDERER_COMMON_SOURCES
        code/renderercommon/tr_font.c
        code/renderercommon/tr_image_bmp.c
        code/renderercommon/tr_image_jpg.c
        code/renderercommon/tr_image_png.c
        code/renderercommon/tr_image_tga.c
        code/renderercommon/tr_image_pcx.c
        code/renderercommon/tr_noise.c
        code/qcommon/q_shared.c
        code/qcommon/q_math.c
        code/qcommon/puff.c
    )

    # OpenGL1 renderer
    add_library(renderer_opengl1 SHARED
        ${RENDERER_COMMON_SOURCES}
        code/renderergl1/tr_animation.c
        code/renderergl1/tr_backend.c
        code/renderergl1/tr_bsp.c
        code/renderergl1/tr_cmds.c
        code/renderergl1/tr_curve.c
        code/renderergl1/tr_flares.c
        code/renderergl1/tr_image.c
        code/renderergl1/tr_init.c
        code/renderergl1/tr_light.c
        code/renderergl1/tr_main.c
        code/renderergl1/tr_marks.c
        code/renderergl1/tr_mesh.c
        code/renderergl1/tr_model.c
        code/renderergl1/tr_model_iqm.c
        code/renderergl1/tr_scene.c
        code/renderergl1/tr_shade.c
        code/renderergl1/tr_shade_calc.c
        code/renderergl1/tr_shader.c
        code/renderergl1/tr_shadows.c
        code/renderergl1/tr_sky.c
        code/renderergl1/tr_surface.c
        code/renderergl1/tr_world.c
        code/sdl/sdl_gamma.c
        code/sdl/sdl_glimp.c
        code/renderergl1/tr_subs.c
    )
    set_target_properties(renderer_opengl1 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "renderer_opengl1_x86_64"
    )
    target_compile_definitions(renderer_opengl1 PRIVATE USE_RENDERER_DLOPEN)
    target_include_directories(renderer_opengl1 PRIVATE ${ZLIB_INCLUDE_DIRS})
    if(USE_FREETYPE)
        target_include_directories(renderer_opengl1 PRIVATE ${FREETYPE_INCLUDE_DIRS})
    endif()
    target_link_libraries(renderer_opengl1 PRIVATE
        ZLIB::ZLIB
        SDL3::SDL3
        ${OPENGL_LIBRARIES}
        JPEG::JPEG
        PNG::PNG
        assimp::assimp
    )
    if(USE_FREETYPE)
        target_compile_definitions(renderer_opengl1 PRIVATE BUILD_FREETYPE)
        target_link_libraries(renderer_opengl1 PRIVATE Freetype::Freetype)
    endif()

    # OpenGL2 renderer
    if(BUILD_RENDERER_OPENGL2)
        add_library(renderer_opengl2 SHARED
            ${RENDERER_COMMON_SOURCES}
            code/renderergl2/tr_animation.c
            code/renderergl2/tr_subs.c
            code/renderergl2/tr_backend.c
            code/renderergl2/tr_bsp.c
            code/renderergl2/tr_cmds.c
            code/renderergl2/tr_curve.c
            code/renderergl2/tr_extramath.c
            code/renderergl2/tr_extensions.c
            code/renderergl2/tr_fbo.c
            code/renderergl2/tr_flares.c
            code/renderergl2/tr_glsl.c
            code/renderergl2/tr_image.c
            code/renderergl2/tr_init.c
            code/renderergl2/tr_light.c
            code/renderergl2/tr_main.c
            code/renderergl2/tr_marks.c
            code/renderergl2/tr_mesh.c
            code/renderergl2/tr_model.c
            code/renderergl2/tr_model_iqm.c
            code/renderergl2/tr_postprocess.c
            code/renderergl2/tr_scene.c
            code/renderergl2/tr_shade.c
            code/renderergl2/tr_shade_calc.c
            code/renderergl2/tr_shader.c
            code/renderergl2/tr_shadows.c
            code/renderergl2/tr_fallback_shaders.c
            code/renderergl2/tr_sky.c
            code/renderergl2/tr_surface.c
            code/renderergl2/tr_vbo.c
            code/renderergl2/tr_world.c
            code/sdl/sdl_gamma.c
            code/sdl/sdl_glimp.c
        )
        set_target_properties(renderer_opengl2 PROPERTIES
            PREFIX ""
            OUTPUT_NAME "renderer_opengl2_x86_64"
        )
        target_compile_definitions(renderer_opengl2 PRIVATE USE_RENDERER_DLOPEN)
        target_include_directories(renderer_opengl2 PRIVATE ${ZLIB_INCLUDE_DIRS})
        if(USE_FREETYPE)
            target_include_directories(renderer_opengl2 PRIVATE ${FREETYPE_INCLUDE_DIRS})
        endif()
        target_link_libraries(renderer_opengl2 PRIVATE
            ZLIB::ZLIB
            SDL3::SDL3
            ${OPENGL_LIBRARIES}
            JPEG::JPEG
            PNG::PNG
            assimp::assimp
        )
        if(USE_FREETYPE)
            target_compile_definitions(renderer_opengl2 PRIVATE BUILD_FREETYPE)
            target_link_libraries(renderer_opengl2 PRIVATE Freetype::Freetype)
        endif()
    endif()
endif()

# ============================================================
# MSVC Settings
# ============================================================
if(MSVC)
    # Список всех целей для применения настроек
    foreach(tgt ${CLIENTBIN} ${SERVERBIN} qagame cgame ui renderer_opengl1 renderer_opengl2)
        if(TARGET ${tgt})
            # Включаем строгие предупреждения
            target_compile_options(${tgt} PRIVATE /W4)

            # Настройки для отладочной сборки (Debug)
            target_compile_options(${tgt} PRIVATE
                $<$<CONFIG:Debug>:/Zi>          # Полная отладочная информация
                $<$<CONFIG:Debug>:/fsanitize=address> # Включаем AddressSanitizer
            )

            target_link_options(${tgt} PRIVATE
                $<$<CONFIG:Debug>:/INCREMENTAL:NO> # Отключаем инкрементальную линковку
                $<$<CONFIG:Debug>:/fsanitize=address> # Флаг линковщика для ASan
            )
        endif()
    endforeach()
endif()