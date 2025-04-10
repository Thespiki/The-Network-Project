using System;
using System.Globalization;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a text string.
    /// </summary>
    public class BoolToTextConverter : IValueConverter
    {
        /// <summary>
        /// Gets or sets the text to display when the value is true.
        /// </summary>
        public string TrueText { get; set; } = "Yes";

        /// <summary>
        /// Gets or sets the text to display when the value is false.
        /// </summary>
        public string FalseText { get; set; } = "No";

        /// <summary>
        /// Converts a boolean value to a text string.
        /// </summary>
        /// <param name="value">The boolean value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// The TrueText property value if the input value is true; otherwise the FalseText property value.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? TrueText : FalseText;
            }

            return FalseText;
        }

        /// <summary>
        /// Converts a text string back to a boolean value.
        /// </summary>
        /// <param name="value">The text string to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// true if the input value equals the TrueText property value; otherwise false.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            var text = value as string;
            return string.Equals(text, TrueText, StringComparison.OrdinalIgnoreCase);
        }
    }
}
