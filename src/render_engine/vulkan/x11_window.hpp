#pragma once

#ifdef NOVA_LINUX

#include <cstdint>

#include <X11/Xlib.h>

#include "nova_renderer/util/attributes.hpp"
#include "nova_renderer/window.hpp"

namespace nova::renderer {
    class x11_window : public iwindow {
    private:
        Window window;
        Display* display;
        bool should_window_close = false;
        Atom wm_protocols;
        Atom wm_delete_window;

    public:
        explicit x11_window(uint32_t width, uint32_t height, const std::string& title);

        x11_window(x11_window&& other) noexcept = delete;
        x11_window& operator=(x11_window&& other) noexcept = delete;

        x11_window(const x11_window& other) = delete;
        x11_window& operator=(const x11_window& other) = delete;

        virtual ~x11_window();

        Window& get_x11_window();
        Display* get_display();

        void on_frame_end() override;
        NOVA_NODISCARD bool should_close() const override;

        NOVA_NODISCARD glm::uvec2 get_window_size() const override;
    };
} // namespace nova::renderer

#endif

// X11 macros that are bad
#ifdef Always
#undef Always
#endif

#ifdef None
#undef None
#endif

#ifdef Bool
#undef Bool
#endif