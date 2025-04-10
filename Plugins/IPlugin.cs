using System;
using System.Windows.Controls;

namespace TNP.App.Plugins
{
    /// <summary>
    /// Interface for plugins.
    /// </summary>
    public interface IPlugin
    {
        /// <summary>
        /// Gets the name of the plugin.
        /// </summary>
        string Name { get; }

        /// <summary>
        /// Gets the description of the plugin.
        /// </summary>
        string Description { get; }

        /// <summary>
        /// Gets the version of the plugin.
        /// </summary>
        string Version { get; }

        /// <summary>
        /// Gets the author of the plugin.
        /// </summary>
        string Author { get; }

        /// <summary>
        /// Initializes the plugin.
        /// </summary>
        void Initialize();

        /// <summary>
        /// Shuts down the plugin.
        /// </summary>
        void Shutdown();

        /// <summary>
        /// Gets the control for the plugin settings.
        /// </summary>
        /// <returns>The control for the plugin settings.</returns>
        Control? GetSettingsControl();

        /// <summary>
        /// Executes a command.
        /// </summary>
        /// <param name="command">The command to execute.</param>
        /// <param name="parameter">The parameter for the command.</param>
        /// <returns>True if the command was executed successfully; otherwise false.</returns>
        bool ExecuteCommand(string command, object? parameter = null);
    }
}
