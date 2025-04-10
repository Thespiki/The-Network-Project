using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a property of a network element or connection for display and editing in the UI.
    /// </summary>
    public class PropertyViewModel : INotifyPropertyChanged
    {
        private object? _value;
        private bool _isReadOnly;
        private bool _isAdvanced;
        private bool _isVisible = true;
        private bool _isModified;

        /// <summary>
        /// Gets or sets the name of the property.
        /// </summary>
        public string Name { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the display name of the property.
        /// </summary>
        public string DisplayName { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the description of the property.
        /// </summary>
        public string Description { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the category of the property.
        /// </summary>
        public string Category { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the value of the property.
        /// </summary>
        public object? Value
        {
            get => _value;
            set
            {
                if (!Equals(_value, value))
                {
                    _value = value;
                    IsModified = true;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the type of the property.
        /// </summary>
        public Type PropertyType { get; set; } = typeof(string);

        /// <summary>
        /// Gets or sets a value indicating whether this property is read-only.
        /// </summary>
        public bool IsReadOnly
        {
            get => _isReadOnly;
            set
            {
                if (_isReadOnly != value)
                {
                    _isReadOnly = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether this property is advanced.
        /// </summary>
        public bool IsAdvanced
        {
            get => _isAdvanced;
            set
            {
                if (_isAdvanced != value)
                {
                    _isAdvanced = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether this property is visible.
        /// </summary>
        public bool IsVisible
        {
            get => _isVisible;
            set
            {
                if (_isVisible != value)
                {
                    _isVisible = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether this property has been modified.
        /// </summary>
        public bool IsModified
        {
            get => _isModified;
            set
            {
                if (_isModified != value)
                {
                    _isModified = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether this property represents a file path.
        /// </summary>
        public bool IsFilePath { get; set; }

        /// <summary>
        /// Gets or sets a value indicating whether this property represents a reference to another object.
        /// </summary>
        public bool IsReference { get; set; }

        /// <summary>
        /// Gets or sets the list of possible values for this property if it is an enum or a reference.
        /// </summary>
        public List<object> PossibleValues { get; set; } = new List<object>();

        /// <summary>
        /// Gets or sets the validation rules for this property.
        /// </summary>
        public List<Func<object?, string?>> ValidationRules { get; set; } = new List<Func<object?, string?>>();

        /// <summary>
        /// Gets or sets the custom editor for this property.
        /// </summary>
        public string? CustomEditor { get; set; }

        /// <summary>
        /// Gets or sets the default value for this property.
        /// </summary>
        public object? DefaultValue { get; set; }

        /// <summary>
        /// Gets or sets the format string for this property.
        /// </summary>
        public string? FormatString { get; set; }

        /// <summary>
        /// Event raised when a property value changes.
        /// </summary>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// Validates the current value of this property.
        /// </summary>
        /// <returns>A validation error message if the value is invalid; otherwise null.</returns>
        public string? Validate()
        {
            return ValidationRules.Select(rule => rule(Value)).FirstOrDefault(result => result != null);
        }

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
