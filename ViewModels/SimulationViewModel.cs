using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using TNP.App.Models;

namespace TNP.App.ViewModels
{
    /// <summary>
    /// Represents a simulation scenario.
    /// </summary>
    public class SimulationScenario
    {
        /// <summary>
        /// Gets or sets the name of the scenario.
        /// </summary>
        public string Name { get; set; } = string.Empty;
        
        /// <summary>
        /// Gets or sets the description of the scenario.
        /// </summary>
        public string Description { get; set; } = string.Empty;
    }
    
    /// <summary>
    /// ViewModel for the simulation view.
    /// </summary>
    public class SimulationViewModel : INotifyPropertyChanged
    {
        private bool _isSimulationRunning;
        private double _simulationSpeed = 1.0;
        private string _statusMessage = "Ready";
        private TimeSpan _simulationTime = TimeSpan.Zero;
        private int _packetCount;
        private int _errorCount;
        private NetworkDesignerViewModel? _networkDesigner;
        private SimulationScenario? _selectedScenario;

        /// <summary>
        /// Gets or sets the network designer view model.
        /// </summary>
        public NetworkDesignerViewModel? NetworkDesigner
        {
            get => _networkDesigner;
            set
            {
                if (_networkDesigner != value)
                {
                    _networkDesigner = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether the simulation is running.
        /// </summary>
        public bool IsSimulationRunning
        {
            get => _isSimulationRunning;
            set
            {
                if (_isSimulationRunning != value)
                {
                    _isSimulationRunning = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the simulation speed.
        /// </summary>
        public double SimulationSpeed
        {
            get => _simulationSpeed;
            set
            {
                if (_simulationSpeed != value)
                {
                    _simulationSpeed = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the status message.
        /// </summary>
        public string StatusMessage
        {
            get => _statusMessage;
            set
            {
                if (_statusMessage != value)
                {
                    _statusMessage = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the simulation time.
        /// </summary>
        public TimeSpan SimulationTime
        {
            get => _simulationTime;
            set
            {
                if (_simulationTime != value)
                {
                    _simulationTime = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the packet count.
        /// </summary>
        public int PacketCount
        {
            get => _packetCount;
            set
            {
                if (_packetCount != value)
                {
                    _packetCount = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the error count.
        /// </summary>
        public int ErrorCount
        {
            get => _errorCount;
            set
            {
                if (_errorCount != value)
                {
                    _errorCount = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the selected scenario.
        /// </summary>
        public SimulationScenario? SelectedScenario
        {
            get => _selectedScenario;
            set
            {
                if (_selectedScenario != value)
                {
                    _selectedScenario = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets the simulation scenarios.
        /// </summary>
        public ObservableCollection<SimulationScenario> SimulationScenarios { get; } = new ObservableCollection<SimulationScenario>();

        /// <summary>
        /// Initializes a new instance of the <see cref="SimulationViewModel"/> class.
        /// </summary>
        public SimulationViewModel()
        {
            // Add some example simulation scenarios
            SimulationScenarios.Add(new SimulationScenario { Name = "Normal Traffic", Description = "Simulates normal network traffic patterns" });
            SimulationScenarios.Add(new SimulationScenario { Name = "High Load", Description = "Simulates high network load conditions" });
            SimulationScenarios.Add(new SimulationScenario { Name = "Failure Scenario", Description = "Simulates network component failures" });
            SimulationScenarios.Add(new SimulationScenario { Name = "Security Test", Description = "Simulates security threats and responses" });
            
            SelectedScenario = SimulationScenarios[0];
        }

        /// <summary>
        /// Sets the network designer view model.
        /// </summary>
        /// <param name="networkDesigner">The network designer view model.</param>
        public void SetNetworkDesigner(NetworkDesignerViewModel networkDesigner)
        {
            NetworkDesigner = networkDesigner;
        }

        /// <summary>
        /// Starts the simulation.
        /// </summary>
        public void StartSimulation()
        {
            if (IsSimulationRunning)
                return;

            IsSimulationRunning = true;
            StatusMessage = $"Running simulation: {SelectedScenario?.Name}";
            // Additional simulation startup logic would go here
        }

        /// <summary>
        /// Stops the simulation.
        /// </summary>
        public void StopSimulation()
        {
            if (!IsSimulationRunning)
                return;

            IsSimulationRunning = false;
            StatusMessage = "Simulation stopped";
            // Additional simulation stop logic would go here
        }

        /// <summary>
        /// Resets the simulation.
        /// </summary>
        public void ResetSimulation()
        {
            SimulationTime = TimeSpan.Zero;
            PacketCount = 0;
            ErrorCount = 0;
            StatusMessage = "Simulation reset";
            // Additional simulation reset logic would go here
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