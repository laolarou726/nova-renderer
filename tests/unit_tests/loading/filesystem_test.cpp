#include "../../src/general_test_setup.hpp"

#undef TEST
#include <gtest/gtest.h>

TEST(NovaFilesystem, ZipReading) {
    TEST_SETUP_LOGGER();

    auto file_test = nova::renderer::zip_folder_accessor(fs::path{CMAKE_DEFINED_RESOURCES_PREFIX "shaderpacks/DefaultShaderpack.zip"});
    auto files_result = file_test.get_all_items_in_folder({"materials"});
    ASSERT_TRUE(files_result);
    std::vector<fs::path> files = files_result.value;
    for(const auto& file : files) {
        NOVA_LOG(INFO) << file.string();
    }
}