using System;
using System.Collections.Generic;
using TNP.App.Models;

namespace TNP.App.Services
{
    /// <summary>
    /// Interface for the simulation service.
    /// </summary>
    public interface ISimulationService
    {
        /// <summary>
        /// Starts the simulation.
        /// </summary>
        void StartSimulation();

        /// <summary>
        /// Stops the simulation.
        /// </summary>
        void StopSimulation();

        /// <summary>
        /// Resets the simulation.
        /// </summary>
        void ResetSimulation();

        /// <summary>
        /// Sets the network elements for the simulation.
        /// </summary>
        /// <param name="elements">The network elements to simulate.</param>
        void SetNetworkElements(IEnumerable<NetworkElement> elements);

        /// <summary>
        /// Sets the connections for the simulation.
        /// </summary>
        /// <param name="connections">The connections to simulate.</param>
        void SetConnections(IEnumerable<Connection> connections);

        /// <summary>
        /// Sets the simulation speed.
        /// </summary>
        /// <param name="speed">The simulation speed factor.</param>
        void SetSimulationSpeed(double speed);

        /// <summary>
        /// Gets the simulation time.
        /// </summary>
        /// <returns>The current simulation time in seconds.</returns>
        double GetSimulationTime();

        /// <summary>
        /// Gets the packet count.
        /// </summary>
        /// <returns>The current packet count.</returns>
        int GetPacketCount();

        /// <summary>
        /// Gets the log entries.
        /// </summary>
        /// <returns>The current log entries.</returns>
        IEnumerable<LogEntry> GetLogEntries();

        /// <summary>
        /// Checks if the simulation is running.
        /// </summary>
        /// <returns>True if the simulation is running; otherwise false.</returns>
        bool IsSimulationRunning();

        /// <summary>
        /// Event raised when the log entries change.
        /// </summary>
        event EventHandler? LogEntriesChanged;

        /// <summary>
        /// Event raised when the simulation state changes.
        /// </summary>
        event EventHandler? SimulationStateChanged;
    }
}
