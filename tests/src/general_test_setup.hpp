//
// Created by jannis on 07.09.18.
//

#ifndef NOVA_RENDERER_GENERAL_TEST_SETUP_HPP
#define NOVA_RENDERER_GENERAL_TEST_SETUP_HPP

#include <fstream>
#include <iostream>

#include "../../src/loading/zip_folder_accessor.hpp"
#include "../../src/nova_renderer.hpp"
#include "../../src/platform.hpp"
#include "../../src/settings/nova_settings.hpp"
#ifdef _WIN32
#include <direct.h>
#include "../../src/render_engine/dx12/dx12_render_engine.hpp"
#define getcwd _getcwd
#else
#include <unistd.h>
#include "../../src/render_engine/vulkan/vulkan_render_engine.hpp"
#endif

#ifndef CMAKE_DEFINED_RESOURCES_PREFIX
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CMAKE_DEFINED_RESOURCES_PREFIX ""
#endif

#include "../../src/util/logger.hpp"

#ifndef TEST_SETUP_LOGGER // Tests are weird... this is done to avoid some linking errors
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TEST_SETUP_LOGGER [] {                                                                                                                                   \
        auto error_log = std::make_shared<std::ofstream>();                                                         \
        error_log->open("test_error_log.log");                                                                      \
        auto test_log = std::make_shared<std::ofstream>("E:\\Documents\\Nova\\nova-renderer\\tests\\resources\\test_log.log");                                            \
        auto& log = nova::renderer::logger::instance;                                                               \
        log.add_log_handler(nova::renderer::TRACE, [test_log](auto msg) {                                           \
            std::cout << "TRACE: " << msg << std::endl;                                                             \
            *test_log << "TRACE: " << msg << std::endl;                                                             \
        });                                                                                                         \
        log.add_log_handler(nova::renderer::DEBUG, [test_log](auto msg) {                                           \
            std::cout << "DEBUG: " << msg << std::endl;                                                             \
            *test_log << "DEBUG: " << msg << std::endl;                                                             \
        });                                                                                                         \
        log.add_log_handler(nova::renderer::INFO, [test_log](auto msg) {                                            \
            std::cout << "INFO: " << msg << std::endl;                                                              \
            *test_log << "INFO: " << msg << std::endl;                                                              \
        });                                                                                                         \
        log.add_log_handler(nova::renderer::WARN, [test_log](auto msg) {                                            \
            std::cerr << "WARN: " << msg << std::endl;                                                              \
            *test_log << "WARN: " << msg << std::endl;                                                              \
        });                                                                                                         \
        log.add_log_handler(nova::renderer::ERROR, [test_log, error_log](auto msg) {                                \
            std::cerr << "ERROR: " << msg << std::endl;                                                             \
            *error_log << "ERROR: " << msg << std::endl << std::flush;                                              \
            *test_log << "ERROR: " << msg << std::endl;                                                             \
        });                                                                                                         \
        log.add_log_handler(nova::renderer::FATAL, [test_log, error_log](auto msg) {                                \
            std::cerr << "FATAL: " << msg << std::endl;                                                             \
            *error_log << "FATAL: " << msg << std::endl << std::flush;                                              \
            *test_log << "FATAL: " << msg << std::endl;                                                             \
        });                                                                                                         \
        log.add_log_handler(nova::renderer::MAX_LEVEL, [test_log, error_log](auto msg) {                            \
            std::cerr << "MAX_LEVEL: " << msg << std::endl;                                                         \
            *error_log << "MAX_LEVEL: " << msg << std::endl << std::flush;                                          \
            *test_log << "MAX_LEVEL: " << msg << std::endl;                                                         \
        });                                                                                                         \
        }
#endif

#endif // NOVA_RENDERER_GENERAL_TEST_SETUP_HPP
