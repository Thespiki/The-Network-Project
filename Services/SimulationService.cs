using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using TNP.App.Models;

namespace TNP.App.Services
{
    /// <summary>
    /// Implementation of the simulation service.
    /// </summary>
    public class SimulationService : ISimulationService
    {
        private readonly List<NetworkElement> _elements = new List<NetworkElement>();
        private readonly List<Connection> _connections = new List<Connection>();
        private readonly List<LogEntry> _logEntries = new List<LogEntry>();
        private CancellationTokenSource? _cancellationTokenSource;
        private Task? _simulationTask;
        private double _simulationSpeed = 1.0;
        private double _simulationTime;
        private int _packetCount;
        private bool _isSimulationRunning;
        private readonly Random _random = new Random();

        /// <summary>
        /// Event raised when the log entries change.
        /// </summary>
        public event EventHandler? LogEntriesChanged;

        /// <summary>
        /// Event raised when the simulation state changes.
        /// </summary>
        public event EventHandler? SimulationStateChanged;

        /// <summary>
        /// Starts the simulation.
        /// </summary>
        public void StartSimulation()
        {
            if (_isSimulationRunning)
            {
                return;
            }

            _isSimulationRunning = true;
            _cancellationTokenSource = new CancellationTokenSource();
            var token = _cancellationTokenSource.Token;

            _simulationTask = Task.Run(() => RunSimulation(token), token);

            OnSimulationStateChanged();
        }

        /// <summary>
        /// Stops the simulation.
        /// </summary>
        public void StopSimulation()
        {
            if (!_isSimulationRunning)
            {
                return;
            }

            _cancellationTokenSource?.Cancel();
            _isSimulationRunning = false;

            OnSimulationStateChanged();
        }

        /// <summary>
        /// Resets the simulation.
        /// </summary>
        public void ResetSimulation()
        {
            StopSimulation();

            _simulationTime = 0;
            _packetCount = 0;
            _logEntries.Clear();

            OnLogEntriesChanged();
        }

        /// <summary>
        /// Sets the network elements for the simulation.
        /// </summary>
        /// <param name="elements">The network elements to simulate.</param>
        public void SetNetworkElements(IEnumerable<NetworkElement> elements)
        {
            _elements.Clear();
            _elements.AddRange(elements);
        }

        /// <summary>
        /// Sets the connections for the simulation.
        /// </summary>
        /// <param name="connections">The connections to simulate.</param>
        public void SetConnections(IEnumerable<Connection> connections)
        {
            _connections.Clear();
            _connections.AddRange(connections);
        }

        /// <summary>
        /// Sets the simulation speed.
        /// </summary>
        /// <param name="speed">The simulation speed factor.</param>
        public void SetSimulationSpeed(double speed)
        {
            _simulationSpeed = Math.Max(0.1, Math.Min(10.0, speed));
        }

        /// <summary>
        /// Gets the simulation time.
        /// </summary>
        /// <returns>The current simulation time in seconds.</returns>
        public double GetSimulationTime()
        {
            return _simulationTime;
        }

        /// <summary>
        /// Gets the packet count.
        /// </summary>
        /// <returns>The current packet count.</returns>
        public int GetPacketCount()
        {
            return _packetCount;
        }

        /// <summary>
        /// Gets the log entries.
        /// </summary>
        /// <returns>The current log entries.</returns>
        public IEnumerable<LogEntry> GetLogEntries()
        {
            return _logEntries.ToList();
        }

        /// <summary>
        /// Checks if the simulation is running.
        /// </summary>
        /// <returns>True if the simulation is running; otherwise false.</returns>
        public bool IsSimulationRunning()
        {
            return _isSimulationRunning;
        }

        /// <summary>
        /// Runs the simulation loop.
        /// </summary>
        /// <param name="cancellationToken">The cancellation token.</param>
        private async Task RunSimulation(CancellationToken cancellationToken)
        {
            // Add initial log entry
            AddLogEntry(LogEntry.Info("Simulation", "Simulation started"));

            while (!cancellationToken.IsCancellationRequested)
            {
                try
                {
                    // Update simulation time
                    _simulationTime += 0.1 * _simulationSpeed;

                    // Check for active connections and simulate packet transfer
                    foreach (var connection in _connections.Where(c => c.IsActive))
                    {
                        // Simulate packet transfer
                        if (_random.NextDouble() < 0.2 * _simulationSpeed)
                        {
                            SimulatePacketTransfer(connection);
                        }
                    }

                    // Simulate random events
                    if (_random.NextDouble() < 0.05 * _simulationSpeed)
                    {
                        SimulateRandomEvent();
                    }

                    await Task.Delay(TimeSpan.FromMilliseconds(100), cancellationToken);
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    AddLogEntry(LogEntry.Error("Simulation", $"Error in simulation: {ex.Message}", ex.ToString()));
                }
            }

            // Add simulation stopped log entry
            AddLogEntry(LogEntry.Info("Simulation", "Simulation stopped"));
        }

        /// <summary>
        /// Simulates a packet transfer between two network elements.
        /// </summary>
        /// <param name="connection">The connection to simulate packet transfer on.</param>
        private void SimulatePacketTransfer(Connection connection)
        {
            if (connection.Source == null || connection.Target == null)
            {
                return;
            }

            // Increment packet count
            _packetCount++;

            // Determine packet size in bytes (random between 64 and 1500)
            var packetSize = _random.Next(64, 1501);

            // Determine latency based on connection settings
            var latency = connection.Latency;

            // Apply packet loss (if any)
            var isLost = _random.NextDouble() < connection.PacketLoss / 100.0;

            if (isLost)
            {
                AddLogEntry(LogEntry.Warning("PacketLoss", 
                    $"Packet lost from {connection.Source.Name} to {connection.Target.Name}", 
                    $"Packet size: {packetSize} bytes, Connection type: {connection.ConnectionType}"));
            }
            else
            {
                // Log packet transfer
                AddLogEntry(LogEntry.Debug("PacketTransfer", 
                    $"Packet sent from {connection.Source.Name} to {connection.Target.Name}", 
                    $"Packet size: {packetSize} bytes, Latency: {latency} ms, Connection type: {connection.ConnectionType}"));
            }
        }

        /// <summary>
        /// Simulates a random network event.
        /// </summary>
        private void SimulateRandomEvent()
        {
            if (_elements.Count == 0)
            {
                return;
            }

            // Select a random element
            var element = _elements[_random.Next(_elements.Count)];

            // Determine the type of event
            var eventType = _random.Next(5);

            switch (eventType)
            {
                case 0:
                    // Link status change
                    var connections = _connections.Where(c => c.Source == element || c.Target == element).ToList();
                    if (connections.Count > 0)
                    {
                        var connection = connections[_random.Next(connections.Count)];
                        connection.IsActive = !connection.IsActive;
                        AddLogEntry(LogEntry.Info("LinkStatus", 
                            $"Connection {connection.Label} is now {(connection.IsActive ? "active" : "inactive")}"));
                    }
                    break;

                case 1:
                    // CPU usage spike
                    if (element.ElementType == NetworkElementType.Server)
                    {
                        var cpuUsage = _random.Next(50, 101);
                        var memory = 64; // Default value if memory can't be determined
                        
                        AddLogEntry(LogEntry.Info("ResourceUsage", 
                            $"CPU usage spike on {element.Name}", 
                            $"CPU usage: {cpuUsage}%, RAM: {memory} GB"));
                    }
                    else if (element.ElementType == NetworkElementType.Computer)
                    {
                        var cpuUsage = _random.Next(30, 91);
                        var memory = 16; // Default value if memory can't be determined
                        
                        AddLogEntry(LogEntry.Info("ResourceUsage", 
                            $"CPU usage spike on {element.Name}", 
                            $"CPU usage: {cpuUsage}%, RAM: {memory} GB"));
                    }
                    break;

                case 2:
                    // DHCP lease renewal
                    if (element.ElementType == NetworkElementType.Router)
                    {
                        AddLogEntry(LogEntry.Info("DHCP", 
                            $"DHCP lease renewal on {element.Name}", 
                            $"IP pool: 192.168.1.0/24"));
                    }
                    break;

                case 3:
                    // Bandwidth usage
                    var bandwidthUsage = _random.Next(1, 101);
                    AddLogEntry(LogEntry.Info("BandwidthUsage", 
                        $"Bandwidth usage on {element.Name}: {bandwidthUsage}%"));
                    break;

                case 4:
                    // Security event
                    if (_random.NextDouble() < 0.3)
                    {
                        AddLogEntry(LogEntry.Warning("Security", 
                            $"Suspicious connection attempt detected on {element.Name}", 
                            "Source IP: 192.168.1." + _random.Next(2, 255)));
                    }
                    else
                    {
                        AddLogEntry(LogEntry.Info("Security", 
                            $"Authentication successful on {element.Name}", 
                            "User: admin"));
                    }
                    break;
            }
        }

        /// <summary>
        /// Adds a log entry to the log entries list.
        /// </summary>
        /// <param name="logEntry">The log entry to add.</param>
        private void AddLogEntry(LogEntry logEntry)
        {
            lock (_logEntries)
            {
                _logEntries.Add(logEntry);

                // Keep only the last 1000 log entries
                while (_logEntries.Count > 1000)
                {
                    _logEntries.RemoveAt(0);
                }
            }

            OnLogEntriesChanged();
        }

        /// <summary>
        /// Raises the LogEntriesChanged event.
        /// </summary>
        protected virtual void OnLogEntriesChanged()
        {
            LogEntriesChanged?.Invoke(this, EventArgs.Empty);
        }

        /// <summary>
        /// Raises the SimulationStateChanged event.
        /// </summary>
        protected virtual void OnSimulationStateChanged()
        {
            SimulationStateChanged?.Invoke(this, EventArgs.Empty);
        }
    }
}
