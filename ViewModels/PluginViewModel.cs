using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using TNP.App.Models;
using TNP.App.Plugins;

namespace TNP.App.ViewModels
{
    /// <summary>
    /// ViewModel for plugin management interface.
    /// </summary>
    public class PluginViewModel : INotifyPropertyChanged
    {
        private readonly IPluginManager _pluginManager;
        private PluginInfo? _selectedPlugin;
        private string _pluginSearchText = string.Empty;
        private bool _isInstalling;
        private string _installationStatus = string.Empty;

        /// <summary>
        /// Gets or sets the search text for filtering plugins.
        /// </summary>
        public string PluginSearchText
        {
            get => _pluginSearchText;
            set
            {
                if (_pluginSearchText != value)
                {
                    _pluginSearchText = value;
                    OnPropertyChanged();
                    FilterPlugins();
                }
            }
        }

        /// <summary>
        /// Gets or sets the selected plugin.
        /// </summary>
        public PluginInfo? SelectedPlugin
        {
            get => _selectedPlugin;
            set
            {
                if (_selectedPlugin != value)
                {
                    _selectedPlugin = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(CanActivatePlugin));
                    OnPropertyChanged(nameof(CanDeactivatePlugin));
                }
            }
        }

        /// <summary>
        /// Gets a value indicating whether the selected plugin can be activated.
        /// </summary>
        public bool CanActivatePlugin => 
            _selectedPlugin != null && 
            !_isInstalling && 
            AvailablePlugins.Contains(_selectedPlugin) && 
            !ActivePlugins.Contains(_selectedPlugin);

        /// <summary>
        /// Gets a value indicating whether the selected plugin can be deactivated.
        /// </summary>
        public bool CanDeactivatePlugin => 
            _selectedPlugin != null && 
            !_isInstalling && 
            ActivePlugins.Contains(_selectedPlugin);

        /// <summary>
        /// Gets or sets a value indicating whether a plugin is currently being installed.
        /// </summary>
        public bool IsInstalling
        {
            get => _isInstalling;
            set
            {
                if (_isInstalling != value)
                {
                    _isInstalling = value;
                    OnPropertyChanged();
                    OnPropertyChanged(nameof(CanActivatePlugin));
                    OnPropertyChanged(nameof(CanDeactivatePlugin));
                }
            }
        }

        /// <summary>
        /// Gets or sets the installation status message.
        /// </summary>
        public string InstallationStatus
        {
            get => _installationStatus;
            set
            {
                if (_installationStatus != value)
                {
                    _installationStatus = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets the collection of available plugins.
        /// </summary>
        public ObservableCollection<PluginInfo> AvailablePlugins { get; } = new ObservableCollection<PluginInfo>();

        /// <summary>
        /// Gets the collection of active plugins.
        /// </summary>
        public ObservableCollection<PluginInfo> ActivePlugins { get; } = new ObservableCollection<PluginInfo>();

        /// <summary>
        /// Initializes a new instance of the <see cref="PluginViewModel"/> class.
        /// </summary>
        /// <param name="pluginManager">The plugin manager service.</param>
        public PluginViewModel(IPluginManager pluginManager)
        {
            _pluginManager = pluginManager ?? throw new ArgumentNullException(nameof(pluginManager));
            
            // Subscribe to plugin events
            _pluginManager.PluginEnabled += OnPluginEnabled;
            _pluginManager.PluginDisabled += OnPluginDisabled;
            
            // Load initial plugins
            RefreshPlugins();
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PluginViewModel"/> class with default PluginManager.
        /// </summary>
        public PluginViewModel()
        {
            // Create a default plugin manager implementation
            _pluginManager = new DefaultPluginManager();
            
            // Subscribe to plugin events
            _pluginManager.PluginEnabled += OnPluginEnabled;
            _pluginManager.PluginDisabled += OnPluginDisabled;
            
            // Load initial plugins
            RefreshPlugins();
        }

        /// <summary>
        /// Refreshes the plugin collections.
        /// </summary>
        public void RefreshPlugins()
        {
            AvailablePlugins.Clear();
            ActivePlugins.Clear();
            
            // Adapter le plugin manager existant pour utiliser l'interface IPluginManager de Plugins
            foreach (var plugin in _pluginManager.GetPlugins())
            {
                if (plugin.IsEnabled)
                {
                    ActivePlugins.Add(plugin);
                }
                else
                {
                    AvailablePlugins.Add(plugin);
                }
            }
            
            FilterPlugins();
        }

        /// <summary>
        /// Activates the selected plugin.
        /// </summary>
        public void ActivatePlugin()
        {
            if (SelectedPlugin == null || !CanActivatePlugin)
                return;
            
            try
            {
                _pluginManager.EnablePlugin(SelectedPlugin.Id);
            }
            catch (Exception ex)
            {
                // Handle activation error
                InstallationStatus = $"Error activating plugin: {ex.Message}";
            }
        }

        /// <summary>
        /// Deactivates the selected plugin.
        /// </summary>
        public void DeactivatePlugin()
        {
            if (SelectedPlugin == null || !CanDeactivatePlugin)
                return;
            
            try
            {
                _pluginManager.DisablePlugin(SelectedPlugin.Id);
            }
            catch (Exception ex)
            {
                // Handle deactivation error
                InstallationStatus = $"Error deactivating plugin: {ex.Message}";
            }
        }

        /// <summary>
        /// Installs a new plugin from a file.
        /// </summary>
        /// <param name="filePath">The plugin file path.</param>
        public void InstallPlugin(string filePath)
        {
            if (string.IsNullOrEmpty(filePath) || !File.Exists(filePath))
            {
                InstallationStatus = "Invalid plugin file";
                return;
            }
            
            IsInstalling = true;
            InstallationStatus = $"Installing plugin from {Path.GetFileName(filePath)}...";
            
            try
            {
                _pluginManager.InstallPlugin(filePath);
                InstallationStatus = "Plugin installed successfully";
                RefreshPlugins();
            }
            catch (Exception ex)
            {
                InstallationStatus = $"Error installing plugin: {ex.Message}";
            }
            finally
            {
                IsInstalling = false;
            }
        }

        /// <summary>
        /// Filters the plugins based on the search text.
        /// </summary>
        private void FilterPlugins()
        {
            if (string.IsNullOrWhiteSpace(PluginSearchText))
                return;
            
            var searchTerms = PluginSearchText.ToLowerInvariant().Split(' ');
            
            // Filter available plugins
            var filteredAvailable = _pluginManager.GetPlugins()
                .Where(p => !p.IsEnabled && MatchesSearch(p, searchTerms))
                .ToList();
            
            AvailablePlugins.Clear();
            foreach (var plugin in filteredAvailable)
            {
                AvailablePlugins.Add(plugin);
            }
            
            // Filter active plugins
            var filteredActive = _pluginManager.GetPlugins()
                .Where(p => p.IsEnabled && MatchesSearch(p, searchTerms))
                .ToList();
            
            ActivePlugins.Clear();
            foreach (var plugin in filteredActive)
            {
                ActivePlugins.Add(plugin);
            }
        }

        /// <summary>
        /// Determines if a plugin matches the search terms.
        /// </summary>
        /// <param name="plugin">The plugin info.</param>
        /// <param name="searchTerms">The search terms.</param>
        /// <returns>True if the plugin matches the search terms.</returns>
        private bool MatchesSearch(PluginInfo plugin, string[] searchTerms)
        {
            return searchTerms.All(term =>
                plugin.Name.ToLowerInvariant().Contains(term) ||
                plugin.Description.ToLowerInvariant().Contains(term));
        }

        /// <summary>
        /// Handles plugin enabled event.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="pluginId">The plugin ID.</param>
        private void OnPluginEnabled(object? sender, Guid pluginId)
        {
            RefreshPlugins();
            OnPropertyChanged(nameof(CanActivatePlugin));
            OnPropertyChanged(nameof(CanDeactivatePlugin));
        }

        /// <summary>
        /// Handles plugin disabled event.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="pluginId">The plugin ID.</param>
        private void OnPluginDisabled(object? sender, Guid pluginId)
        {
            RefreshPlugins();
            OnPropertyChanged(nameof(CanActivatePlugin));
            OnPropertyChanged(nameof(CanDeactivatePlugin));
        }

        /// <summary>
        /// Event raised when a property value changes.
        /// </summary>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// Called when a property value changes.
        /// </summary>
        /// <param name="propertyName">The name of the property.</param>
        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}