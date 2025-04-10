using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using TNP.App.Models;

namespace TNP.App.Services
{
    /// <summary>
    /// Service for managing network configurations.
    /// </summary>
    public class NetworkConfigService : INetworkConfigService
    {
        private readonly string _configFolder;
        
        /// <summary>
        /// Event raised when a configuration is saved.
        /// </summary>
        public event EventHandler<string>? ConfigurationSaved;
        
        /// <summary>
        /// Event raised when a configuration is loaded.
        /// </summary>
        public event EventHandler<string>? ConfigurationLoaded;

        /// <summary>
        /// Initializes a new instance of the <see cref="NetworkConfigService"/> class.
        /// </summary>
        public NetworkConfigService()
        {
            _configFolder = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "TNP",
                "Configurations");
            
            Directory.CreateDirectory(_configFolder);
        }

        /// <summary>
        /// Saves a network configuration to the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to save to.</param>
        /// <param name="elements">The network elements to save.</param>
        /// <param name="connections">The connections to save.</param>
        /// <returns>A task representing the asynchronous operation.</returns>
        public async Task SaveConfigurationAsync(string filePath, IEnumerable<NetworkComponent> elements, IEnumerable<Connection> connections)
        {
            var configPath = GetConfigPath(Path.GetFileName(filePath));
            var config = new NetworkConfigData
            {
                Elements = elements.Select(e => new NetworkElementData
                {
                    Id = e.Id,
                    Name = e.Name,
                    Description = e.Description,
                    Position = new Point2D { X = e.Position.X, Y = e.Position.Y },
                    Type = e.ElementType.ToString(),
                    Properties = e.Properties.ToDictionary(kvp => kvp.Key, kvp => kvp.Value)
                }).ToList(),
                
                Connections = connections.Select(c => new ConnectionData
                {
                    Id = c.Id,
                    SourceId = c.Source?.Id ?? Guid.Empty,
                    TargetId = c.Target?.Id ?? Guid.Empty,
                    Label = c.Label,
                    ConnectionType = c.ConnectionType.ToString(),
                    IsActive = c.IsActive,
                    Bandwidth = c.Bandwidth,
                    Latency = c.Latency,
                    PacketLoss = c.PacketLoss
                }).ToList()
            };

            var json = JsonSerializer.Serialize(config, new JsonSerializerOptions { WriteIndented = true });
            await File.WriteAllTextAsync(configPath, json);
            ConfigurationSaved?.Invoke(this, filePath);
        }

        /// <summary>
        /// Loads a network configuration from the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to load from.</param>
        /// <returns>A tuple containing the loaded network elements and connections.</returns>
        public async Task<(IEnumerable<NetworkElement> Elements, IEnumerable<Connection> Connections)> LoadConfigurationAsync(string filePath)
        {
            var configPath = GetConfigPath(Path.GetFileName(filePath));
            if (!File.Exists(configPath))
            {
                throw new FileNotFoundException($"Configuration file not found: {filePath}");
            }

            var json = await File.ReadAllTextAsync(configPath);
            var config = JsonSerializer.Deserialize<NetworkConfigData>(json);

            if (config == null)
            {
                throw new InvalidOperationException("Failed to deserialize configuration");
            }

            var elements = new List<NetworkElement>();
            var elementMap = new Dictionary<Guid, NetworkElement>();

            // Create elements
            foreach (var elementData in config.Elements)
            {
                NetworkElement element = CreateNetworkElement(elementData);
                elements.Add(element);
                elementMap.Add(element.Id, element);
            }

            // Create connections
            var connections = new List<Connection>();
            foreach (var connectionData in config.Connections)
            {
                if (elementMap.TryGetValue(connectionData.SourceId, out var source) &&
                    elementMap.TryGetValue(connectionData.TargetId, out var target))
                {
                    var connectionType = Enum.Parse<ConnectionType>(connectionData.ConnectionType);
                    var connection = new Connection(source, target, connectionType)
                    {
                        Id = connectionData.Id,
                        Label = connectionData.Label,
                        IsActive = connectionData.IsActive,
                        Bandwidth = connectionData.Bandwidth,
                        Latency = connectionData.Latency,
                        PacketLoss = connectionData.PacketLoss
                    };
                    
                    connections.Add(connection);
                    
                    // Add the connection to the elements
                    source.AddConnection(connection);
                    target.AddConnection(connection);
                }
            }

            ConfigurationLoaded?.Invoke(this, filePath);
            return (elements, connections);
        }

        /// <summary>
        /// Exports a network configuration to the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to export to.</param>
        /// <param name="elements">The network elements to export.</param>
        /// <param name="connections">The connections to export.</param>
        /// <returns>A task representing the asynchronous operation.</returns>
        public async Task ExportConfigurationAsync(string filePath, IEnumerable<NetworkComponent> elements, IEnumerable<Connection> connections)
        {
            var config = new NetworkConfigData
            {
                Elements = elements.Select(e => new NetworkElementData
                {
                    Id = e.Id,
                    Name = e.Name,
                    Description = e.Description,
                    Position = new Point2D { X = e.Position.X, Y = e.Position.Y },
                    Type = e.ElementType.ToString(),
                    Properties = e.Properties.ToDictionary(kvp => kvp.Key, kvp => kvp.Value)
                }).ToList(),
                
                Connections = connections.Select(c => new ConnectionData
                {
                    Id = c.Id,
                    SourceId = c.Source?.Id ?? Guid.Empty,
                    TargetId = c.Target?.Id ?? Guid.Empty,
                    Label = c.Label,
                    ConnectionType = c.ConnectionType.ToString(),
                    IsActive = c.IsActive,
                    Bandwidth = c.Bandwidth,
                    Latency = c.Latency,
                    PacketLoss = c.PacketLoss
                }).ToList()
            };

            var json = JsonSerializer.Serialize(config, new JsonSerializerOptions { WriteIndented = true });
            await File.WriteAllTextAsync(filePath, json);
        }

        /// <summary>
        /// Imports a network configuration from the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to import from.</param>
        /// <returns>A tuple containing the imported network elements and connections.</returns>
        public async Task<(IEnumerable<NetworkElement> Elements, IEnumerable<Connection> Connections)> ImportConfigurationAsync(string filePath)
        {
            if (!File.Exists(filePath))
            {
                throw new FileNotFoundException($"Configuration file not found: {filePath}");
            }

            var json = await File.ReadAllTextAsync(filePath);
            var config = JsonSerializer.Deserialize<NetworkConfigData>(json);

            if (config == null)
            {
                throw new InvalidOperationException("Failed to deserialize configuration");
            }

            var elements = new List<NetworkElement>();
            var elementMap = new Dictionary<Guid, NetworkElement>();

            // Create elements
            foreach (var elementData in config.Elements)
            {
                NetworkElement element = CreateNetworkElement(elementData);
                elements.Add(element);
                elementMap.Add(element.Id, element);
            }

            // Create connections
            var connections = new List<Connection>();
            foreach (var connectionData in config.Connections)
            {
                if (elementMap.TryGetValue(connectionData.SourceId, out var source) &&
                    elementMap.TryGetValue(connectionData.TargetId, out var target))
                {
                    var connectionType = Enum.Parse<ConnectionType>(connectionData.ConnectionType);
                    var connection = new Connection(source, target, connectionType)
                    {
                        Id = connectionData.Id,
                        Label = connectionData.Label,
                        IsActive = connectionData.IsActive,
                        Bandwidth = connectionData.Bandwidth,
                        Latency = connectionData.Latency,
                        PacketLoss = connectionData.PacketLoss
                    };
                    
                    connections.Add(connection);
                    
                    // Add the connection to the elements
                    source.AddConnection(connection);
                    target.AddConnection(connection);
                }
            }

            return (elements, connections);
        }

        /// <summary>
        /// Creates a new network configuration.
        /// </summary>
        /// <returns>A tuple containing the new network elements and connections.</returns>
        public (IEnumerable<NetworkElement> Elements, IEnumerable<Connection> Connections) CreateNewConfiguration()
        {
            return (new List<NetworkElement>(), new List<Connection>());
        }

        /// <summary>
        /// Validates a network configuration.
        /// </summary>
        /// <param name="elements">The network elements to validate.</param>
        /// <param name="connections">The connections to validate.</param>
        /// <returns>A list of validation errors, if any.</returns>
        public IEnumerable<string> ValidateConfiguration(IEnumerable<NetworkComponent> elements, IEnumerable<Connection> connections)
        {
            var result = ValidateConfiguration(elements.Cast<NetworkElement>(), connections);
            return result.Errors;
        }

        /// <summary>
        /// Gets a list of available configuration filenames.
        /// </summary>
        /// <returns>A list of configuration filenames.</returns>
        public List<string> GetAvailableConfigurations()
        {
            var files = Directory.GetFiles(_configFolder, "*.tnp");
            return files.Select(Path.GetFileNameWithoutExtension).ToList();
        }

        /// <summary>
        /// Validates a network configuration.
        /// </summary>
        /// <param name="elements">The network elements to validate.</param>
        /// <param name="connections">The connections to validate.</param>
        /// <returns>A tuple containing a boolean indicating if validation passed and a list of errors.</returns>
        public (bool IsValid, List<string> Errors) ValidateConfiguration(
            IEnumerable<NetworkElement> elements, 
            IEnumerable<Connection> connections)
        {
            var errors = new List<string>();
            
            // Check if there are any elements
            if (!elements.Any())
            {
                errors.Add("The network configuration is empty");
                return (false, errors);
            }
            
            // Check if there are any connections
            if (!connections.Any())
            {
                errors.Add("The network has no connections");
                return (false, errors);
            }

            // Check for elements with no connections
            var elementsWithNoConnections = elements.Where(e => !connections.Any(c => 
                c.Source == e || c.Target == e
            )).ToList();
            
            if (elementsWithNoConnections.Any())
            {
                errors.Add($"The following elements have no connections: {string.Join(", ", elementsWithNoConnections.Select(e => e.Name))}");
            }
            
            // Check for invalid connections (source or target missing or not in the configuration)
            foreach (var connection in connections)
            {
                if (connection.Source == null || connection.Target == null)
                {
                    errors.Add($"Invalid connection: {connection.Label} - source or target is missing");
                }
                else if (!elements.Contains(connection.Source) || !elements.Contains(connection.Target))
                {
                    errors.Add($"Invalid connection: {connection.Label} - source or target is not in the configuration");
                }
            }
            
            // Check for element configuration errors
            foreach (var element in elements)
            {
                if (element is NetworkComponent component)
                {
                    if (!component.ValidateConfiguration(out var errorMessage))
                    {
                        errors.Add($"Invalid configuration for {element.Name}: {errorMessage}");
                    }
                }
            }
            
            return (errors.Count == 0, errors);
        }

        /// <summary>
        /// Creates a network element from element data.
        /// </summary>
        /// <param name="elementData">The element data.</param>
        /// <returns>A network element.</returns>
        private NetworkElement CreateNetworkElement(NetworkElementData elementData)
        {
            NetworkComponent element;
            
            switch (elementData.Type)
            {
                case "Router":
                    element = new Router();
                    break;
                case "Switch":
                    element = new Switch();
                    break;
                case "Server":
                    element = new Server();
                    break;
                case "Computer":
                    element = new Computer();
                    break;
                case "Modem":
                    element = new Modem();
                    break;
                case "Printer":
                    element = new Printer();
                    break;
                case "Firewall":
                    element = new Firewall();
                    break;
                case "AccessPoint":
                    element = new AccessPoint();
                    break;
                default:
                    throw new ArgumentException($"Unknown element type: {elementData.Type}");
            }

            element.Id = elementData.Id;
            element.Name = elementData.Name;
            element.Description = elementData.Description;
            element.Position = new System.Windows.Point(elementData.Position.X, elementData.Position.Y);
            
            // Set properties
            if (elementData.Properties != null)
            {
                foreach (var property in elementData.Properties)
                {
                    element.Properties[property.Key] = property.Value;
                }
            }
            
            return element;
        }

        /// <summary>
        /// Gets the full path for a configuration file.
        /// </summary>
        /// <param name="filename">The filename.</param>
        /// <returns>The full path.</returns>
        private string GetConfigPath(string filename)
        {
            // Ensure filename has the correct extension
            if (!filename.EndsWith(".tnp", StringComparison.OrdinalIgnoreCase))
            {
                filename += ".tnp";
            }
            
            return Path.Combine(_configFolder, filename);
        }
    }

    /// <summary>
    /// Class for serializing/deserializing network configuration data.
    /// </summary>
    public class NetworkConfigData
    {
        /// <summary>
        /// Gets or sets the list of network elements.
        /// </summary>
        public List<NetworkElementData> Elements { get; set; } = new List<NetworkElementData>();
        
        /// <summary>
        /// Gets or sets the list of connections.
        /// </summary>
        public List<ConnectionData> Connections { get; set; } = new List<ConnectionData>();
    }

    /// <summary>
    /// Class for serializing/deserializing network element data.
    /// </summary>
    public class NetworkElementData
    {
        /// <summary>
        /// Gets or sets the element's unique identifier.
        /// </summary>
        public Guid Id { get; set; }
        
        /// <summary>
        /// Gets or sets the element's name.
        /// </summary>
        public string Name { get; set; } = string.Empty;
        
        /// <summary>
        /// Gets or sets the element's description.
        /// </summary>
        public string Description { get; set; } = string.Empty;
        
        /// <summary>
        /// Gets or sets the element's position.
        /// </summary>
        public Point2D Position { get; set; } = new Point2D();
        
        /// <summary>
        /// Gets or sets the element's type.
        /// </summary>
        public string Type { get; set; } = string.Empty;
        
        /// <summary>
        /// Gets or sets the element's properties.
        /// </summary>
        public Dictionary<string, object> Properties { get; set; } = new Dictionary<string, object>();
    }

    /// <summary>
    /// Class for serializing/deserializing connection data.
    /// </summary>
    public class ConnectionData
    {
        /// <summary>
        /// Gets or sets the connection's unique identifier.
        /// </summary>
        public Guid Id { get; set; }
        
        /// <summary>
        /// Gets or sets the source element's identifier.
        /// </summary>
        public Guid SourceId { get; set; }
        
        /// <summary>
        /// Gets or sets the target element's identifier.
        /// </summary>
        public Guid TargetId { get; set; }
        
        /// <summary>
        /// Gets or sets the connection's label.
        /// </summary>
        public string Label { get; set; } = string.Empty;
        
        /// <summary>
        /// Gets or sets the connection type.
        /// </summary>
        public string ConnectionType { get; set; } = "Ethernet";
        
        /// <summary>
        /// Gets or sets a value indicating whether the connection is active.
        /// </summary>
        public bool IsActive { get; set; } = true;
        
        /// <summary>
        /// Gets or sets the connection's bandwidth in Mbps.
        /// </summary>
        public double Bandwidth { get; set; } = 1000.0;
        
        /// <summary>
        /// Gets or sets the connection's latency in ms.
        /// </summary>
        public double Latency { get; set; } = 1.0;
        
        /// <summary>
        /// Gets or sets the connection's packet loss percentage.
        /// </summary>
        public double PacketLoss { get; set; } = 0.0;
    }

    /// <summary>
    /// Represents a 2D point for serialization.
    /// </summary>
    public class Point2D
    {
        /// <summary>
        /// Gets or sets the X coordinate.
        /// </summary>
        public double X { get; set; }
        
        /// <summary>
        /// Gets or sets the Y coordinate.
        /// </summary>
        public double Y { get; set; }
    }
}
