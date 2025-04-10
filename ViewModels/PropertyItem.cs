using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace TNP.App.ViewModels
{
    /// <summary>
    /// Represents a property item for the property editor.
    /// </summary>
    public class PropertyItem : INotifyPropertyChanged
    {
        private string _key;
        private object _value;
        private Type _propertyType;
        private bool _isReadOnly;
        private List<string> _allowedValues;
        private string _description;
        private string _category;

        /// <summary>
        /// Initializes a new instance of the <see cref="PropertyItem"/> class.
        /// </summary>
        public PropertyItem()
        {
            _key = string.Empty;
            _value = null!;
            _propertyType = typeof(string);
            _isReadOnly = false;
            _allowedValues = new List<string>();
            _description = string.Empty;
            _category = string.Empty;
        }

        /// <summary>
        /// Gets or sets the key (property name).
        /// </summary>
        public string Key
        {
            get => _key;
            set
            {
                if (_key != value)
                {
                    _key = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the value of the property.
        /// </summary>
        public object Value
        {
            get => _value;
            set
            {
                if (!object.Equals(_value, value))
                {
                    _value = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the property type.
        /// </summary>
        public Type PropertyType
        {
            get => _propertyType;
            set
            {
                if (_propertyType != value)
                {
                    _propertyType = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether the property is read-only.
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
        /// Gets or sets the allowed values for the property (for enum-like properties).
        /// </summary>
        public List<string> AllowedValues
        {
            get => _allowedValues;
            set
            {
                if (_allowedValues != value)
                {
                    _allowedValues = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the property description.
        /// </summary>
        public string Description
        {
            get => _description;
            set
            {
                if (_description != value)
                {
                    _description = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the property category.
        /// </summary>
        public string Category
        {
            get => _category;
            set
            {
                if (_category != value)
                {
                    _category = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Event triggered when a property value changes.
        /// </summary>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// Raises the PropertyChanged event.
        /// </summary>
        /// <param name="propertyName">Name of the property that changed.</param>
        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
