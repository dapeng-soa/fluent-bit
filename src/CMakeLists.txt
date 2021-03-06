add_definitions(-DFLB_CORE=1)

# Core Source
set(src
  ${src}
  flb_mp.c
  flb_api.c
  flb_lib.c
  flb_log.c
  flb_env.c
  flb_uri.c
  flb_hash.c
  flb_pack.c
  flb_sds.c

  flb_sha1.c
  flb_pipe.c
  flb_meta.c
  flb_kernel.c
  flb_input.c
  flb_filter.c
  flb_output.c
  flb_config.c
  flb_network.c
  flb_utils.c
  flb_engine.c
  flb_engine_dispatch.c
  flb_task.c
  flb_scheduler.c
  flb_io.c
  flb_upstream.c
  flb_router.c
  flb_http_client.c
  flb_worker.c
  flb_time.c
  flb_sosreport.c
  )

include_directories(
  .
  ../lib/
  ../lib/jsmn
  ../lib/msgpack-c-0b7cabd/include
  ../lib/monkey/include/
  ${extra_headers}
  )

if(FLB_REGEX)
  set(src
    ${src}
    flb_parser.c
    flb_parser_regex.c
    flb_parser_json.c
    flb_parser_decoder.c
    )
endif()

# Fluent Bit have TLS support
if(FLB_TLS)
  # Register the TLS interface and functions
  set(src
    ${src}
    "flb_io_tls.c"
    )

  # Make sure our output targets links to the TLS library
  set(extra_libs
    ${extra_libs}
    "mbedtls"
    )
endif()

if(FLB_PROXY_GO)
  set(src
    ${src}
    "flb_plugin_proxy.c"
    )
endif()

if(FLB_STATS)
  set(src
    ${src}
    "flb_stats.c"
    )
endif()

if(FLB_METRICS)
  set(src
    ${src}
    "flb_metrics.c"
    "flb_metrics_exporter.c"
    )
endif()

if(FLB_BUFFERING)
  set(src
    ${src}
    "flb_buffer.c"
    "flb_buffer_chunk.c"
    "flb_buffer_qchunk.c"
    )
endif()

# Link to libco
set(extra_libs
  ${extra_libs}
  "co")


if(FLB_JEMALLOC)
  set(extra_libs
    ${extra_libs}
    "libjemalloc")
endif()

if(FLB_REGEX)
  set(extra_libs
    ${extra_libs}
    "libonigmo")
  set(src
    ${src}
    "flb_regex.c"
    )
endif()

if(FLB_SQLDB)
  set(src
    ${src}
    "flb_sqldb.c"
    )
  set(extra_libs
    ${extra_libs}
    "sqlite3")
endif()

include(CheckSymbolExists)
check_symbol_exists(accept4 "sys/socket.h" HAVE_ACCEPT4)

# Core dependencies
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(FLB_DEPS
    "ws2_32.lib"
    )
else()
  set(FLB_DEPS
    ${FLB_DEPS}
    ${CMAKE_DL_LIBS}
    m
    )
endif()

# Link timer library
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(FLB_DEPS
    ${FLB_DEPS}
    rt
    )
endif()

# HTTP Server
if(FLB_HTTP_SERVER)
  add_subdirectory(http_server)
endif()

# Proxy interfaces
add_subdirectory(proxy)

set(FLB_PROXY_PLUGINS "")
if(FLB_PROXY_GO)
  set(FLB_PROXY_PLUGINS ${FLB_PROXY_PLUGINS} flb-plugin-proxy-go)
endif()

# HTTP Server
if(FLB_HTTP_SERVER)
  set(FLB_DEPS
    ${FLB_DEPS}
    flb-http-server)
endif()


# Set static dependencies
set(FLB_DEPS
  ${FLB_DEPS}
  mk_core
  jsmn
  msgpackc-static
  ${FLB_PLUGINS}
  ${FLB_PROXY_PLUGINS}
  ${extra_libs}
  )

# Shared Library
if(NOT FLB_WITHOUT_SHARED_LIB)
  add_library(fluent-bit-shared SHARED ${src})
  target_link_libraries(fluent-bit-shared ${FLB_DEPS} -lpthread)
  set_target_properties(fluent-bit-shared
    PROPERTIES OUTPUT_NAME fluent-bit)

  # Library install routines
  install(TARGETS fluent-bit-shared LIBRARY DESTINATION lib)
endif()

# Static Library
add_library(fluent-bit-static STATIC ${src})
target_link_libraries(fluent-bit-static ${FLB_DEPS})

if(MSVC)
  # Rename the output for Windows environment to avoid naming issues
  set_target_properties(fluent-bit-static PROPERTIES OUTPUT_NAME libfluent-bit)
else()
  set_target_properties(fluent-bit-static PROPERTIES OUTPUT_NAME fluent-bit)
endif(MSVC)

if(FLB_JEMALLOC)
  target_link_libraries(fluent-bit-static libjemalloc)
endif()

if(FLB_BUFFERING)
  target_link_libraries(fluent-bit-static sha1)
endif()

# Binary / Executable
if(NOT FLB_WITHOUT_BIN)
  find_package (Threads)
  add_executable(fluent-bit-bin fluent-bit.c)

  if(FLB_JEMALLOC)
    target_link_libraries(fluent-bit-bin libjemalloc)
  endif()

  if(FLB_BACKTRACE)
    target_link_libraries(fluent-bit-bin libbacktrace)
  endif()

  target_link_libraries(fluent-bit-bin fluent-bit-static ${CMAKE_THREAD_LIBS_INIT})

  set_target_properties(fluent-bit-bin
    PROPERTIES
    OUTPUT_NAME ${FLB_OUT_NAME}
    ENABLE_EXPORTS ON)
  install(TARGETS fluent-bit-bin RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

  # Detect init system, install systemd or init.d script
  if(IS_DIRECTORY /lib/systemd/system)
    set(FLB_SYSTEMD_SCRIPT "${PROJECT_SOURCE_DIR}/init/${FLB_OUT_NAME}.service")
    configure_file(
      "${PROJECT_SOURCE_DIR}/init/systemd.in"
      ${FLB_SYSTEMD_SCRIPT}
      )
    install(FILES ${FLB_SYSTEMD_SCRIPT} DESTINATION /lib/systemd/system)
    install(DIRECTORY DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${FLB_OUT_NAME}/")
  else()
    # FIXME: should we support Sysv init script ?
  endif()

  install(FILES
    "${PROJECT_SOURCE_DIR}/conf/fluent-bit.conf"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${FLB_OUT_NAME}/"
    RENAME "${FLB_OUT_NAME}.conf")

  install(FILES
    "${PROJECT_SOURCE_DIR}/conf/parsers.conf"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/${FLB_OUT_NAME}/")

endif()
