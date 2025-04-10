using System;
using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;
using TNP.App.Models;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a LogLevel enum value to a color.
    /// </summary>
    public class LogLevelToColorConverter : IValueConverter
    {
        /// <summary>
        /// Converts a LogLevel enum value to a color.
        /// </summary>
        /// <param name="value">The LogLevel enum value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// A color corresponding to the LogLevel enum value.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is LogLevel logLevel)
            {
                return logLevel switch
                {
                    LogLevel.Debug => new SolidColorBrush(Colors.Gray),
                    LogLevel.Info => new SolidColorBrush(Colors.Black),
                    LogLevel.Warning => new SolidColorBrush(Colors.Orange),
                    LogLevel.Error => new SolidColorBrush(Colors.Red),
                    LogLevel.Critical => new SolidColorBrush(Colors.DarkRed),
                    _ => new SolidColorBrush(Colors.Black)
                };
            }

            return new SolidColorBrush(Colors.Black);
        }

        /// <summary>
        /// Converts a color back to a LogLevel enum value.
        /// </summary>
        /// <param name="value">The color to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// A LogLevel enum value corresponding to the color.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            // Conversion de retour non prise en charge
            return LogLevel.Info;
        }
    }
}
