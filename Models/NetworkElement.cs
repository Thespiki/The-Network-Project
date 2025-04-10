using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows;

namespace TNP.App.Models
{
    /// <summary>
    /// Base class for all network elements.
    /// </summary>
    public class NetworkElement : INotifyPropertyChanged
    {
        private string _name = string.Empty;
        private string _description = string.Empty;
        private Point _position;
        private bool _isSelected;
        private bool _isEnabled = true;
        private string _iconPath = string.Empty;

        /// <summary>
        /// Gets or sets the unique identifier of the network element.
        /// </summary>
        public Guid Id { get; set; } = Guid.NewGuid();

        /// <summary>
        /// Gets or sets the name of the network element.
        /// </summary>
        public string Name
        {
            get => _name;
            set
            {
                if (_name != value)
                {
                    _name = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the description of the network element.
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
        /// Gets or sets the position of the network element on the canvas.
        /// </summary>
        public Point Position
        {
            get => _position;
            set
            {
                if (_position != value)
                {
                    _position = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether this network element is selected.
        /// </summary>
        public bool IsSelected
        {
            get => _isSelected;
            set
            {
                if (_isSelected != value)
                {
                    _isSelected = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether this network element is enabled.
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
        /// Gets or sets the path to the icon representing this network element.
        /// </summary>
        public string IconPath
        {
            get => _iconPath;
            set
            {
                if (_iconPath != value)
                {
                    _iconPath = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets the connections associated with this network element.
        /// </summary>
        public ObservableCollection<Connection> Connections { get; } = new ObservableCollection<Connection>();

        /// <summary>
        /// Gets the type of the network element.
        /// </summary>
        public virtual NetworkElementType ElementType => NetworkElementType.Generic;

        /// <summary>
        /// Event raised when a property value changes.
        /// </summary>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// Adds a connection to this network element.
        /// </summary>
        /// <param name="connection">The connection to add.</param>
        public virtual void AddConnection(Connection connection)
        {
            if (!Connections.Contains(connection))
            {
                Connections.Add(connection);
            }
        }

        /// <summary>
        /// Removes a connection from this network element.
        /// </summary>
        /// <param name="connection">The connection to remove.</param>
        /// <returns>True if the connection was removed, false otherwise.</returns>
        public virtual bool RemoveConnection(Connection connection)
        {
            return Connections.Remove(connection);
        }

        /// <summary>
        /// Raises the PropertyChanged event.
        /// </summary>
        /// <param name="propertyName">The name of the property that changed.</param>
        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        /// <summary>
        /// Returns a string that represents the current object.
        /// </summary>
        /// <returns>A string that represents the current object.</returns>
        public override string ToString()
        {
            return $"{ElementType} - {Name}";
        }
    }

    /// <summary>
    /// Represents the type of a network element.
    /// </summary>
    public enum NetworkElementType
    {
        /// <summary>
        /// Generic network element.
        /// </summary>
        Generic,

        /// <summary>
        /// Router network element.
        /// </summary>
        Router,

        /// <summary>
        /// Switch network element.
        /// </summary>
        Switch,

        /// <summary>
        /// Server network element.
        /// </summary>
        Server,

        /// <summary>
        /// Computer network element.
        /// </summary>
        Computer,

        /// <summary>
        /// Modem network element.
        /// </summary>
        Modem,

        /// <summary>
        /// Firewall network element.
        /// </summary>
        Firewall,

        /// <summary>
        /// Access point network element.
        /// </summary>
        AccessPoint,

        /// <summary>
        /// Printer network element.
        /// </summary>
        Printer,

        /// <summary>
        /// Network-attached storage (NAS) network element.
        /// </summary>
        NAS,

        /// <summary>
        /// Cloud network element.
        /// </summary>
        Cloud,

        /// <summary>
        /// Network interface card (NIC) network element.
        /// </summary>
        NIC,

        /// <summary>
        /// Mobile device network element.
        /// </summary>
        MobileDevice,

        /// <summary>
        /// Internet of Things (IoT) device network element.
        /// </summary>
        IoTDevice,

        /// <summary>
        /// Load balancer network element.
        /// </summary>
        LoadBalancer
    }
}
