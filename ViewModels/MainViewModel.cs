using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using TNP.App.Services;

namespace TNP.App.ViewModels
{
    /// <summary>
    /// Main view model that coordinates all other view models.
    /// </summary>
    public class MainViewModel : INotifyPropertyChanged
    {
        private readonly INetworkConfigService _networkConfigService;
        private int _elementCount;
        private int _connectionCount;

        /// <summary>
        /// Gets the network designer view model.
        /// </summary>
        public NetworkDesignerViewModel NetworkDesigner { get; }

        /// <summary>
        /// Gets the element properties view model.
        /// </summary>
        public ElementPropertiesViewModel ElementProperties { get; }

        /// <summary>
        /// Gets the simulation view model.
        /// </summary>
        public SimulationViewModel Simulation { get; }

        /// <summary>
        /// Gets the plugin view model.
        /// </summary>
        public PluginViewModel Plugin { get; }

        /// <summary>
        /// Gets or sets the element count.
        /// </summary>
        public int ElementCount
        {
            get => _elementCount;
            set
            {
                if (_elementCount != value)
                {
                    _elementCount = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the connection count.
        /// </summary>
        public int ConnectionCount
        {
            get => _connectionCount;
            set
            {
                if (_connectionCount != value)
                {
                    _connectionCount = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="MainViewModel"/> class.
        /// </summary>
        /// <param name="networkDesigner">The network designer view model.</param>
        /// <param name="elementProperties">The element properties view model.</param>
        /// <param name="simulation">The simulation view model.</param>
        /// <param name="plugin">The plugin view model.</param>
        /// <param name="networkConfigService">The network configuration service.</param>
        public MainViewModel(
            NetworkDesignerViewModel networkDesigner,
            ElementPropertiesViewModel elementProperties,
            SimulationViewModel simulation,
            PluginViewModel plugin,
            INetworkConfigService networkConfigService)
        {
            NetworkDesigner = networkDesigner ?? throw new ArgumentNullException(nameof(networkDesigner));
            ElementProperties = elementProperties ?? throw new ArgumentNullException(nameof(elementProperties));
            Simulation = simulation ?? throw new ArgumentNullException(nameof(simulation));
            Plugin = plugin ?? throw new ArgumentNullException(nameof(plugin));
            _networkConfigService = networkConfigService ?? throw new ArgumentNullException(nameof(networkConfigService));

            // Connect the components
            NetworkDesigner.ComponentSelected += (_, component) => ElementProperties.SetSelectedNetworkComponent(component);
            NetworkDesigner.ConnectionSelected += (_, connection) => ElementProperties.SetSelectedConnection(connection);
            
            // Update counts when collections change
            NetworkDesigner.Components.CollectionChanged += (_, _) => UpdateCounts();
            NetworkDesigner.Connections.CollectionChanged += (_, _) => UpdateCounts();
            
            // Initialize the simulation with the network designer
            Simulation.SetNetworkDesigner(NetworkDesigner);
            
            // Subscribe to network config events
            _networkConfigService.ConfigurationLoaded += (_, _) => UpdateCounts();
            _networkConfigService.ConfigurationSaved += (_, _) => { /* Handle save confirmation */ };
            
            // Initial count update
            UpdateCounts();
        }

        /// <summary>
        /// Updates the element and connection counts.
        /// </summary>
        private void UpdateCounts()
        {
            ElementCount = NetworkDesigner.Components.Count;
            ConnectionCount = NetworkDesigner.Connections.Count;
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