#include "shaderpack_loading.hpp"

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include "../../tasks/task_scheduler.hpp"
#include "../folder_accessor.hpp"
#include "../json_utils.hpp"
#include "../loading_utils.hpp"
#include "../regular_folder_accessor.hpp"
#include "../zip_folder_accessor.hpp"
#include "json_interop.hpp"
#include "render_graph_builder.hpp"
#include "shaderpack_validator.hpp"

namespace nova::renderer {
    // Removed from the GLSLang version we're using
    const TBuiltInResource default_built_in_resource = {
        /* .MaxLights = */ 32,
        /* .MaxClipPlanes = */ 6,
        /* .MaxTextureUnits = */ 32,
        /* .MaxTextureCoords = */ 32,
        /* .MaxVertexAttribs = */ 64,
        /* .MaxVertexUniformComponents = */ 4096,
        /* .MaxVaryingFloats = */ 64,
        /* .MaxVertexTextureImageUnits = */ 32,
        /* .MaxCombinedTextureImageUnits = */ 80,
        /* .MaxTextureImageUnits = */ 32,
        /* .MaxFragmentUniformComponents = */ 4096,
        /* .MaxDrawBuffers = */ 32,
        /* .MaxVertexUniformVectors = */ 128,
        /* .MaxVaryingVectors = */ 8,
        /* .MaxFragmentUniformVectors = */ 16,
        /* .MaxVertexOutputVectors = */ 16,
        /* .MaxFragmentInputVectors = */ 15,
        /* .MinProgramTexelOffset = */ -8,
        /* .MaxProgramTexelOffset = */ 7,
        /* .MaxClipDistances = */ 8,
        /* .MaxComputeWorkGroupCountX = */ 65535,
        /* .MaxComputeWorkGroupCountY = */ 65535,
        /* .MaxComputeWorkGroupCountZ = */ 65535,
        /* .MaxComputeWorkGroupSizeX = */ 1024,
        /* .MaxComputeWorkGroupSizeY = */ 1024,
        /* .MaxComputeWorkGroupSizeZ = */ 64,
        /* .MaxComputeUniformComponents = */ 1024,
        /* .MaxComputeTextureImageUnits = */ 16,
        /* .MaxComputeImageUniforms = */ 8,
        /* .MaxComputeAtomicCounters = */ 8,
        /* .MaxComputeAtomicCounterBuffers = */ 1,
        /* .MaxVaryingComponents = */ 60,
        /* .MaxVertexOutputComponents = */ 64,
        /* .MaxGeometryInputComponents = */ 64,
        /* .MaxGeometryOutputComponents = */ 128,
        /* .MaxFragmentInputComponents = */ 128,
        /* .MaxImageUnits = */ 8,
        /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
        /* .MaxCombinedShaderOutputResources = */ 8,
        /* .MaxImageSamples = */ 0,
        /* .MaxVertexImageUniforms = */ 0,
        /* .MaxTessControlImageUniforms = */ 0,
        /* .MaxTessEvaluationImageUniforms = */ 0,
        /* .MaxGeometryImageUniforms = */ 0,
        /* .MaxFragmentImageUniforms = */ 8,
        /* .MaxCombinedImageUniforms = */ 8,
        /* .MaxGeometryTextureImageUnits = */ 16,
        /* .MaxGeometryOutputVertices = */ 256,
        /* .MaxGeometryTotalOutputComponents = */ 1024,
        /* .MaxGeometryUniformComponents = */ 1024,
        /* .MaxGeometryVaryingComponents = */ 64,
        /* .MaxTessControlInputComponents = */ 128,
        /* .MaxTessControlOutputComponents = */ 128,
        /* .MaxTessControlTextureImageUnits = */ 16,
        /* .MaxTessControlUniformComponents = */ 1024,
        /* .MaxTessControlTotalOutputComponents = */ 4096,
        /* .MaxTessEvaluationInputComponents = */ 128,
        /* .MaxTessEvaluationOutputComponents = */ 128,
        /* .MaxTessEvaluationTextureImageUnits = */ 16,
        /* .MaxTessEvaluationUniformComponents = */ 1024,
        /* .MaxTessPatchComponents = */ 120,
        /* .MaxPatchVertices = */ 32,
        /* .MaxTessGenLevel = */ 64,
        /* .MaxViewports = */ 16,
        /* .MaxVertexAtomicCounters = */ 0,
        /* .MaxTessControlAtomicCounters = */ 0,
        /* .MaxTessEvaluationAtomicCounters = */ 0,
        /* .MaxGeometryAtomicCounters = */ 0,
        /* .MaxFragmentAtomicCounters = */ 8,
        /* .MaxCombinedAtomicCounters = */ 8,
        /* .MaxAtomicCounterBindings = */ 1,
        /* .MaxVertexAtomicCounterBuffers = */ 0,
        /* .MaxTessControlAtomicCounterBuffers = */ 0,
        /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
        /* .MaxGeometryAtomicCounterBuffers = */ 0,
        /* .MaxFragmentAtomicCounterBuffers = */ 1,
        /* .MaxCombinedAtomicCounterBuffers = */ 1,
        /* .MaxAtomicCounterBufferSize = */ 16384,
        /* .MaxTransformFeedbackBuffers = */ 4,
        /* .MaxTransformFeedbackInterleavedComponents = */ 64,
        /* .MaxCullDistances = */ 8,
        /* .MaxCombinedClipAndCullDistances = */ 8,
        /* .MaxSamples = */ 4,
        /* .limits = */
        {
            /* .nonInductiveForLoops = */ true,
            /* .whileLoops = */ true,
            /* .doWhileLoops = */ true,
            /* .generalUniformIndexing = */ true,
            /* .generalAttributeMatrixVectorIndexing = */ true,
            /* .generalVaryingIndexing = */ true,
            /* .generalSamplerIndexing = */ true,
            /* .generalVariableIndexing = */ true,
            /* .generalConstantMatrixVectorIndexing = */ true
        }
    };

    result<std::shared_ptr<folder_accessor_base>> get_shaderpack_accessor(const fs::path& shaderpack_name);

    result<shaderpack_resources_data> load_dynamic_resources_file(const std::shared_ptr<folder_accessor_base>& folder_access);

    result<std::vector<render_pass_data>> load_passes_file(const std::shared_ptr<folder_accessor_base>& folder_access);

    result<std::vector<pipeline_data>> load_pipeline_files(const std::shared_ptr<folder_accessor_base>& folder_access);
    result<pipeline_data> load_single_pipeline(const std::shared_ptr<folder_accessor_base>& folder_access, const fs::path& pipeline_path);

    result<std::vector<material_data>> load_material_files(const std::shared_ptr<folder_accessor_base>& folder_access);
    result<material_data> load_single_material(const std::shared_ptr<folder_accessor_base>& folder_access, const fs::path& material_path);

    result<std::vector<uint32_t>> load_shader_file(const fs::path& filename,
                                           const std::shared_ptr<folder_accessor_base>& folder_access,
                                           EShLanguage stage,
                                           const std::vector<std::string>& defines);

    bool loading_failed = false;

    result<shaderpack_data> load_shaderpack_data(const fs::path& shaderpack_name) {
        loading_failed = false;
        auto folder_access_result = get_shaderpack_accessor(shaderpack_name);
        if(!folder_access_result) {
            return folder_access_result.convert<shaderpack_data>();
        }
        const std::shared_ptr<folder_accessor_base> folder_access = folder_access_result.get();

        // The shaderpack has a number of items: There's the shaders themselves, of course, but there's so, so much more
        // What else is there?
        // - resources.json, to describe the dynamic resources that a shaderpack needs
        // - passes.json, to describe the frame graph itself
        // - All the pipeline descriptions
        // - All the material descriptions
        //
        // All these things are loaded from the filesystem

        shaderpack_data data{};
        auto resources_result = load_dynamic_resources_file(folder_access);
        if(!resources_result) {
            return resources_result.convert<shaderpack_data>("Failed to load dynamic resources");
        }
        data.resources = resources_result.value;

        auto passes_result = load_passes_file(folder_access);
        if(!passes_result) {
            return passes_result.convert<shaderpack_data>("Failed to load passes");
        }

        data.passes = passes_result.value;

        auto pipelines_result = load_pipeline_files(folder_access);
        if(!pipelines_result) {
            return pipelines_result.convert<shaderpack_data>(); // Already contains a good error message
        }

        data.pipelines = pipelines_result.value;

        auto materials_result = load_material_files(folder_access);
        if(!materials_result) {
            return materials_result.convert<shaderpack_data>("Failed to load materials");
        }
        data.materials = materials_result.value;

        return data;
    }

    result<std::shared_ptr<folder_accessor_base>> get_shaderpack_accessor(const fs::path& shaderpack_name) {
        fs::path path_to_shaderpack = shaderpack_name;

        // Where is the shaderpack, and what kind of folder is it in?
        if(is_zip_folder(path_to_shaderpack)) {
            // zip folder in shaderpacks folder
            path_to_shaderpack.replace_extension(".zip");
            return result<std::shared_ptr<folder_accessor_base>>(std::make_shared<zip_folder_accessor>(path_to_shaderpack));
        }
        if(fs::exists(path_to_shaderpack)) {
            // regular folder in shaderpacks folder
            return result<std::shared_ptr<folder_accessor_base>>(std::make_shared<regular_folder_accessor>(path_to_shaderpack));
        }

        return MAKE_ERROR("Resource {} not found", shaderpack_name.string());
    }

    result<shaderpack_resources_data> load_dynamic_resources_file(const std::shared_ptr<folder_accessor_base>& folder_access) {
        NOVA_LOG(TRACE) << "load_dynamic_resource_file called";
        auto resources_string_result = folder_access->read_text_file("resources.json");
        if(!resources_string_result) {
            return resources_string_result.convert<shaderpack_resources_data>("Failed to read resources.json");
        }

        std::string resources_string = resources_string_result.value;
        try {
            auto json_resources = nlohmann::json::parse(resources_string.c_str());
            const validation_report report = validate_shaderpack_resources_data(json_resources);
            print(report);
            if(!report.errors.empty()) {
                return result<shaderpack_resources_data>("Validation report contained errors"_err);
            }

            return json_resources.get<shaderpack_resources_data>();
        }
        catch(nlohmann::json::parse_error& err) {
            return MAKE_ERROR("Could not parse your shaderpack's resources.json: {}", err.what());
        }
    }

    result<std::vector<render_pass_data>> load_passes_file(const std::shared_ptr<folder_accessor_base>& folder_access) {
        NOVA_LOG(TRACE) << "load_passes_file called";
        auto passes_bytes_result = folder_access->read_text_file("passes.json");
        if(!passes_bytes_result) {
            return passes_bytes_result.convert<std::vector<render_pass_data>>("Failed to read passes.json");
        }
        const auto passes_bytes = passes_bytes_result.value;
        try {
            auto json_passes = nlohmann::json::parse(passes_bytes);
            auto passes = json_passes.get<std::vector<render_pass_data>>();

            std::unordered_map<std::string, render_pass_data> passes_by_name;
            passes_by_name.reserve(passes.size());
            for(const auto& pass : passes) {
                passes_by_name[pass.name] = pass;
            }

            auto order_pass_names_result = order_passes(passes_by_name);
            if(!order_pass_names_result) {
                return order_pass_names_result.convert<std::vector<render_pass_data>>();
            }
            const auto ordered_pass_names = order_pass_names_result.value;
            passes.clear();
            for(const auto& named_pass : ordered_pass_names) {
                passes.push_back(passes_by_name.at(named_pass));
            }

            return passes;
        }
        catch(nlohmann::json::parse_error& err) {
            return MAKE_ERROR("Could not parse shaderpack's passes.json: {}", err.what());
        }
    }

    result<std::vector<pipeline_data>> load_pipeline_files(const std::shared_ptr<folder_accessor_base>& folder_access) {
        NOVA_LOG(TRACE) << "load_pipeline_files called";
        result<std::vector<fs::path>> potential_pipeline_files_result = folder_access->get_all_items_in_folder("materials");
        if(!potential_pipeline_files_result) {
            return potential_pipeline_files_result.convert<std::vector<pipeline_data>>("Materials folder does not exist");
        }
        std::vector<fs::path> potential_pipeline_files = potential_pipeline_files_result.value;

        std::vector<pipeline_data> output;

        // The resize will make this vector about twice as big as it should be, but there won't be any reallocating
        // so I'm into it
        output.reserve(potential_pipeline_files.size());

        for(const fs::path& potential_file : potential_pipeline_files) {
            if(potential_file.extension() == ".pipeline") {
                // Pipeline file!
                auto pipeline_load_result = load_single_pipeline(folder_access, potential_file);
                if(!pipeline_load_result) {
                    return pipeline_load_result.convert<std::vector<pipeline_data>>("Failed to load pipelines");
                }

                const pipeline_data& pipeline = pipeline_load_result.value;
                output.push_back(pipeline);
            }
        }

        return output;
    }

    result<pipeline_data> load_single_pipeline(const std::shared_ptr<folder_accessor_base>& folder_access, const fs::path& pipeline_path) {
        NOVA_LOG(TRACE) << "Task to load pipeline " << pipeline_path << " started";

        auto pipeline_bytes_result = folder_access->read_text_file(pipeline_path);
        if(!pipeline_bytes_result) {
            return pipeline_bytes_result.convert<pipeline_data>(FORMAT("Failed to read {:s}", pipeline_path.string()));
        }
        const auto pipeline_bytes = pipeline_bytes_result.value;

        auto json_pipeline = nlohmann::json::parse(pipeline_bytes);
        NOVA_LOG(TRACE) << "Parsed JSON from disk for pipeline " << pipeline_path;
        const validation_report report = validate_graphics_pipeline(json_pipeline);
        NOVA_LOG(TRACE) << "Finished validating JSON for pipeline " << pipeline_path;
        print(report);
        if(!report.errors.empty()) {
            loading_failed = true;
            NOVA_LOG(TRACE) << "Loading pipeline file " << pipeline_path << " failed";
            return result<pipeline_data>("Validation report contained errors"_err);
        }

        auto new_pipeline = json_pipeline.get<pipeline_data>();
        NOVA_LOG(TRACE) << "Parsed JSON into pipeline_data for pipeline " << pipeline_path;

        {
            auto load_result = load_shader_file(new_pipeline.vertex_shader.filename,
                                                folder_access,
                                                EShLangVertex,
                                                new_pipeline.defines);

            if(!load_result) {
                return load_result.convert<pipeline_data>(FORMAT("Failed to load pipeline {:s}", pipeline_path.string()));
            }

            new_pipeline.vertex_shader.source = load_result.value;
        }

        if(new_pipeline.geometry_shader) {
            auto load_result = load_shader_file((*new_pipeline.geometry_shader).filename,
                                                folder_access,
                                                EShLangGeometry,
                                                new_pipeline.defines);

            if(!load_result) {
                return load_result.convert<pipeline_data>(FORMAT("Failed to load pipeline {:s}", pipeline_path.string()));
            }

            (*new_pipeline.geometry_shader).source = load_result.value;
        }

        if(new_pipeline.tessellation_control_shader) {
            auto load_result = load_shader_file((*new_pipeline.tessellation_control_shader).filename,
                                                folder_access,
                                                EShLangTessControl,
                                                new_pipeline.defines);

            if(!load_result) {
                return load_result.convert<pipeline_data>(FORMAT("Failed to load pipeline {:s}", pipeline_path.string()));
            }

            (*new_pipeline.tessellation_control_shader).source = load_result.value;
        }
        if(new_pipeline.tessellation_evaluation_shader) {
            auto load_result = load_shader_file((*new_pipeline.tessellation_evaluation_shader).filename,
                                                folder_access,
                                                EShLangTessEvaluation,
                                                new_pipeline.defines);

            if(!load_result) {
                return load_result.convert<pipeline_data>(FORMAT("Failed to load pipeline {:s}", pipeline_path.string()));
            }

            (*new_pipeline.tessellation_evaluation_shader)
                .source = load_result.value;
        }

        if(new_pipeline.fragment_shader) {
            auto load_result = load_shader_file((*new_pipeline.fragment_shader).filename,
                                                folder_access,
                                                EShLangFragment,
                                                new_pipeline.defines);

            if(!load_result) {
                return load_result.convert<pipeline_data>(FORMAT("Failed to load pipeline {:s}", pipeline_path.string()));
            }

            (*new_pipeline.fragment_shader).source = load_result.value;
        }

        NOVA_LOG(TRACE) << "Load of pipeline " << pipeline_path << " succeeded";

        return new_pipeline;
    }

    result<std::vector<uint32_t>> load_shader_file(const fs::path& filename,
                                           const std::shared_ptr<folder_accessor_base>& folder_access,
                                           const EShLanguage stage,
                                           const std::vector<std::string>& defines) {
        static std::unordered_map<EShLanguage, std::vector<fs::path>> extensions_by_shader_stage = {{EShLangVertex,
                                                                                                     {
                                                                                                         ".vert.spirv",
                                                                                                         ".vsh.spirv",
                                                                                                         ".vertex.spirv",

                                                                                                         ".vert",
                                                                                                         ".vsh",

                                                                                                         ".vertex",

                                                                                                         ".vert.hlsl",
                                                                                                         ".vsh.hlsl",
                                                                                                         ".vertex.hlsl",
                                                                                                     }},
                                                                                                    {EShLangFragment,
                                                                                                     {
                                                                                                         ".frag.spirv",
                                                                                                         ".fsh.spirv",
                                                                                                         ".fragment.spirv",

                                                                                                         ".frag",
                                                                                                         ".fsh",

                                                                                                         ".fragment",

                                                                                                         ".frag.hlsl",
                                                                                                         ".fsh.hlsl",
                                                                                                         ".fragment.hlsl",
                                                                                                     }},
                                                                                                    {EShLangGeometry,
                                                                                                     {
                                                                                                         ".geom.spirv",
                                                                                                         ".geo.spirv",
                                                                                                         ".geometry.spirv",

                                                                                                         ".geom",
                                                                                                         ".geo",

                                                                                                         ".geometry",

                                                                                                         ".geom.hlsl",
                                                                                                         ".geo.hlsl",
                                                                                                         ".geometry.hlsl",
                                                                                                     }},
                                                                                                    {EShLangTessEvaluation,
                                                                                                     {
                                                                                                         ".tese.spirv",
                                                                                                         ".tse.spirv",
                                                                                                         ".tess_eval.spirv",

                                                                                                         ".tese",
                                                                                                         ".tse",

                                                                                                         ".tess_eval",

                                                                                                         ".tese.hlsl",
                                                                                                         ".tse.hlsl",
                                                                                                         ".tess_eval.hlsl",
                                                                                                     }},
                                                                                                    {EShLangTessControl,
                                                                                                     {
                                                                                                         ".tesc.spirv",
                                                                                                         ".tsc.spirv",
                                                                                                         ".tess_control.spirv",

                                                                                                         ".tesc",
                                                                                                         ".tsc",

                                                                                                         ".tess_control",

                                                                                                         ".tesc.hlsl",
                                                                                                         ".tsc.hlsl",
                                                                                                         ".tess_control.hlsl",
                                                                                                     }}};

        std::vector<fs::path> extensions_for_current_stage = extensions_by_shader_stage.at(stage);

        for(const fs::path& extension : extensions_for_current_stage) {
            fs::path full_filename = filename;
            full_filename.replace_extension(extension);

            if(!folder_access->does_resource_exist(full_filename)) {
                continue;
            }

            glslang::TShader shader(stage);

            // Check the extension to know what kind of shader file the user has provided. SPIR-V files can be loaded
            // as-is, but GLSL, GLSL ES, and HLSL files need to be transpiled to SPIR-V
            if(extension.string().find(".spirv") != std::string::npos) {
                // SPIR-V file!
                // TODO: figure out how to handle defines with SPIRV
                return result(folder_access->read_spirv_file(full_filename));
            }
            if(extension.string().find(".hlsl") != std::string::npos) {
                shader.setEnvInput(glslang::EShSourceHlsl, stage, glslang::EShClientVulkan, 0);
            } else {
                // GLSL files have a lot of possible extensions, but SPIR-V and HLSL don't!
                shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 0);
            }

            auto shader_source_result = folder_access->read_text_file(full_filename);
            if(!shader_source_result) {
                return shader_source_result.convert<std::vector<uint32_t>>(FORMAT("Failed to read {:s}", full_filename.string()));
            }

            std::string shader_source = shader_source_result.value;
            std::string::size_type version_pos = shader_source.find("#version");
            std::string::size_type inject_pos = 0;
            if(version_pos != std::string::npos) {
                std::string::size_type break_after_version_pos = shader_source.find('\n', version_pos);
                if(break_after_version_pos != std::string::npos) {
                    inject_pos = break_after_version_pos + 1;
                }
            }
            for(auto i = defines.crbegin(); i != defines.crend(); ++i) {
                shader_source.insert(inject_pos, "#define " + *i + "\n");
            }

            auto* shader_source_data = shader_source.data();
            shader.setStrings(&shader_source_data, 1);
            const bool shader_compiled = shader.parse(&default_built_in_resource,
                                                      450,
                                                      ECoreProfile,
                                                      false,
                                                      false,
                                                      EShMessages(EShMsgVulkanRules | EShMsgSpvRules));

            const char* info_log = shader.getInfoLog();
            if(std::strlen(info_log) > 0) {
                const char* info_debug_log = shader.getInfoDebugLog();
                NOVA_LOG(INFO) << full_filename.string() << " compilation messages:\n" << info_log << "\n" << info_debug_log;
            }

            if(!shader_compiled) {
                return result<std::vector<uint32_t>>(nova_error(std::string(info_log)));
            }

            glslang::TProgram program;
            program.addShader(&shader);
            const bool shader_linked = program.link(EShMsgDefault);
            if(!shader_linked) {
                const char* program_info_log = program.getInfoLog();
                const char* program_debug_info_log = program.getInfoDebugLog();
                NOVA_LOG(ERROR) << "Program failed to link: " << program_info_log << "\n" << program_debug_info_log;
            }

            std::vector<uint32_t> spirv;
            GlslangToSpv(*program.getIntermediate(stage), spirv);

            fs::path dump_filename = filename.filename();
            dump_filename.replace_extension(std::to_string(stage) + ".spirv.generated");
            write_to_file(spirv, dump_filename);

            return spirv;
        }

        return MAKE_ERROR("Could not find shader {}", filename.string());
    }

    result<std::vector<material_data>> load_material_files(const std::shared_ptr<folder_accessor_base>& folder_access) {
        result<std::vector<fs::path>> potential_material_files_result = folder_access->get_all_items_in_folder("materials");
        if(!potential_material_files_result) {
            return potential_material_files_result.convert<std::vector<material_data>>("Materials folder does not exist");
        }
        std::vector<fs::path> potential_material_files = potential_material_files_result.value;

        // The resize will make this vector about twice as big as it should be, but there won't be any reallocating
        // so I'm into it
        std::vector<material_data> output;
        output.reserve(potential_material_files.size());

        for(const fs::path& potential_file : potential_material_files) {
            if(potential_file.extension() == ".mat") {
                auto material_load_result = load_single_material(folder_access, potential_file);
                if(!material_load_result) {
                    return material_load_result.convert<std::vector<material_data>>();
                }

                const material_data& material = material_load_result.value;
                output.push_back(material);
            }
        }

        return output;
    }

    result<material_data> load_single_material(const std::shared_ptr<folder_accessor_base>& folder_access, const fs::path& material_path) {
        auto material_text_result = folder_access->read_text_file(material_path);
        if(!material_text_result) {
            return material_text_result.convert<material_data>(FORMAT("Failed to read {:s}", material_path.string()));
        }
        const std::string material_text = material_text_result.value;

        auto json_material = nlohmann::json::parse(material_text);
        const auto report = validate_material(json_material);
        print(report);
        if(!report.errors.empty()) {
            // There were errors, this material can't be loaded
            loading_failed = true;
            NOVA_LOG(TRACE) << "Load of material " << material_path << " failed";
            return MAKE_ERROR("Load of material {:s} failed, validation report contained errors", material_path.string());
        }

        auto material = json_material.get<material_data>();
        material.name = material_path.stem().string();
        NOVA_LOG(TRACE) << "Load of material " << material_path << " succeeded";
        return material;
    }
} // namespace nova::renderer
