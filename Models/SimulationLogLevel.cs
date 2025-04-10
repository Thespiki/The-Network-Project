namespace TNP.App.Models
{
    /// <summary>
    /// Defines the log levels for simulation messages.
    /// </summary>
    public enum SimulationLogLevel
    {
        /// <summary>
        /// Debug information, typically only visible during development or debugging.
        /// </summary>
        Debug,

        /// <summary>
        /// Informational messages about the simulation's progress.
        /// </summary>
        Info,

        /// <summary>
        /// Warning messages that indicate potential issues or suboptimal configurations.
        /// </summary>
        Warning,

        /// <summary>
        /// Error messages that indicate significant problems or simulation failures.
        /// </summary>
        Error,

        /// <summary>
        /// Success messages that indicate successful operations or validations.
        /// </summary>
        Success
    }
}
