using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using TNP.App.Models;

namespace TNP.App.Plugins
{
    /// <summary>
    /// Default implementation of the plugin manager.
    /// </summary>
    public class DefaultPluginManager : IPluginManager
    {
        private readonly List<PluginInfo> _plugins = new List<PluginInfo>();
        private readonly Dictionary<Guid, IPlugin> _loadedPlugins = new Dictionary<Guid, IPlugin>();

        /// <summary>
        /// Initializes a new instance of the <see cref="DefaultPluginManager"/> class.
        /// </summary>
        public DefaultPluginManager()
        {
            // Initialize with some default plugins for demo purposes
            _plugins.Add(new PluginInfo
            {
                Id = Guid.NewGuid(),
                Name = "Network Analysis",
                Description = "Provides advanced network analysis tools",
                Version = "1.0.0",
                Author = "TNP Team",
                IsEnabled = false,
                InstallDate = DateTime.Now.AddDays(-10)
            });
            
            _plugins.Add(new PluginInfo
            {
                Id = Guid.NewGuid(),
                Name = "Device Discovery",
                Description = "Automatically discovers devices on the network",
                Version = "1.2.0",
                Author = "TNP Team",
                IsEnabled = false,
                InstallDate = DateTime.Now.AddDays(-5)
            });
            
            _plugins.Add(new PluginInfo
            {
                Id = Guid.NewGuid(),
                Name = "Security Scanner",
                Description = "Scans network for security vulnerabilities",
                Version = "0.9.1",
                Author = "Security Experts Inc.",
                IsEnabled = false,
                InstallDate = DateTime.Now.AddDays(-2)
            });
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
            return _plugins.AsReadOnly();
        }

        /// <summary>
        /// Gets a plugin by ID.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to get.</param>
        /// <returns>The plugin with the specified ID.</returns>
        public PluginInfo GetPlugin(Guid pluginId)
        {
            var plugin = _plugins.FirstOrDefault(p => p.Id == pluginId);
            if (plugin == null)
            {
                throw new ArgumentException($"Plugin with ID {pluginId} not found");
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
            // In a real implementation, this would load the plugin assembly
            if (_loadedPlugins.TryGetValue(pluginId, out var plugin))
            {
                return plugin;
            }
            
            throw new NotImplementedException("Plugin loading not implemented in demo version");
        }

        /// <summary>
        /// Unloads a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to unload.</param>
        public void UnloadPlugin(Guid pluginId)
        {
            if (_loadedPlugins.ContainsKey(pluginId))
            {
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
            plugin.IsActive = true;
            
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
            plugin.IsActive = false;
            
            if (_loadedPlugins.ContainsKey(pluginId))
            {
                UnloadPlugin(pluginId);
            }
            
            PluginDisabled?.Invoke(this, pluginId);
        }

        /// <summary>
        /// Installs a plugin.
        /// </summary>
        /// <param name="filePath">The file path of the plugin to install.</param>
        /// <returns>The installed plugin.</returns>
        public PluginInfo InstallPlugin(string filePath)
        {
            // In a real implementation, this would install the plugin from the specified file
            if (string.IsNullOrEmpty(filePath))
            {
                throw new ArgumentException("File path cannot be null or empty");
            }
            
            if (!File.Exists(filePath))
            {
                throw new FileNotFoundException("Plugin file not found", filePath);
            }
            
            // Create a mock plugin for demo purposes
            var pluginInfo = new PluginInfo
            {
                Id = Guid.NewGuid(),
                Name = Path.GetFileNameWithoutExtension(filePath),
                Description = "Installed plugin",
                Version = "1.0.0",
                Author = "Unknown",
                IsEnabled = false,
                InstallDate = DateTime.Now
            };
            
            _plugins.Add(pluginInfo);
            
            PluginInstalled?.Invoke(this, pluginInfo);
            
            return pluginInfo;
        }

        /// <summary>
        /// Uninstalls a plugin.
        /// </summary>
        /// <param name="pluginId">The ID of the plugin to uninstall.</param>
        public void UninstallPlugin(Guid pluginId)
        {
            var plugin = GetPlugin(pluginId);
            
            if (plugin.IsEnabled)
            {
                DisablePlugin(pluginId);
            }
            
            _plugins.Remove(plugin);
            
            PluginUninstalled?.Invoke(this, pluginId);
        }

        /// <summary>
        /// Refreshes the plugin list.
        /// </summary>
        public void RefreshPlugins()
        {
            // In a real implementation, this would rescan the plugin directory
            // But in this demo, we do nothing
        }
    }
}