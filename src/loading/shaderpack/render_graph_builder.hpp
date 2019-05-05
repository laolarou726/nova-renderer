#pragma once

#include <string>
#include <unordered_map>

#include "nova_renderer/shaderpack_data.hpp"
#include "nova_renderer/util/attributes.hpp"

namespace nova::renderer {
    struct range {
        uint32_t first_write_pass = ~0U;
        uint32_t last_write_pass = 0;
        uint32_t first_read_pass = ~0U;
        uint32_t last_read_pass = 0;

        NOVA_NODISCARD bool has_writer() const;

        NOVA_NODISCARD bool has_reader() const;

        NOVA_NODISCARD bool is_used() const;

        NOVA_NODISCARD bool can_alias() const;

        NOVA_NODISCARD unsigned last_used_pass() const;

        NOVA_NODISCARD unsigned first_used_pass() const;

        NOVA_NODISCARD bool is_disjoint_with(const range& other) const;
    };

    /*!
     * \brief Orders the provided render passes to satisfy both their implicit and explicit dependencies
     *
     * \param passes A map from pass name to pass of all the passes to order
     * \return The names of the passes in submission order
     */
    result<std::vector<std::string>> order_passes(const std::unordered_map<std::string, render_pass_data>& passes);

    /*!
     * \brief Puts textures in usage order and determines which have overlapping usage ranges
     *
     * Knowing which textures have an overlapping usage range is super important cause if their ranges overlap, they can't be aliased
     *
     * \param passes All the passes in the current frame graph
     * \param resource_used_range A map to hold the usage ranges of each texture
     * \param resources_in_order A vector to hold the textures in usage order
     */
    void determine_usage_order_of_textures(const std::vector<render_pass_data>& passes,
                                           std::unordered_map<std::string, range>& resource_used_range,
                                           std::vector<std::string>& resources_in_order);

    /*!
     * \brief Determines which textures can be aliased to which other textures
     *
     * \param textures All the dynamic textures that this frame graph needs
     * \param resource_used_range The range of passes where each texture is used
     * \param resources_in_order The dynamic textures in usage order
     *
     * \return A map from texture name to the name of the texture the first texture can be aliased with
     */
    std::unordered_map<std::string, std::string> determine_aliasing_of_textures(
        const std::unordered_map<std::string, texture_resource_data>& textures,
        const std::unordered_map<std::string, range>& resource_used_range,
        const std::vector<std::string>& resources_in_order);
} // namespace nova::renderer
