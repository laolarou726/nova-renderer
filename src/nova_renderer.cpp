#include "nova_renderer/nova_renderer.hpp"

#include <array>
#include <future>

#include <glslang/MachineIndependent/Initialize.h>
#include <minitrace/minitrace.h>

#include "loading/shaderpack/shaderpack_loading.hpp"
#if defined(NOVA_WINDOWS)
#include "render_engine/dx12/dx12_render_engine.hpp"
#endif
#include "debugging/renderdoc.hpp"
#include "render_engine/vulkan/vulkan_render_engine.hpp"
#include "util/logger.hpp"

namespace nova::renderer {
    std::unique_ptr<nova_renderer> nova_renderer::instance;

    nova_renderer::nova_renderer(nova_settings settings) : render_settings(settings) {

        mtr_init("trace.json");

        MTR_META_PROCESS_NAME("NovaRenderer");
        MTR_META_THREAD_NAME("Main");

        MTR_SCOPE("Init", "nova_renderer::nova_renderer");

        if(settings.debug.renderdoc.enabled) {
            MTR_SCOPE("Init", "LoadRenderdoc");
            auto rd_load_result = load_renderdoc(settings.debug.renderdoc.renderdoc_dll_path);

            rd_load_result
                .map([&](RENDERDOC_API_1_3_0* api) {
                    render_doc = api;

                    render_doc->SetCaptureFilePathTemplate(settings.debug.renderdoc.capture_path.c_str());

                    RENDERDOC_InputButton capture_key[] = {eRENDERDOC_Key_F12, eRENDERDOC_Key_PrtScrn};
                    render_doc->SetCaptureKeys(capture_key, 2);

                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_AllowFullscreen, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_AllowVSync, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_VerifyMapWrites, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_SaveAllInitials, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_APIValidation, 1U);

                    NOVA_LOG(INFO) << "Loaded RenderDoc successfully";

                    return 0;
                })
                .on_error([](const nova_error& error) { NOVA_LOG(ERROR) << error.to_string(); });
        }

        switch(settings.api) {
            case graphics_api::dx12:
#if defined(NOVA_WINDOWS)
            {
                MTR_SCOPE("Init", "InitDirectX12RenderEngine");
                engine = std::make_unique<dx12_render_engine>(render_settings);
            } break;
#else
                NOVA_LOG(WARN) << "You selected the DX12 graphics API, but your system doesn't support it. Defaulting to Vulkan";
                [[fallthrough]];
#endif
            case graphics_api::vulkan:
                MTR_SCOPE("Init", "InitVulkanRenderEngine");
                engine = std::make_unique<vulkan_render_engine>(render_settings, render_doc);
        }
    }

    nova_renderer::~nova_renderer() { mtr_shutdown(); }

    nova_settings& nova_renderer::get_settings() { return render_settings; }

    void nova_renderer::execute_frame() const {
        MTR_SCOPE("RenderLoop", "execute_frame");
        engine->render_frame();

        mtr_flush();
    }

    result<void> nova_renderer::load_shaderpack(const std::string& shaderpack_name) const {
        MTR_SCOPE("ShaderpackLoading", "load_shaderpack");
        if(!glslang::InitializeProcess()) {
            return "Failed to initialize GLSLang process!"_err;
        }

        auto shaderpack_load_result = load_shaderpack_data(fs::path(shaderpack_name));
        if(!shaderpack_load_result) {
            NOVA_LOG(ERROR) << "Failed to load shaderpack: " << shaderpack_load_result.error.to_string();
            return shaderpack_load_result.convert<void>("Failed to load shaderpack");
        }

        engine->set_shaderpack(shaderpack_load_result.value);
        NOVA_LOG(INFO) << "Shaderpack " << shaderpack_name << " loaded successfully";
        return result<void>();
    }

    render_engine* nova_renderer::get_engine() const { return engine.get(); }

    nova_renderer* nova_renderer::get_instance() { return instance.get(); }

    nova_renderer* nova_renderer::initialize(const nova_settings& settings) {
        return (instance = std::make_unique<nova_renderer>(settings)).get();
    }

    void nova_renderer::deinitialize() { instance.reset(); }
} // namespace nova::renderer
