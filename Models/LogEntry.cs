using System;

namespace TNP.App.Models
{
    /// <summary>
    /// Represents a log entry in the simulation or application log.
    /// </summary>
    public class LogEntry
    {
        /// <summary>
        /// Gets or sets the timestamp of the log entry.
        /// </summary>
        public DateTime Timestamp { get; set; } = DateTime.Now;

        /// <summary>
        /// Gets or sets the level of the log entry.
        /// </summary>
        public LogLevel Level { get; set; } = LogLevel.Info;

        /// <summary>
        /// Gets or sets the source of the log entry.
        /// </summary>
        public string Source { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the message of the log entry.
        /// </summary>
        public string Message { get; set; } = string.Empty;

        /// <summary>
        /// Gets or sets the details of the log entry.
        /// </summary>
        public string? Details { get; set; }

        /// <summary>
        /// Initializes a new instance of the <see cref="LogEntry"/> class.
        /// </summary>
        public LogEntry()
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LogEntry"/> class with the specified properties.
        /// </summary>
        /// <param name="level">The level of the log entry.</param>
        /// <param name="source">The source of the log entry.</param>
        /// <param name="message">The message of the log entry.</param>
        /// <param name="details">The details of the log entry.</param>
        public LogEntry(LogLevel level, string source, string message, string? details = null)
        {
            Level = level;
            Source = source;
            Message = message;
            Details = details;
        }

        /// <summary>
        /// Creates a debug log entry.
        /// </summary>
        /// <param name="source">The source of the log entry.</param>
        /// <param name="message">The message of the log entry.</param>
        /// <param name="details">The details of the log entry.</param>
        /// <returns>A new log entry with the Debug level.</returns>
        public static LogEntry Debug(string source, string message, string? details = null)
        {
            return new LogEntry(LogLevel.Debug, source, message, details);
        }

        /// <summary>
        /// Creates an information log entry.
        /// </summary>
        /// <param name="source">The source of the log entry.</param>
        /// <param name="message">The message of the log entry.</param>
        /// <param name="details">The details of the log entry.</param>
        /// <returns>A new log entry with the Info level.</returns>
        public static LogEntry Info(string source, string message, string? details = null)
        {
            return new LogEntry(LogLevel.Info, source, message, details);
        }

        /// <summary>
        /// Creates a warning log entry.
        /// </summary>
        /// <param name="source">The source of the log entry.</param>
        /// <param name="message">The message of the log entry.</param>
        /// <param name="details">The details of the log entry.</param>
        /// <returns>A new log entry with the Warning level.</returns>
        public static LogEntry Warning(string source, string message, string? details = null)
        {
            return new LogEntry(LogLevel.Warning, source, message, details);
        }

        /// <summary>
        /// Creates an error log entry.
        /// </summary>
        /// <param name="source">The source of the log entry.</param>
        /// <param name="message">The message of the log entry.</param>
        /// <param name="details">The details of the log entry.</param>
        /// <returns>A new log entry with the Error level.</returns>
        public static LogEntry Error(string source, string message, string? details = null)
        {
            return new LogEntry(LogLevel.Error, source, message, details);
        }

        /// <summary>
        /// Creates a critical log entry.
        /// </summary>
        /// <param name="source">The source of the log entry.</param>
        /// <param name="message">The message of the log entry.</param>
        /// <param name="details">The details of the log entry.</param>
        /// <returns>A new log entry with the Critical level.</returns>
        public static LogEntry Critical(string source, string message, string? details = null)
        {
            return new LogEntry(LogLevel.Critical, source, message, details);
        }

        /// <summary>
        /// Returns a string that represents the current object.
        /// </summary>
        /// <returns>A string that represents the current object.</returns>
        public override string ToString()
        {
            return $"[{Timestamp:yyyy-MM-dd HH:mm:ss}] [{Level}] [{Source}] {Message}";
        }
    }

    /// <summary>
    /// Represents the level of a log entry.
    /// </summary>
    public enum LogLevel
    {
        /// <summary>
        /// Debug log level.
        /// </summary>
        Debug,

        /// <summary>
        /// Information log level.
        /// </summary>
        Info,

        /// <summary>
        /// Warning log level.
        /// </summary>
        Warning,

        /// <summary>
        /// Error log level.
        /// </summary>
        Error,

        /// <summary>
        /// Critical log level.
        /// </summary>
        Critical
    }
}
