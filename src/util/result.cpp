#include "nova_renderer/util/result.hpp"

#include <fmt/format.h>

namespace nova::renderer {

    nova_error::nova_error(std::string message) : message(std::move(message)) {}

    nova_error::nova_error(nova_error&& other) noexcept : message(std::move(other.message)), cause(std::move(other.cause)) {}

    nova_error::nova_error(std::string message, const nova_error& cause) : message(std::move(message)) {
        this->cause = std::make_unique<nova_error>(cause.message);
    }

    std::string nova_error::to_string() const {
        if(cause) {
            return fmt::format(fmt("{:s}\nCaused by: {:s}"), message, cause->to_string());
        } else {
            return message;
        }
    }

    nova_error nova_error::operator+(const nova_error& other) const { return nova_error(message, std::move(other)); }

} // namespace nova::renderer
