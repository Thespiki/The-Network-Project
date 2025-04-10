using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using TNP.App.Models;

namespace TNP.App.Services
{
    /// <summary>
    /// Interface for the plugin manager service.
    /// </summary>
    public interface IPluginManager
    {
        /// <summary>
        /// Event raised when a plugin is activated.
        /// </summary>
        event EventHandler<string> PluginActivated;
        
        /// <summary>
        /// Event raised when a plugin is deactivated.
        /// </summary>
        event EventHandler<string> PluginDeactivated;
        
        /// <summary>
        /// Gets available plugins.
        /// </summary>
        /// <returns>The available plugins.</returns>
        IEnumerable<PluginInfo> GetAvailablePlugins();
        
        /// <summary>
        /// Gets active plugins.
        /// </summary>
        /// <returns>The active plugins.</returns>
        IEnumerable<PluginInfo> GetActivePlugins();
        
        /// <summary>
        /// Activates a plugin.
        /// </summary>
        /// <param name="pluginName">The plugin name.</param>
        void ActivatePlugin(string pluginName);
        
        /// <summary>
        /// Deactivates a plugin.
        /// </summary>
        /// <param name="pluginName">The plugin name.</param>
        void DeactivatePlugin(string pluginName);
        
        /// <summary>
        /// Installs a plugin from a file.
        /// </summary>
        /// <param name="filePath">The plugin file path.</param>
        /// <returns>A task representing the asynchronous operation.</returns>
        Task InstallPluginAsync(string filePath);
    }
}