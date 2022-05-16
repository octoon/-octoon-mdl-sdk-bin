/******************************************************************************
 * Copyright 2022 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/

 /// \file
 /// \brief sets up a basic mdl environment

#ifndef MDL_SDK_EXAMPLES_MDL_SDK_HELPER_H 
#define MDL_SDK_EXAMPLES_MDL_SDK_HELPER_H

// MDL SDK (and utils)
#include <mi/mdl_sdk.h>
#include "mdl_browser_command_line_options.h"


/// Custom logger 
class Mdl_browser_logger : public mi::base::Interface_implement<mi::base::ILogger>
{
public:
    Mdl_browser_logger(bool trace) : m_trace(trace) {}

    void message(
        mi::base::Message_severity level,
        const char* /*category*/,
        const mi::base::Message_details& /*details*/,
        const char* message) override
    {
        if ((m_trace) || (level == mi::base::MESSAGE_SEVERITY_ERROR))
            fprintf(stderr, "%s\n", message);
    }

private:
    bool m_trace;
};

mi::neuraylib::INeuray* load_mdl_sdk(const Mdl_browser_command_line_options& options)
{
    // Access the MDL SDK
    auto neuray = mi::base::Handle<mi::neuraylib::INeuray>(
        mi::examples::mdl::load_and_get_ineuray());

    if (!neuray.is_valid_interface())
    {
        std::cerr << "[MDL Browser] Failed to load 'libmdl_sdk' library.\n";
        return nullptr;
    }

    // Add MDL search paths
    auto mdl_configuration = mi::base::Handle<mi::neuraylib::IMdl_configuration>(
        neuray->get_api_component<mi::neuraylib::IMdl_configuration>());

    auto logger = mi::base::make_handle(new Mdl_browser_logger(false));
    mdl_configuration->set_logger(logger.get());

    // Disable encoded names for now
    mdl_configuration->set_encoded_names_enabled(false);

    // clear all search paths and add specified default ones
    mdl_configuration->clear_mdl_paths();
    mdl_configuration->clear_resource_paths();

    // use default paths if none are specified explicitly
    if (options.search_paths.empty())
    {
        // add admin space search paths before user space paths
        mdl_configuration->add_mdl_system_paths();
        mdl_configuration->add_mdl_user_paths();

        for (mi::Size i = 0, n = mdl_configuration->get_mdl_system_paths_length(); i < n; ++i)
            mdl_configuration->add_resource_path(mdl_configuration->get_mdl_system_path( i));

        for (mi::Size i = 0, n = mdl_configuration->get_mdl_user_paths_length(); i < n; ++i)
            mdl_configuration->add_resource_path(mdl_configuration->get_mdl_user_path( i));
    }
    else
    {
        // add the paths specified on the command line
        for (const auto& path : options.search_paths)
        {
            mdl_configuration->add_mdl_path(path.c_str());
            mdl_configuration->add_resource_path(path.c_str());
        }
    }

    // print paths to check
    for (mi::Size i = 0, n = mdl_configuration->get_mdl_paths_length(); i < n; ++i)
        std::cerr << "MDL Module Path: " << mdl_configuration->get_mdl_path(i)->get_c_str() << "\n";

    // Configure the MDL SDK
    // Load plugin required for loading textures
    if (mi::examples::mdl::load_plugin(neuray.get(), "nv_freeimage" MI_BASE_DLL_FILE_EXT) != 0)
    {
        std::cerr << "[MDL Browser] Failed to load 'nv_freeimage' library.\n";
        return nullptr;
    }

    // setup the locale if specified by the user
    if (!options.locale.empty())
    {
        mi::base::Handle<mi::neuraylib::IMdl_i18n_configuration> i18n_configuration(
            neuray->get_api_component<mi::neuraylib::IMdl_i18n_configuration>());

        if (!i18n_configuration.is_valid_interface())
        {
            std::cerr << "[MDL Browser] Failed to setup locale.\n";
            return nullptr;
        }

        // set the local
        i18n_configuration->set_locale(options.locale.c_str());
    }

    // Start the MDL SDK
    mi::Sint32 ret = neuray->start();
    if (ret != 0)
        // NOTE: this kills the app in case of failure
        exit_failure("Failed to initialize the SDK. Result code: %d", ret);

    neuray->retain();
    return neuray.get();
}

mi::neuraylib::ITransaction* create_transaction(mi::neuraylib::INeuray* neuray)
{
    auto database = mi::base::Handle<mi::neuraylib::IDatabase>(
        neuray->get_api_component<mi::neuraylib::IDatabase>());

    auto scope = mi::base::Handle<mi::neuraylib::IScope>(database->get_global_scope());

    mi::base::Handle<mi::neuraylib::ITransaction> transaction(scope->create_transaction());
    transaction->retain();
    return transaction.get();
}


#endif
