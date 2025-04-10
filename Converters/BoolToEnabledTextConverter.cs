using System;
using System.Globalization;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to text describing whether something is enabled or disabled.
    /// </summary>
    public class BoolToEnabledTextConverter : IValueConverter
    {
        /// <summary>
        /// Gets or sets the text to display when the value is true.
        /// </summary>
        public string EnabledText { get; set; } = "Enabled";

        /// <summary>
        /// Gets or sets the text to display when the value is false.
        /// </summary>
        public string DisabledText { get; set; } = "Disabled";

        /// <summary>
        /// Converts a boolean value to text.
        /// </summary>
        /// <param name="value">The boolean value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// The EnabledText property value if the input value is true; otherwise the DisabledText property value.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? EnabledText : DisabledText;
            }

            return DisabledText;
        }

        /// <summary>
        /// Converts text back to a boolean value.
        /// </summary>
        /// <param name="value">The text to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// true if the input value equals the EnabledText property value; otherwise false.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value?.ToString() == EnabledText;
        }
    }
}
