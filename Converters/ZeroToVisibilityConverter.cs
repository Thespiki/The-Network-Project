using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a numeric value to a Visibility enumeration value.
    /// </summary>
    public class ZeroToVisibilityConverter : IValueConverter
    {
        /// <summary>
        /// Gets or sets a value indicating whether to invert the conversion logic.
        /// </summary>
        public bool Invert { get; set; } = false;

        /// <summary>
        /// Converts a numeric value to a Visibility enumeration value.
        /// </summary>
        /// <param name="value">The numeric value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// Visibility.Collapsed if the input value is zero; otherwise Visibility.Visible. If Invert is true, the logic is reversed.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value == null)
            {
                return Invert ? Visibility.Visible : Visibility.Collapsed;
            }

            bool isZero = false;

            if (value is int intValue)
            {
                isZero = intValue == 0;
            }
            else if (value is double doubleValue)
            {
                isZero = Math.Abs(doubleValue) < 0.001;
            }
            else if (value is float floatValue)
            {
                isZero = Math.Abs(floatValue) < 0.001f;
            }
            else if (value is long longValue)
            {
                isZero = longValue == 0;
            }
            else if (value is decimal decimalValue)
            {
                isZero = decimalValue == 0;
            }

            return (isZero ^ Invert) ? Visibility.Collapsed : Visibility.Visible;
        }

        /// <summary>
        /// Converts a Visibility enumeration value back to a numeric value.
        /// </summary>
        /// <param name="value">The Visibility enumeration value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// 0 if the input value is Visibility.Collapsed; otherwise 1. If Invert is true, the logic is reversed.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            // Conversion de retour non prise en charge
            return 0;
        }
    }
}
