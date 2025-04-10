using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using TNP.App.Models;

namespace TNP.App.Services
{
    /// <summary>
    /// Interface for the network configuration service.
    /// </summary>
    public interface INetworkConfigService
    {
        /// <summary>
        /// Saves a network configuration to the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to save to.</param>
        /// <param name="elements">The network elements to save.</param>
        /// <param name="connections">The connections to save.</param>
        /// <returns>A task representing the asynchronous operation.</returns>
        Task SaveConfigurationAsync(string filePath, IEnumerable<NetworkComponent> elements, IEnumerable<Connection> connections);

        /// <summary>
        /// Loads a network configuration from the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to load from.</param>
        /// <returns>A tuple containing the loaded network elements and connections.</returns>
        Task<(IEnumerable<NetworkElement> Elements, IEnumerable<Connection> Connections)> LoadConfigurationAsync(string filePath);

        /// <summary>
        /// Exports a network configuration to the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to export to.</param>
        /// <param name="elements">The network elements to export.</param>
        /// <param name="connections">The connections to export.</param>
        /// <returns>A task representing the asynchronous operation.</returns>
        Task ExportConfigurationAsync(string filePath, IEnumerable<NetworkComponent> elements, IEnumerable<Connection> connections);

        /// <summary>
        /// Imports a network configuration from the specified file path.
        /// </summary>
        /// <param name="filePath">The file path to import from.</param>
        /// <returns>A tuple containing the imported network elements and connections.</returns>
        Task<(IEnumerable<NetworkElement> Elements, IEnumerable<Connection> Connections)> ImportConfigurationAsync(string filePath);

        /// <summary>
        /// Creates a new network configuration.
        /// </summary>
        /// <returns>A tuple containing the new network elements and connections.</returns>
        (IEnumerable<NetworkElement> Elements, IEnumerable<Connection> Connections) CreateNewConfiguration();

        /// <summary>
        /// Validates a network configuration.
        /// </summary>
        /// <param name="elements">The network elements to validate.</param>
        /// <param name="connections">The connections to validate.</param>
        /// <returns>A list of validation errors, if any.</returns>
        IEnumerable<string> ValidateConfiguration(IEnumerable<NetworkComponent> elements, IEnumerable<Connection> connections);

        /// <summary>
        /// Event raised when a configuration is saved.
        /// </summary>
        event EventHandler<string>? ConfigurationSaved;

        /// <summary>
        /// Event raised when a configuration is loaded.
        /// </summary>
        event EventHandler<string>? ConfigurationLoaded;
    }
}
