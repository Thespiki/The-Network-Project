using System;
using System.Collections.Generic;
using TNP.App.Models;

namespace TNP.App.Plugins
{
    /// <summary>
    /// Interface for the plugin manager.
    /// </summary>
    public interface IPluginManager
    {
        /// <summary>
        /// Gets all available plugins.
        /// </summary>
        /// <returns>The list of available plugins.</returns>
        IEnumerable<PluginInfo> GetPlugins();

        /// <summary>
        /// Gets a plugin by ID.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to get.</param>
        /// <returns>The plugin with the specified ID.</returns>
        PluginInfo GetPlugin(Guid pluginId);

        /// <summary>
        /// Loads a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to load.</param>
        /// <returns>The loaded plugin.</returns>
        IPlugin LoadPlugin(Guid pluginId);

        /// <summary>
        /// Unloads a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to unload.</param>
        void UnloadPlugin(Guid pluginId);

        /// <summary>
        /// Enables a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to enable.</param>
        void EnablePlugin(Guid pluginId);

        /// <summary>
        /// Disables a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to disable.</param>
        void DisablePlugin(Guid pluginId);

        /// <summary>
        /// Installs a plugin.
        /// </summary>
        /// <param name="filePath">The file path of the plugin to install.</param>
        /// <returns>The installed plugin.</returns>
        PluginInfo InstallPlugin(string filePath);

        /// <summary>
        /// Uninstalls a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to uninstall.</param>
        void UninstallPlugin(Guid pluginId);

        /// <summary>
        /// Refreshes the plugin list.
        /// </summary>
        void RefreshPlugins();

        /// <summary>
        /// Event raised when a plugin is installed.
        /// </summary>
        event EventHandler<PluginInfo>? PluginInstalled;

        /// <summary>
        /// Event raised when a plugin is uninstalled.
        /// </summary>
        event EventHandler<Guid>? PluginUninstalled;

        /// <summary>
        /// Event raised when a plugin is enabled.
        /// </summary>
        event EventHandler<Guid>? PluginEnabled;

        /// <summary>
        /// Event raised when a plugin is disabled.
        /// </summary>
        event EventHandler<Guid>? PluginDisabled;
    }
}
