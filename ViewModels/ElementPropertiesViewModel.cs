using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using TNP.App.Models;

namespace TNP.App.ViewModels
{
    /// <summary>
    /// ViewModel for the element properties panel.
    /// </summary>
    public class ElementPropertiesViewModel : INotifyPropertyChanged
    {
        private string _selectionName = string.Empty;
        private string _selectionType = string.Empty;
        private bool _hasSelection;
        private KeyValuePair<string, object> _selectedProperty = new KeyValuePair<string, object>(string.Empty, new object());

        /// <summary>
        /// Gets or sets the name of the selected element.
        /// </summary>
        public string SelectionName
        {
            get => _selectionName;
            set
            {
                if (_selectionName != value)
                {
                    _selectionName = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the type of the selected element.
        /// </summary>
        public string SelectionType
        {
            get => _selectionType;
            set
            {
                if (_selectionType != value)
                {
                    _selectionType = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether there is a selection.
        /// </summary>
        public bool HasSelection
        {
            get => _hasSelection;
            set
            {
                if (_hasSelection != value)
                {
                    _hasSelection = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the selected property.
        /// </summary>
        public KeyValuePair<string, object> SelectedProperty
        {
            get => _selectedProperty;
            set
            {
                if (!Equals(_selectedProperty, value))
                {
                    _selectedProperty = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets the collection of properties for the selected element.
        /// </summary>
        public ObservableCollection<KeyValuePair<string, object>> Properties { get; } = new ObservableCollection<KeyValuePair<string, object>>();

        /// <summary>
        /// Sets the selected network component.
        /// </summary>
        /// <param name="component">The component.</param>
        public void SetSelectedNetworkComponent(NetworkComponent? component)
        {
            Properties.Clear();
            
            if (component == null)
            {
                SelectionName = string.Empty;
                SelectionType = string.Empty;
                HasSelection = false;
                return;
            }
            
            SelectionName = component.Name;
            SelectionType = component.ElementType.ToString();
            HasSelection = true;
            
            // Add basic properties
            Properties.Add(new KeyValuePair<string, object>("Id", component.Id));
            Properties.Add(new KeyValuePair<string, object>("Name", component.Name));
            Properties.Add(new KeyValuePair<string, object>("Description", component.Description));
            Properties.Add(new KeyValuePair<string, object>("Position X", component.Position.X));
            Properties.Add(new KeyValuePair<string, object>("Position Y", component.Position.Y));
            
            // Add component-specific properties
            if (component.Properties != null)
            {
                foreach (var property in component.Properties)
                {
                    Properties.Add(property);
                }
            }
        }

        /// <summary>
        /// Sets the selected connection.
        /// </summary>
        /// <param name="connection">The connection.</param>
        public void SetSelectedConnection(Connection? connection)
        {
            Properties.Clear();
            
            if (connection == null)
            {
                SelectionName = string.Empty;
                SelectionType = string.Empty;
                HasSelection = false;
                return;
            }
            
            SelectionName = connection.Label;
            SelectionType = $"Connection ({connection.ConnectionType})";
            HasSelection = true;
            
            // Add basic properties
            Properties.Add(new KeyValuePair<string, object>("Id", connection.Id));
            Properties.Add(new KeyValuePair<string, object>("Label", connection.Label));
            Properties.Add(new KeyValuePair<string, object>("Type", connection.ConnectionType));
            Properties.Add(new KeyValuePair<string, object>("Source", connection.Source?.Name ?? "None"));
            Properties.Add(new KeyValuePair<string, object>("Target", connection.Target?.Name ?? "None"));
            Properties.Add(new KeyValuePair<string, object>("Is Active", connection.IsActive));
            Properties.Add(new KeyValuePair<string, object>("Bandwidth", connection.Bandwidth));
            Properties.Add(new KeyValuePair<string, object>("Latency", connection.Latency));
            Properties.Add(new KeyValuePair<string, object>("Packet Loss", connection.PacketLoss));
        }

        /// <summary>
        /// Updates the property value.
        /// </summary>
        /// <param name="propertyName">The property name.</param>
        /// <param name="value">The new value.</param>
        public void UpdatePropertyValue(string propertyName, object value)
        {
            // Find the property in the collection
            for (int i = 0; i < Properties.Count; i++)
            {
                if (Properties[i].Key == propertyName)
                {
                    // Replace with the new value
                    Properties[i] = new KeyValuePair<string, object>(propertyName, value);
                    break;
                }
            }
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