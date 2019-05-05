#pragma once

#include <miniz/miniz_zip.h>

#include "nova_renderer/util/attributes.hpp"

#include "folder_accessor.hpp"

namespace nova::renderer {
    struct file_tree_node {
        std::string name;
        std::vector<std::unique_ptr<file_tree_node>> children;
        file_tree_node* parent = nullptr;

        NOVA_NODISCARD std::string get_full_path() const;
    };

    /*!
     * \brief Allows access to a zip folder
     */
    class zip_folder_accessor : public folder_accessor_base {
    public:
        explicit zip_folder_accessor(const fs::path& folder);

        zip_folder_accessor(zip_folder_accessor&& other) noexcept = default;
        zip_folder_accessor& operator=(zip_folder_accessor&& other) noexcept = default;

        zip_folder_accessor(const zip_folder_accessor& other) = delete;
        zip_folder_accessor& operator=(const zip_folder_accessor& other) = delete;

        ~zip_folder_accessor() override;

        result<std::string> read_text_file(const fs::path& resource_path) override;

        result<std::vector<fs::path>> get_all_items_in_folder(const fs::path& folder) override;

    private:
        /*!
         * \brief Map from filename to its index in the zip folder. Miniz seems to like indexes
         */
        std::unordered_map<std::string, uint32_t> resource_indexes;

        mz_zip_archive zip_archive = {};

        std::unique_ptr<file_tree_node> files = nullptr;

        bool ok = false;

        void delete_file_tree(std::unique_ptr<file_tree_node>& node);

        void build_file_tree();

        bool does_resource_exist_on_filesystem(const fs::path& resource_path) override;
    };

    /*!
     * \brief Prints out the nodes in a depth-first fashion
     */
    void print_file_tree(const std::unique_ptr<file_tree_node>& folder, uint32_t depth);
} // namespace nova::renderer
