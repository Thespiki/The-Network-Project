using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.IO;
using TNP.App.Models;
using System.Threading.Tasks;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a network configuration containing elements and connections.
    /// </summary>
    public class NetworkConfiguration
    {
        private readonly List<NetworkElement> _elements;
        private readonly List<Connection> _connections;

        /// <summary>
        /// Initializes a new instance of the <see cref="NetworkConfiguration"/> class.
        /// </summary>
        public NetworkConfiguration()
        {
            _elements = new List<NetworkElement>();
            _connections = new List<Connection>();
        }

        /// <summary>
        /// Gets the elements in the configuration.
        /// </summary>
        public IReadOnlyList<NetworkElement> Elements => _elements.AsReadOnly();

        /// <summary>
        /// Gets the connections in the configuration.
        /// </summary>
        public IReadOnlyList<Connection> Connections => _connections.AsReadOnly();

        /// <summary>
        /// Adds an element to the configuration.
        /// </summary>
        /// <param name="element">The element to add.</param>
        public void AddElement(NetworkElement element)
        {
            if (element != null && !_elements.Contains(element))
            {
                _elements.Add(element);
            }
        }

        /// <summary>
        /// Removes an element from the configuration.
        /// </summary>
        /// <param name="element">The element to remove.</param>
        public void RemoveElement(NetworkElement element)
        {
            if (element != null)
            {
                // Remove any connections associated with this element
                var connectionsToRemove = _connections.Where(c => c.Source == element || c.Target == element).ToList();
                foreach (var connection in connectionsToRemove)
                {
                    _connections.Remove(connection);
                }

                _elements.Remove(element);
            }
        }

        /// <summary>
        /// Adds a connection to the configuration.
        /// </summary>
        /// <param name="connection">The connection to add.</param>
        public void AddConnection(Connection connection)
        {
            if (connection != null && !_connections.Contains(connection))
            {
                _connections.Add(connection);
            }
        }

        /// <summary>
        /// Removes a connection from the configuration.
        /// </summary>
        /// <param name="connection">The connection to remove.</param>
        public void RemoveConnection(Connection connection)
        {
            if (connection != null)
            {
                _connections.Remove(connection);
            }
        }
        
        /// <summary>
        /// Validates the network configuration.
        /// </summary>
        /// <param name="errorMessage">Output error message if validation fails.</param>
        /// <returns>True if the configuration is valid, false otherwise.</returns>
        public bool ValidateConfiguration(out string errorMessage)
        {
            return Validate(out errorMessage);
        }

        /// <summary>
        /// Clears all elements and connections from the configuration.
        /// </summary>
        public void Clear()
        {
            _elements.Clear();
            _connections.Clear();
        }

        /// <summary>
        /// Validates the network configuration.
        /// </summary>
        /// <param name="errorMessage">When this method returns, contains an error message if validation failed.</param>
        /// <returns>true if the configuration is valid; otherwise, false.</returns>
        public bool Validate(out string errorMessage)
        {
            // Check if there are any elements
            if (_elements.Count == 0)
            {
                errorMessage = "The network configuration is empty.";
                return false;
            }

            // Check if there are any connections
            if (_connections.Count == 0)
            {
                errorMessage = "The network configuration has no connections.";
                return false;
            }

            // Check for isolated elements (not connected to anything)
            var isolatedElements = _elements.Where(e => !_connections.Any(c => c.Source == e || c.Target == e)).ToList();
            if (isolatedElements.Any())
            {
                var elementNames = string.Join(", ", isolatedElements.Select(e => e.Name));
                errorMessage = $"The following elements are not connected to anything: {elementNames}.";
                return false;
            }

            // Check for any invalid connections
            foreach (var connection in _connections)
            {
                if (connection.Source == null || connection.Target == null)
                {
                    errorMessage = "Invalid connection: source or target is missing.";
                    return false;
                }

                if (!_elements.Contains(connection.Source) || !_elements.Contains(connection.Target))
                {
                    errorMessage = "Invalid connection: source or target is not in the configuration.";
                    return false;
                }
            }

            // Validate each element's configuration
            foreach (var element in _elements)
            {
                if (element is NetworkComponent component && !component.ValidateConfiguration(out var elementErrorMessage))
                {
                    errorMessage = $"Invalid configuration for {element.Name}: {elementErrorMessage}";
                    return false;
                }
            }

            // Check for connection cycles (not implemented for simplicity)

            errorMessage = string.Empty;
            return true;
        }

        /// <summary>
        /// Exports the network configuration to a JSON file.
        /// </summary>
        /// <param name="fileName">The name of the file to export to.</param>
        public async Task ExportToJsonAsync(string fileName)
        {
            var exportData = new NetworkConfigurationExport
            {
                Elements = _elements.Select(e => CreateElementData(e)).ToList(),
                Connections = _connections.Select(c => new ConnectionData
                {
                    Id = c.Id,
                    SourceId = c.Source?.Id ?? Guid.Empty,
                    TargetId = c.Target?.Id ?? Guid.Empty,
                    ConnectionType = c.ConnectionType,
                    Label = c.Label,
                    IsActive = c.IsActive,
                    Bandwidth = c.Bandwidth,
                    Latency = c.Latency,
                    PacketLoss = c.PacketLoss
                }).ToList()
            };

            var options = new JsonSerializerOptions
            {
                WriteIndented = true,
                DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
            };

            using var stream = File.Create(fileName);
            await JsonSerializer.SerializeAsync(stream, exportData, options);
        }

        /// <summary>
        /// Imports a network configuration from a JSON file.
        /// </summary>
        /// <param name="fileName">The name of the file to import from.</param>
        public async Task ImportFromJsonAsync(string fileName)
        {
            using var stream = File.OpenRead(fileName);
            var importData = await JsonSerializer.DeserializeAsync<NetworkConfigurationExport>(stream);

            if (importData == null)
            {
                throw new InvalidOperationException("Failed to deserialize the network configuration.");
            }

            Clear();

            // Create and add elements
            var elementDict = new Dictionary<Guid, NetworkElement>();
            foreach (var elementData in importData.Elements)
            {
                var element = CreateElementFromData(elementData);
                if (element != null)
                {
                    _elements.Add(element);
                    elementDict[element.Id] = element;
                }
            }

            // Create and add connections
            foreach (var connectionData in importData.Connections)
            {
                if (elementDict.TryGetValue(connectionData.SourceId, out var source) &&
                    elementDict.TryGetValue(connectionData.TargetId, out var target))
                {
                    var connection = new Connection(source, target, connectionData.ConnectionType)
                    {
                        Id = connectionData.Id,
                        Label = connectionData.Label,
                        IsActive = connectionData.IsActive,
                        Bandwidth = connectionData.Bandwidth,
                        Latency = connectionData.Latency,
                        PacketLoss = connectionData.PacketLoss
                    };

                    _connections.Add(connection);
                }
            }
        }

        /// <summary>
        /// Creates element data for export from a network element.
        /// </summary>
        /// <param name="element">The network element to create data from.</param>
        /// <returns>Element data for export.</returns>
        private ElementData CreateElementData(NetworkElement element)
        {
            var elementData = new ElementData
            {
                Id = element.Id,
                Name = element.Name,
                Type = element.ElementType.ToString()
            };

            var properties = new Dictionary<string, object>();

            if (element is NetworkComponent component)
            {
                foreach (var kvp in component.Properties)
                {
                    properties[kvp.Key] = kvp.Value;
                }
            }

            elementData.Properties = properties;
            return elementData;
        }

        /// <summary>
        /// Creates a network element from element data.
        /// </summary>
        /// <param name="elementData">The element data to create an element from.</param>
        /// <returns>A network element.</returns>
        private NetworkElement? CreateElementFromData(ElementData elementData)
        {
            NetworkElement? element = null;

            // Create the appropriate element type
            if (Enum.TryParse<NetworkElementType>(elementData.Type, out var elementType))
            {
                element = elementType switch
                {
                    NetworkElementType.Router => new Router(),
                    NetworkElementType.Switch => new Switch(),
                    NetworkElementType.Modem => new Modem(),
                    NetworkElementType.Server => new Server(),
                    NetworkElementType.Computer => new Computer(),
                    NetworkElementType.Printer => new Printer(),
                    NetworkElementType.Firewall => new Firewall(),
                    NetworkElementType.AccessPoint => new AccessPoint(),
                    _ => null
                };
            }

            if (element == null)
            {
                return null;
            }

            element.Id = elementData.Id;
            element.Name = elementData.Name;

            // Set properties for NetworkComponent
            if (element is NetworkComponent component && elementData.Properties != null)
            {
                foreach (var kvp in elementData.Properties)
                {
                    component.Properties[kvp.Key] = kvp.Value;
                }
            }

            return element;
        }
    }

    /// <summary>
    /// Class for exporting a network configuration.
    /// </summary>
    public class NetworkConfigurationExport
    {
        /// <summary>
        /// Gets or sets the elements in the configuration.
        /// </summary>
        public List<ElementData> Elements { get; set; } = new();

        /// <summary>
        /// Gets or sets the connections in the configuration.
        /// </summary>
        public List<ConnectionData> Connections { get; set; } = new();
    }

    /// <summary>
    /// Data for a network element.
    /// </summary>
    public class ElementData
    {
        /// <summary>
        /// Gets or sets the unique identifier of the element.
        /// </summary>
        public Guid Id { get; set; }

        /// <summary>
        /// Gets or sets the name of the element.
        /// </summary>
        public string Name { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the type of the element.
        /// </summary>
        public string Type { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the properties of the element.
        /// </summary>
        public Dictionary<string, object>? Properties { get; set; }
    }

    /// <summary>
    /// Data for a connection.
    /// </summary>
    public class ConnectionData
    {
        /// <summary>
        /// Gets or sets the unique identifier of the connection.
        /// </summary>
        public Guid Id { get; set; }

        /// <summary>
        /// Gets or sets the identifier of the source element.
        /// </summary>
        public Guid SourceId { get; set; }

        /// <summary>
        /// Gets or sets the identifier of the target element.
        /// </summary>
        public Guid TargetId { get; set; }

        /// <summary>
        /// Gets or sets the type of the connection.
        /// </summary>
        public ConnectionType ConnectionType { get; set; }

        /// <summary>
        /// Gets or sets the label of the connection.
        /// </summary>
        public string Label { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets a value indicating whether the connection is active.
        /// </summary>
        public bool IsActive { get; set; } = true;

        /// <summary>
        /// Gets or sets the bandwidth of the connection in megabits per second.
        /// </summary>
        public double Bandwidth { get; set; } = 1000.0;

        /// <summary>
        /// Gets or sets the latency of the connection in milliseconds.
        /// </summary>
        public double Latency { get; set; } = 1.0;

        /// <summary>
        /// Gets or sets the packet loss of the connection as a percentage.
        /// </summary>
        public double PacketLoss { get; set; } = 0.0;
    }
}
