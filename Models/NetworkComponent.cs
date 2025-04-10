using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Linq;

namespace TNP.App.Models
{
    /// <summary>
    /// Base class for components in a network configuration.
    /// </summary>
    public abstract class NetworkComponent : NetworkElement
    {
        private Dictionary<string, object> _properties;

        /// <summary>
        /// Gets or sets the properties specific to this component type.
        /// </summary>
        public Dictionary<string, object> Properties
        {
            get => _properties;
            set
            {
                _properties = value;
                OnPropertyChanged();
            }
        }

        /// <summary>
        /// Gets the type name of the component.
        /// </summary>
        public abstract string ComponentType { get; }

        /// <summary>
        /// Gets the maximum number of connections this component can have.
        /// </summary>
        public abstract int MaxConnections { get; }

        /// <summary>
        /// Gets the element type based on the component type.
        /// </summary>
        public override NetworkElementType ElementType
        {
            get
            {
                return ComponentType switch
                {
                    "Router" => NetworkElementType.Router,
                    "Switch" => NetworkElementType.Switch,
                    "Server" => NetworkElementType.Server,
                    "Computer" => NetworkElementType.Computer,
                    "Modem" => NetworkElementType.Modem,
                    "Firewall" => NetworkElementType.Firewall,
                    "AccessPoint" => NetworkElementType.AccessPoint,
                    "Printer" => NetworkElementType.Printer,
                    _ => NetworkElementType.Generic
                };
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="NetworkComponent"/> class.
        /// </summary>
        protected NetworkComponent()
        {
            _properties = new Dictionary<string, object>();
            InitializeDefaultProperties();
        }

        /// <summary>
        /// Initializes the default properties for this component type.
        /// </summary>
        protected abstract void InitializeDefaultProperties();

        /// <summary>
        /// Validates the configuration of this network component.
        /// </summary>
        /// <param name="errorMessage">When this method returns, contains an error message if validation failed.</param>
        /// <returns>true if the configuration is valid; otherwise, false.</returns>
        public abstract bool ValidateConfiguration(out string errorMessage);

        /// <summary>
        /// Adds a connection to this component.
        /// </summary>
        /// <param name="connection">The connection to add.</param>
        /// <returns>true if the connection was successfully added; otherwise, false.</returns>
        public new bool AddConnection(Connection connection)
        {
            if (Connections.Count >= MaxConnections)
            {
                return false;
            }

            base.AddConnection(connection);
            return true;
        }

        /// <summary>
        /// Creates a deep copy of this network component.
        /// </summary>
        /// <returns>A new instance of a network component with the same properties.</returns>
        public abstract NetworkComponent Clone();
    }
}
