using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using Newtonsoft.Json;
using TNP.App.Models;

namespace TNP.App.Plugins
{
    /// <summary>
    /// Implementation of the plugin manager.
    /// </summary>
    public class PluginManager : IPluginManager
    {
        private readonly string _pluginsDirectory;
        private readonly Dictionary<Guid, PluginInfo> _plugins = new Dictionary<Guid, PluginInfo>();
        private readonly Dictionary<Guid, IPlugin> _loadedPlugins = new Dictionary<Guid, IPlugin>();
        private readonly Dictionary<string, Assembly> _loadedAssemblies = new Dictionary<string, Assembly>();

        /// <summary>
        /// Initializes a new instance of the <see cref="PluginManager"/> class.
        /// </summary>
        /// <param name="pluginsDirectory">The directory containing plugins.</param>
        public PluginManager(string pluginsDirectory)
        {
            _pluginsDirectory = pluginsDirectory ?? throw new ArgumentNullException(nameof(pluginsDirectory));

            // Ensure the plugins directory exists
            if (!Directory.Exists(_pluginsDirectory))
            {
                Directory.CreateDirectory(_pluginsDirectory);
            }

            // Load plugins
            RefreshPlugins();
        }

        /// <summary>
        /// Event raised when a plugin is installed.
        /// </summary>
        public event EventHandler<PluginInfo>? PluginInstalled;

        /// <summary>
        /// Event raised when a plugin is uninstalled.
        /// </summary>
        public event EventHandler<Guid>? PluginUninstalled;

        /// <summary>
        /// Event raised when a plugin is enabled.
        /// </summary>
        public event EventHandler<Guid>? PluginEnabled;

        /// <summary>
        /// Event raised when a plugin is disabled.
        /// </summary>
        public event EventHandler<Guid>? PluginDisabled;

        /// <summary>
        /// Gets all available plugins.
        /// </summary>
        /// <returns>The list of available plugins.</returns>
        public IEnumerable<PluginInfo> GetPlugins()
        {
            return _plugins.Values.ToList();
        }

        /// <summary>
        /// Gets a plugin by ID.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to get.</param>
        /// <returns>The plugin with the specified ID.</returns>
        public PluginInfo GetPlugin(Guid pluginId)
        {
            if (!_plugins.TryGetValue(pluginId, out var plugin))
            {
                throw new ArgumentException($"Plugin with ID {pluginId} not found.", nameof(pluginId));
            }

            return plugin;
        }

        /// <summary>
        /// Loads a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to load.</param>
        /// <returns>The loaded plugin.</returns>
        public IPlugin LoadPlugin(Guid pluginId)
        {
            if (_loadedPlugins.TryGetValue(pluginId, out var plugin))
            {
                return plugin;
            }

            var pluginInfo = GetPlugin(pluginId);
            if (!pluginInfo.IsEnabled)
            {
                throw new InvalidOperationException($"Plugin {pluginInfo.Name} is disabled.");
            }

            var pluginAssemblyPath = Path.Combine(_pluginsDirectory, pluginInfo.DirectoryName, pluginInfo.EntryPoint);
            if (!File.Exists(pluginAssemblyPath))
            {
                throw new FileNotFoundException($"Plugin assembly not found: {pluginAssemblyPath}");
            }

            // Load the assembly
            Assembly assembly;
            if (_loadedAssemblies.TryGetValue(pluginAssemblyPath, out var loadedAssembly))
            {
                assembly = loadedAssembly;
            }
            else
            {
                assembly = Assembly.LoadFrom(pluginAssemblyPath);
                _loadedAssemblies[pluginAssemblyPath] = assembly;
            }

            // Find plugin types
            var pluginType = assembly.GetTypes()
                .FirstOrDefault(t => typeof(IPlugin).IsAssignableFrom(t) && !t.IsInterface && !t.IsAbstract);

            if (pluginType == null)
            {
                throw new InvalidOperationException($"No plugin implementation found in assembly: {pluginAssemblyPath}");
            }

            // Create plugin instance
            plugin = (IPlugin)Activator.CreateInstance(pluginType)!;
            _loadedPlugins[pluginId] = plugin;

            // Initialize plugin
            plugin.Initialize();

            return plugin;
        }

        /// <summary>
        /// Unloads a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to unload.</param>
        public void UnloadPlugin(Guid pluginId)
        {
            if (_loadedPlugins.TryGetValue(pluginId, out var plugin))
            {
                plugin.Shutdown();
                _loadedPlugins.Remove(pluginId);
            }
        }

        /// <summary>
        /// Enables a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to enable.</param>
        public void EnablePlugin(Guid pluginId)
        {
            var plugin = GetPlugin(pluginId);
            plugin.IsEnabled = true;

            // Update plugin.json
            SavePluginConfig(plugin);

            PluginEnabled?.Invoke(this, pluginId);
        }

        /// <summary>
        /// Disables a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to disable.</param>
        public void DisablePlugin(Guid pluginId)
        {
            var plugin = GetPlugin(pluginId);
            plugin.IsEnabled = false;

            // Update plugin.json
            SavePluginConfig(plugin);

            // Unload the plugin if it's loaded
            UnloadPlugin(pluginId);

            PluginDisabled?.Invoke(this, pluginId);
        }

        /// <summary>
        /// Installs a plugin.
        /// </summary>
        /// <param name="filePath">The file path of the plugin to install.</param>
        /// <returns>The installed plugin.</returns>
        public PluginInfo InstallPlugin(string filePath)
        {
            if (!File.Exists(filePath))
            {
                throw new FileNotFoundException($"Plugin file not found: {filePath}");
            }

            // Create a unique directory for the plugin
            var pluginDirectoryName = Guid.NewGuid().ToString();
            var pluginDirectory = Path.Combine(_pluginsDirectory, pluginDirectoryName);
            Directory.CreateDirectory(pluginDirectory);

            try
            {
                // Extract plugin files
                ZipFile.ExtractToDirectory(filePath, pluginDirectory);

                // Look for plugin.json
                var pluginJsonPath = Path.Combine(pluginDirectory, "plugin.json");
                if (!File.Exists(pluginJsonPath))
                {
                    throw new InvalidOperationException("Invalid plugin package: plugin.json not found.");
                }

                // Load plugin.json
                var pluginJson = File.ReadAllText(pluginJsonPath);
                var pluginInfo = JsonConvert.DeserializeObject<PluginInfo>(pluginJson);

                if (pluginInfo == null)
                {
                    throw new InvalidOperationException("Invalid plugin.json format.");
                }

                // Update plugin info
                pluginInfo.Id = Guid.NewGuid();
                pluginInfo.DirectoryName = pluginDirectoryName;
                pluginInfo.IsEnabled = true;
                pluginInfo.InstallDate = DateTime.Now;

                // Save updated plugin.json
                SavePluginConfig(pluginInfo);

                // Add to plugins list
                _plugins[pluginInfo.Id] = pluginInfo;

                PluginInstalled?.Invoke(this, pluginInfo);

                return pluginInfo;
            }
            catch (Exception)
            {
                // Clean up on error
                if (Directory.Exists(pluginDirectory))
                {
                    Directory.Delete(pluginDirectory, true);
                }
                throw;
            }
        }

        /// <summary>
        /// Uninstalls a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to uninstall.</param>
        public void UninstallPlugin(Guid pluginId)
        {
            var plugin = GetPlugin(pluginId);

            // Unload the plugin if it's loaded
            UnloadPlugin(pluginId);

            // Remove plugin directory
            var pluginDirectory = Path.Combine(_pluginsDirectory, plugin.DirectoryName);
            if (Directory.Exists(pluginDirectory))
            {
                Directory.Delete(pluginDirectory, true);
            }

            // Remove from plugins list
            _plugins.Remove(pluginId);

            PluginUninstalled?.Invoke(this, pluginId);
        }

        /// <summary>
        /// Refreshes the plugin list.
        /// </summary>
        public void RefreshPlugins()
        {
            _plugins.Clear();

            foreach (var pluginDirectory in Directory.GetDirectories(_pluginsDirectory))
            {
                var pluginJsonPath = Path.Combine(pluginDirectory, "plugin.json");
                if (File.Exists(pluginJsonPath))
                {
                    try
                    {
                        var pluginJson = File.ReadAllText(pluginJsonPath);
                        var pluginInfo = JsonConvert.DeserializeObject<PluginInfo>(pluginJson);

                        if (pluginInfo != null && pluginInfo.Id != Guid.Empty)
                        {
                            pluginInfo.DirectoryName = Path.GetFileName(pluginDirectory);
                            _plugins[pluginInfo.Id] = pluginInfo;
                        }
                    }
                    catch (Exception)
                    {
                        // Skip invalid plugin
                    }
                }
            }
        }

        /// <summary>
        /// Saves a plugin configuration.
        /// </summary>
        /// <param name="plugin">The plugin configuration to save.</param>
        private void SavePluginConfig(PluginInfo plugin)
        {
            var pluginDirectory = Path.Combine(_pluginsDirectory, plugin.DirectoryName);
            var pluginJsonPath = Path.Combine(pluginDirectory, "plugin.json");

            var pluginJson = JsonConvert.SerializeObject(plugin, Formatting.Indented);
            File.WriteAllText(pluginJsonPath, pluginJson);
        }
    }
}
