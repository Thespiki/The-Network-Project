using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using System.Text.Json;
using TNP.App.Models;

namespace TNP.App.Services
{
    /// <summary>
    /// Manages the discovery, loading, and activation of plugins.
    /// </summary>
    public class PluginManager : IPluginManager
    {
        private readonly string _pluginsDirectory;
        private readonly List<PluginInfo> _availablePlugins;
        private readonly List<PluginInfo> _activePlugins;
        private readonly List<Assembly> _loadedAssemblies;

        /// <summary>
        /// Occurs when a plugin is activated.
        /// </summary>
        public event EventHandler<string>? PluginActivated;

        /// <summary>
        /// Occurs when a plugin is deactivated.
        /// </summary>
        public event EventHandler<string>? PluginDeactivated;

        /// <summary>
        /// Initializes a new instance of the <see cref="PluginManager"/> class.
        /// </summary>
        /// <param name="pluginsDirectory">The directory where plugins are located.</param>
        public PluginManager(string pluginsDirectory)
        {
            _pluginsDirectory = pluginsDirectory;
            _availablePlugins = new List<PluginInfo>();
            _activePlugins = new List<PluginInfo>();
            _loadedAssemblies = new List<Assembly>();

            // Create plugins directory if it doesn't exist
            if (!Directory.Exists(_pluginsDirectory))
            {
                Directory.CreateDirectory(_pluginsDirectory);
            }
        }

        /// <summary>
        /// Discovers available plugins in the plugins directory.
        /// </summary>
        /// <returns>A task that represents the asynchronous discover operation.</returns>
        public async Task DiscoverPluginsAsync()
        {
            _availablePlugins.Clear();

            // Find all plugin.json files in subdirectories
            foreach (var subDir in Directory.GetDirectories(_pluginsDirectory))
            {
                var pluginJsonPath = Path.Combine(subDir, "plugin.json");
                if (File.Exists(pluginJsonPath))
                {
                    try
                    {
                        // Read plugin.json
                        var jsonContent = await File.ReadAllTextAsync(pluginJsonPath);
                        var pluginInfo = JsonSerializer.Deserialize<PluginInfo>(jsonContent);

                        if (pluginInfo != null)
                        {
                            // Store the plugin directory
                            pluginInfo.Directory = subDir;
                            _availablePlugins.Add(pluginInfo);
                        }
                    }
                    catch (Exception ex)
                    {
                        // Log error but continue with other plugins
                        Console.WriteLine($"Error loading plugin from {subDir}: {ex.Message}");
                    }
                }
            }
        }

        /// <summary>
        /// Gets the list of available plugins.
        /// </summary>
        /// <returns>A list of available plugins.</returns>
        public IEnumerable<PluginInfo> GetAvailablePlugins()
        {
            return _availablePlugins.AsReadOnly();
        }

        /// <summary>
        /// Gets the list of active plugins.
        /// </summary>
        /// <returns>A list of active plugins.</returns>
        public IEnumerable<PluginInfo> GetActivePlugins()
        {
            return _activePlugins.AsReadOnly();
        }

        /// <summary>
        /// Activates a plugin.
        /// </summary>
        /// <param name="pluginInfo">The plugin to activate.</param>
        /// <returns>true if the plugin was successfully activated; otherwise, false.</returns>
        public bool ActivatePlugin(PluginInfo pluginInfo)
        {
            if (pluginInfo == null || _activePlugins.Contains(pluginInfo))
                return false;

            try
            {
                // Load the plugin assembly
                var assemblyPath = Path.Combine(pluginInfo.Directory, pluginInfo.EntryPoint);
                var assembly = Assembly.LoadFrom(assemblyPath);
                _loadedAssemblies.Add(assembly);

                // Initialize the plugin (find and call initialization method)
                // This would involve finding classes implementing a plugin interface and initializing them
                // For the skeleton, we'll just mark it as active

                pluginInfo.IsActive = true;
                _activePlugins.Add(pluginInfo);
                _availablePlugins.Remove(pluginInfo);

                OnPluginActivated(pluginInfo);
                return true;
            }
            catch (Exception ex)
            {
                // Log error
                Console.WriteLine($"Error activating plugin {pluginInfo.Name}: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Deactivates a plugin.
        /// </summary>
        /// <param name="pluginInfo">The plugin to deactivate.</param>
        /// <returns>true if the plugin was successfully deactivated; otherwise, false.</returns>
        public bool DeactivatePlugin(PluginInfo pluginInfo)
        {
            if (pluginInfo == null || !_activePlugins.Contains(pluginInfo))
                return false;

            try
            {
                // Call plugin cleanup/shutdown methods if needed
                // For the skeleton, we'll just mark it as inactive

                pluginInfo.IsActive = false;
                _activePlugins.Remove(pluginInfo);
                _availablePlugins.Add(pluginInfo);

                OnPluginDeactivated(pluginInfo);
                return true;
            }
            catch (Exception ex)
            {
                // Log error
                Console.WriteLine($"Error deactivating plugin {pluginInfo.Name}: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Raises the PluginActivated event.
        /// </summary>
        /// <param name="pluginInfo">The plugin that was activated.</param>
        private void OnPluginActivated(PluginInfo pluginInfo)
        {
            PluginActivated?.Invoke(this, pluginInfo.Name);
        }

        /// <summary>
        /// Raises the PluginDeactivated event.
        /// </summary>
        /// <param name="pluginInfo">The plugin that was deactivated.</param>
        private void OnPluginDeactivated(PluginInfo pluginInfo)
        {
            PluginDeactivated?.Invoke(this, pluginInfo.Name);
        }
        
        /// <summary>
        /// Activates a plugin by name.
        /// </summary>
        /// <param name="pluginName">The name of the plugin to activate.</param>
        public void ActivatePlugin(string pluginName)
        {
            var plugin = _availablePlugins.Find(p => p.Name == pluginName);
            if (plugin != null)
            {
                ActivatePlugin(plugin);
            }
        }
        
        /// <summary>
        /// Deactivates a plugin by name.
        /// </summary>
        /// <param name="pluginName">The name of the plugin to deactivate.</param>
        public void DeactivatePlugin(string pluginName)
        {
            var plugin = _activePlugins.Find(p => p.Name == pluginName);
            if (plugin != null)
            {
                DeactivatePlugin(plugin);
            }
        }
        
        /// <summary>
        /// Installs a plugin from a file.
        /// </summary>
        /// <param name="filePath">The path to the plugin file.</param>
        /// <returns>A task representing the installation process.</returns>
        public async Task InstallPluginAsync(string filePath)
        {
            // Implementation will extract the plugin package and install it
            // For now, this is a stub implementation
            await Task.CompletedTask;
        }
    }
}
