#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "nova_renderer/util/filesystem.hpp"
#include "nova_renderer/util/result.hpp"
#include "nova_renderer/util/utils.hpp"

namespace nova::renderer {
    class filesystem_exception : public std::exception { // Convert fs::filesystem_error into a nova class
    private:
        const std::string message;
        const std::error_code error_code;

    public:
        explicit filesystem_exception(const fs::filesystem_error& error) : message(error.what()), error_code(error.code()) {}

        [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }

        [[nodiscard]] std::error_code code() const noexcept { return error_code; }
    };

    /*!
     * \brief A collection of resources on the filsysstem
     *
     * "resourcepack" isn't the exact right name here. This isn't strictly a resourcepack in the Minecraft sense - it
     * can be, sure, but it can also be a pure shaderpack. Ths main point is to abstract away loading resources from a
     * folder or a zip file - the calling code shouldn't care how the data is stored on the filesystem
     */
    class folder_accessor_base {
    public:
        /*!
         * \brief Initializes this resourcepack to load resources from the folder/zip file with the provided name
         * \param folder The name of the folder or zip file to load resources from, relative to Nova's working directory
         */
        explicit folder_accessor_base(const fs::path& folder);

        folder_accessor_base(folder_accessor_base&& other) noexcept = default;
        folder_accessor_base& operator=(folder_accessor_base&& other) noexcept = default;

        folder_accessor_base(const folder_accessor_base& other) = delete;
        folder_accessor_base& operator=(const folder_accessor_base& other) = delete;

        virtual ~folder_accessor_base() = default;

        /*!
         * \brief Checks if the given resource exists
         *
         * This function locks resource_existence_mutex, so any methods which are called by this -
         * does_resource_exist_internal and does_resource_exist_in_map - MUST not try to lock resource_existence_mutex
         *
         * \param resource_path The path to the resource you want to know the existence of, relative to this
         * resourcepack's root
         * \return True if the resource exists, false if it does not
         */
        bool does_resource_exist(const fs::path& resource_path);

        /*!
         * \brief Loads the resource with the given path
         * \param resource_path The path to the resource to load, relative to this resourcepack's root
         * \return All the bytes in the loaded resource
         */
        virtual result<std::string> read_text_file(const fs::path& resource_path) = 0;

        /*!
         * \brief Loads the file at the provided path as a series of 32-bit numbers
         *
         * \param resource_path The path to the SPIR-V file to load, relative to this resourcepack's root
         * \return All the 32-bit numbers in the SPIR-V file
         */
        result<std::vector<uint32_t>> read_spirv_file(fs::path& resource_path);

        /*!
         * \brief Retrieves the paths of all the items in the specified folder
         * \param folder The folder to get all items from
         * \return A list of all the paths in the provided folder
         */
        virtual result<std::vector<fs::path>> get_all_items_in_folder(const fs::path& folder) = 0;

        std::shared_ptr<fs::path> get_root() const;

    protected:
        std::shared_ptr<fs::path> root_folder;

        /*!
         * \brief I expect certain resources, like textures, to be requested a lot as Nova streams them in and out of
         * VRAM. This map caches if a resource exists or not - if a path is absent from the map, it's never been
         * requested and we don't know if it exists. However, if a path has been checked before, we can now save an IO
         * call!
         */
        std::unordered_map<std::string, bool> resource_existence;

        std::unique_ptr<std::mutex> resource_existence_mutex;

        std::optional<bool> does_resource_exist_in_map(const std::string& resource_string) const;

        /*!
         * \brief Like the non-internal one, but does not add the folder's root to resource_path
         *
         * \param resource_path The path to the resource, with `our_root` already appended
         */
        virtual bool does_resource_exist_on_filesystem(const fs::path& resource_path) = 0;
    };

    /*!
     * \brief Checks if the given path has the other path as its root
     * \param path The path to check if it has the root
     * \param root The potential root path of the file
     * \return True if `path` has `root` as its root, false otherwise
     */
    bool has_root(const fs::path& path, const fs::path& root);
} // namespace nova::renderer
