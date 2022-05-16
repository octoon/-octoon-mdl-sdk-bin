/******************************************************************************
 * Copyright 2022 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/

 // examples/mdl_sdk/code_gen/example_code_gen.cpp
 //
 // Loads an MDL material and processes it up to code generation and beyond.

#include <iostream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include "example_shared.h"

using TD = mi::neuraylib::Target_function_description;  // only for readability

/// The options parsed from the command-line.
class Options
{
public:
    /// Prints the usage information to the stream \p s.
    static void print_usage(std::ostream& s);

    /// Parses the command-line given by \p argc and \p argv.
    ///
    /// \return   \c true in case of success and \c false otherwise.
    bool parse(int argc, char* argv[]);

    /// General options.
    bool m_help = false;
    bool m_nostdpath = false;
    std::vector<std::string> m_mdl_paths;
    std::string m_output_file;

    /// Code compilation and generation options
    bool m_use_class_compilation = true;
    bool m_fold_ternary_on_df = false;
    bool m_fold_all_bool_parameters = false;
    bool m_fold_all_enum_parameters = false;
    bool m_ignore_noinline = true;
    std::string m_backend = "hlsl";
    bool m_use_derivatives = false;
    std::string m_num_texture_results = "16";

    /// MDL qualified material name to generate code for.
    std::string m_qualified_material_name = "::nvidia::sdk_examples::tutorials::example_material";

    /// The expressions to generate code for.
    std::vector<TD> m_descs;
    std::vector<std::unique_ptr<std::string> > m_desc_strs;  // collection for storing the strings

#ifndef MDL_SOURCE_RELEASE
    /// The target in case distilling is done before code generation
    std::string m_distilling_target = "";
#endif
};

void dump_dag(
    mi::neuraylib::ITransaction *transaction,
    mi::neuraylib::IMdl_factory *mdl_factory,
    const mi::neuraylib::ICompiled_material *cm,
    std::ostream &s)
{
    mi::base::Handle<mi::neuraylib::IType_factory> tf(
        mdl_factory->create_type_factory(transaction));
    mi::base::Handle<mi::neuraylib::IValue_factory> vf(
        mdl_factory->create_value_factory(transaction));
    mi::base::Handle<mi::neuraylib::IExpression_factory> ef(
        mdl_factory->create_expression_factory(transaction));

    for (mi::Size i = 0; i < cm->get_parameter_count(); ++i) {
        mi::base::Handle<const mi::neuraylib::IValue> argument(cm->get_argument(i));
        std::stringstream name;
        name << i;
        mi::base::Handle<const mi::IString> result(
            vf->dump(argument.get(), name.str().c_str(), 1));
        const char *pn = cm->get_parameter_name(i);
        if (!pn)
            pn = "n/a";
        s << "    argument (original parameter name: " << pn << ") "
            << result->get_c_str() << std::endl;
    }

    for (mi::Uint32 i = 0; i < cm->get_temporary_count(); ++i) {
        mi::base::Handle<const mi::neuraylib::IExpression> temporary(cm->get_temporary(i));
        std::stringstream name;
        name << i;
        mi::base::Handle<const mi::IString> result(
            ef->dump(temporary.get(), name.str().c_str(), 1));
        s << "    temporary " << result->get_c_str() << std::endl;
    }

    mi::base::Handle<const mi::neuraylib::IExpression_direct_call> body(cm->get_body());
    mi::base::Handle<const mi::IString> result(
        ef->dump(body.get(), 0, 1));
    s << "    body " << result->get_c_str() << std::endl;
}

/// The main content of the example
void code_gen(mi::neuraylib::INeuray* neuray, Options& options)
{
    // Access the database and create a transaction.
    // This is required for loading MDL modules and their dependencies.
    // All loaded elements are stored in the DB as soon as the transaction is
    // committed. Until then, changes are only visible to the current open transaction.
    mi::base::Handle<mi::neuraylib::IDatabase> database(
        neuray->get_api_component<mi::neuraylib::IDatabase>());
    mi::base::Handle<mi::neuraylib::IScope> scope(database->get_global_scope());
    mi::base::Handle<mi::neuraylib::ITransaction> trans(scope->create_transaction());
    {
        mi::base::Handle<mi::neuraylib::IMdl_impexp_api> mdl_impexp_api(
            neuray->get_api_component<mi::neuraylib::IMdl_impexp_api>());

        mi::base::Handle<mi::neuraylib::IMdl_factory> mdl_factory(
            neuray->get_api_component<mi::neuraylib::IMdl_factory>());

        // The context is used to pass options to different components and operations.
        // It also carries errors, warnings, and warnings produces by the operations.
        mi::base::Handle < mi::neuraylib::IMdl_execution_context> context(
            mdl_factory->create_execution_context());

        // Split the material name passed on the command line into a module
        // and (unqualified) material name
        // The expected input is fully-qualified absolute MDL material name of the form:
        // [::<package>]::<module>::<material>
        std::string module_name, material_name;
        if (!mi::examples::mdl::parse_cmd_argument_material_name(
            options.m_qualified_material_name, module_name, material_name))
                exit_failure("Failed to parse the qualified material name: %s",
                    options.m_qualified_material_name.c_str());

        // ----------------------------------------------------------------------------------------

        // Load the selected module.
        mdl_impexp_api->load_module(trans.get(), module_name.c_str(), context.get());
        if (!print_messages(context.get()))
            exit_failure("Failed to load the selected module.");

        // Access the module by name, which has to be queried using the factory.
        mi::base::Handle<const mi::IString> module_db_name(
            mdl_factory->get_db_module_name(module_name.c_str()));

        mi::base::Handle<const mi::neuraylib::IModule> module(
            trans->access<mi::neuraylib::IModule>(module_db_name->get_c_str()));
        if (!module)
            exit_failure("Failed to access the loaded module.");
        
        // ----------------------------------------------------------------------------------------

        // Access the material definition of the selected material.
        // A module can export multiple materials or none at all.
        std::string material_db_name
            = std::string(module_db_name->get_c_str()) + "::" + material_name;
        material_db_name = mi::examples::mdl::add_missing_material_signature(
            module.get(), material_db_name);
        if (material_db_name.empty())
            exit_failure("Failed to find the material %s in the module %s.",
                material_name.c_str(), module_name.c_str());

        // Check if there is such a definition.
        // This is not really required here because the definition wrapper in the next step
        // will also check if the definition is valid.
        mi::base::Handle<const mi::neuraylib::IFunction_definition> material_definition(
            trans->access<mi::neuraylib::IFunction_definition>(material_db_name.c_str()));
        if (!material_definition)
            exit_failure("Failed to access the material definition.");

        // ----------------------------------------------------------------------------------------

        // Create an instance of the material.
        // An easy way to do that is by using the definition wrapper which makes sure that there
        // are valid parameters set even when there are no default values defined in MDL source.
        mi::neuraylib::Definition_wrapper dw(
            trans.get(), material_db_name.c_str(), mdl_factory.get());
        if (!dw.is_valid())
            exit_failure("Failed to access the material definition.");

        mi::Sint32 result = -1;
        mi::base::Handle<mi::neuraylib::IFunction_call> material_instance(
            dw.create_instance<mi::neuraylib::IFunction_call>(nullptr, &result));
        if (result != 0)
            exit_failure("Failed to create a material instance of: %s::%s (%d)",
                module_name.c_str(), material_name.c_str(), result);

        // Alternatively, the instance can be created without the definition wrapper using:
        // material_definition->create_function_call(...)
        // In that case the API user needs to provide a argument list at least for those parameters
        // that do not have a default value.

        // ----------------------------------------------------------------------------------------

        // The next step is to create a compiled material. This already applies some optimizations
        // depending on the following flags and context options.

        // class and instance compilation is a trade-off between real-time parameter editing and
        // performance optimization.
        const mi::Uint32 flags = options.m_use_class_compilation
            ? mi::neuraylib::IMaterial_instance::CLASS_COMPILATION
            : mi::neuraylib::IMaterial_instance::DEFAULT_OPTIONS; // Instance Compilation

        // Set more optimization options (by default, they are all disabled)
        // The first three only affect class compilation and allow to select individual
        // optimizations to narrow the performance gap towards instance compilation while
        // sacrificing some parts of the real-time parameter edits.
        context->set_option("fold_ternary_on_df", options.m_fold_ternary_on_df);
        context->set_option("fold_all_bool_parameters", options.m_fold_all_bool_parameters);
        context->set_option("fold_all_enum_parameters", options.m_fold_all_enum_parameters);
        context->set_option("ignore_noinline", options.m_ignore_noinline);

        mi::base::Handle<mi::neuraylib::IMaterial_instance> material_instance2(
            material_instance->get_interface<mi::neuraylib::IMaterial_instance>());
        mi::base::Handle<const mi::neuraylib::ICompiled_material> compiled_material(
            material_instance2->create_compiled_material(flags, context.get()));

#ifndef MDL_SOURCE_RELEASE
        // ----------------------------------------------------------------------------------------

        // In case the material should be distilled before generating code,
        // the compiled material will be transformed before going on.
        // Usually distilling is used to simplify a material or to convert it to an uber-shader.

        if (!options.m_distilling_target.empty()) {
            mi::base::Handle<mi::neuraylib::IMdl_distiller_api> distiller(
                neuray->get_api_component<mi::neuraylib::IMdl_distiller_api>());

            // check if the selected target is known
            bool target_supported = false;
            for (mi::Size i = 0, n = distiller->get_target_count(); i < n; ++i) {
                if (options.m_distilling_target == distiller->get_target_name(i)) {
                    target_supported = true;
                    break;
                }
            }
            if (!target_supported)
                exit_failure("Distilling target '%s' is unknown.",
                    options.m_distilling_target.c_str());

            // The compiled material is transformed to match a given target.
            // Depending on the input and the selected target, the resulting material
            // will match the original in appearance to some extend.
            mi::Sint32 result = 0;
            compiled_material = distiller->distill_material(
                compiled_material.get(), options.m_distilling_target.c_str(), nullptr, &result);

            if (result != 0)
                exit_failure("Failed to distill a material instance of: %s::%s (%d)",
                    module_name.c_str(), material_name.c_str(), result);
        }
#endif

        // ----------------------------------------------------------------------------------------

        if (options.m_backend == "dag") {
            if (!options.m_output_file.empty())
            {
                std::ofstream file_stream;
                file_stream.open(options.m_output_file.c_str());
                if (file_stream)
                {
                    file_stream << "\n\n\n";
                    dump_dag(trans.get(), mdl_factory.get(), compiled_material.get(), file_stream);
                    file_stream << "\n\n\n";
                    file_stream.close();
                }
            } else {
                std::cout << "\n\n\n";
                dump_dag(trans.get(), mdl_factory.get(), compiled_material.get(), std::cout);
                std::cout << "\n\n\n";
            }
        } else {

            // For generating code we now need to select a back-end
            mi::base::Handle<mi::neuraylib::IMdl_backend_api> mdl_backend_api(
                neuray->get_api_component<mi::neuraylib::IMdl_backend_api>());

            mi::base::Handle<mi::neuraylib::IMdl_backend> backend;
            if (options.m_backend == "hlsl")
                backend = mdl_backend_api->get_backend(mi::neuraylib::IMdl_backend_api::MB_HLSL);
            else if (options.m_backend == "ptx")
                backend = mdl_backend_api->get_backend(mi::neuraylib::IMdl_backend_api::MB_CUDA_PTX);
            else if (options.m_backend == "llvm")
                backend = mdl_backend_api->get_backend(mi::neuraylib::IMdl_backend_api::MB_LLVM_IR);
            else
                exit_failure("Selected back-end '%s' is invalid or not supported.",
                    options.m_backend.c_str());

            // back-end specific options
            backend->set_option("texture_runtime_with_derivs",
                options.m_use_derivatives ? "on" : "off");
            backend->set_option("num_texture_results", options.m_num_texture_results.c_str());
            backend->set_option("num_texture_spaces", "4");

            // ----------------------------------------------------------------------------------------

            // and we need a link unit that will contain all the functions we want to generate code for
            mi::base::Handle<mi::neuraylib::ILink_unit> link_unit(
                backend->create_link_unit(trans.get(), context.get()));
            if (!print_messages(context.get()))
                exit_failure("Failed to create a link unit for the %s back-end",
                    options.m_backend.c_str());

            if (options.m_descs.empty()) {
                // select some default expressions to generate code for.
                // The functions to select depends on the renderer.
                // To get started, generating 'surface.scattering' would be enough
                // (see the other examples, like DXR, for how to consume the generated code in a shader).
                auto &descs = options.m_descs;
                descs.push_back(TD("ior", "ior"));
                descs.push_back(TD("thin_walled", "thin_walled"));
                descs.push_back(TD("surface.scattering", "surface_scattering"));
                descs.push_back(TD("surface.emission.emission", "surface_emission_emission"));
                descs.push_back(TD("surface.emission.intensity", "surface_emission_intensity"));
                descs.push_back(TD("surface.emission.mode", "surface_emission_mode"));
                descs.push_back(TD("backface.scattering", "backface_scattering"));
                descs.push_back(TD("backface.emission.emission", "backface_emission_emission"));
                descs.push_back(TD("backface.emission.intensity", "backface_emission_intensity"));
                descs.push_back(TD("backface.emission.mode", "backface_emission_mode"));
                descs.push_back(TD("volume.absorption_coefficient", "volume_absorption_coefficient"));
                descs.push_back(TD("volume.scattering_coefficient", "volume_scattering_coefficient"));
                descs.push_back(TD("geometry.normal", "geometry_normal"));
                descs.push_back(TD("geometry.cutout_opacity", "geometry_cutout_opacity"));
                descs.push_back(TD("geometry.displacement", "geometry_displacement"));
            }

            link_unit->add_material(
                compiled_material.get(), options.m_descs.data(), options.m_descs.size(), context.get());
            if (!print_messages(context.get()))
                exit_failure("Failed to select functions for code generation.");

            // ----------------------------------------------------------------------------------------

            // Translating the link unit into the target language is the last step and the only one
            // that can be time consuming. All the steps before are designed to be lightweight for
            // interactively changing materials and parameters.
            mi::base::Handle<const mi::neuraylib::ITarget_code> target_code(
                backend->translate_link_unit(link_unit.get(), context.get()));
            if (!print_messages(context.get()))
                exit_failure("Failed to translate the link unit to %s code.",
                    options.m_backend.c_str());

            // ----------------------------------------------------------------------------------------

            // The resulting target code contains all the information that is required for rendering.
            // This includes the actual shader code, resources, constants and an argument block
            // that contains raw parameter data for real-time parameter editing.

            // Print the generated code. Note, this is not enough. A renderer needs to handle resources,
            // argument blocks and so on as well. Additionally, a renderer runtime is required
            // as an interface between the generated code and the applications resource pipeline.
            if (!options.m_output_file.empty())
            {
                std::ofstream file_stream;
                file_stream.open(options.m_output_file.c_str());
                if (file_stream)
                {
                    file_stream << target_code->get_code();
                    file_stream.close();
                }
            }
            else {
                std::cout << "\n\n\n" << target_code->get_code() << "\n\n\n";
            }

            // Serialization is not supported by the LLVM-IR backend.
            if (options.m_backend != "llvm") {
                // If disabled, instance specific data is discarded. this makes sense for applications
                // that use class compilation and reuse materials that only differ in their parameter
                // set, meaning that they have the same hash and thereby the same generated code but
                // different argument blocks. These argument blocks be created separately using the
                // target code and also the deserialized target code.
                context->set_option("serialize_class_instance_data", true);

                // Serialize the target code object into a buffer.
                mi::base::Handle<const mi::neuraylib::IBuffer> tc_buffer(
                    target_code->serialize(context.get()));
                if (!print_messages(context.get()))
                    exit_failure("MDL target code serialization failed.");

                // Access the serialized target code data.
                // This is usually stored in some kind of cache along with other application data.
                // For deserialization, use backend->deserialize_target_code(...)
                /*const size_t tc_buffer_size =*/ tc_buffer->get_data_size();
                /*const mi::Uint8* tc_buffer_data =*/ tc_buffer->get_data();
            }
        }
        // ----------------------------------------------------------------------------------------

        // The target code can also be serialized for later reuse.
        // This can make sense to reduce startup time for larger scenes that have been loaded
        // before. A key for this kind of cache is the hash of a compiled material:
        mi::base::Uuid hash = compiled_material->get_hash();
        std::cout << "compiled material hash: \n" << std::hex << hash.m_id1 << " " << hash.m_id2
                    << " " << hash.m_id3 << " " << hash.m_id4 << std::dec  <<"\n";
    }

    // All transactions need to get committed or aborted before closing the application.
    // The code above is inside a block to ensure that we do not keep handles to database objects
    // while closing the transaction. This would result in an inconsistent state and produce an
    // error message.
    trans->commit();
}

// ------------------------------------------------------------------------------------------------

int MAIN_UTF8(int argc, char* argv[])
{
    // Parse command line options
    Options options;
    mi::examples::mdl::Configure_options configure_options;
    if (!options.parse(argc, argv))
    {
        options.print_usage(std::cout);
        exit_failure("Failed to parse command line arguments.");
    }
    if (options.m_help) {
        options.print_usage(std::cout);
        exit_success();
    }

    // Apply the search path setup described on the command line
    configure_options.additional_mdl_paths = options.m_mdl_paths;

    if (options.m_nostdpath)
    {
        configure_options.add_admin_space_search_paths = false;
        configure_options.add_user_space_search_paths = false;
        configure_options.add_example_search_path = false;
    }

    // Access the MDL SDK
    mi::base::Handle<mi::neuraylib::INeuray> neuray(mi::examples::mdl::load_and_get_ineuray());
    if (!neuray.is_valid_interface())
        exit_failure("Failed to load the SDK.");

    // Configure the MDL SDK
    if (!mi::examples::mdl::configure(neuray.get(), configure_options))
        exit_failure("Failed to initialize the SDK.");

#ifndef MDL_SOURCE_RELEASE
    // Load the distilling plugin
    if (mi::examples::mdl::load_plugin(neuray.get(), "mdl_distiller" MI_BASE_DLL_FILE_EXT) != 0)
        exit_failure("Failed to load the mdl_distiller plugin.");
#endif

    // Start the MDL SDK
    mi::Sint32 ret = neuray->start();
    if (ret != 0)
        exit_failure("Failed to initialize the SDK. Result code: %d", ret);

    // the main content of the example
    code_gen(neuray.get(), options);

    // Shut down the MDL SDK
    if (neuray->shutdown() != 0)
        exit_failure("Failed to shutdown the SDK.");

    // Unload the MDL SDK
    neuray = nullptr;
    if (!mi::examples::mdl::unload())
        exit_failure("Failed to unload the SDK.");

    exit_success();
}

// Convert command line arguments to UTF8 on Windows
COMMANDLINE_TO_UTF8

// ------------------------------------------------------------------------------------------------

void Options::print_usage(std::ostream& s)
{
    s << R"(
code_gen [options] <qualified_material_name>

options:

  -h|--help                     Print this usage message and exit.
  -p|--mdl_path <path>          Add the given path to the MDL search path.
  -n|--nostdpath                Prevent adding the MDL system and user search
                                path(s) to the MDL search path.
  -o|--output <file>            Export the module to this file. Default: stdout
  -b|--backend <backend>        Select the back-end to generate code for. {DAG, HLSL, PTX, LLVM}
                                Default: HLSL
  -e|--expr_path <path>         Add an MDL expression path to generate, like \"surface.scattering\".
                                Defaults to a set of expression paths.
  -d|--derivatives              Generate code with derivative support.
  -i|--instance_compilation     Use instance compilation instead of class compilation.
  -t|--text_results <num>       Number of float4 texture result slots in the state. Default: 16
  --ft                          Fold ternary operators when used on distribution functions.
  --fb                          Fold boolean parameters.
  --fe                          Fold enum parameters.
  --dian                        Disable ignoring anno::noinline() annotations.)";

#ifndef MDL_SOURCE_RELEASE
    s << R"(
  --distill <target>            Distill the material before running the code generation.)";
#endif

    s << std::endl;
}

// ------------------------------------------------------------------------------------------------

bool Options::parse(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg[0] == '-') {
            if (arg == "-h" || arg == "--help")
                m_help = true;
            else if (arg == "-n" || arg == "--nostdpath")
                m_nostdpath = true;
            else if (arg == "-d" || arg == "--derivatives")
                m_use_derivatives = true;
            else if (arg == "-i" || arg == "--instance_compilation")
                m_use_class_compilation = false;
            else if (arg == "--ft")
                m_fold_ternary_on_df = true;
            else if (arg == "--fb")
                m_fold_all_bool_parameters = true;
            else if (arg == "--ft")
                m_fold_all_enum_parameters = true;
            else if (arg == "--dian")
                m_ignore_noinline = false;
            else if (arg == "-p" || arg == "--mdl_path")
            {
                if (i == argc - 1)
                {
                    std::cerr << "error: Argument for -p|--mdl_path missing." << std::endl;
                    return false;
                }
                m_mdl_paths.push_back(argv[++i]);
            }
            else if (arg == "-o" || arg == "--output")
            {
                if (i == argc - 1)
                {
                    std::cerr << "error: Argument for -o|--output missing." << std::endl;
                    return false;
                }
                m_output_file = argv[++i];
            }
            else if (arg == "-b" || arg == "--backend")
            {
                if (i == argc - 1)
                {
                    std::cerr << "error: Argument for -b|--backend missing." << std::endl;
                    return false;
                }
                m_backend = argv[++i];
                std::transform(m_backend.begin(), m_backend.end(), m_backend.begin(),
                    [](unsigned char c) { return std::tolower(c); });
            }
            else if (arg == "-e" || arg == "--expr_path")
            {
                if (i == argc - 1)
                {
                    std::cerr << "error: Argument for -e|--expr_path missing." << std::endl;
                    return false;
                }
                std::string expr = argv[++i];
                std::string func_name = expr;
                std::transform(func_name.begin(), func_name.end(), func_name.begin(),
                    [](unsigned char c) { return char(c) == '.' ? '_' : c; });
                m_desc_strs.push_back(std::unique_ptr<std::string>(new std::string(expr)));
                m_desc_strs.push_back(std::unique_ptr<std::string>(new std::string(func_name)));
                m_descs.push_back(TD(
                    m_desc_strs[m_desc_strs.size() - 2].get()->c_str(),
                    m_desc_strs.back().get()->c_str()));
            }
            else if (arg == "-t" || arg == "--text_results")
            {
                if (i == argc - 1)
                {
                    std::cerr << "error: Argument for -t|--text_results missing." << std::endl;
                    return false;
                }
                m_num_texture_results = argv[++i];
            }
    #ifndef MDL_SOURCE_RELEASE
            else if (arg == "--distill")
            {
                if (i == argc - 1)
                {
                    std::cerr << "error: Argument for --distill missing." << std::endl;
                    return false;
                }
                m_distilling_target = argv[++i];
            }
    #endif
            else
            {
                std::cerr << "error: Unknown option \"" << arg << "\"." << std::endl;
                return false;
            }
        }
        else
        {
            if (i != argc - 1)
            {
                std::cerr << "error: Unexpected argument \"" << arg << "\"." << std::endl;
                return false;
            }
            m_qualified_material_name = arg;
        }
    }

    if (!m_help)
    {
        if (m_qualified_material_name.empty())
        {
            std::cerr << "error: Qualified material name missing." << std::endl;
            return false;
        }

        if (m_backend != "dag" && m_backend != "hlsl" && m_backend != "ptx" && m_backend != "llvm")
        {
            std::cerr << "error: Back-end is missing or invalid." << std::endl;
            return false;
        }
    }
    return true;
}
