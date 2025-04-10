using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents information about a plugin.
    /// </summary>
    public class PluginInfo : INotifyPropertyChanged
    {
        private bool _isEnabled;
        private bool _isActive;

        /// <summary>
        /// Gets or sets the unique identifier of the plugin.
        /// </summary>
        public Guid Id { get; set; }

        /// <summary>
        /// Gets or sets the name of the plugin.
        /// </summary>
        public string Name { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the description of the plugin.
        /// </summary>
        public string Description { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the version of the plugin.
        /// </summary>
        public string Version { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the author of the plugin.
        /// </summary>
        public string Author { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the URL of the plugin.
        /// </summary>
        public string Url { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the entry point of the plugin.
        /// </summary>
        public string EntryPoint { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the directory name of the plugin.
        /// </summary>
        public string DirectoryName { get; set; } = string.Empty;
        
        /// <summary>
        /// Gets or sets the directory path of the plugin.
        /// </summary>
        public string Directory { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the install date of the plugin.
        /// </summary>
        public DateTime InstallDate { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether the plugin is enabled.
        /// </summary>
        public bool IsEnabled
        {
            get => _isEnabled;
            set
            {
                if (_isEnabled != value)
                {
                    _isEnabled = value;
                    OnPropertyChanged();
                }
            }
        }
        
        /// <summary>
        /// Gets or sets a value indicating whether the plugin is active.
        /// </summary>
        public bool IsActive
        {
            get => _isActive;
            set
            {
                if (_isActive != value)
                {
                    _isActive = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Event raised when a property value changes.
        /// </summary>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// Raises the PropertyChanged event.
        /// </summary>
        /// <param name="propertyName">The name of the property that changed.</param>
        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
