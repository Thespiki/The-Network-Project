using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a font weight.
    /// </summary>
    public class BoolToFontWeightConverter : IValueConverter
    {
        /// <summary>
        /// Gets or sets the font weight for true values.
        /// </summary>
        public FontWeight TrueFontWeight { get; set; } = FontWeights.Bold;

        /// <summary>
        /// Gets or sets the font weight for false values.
        /// </summary>
        public FontWeight FalseFontWeight { get; set; } = FontWeights.Normal;

        /// <summary>
        /// Converts a boolean value to a font weight.
        /// </summary>
        /// <param name="value">The boolean value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// The TrueFontWeight property value if the input value is true; otherwise the FalseFontWeight property value.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? TrueFontWeight : FalseFontWeight;
            }

            return FalseFontWeight;
        }

        /// <summary>
        /// Converts a font weight back to a boolean value.
        /// </summary>
        /// <param name="value">The font weight to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// true if the input value equals the TrueFontWeight property value; otherwise false.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is FontWeight fontWeight)
            {
                return fontWeight == TrueFontWeight;
            }

            return false;
        }
    }
}
