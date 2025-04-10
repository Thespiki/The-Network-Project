using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a thickness value for connections based on selection state.
    /// True = Selected connection thickness (thicker), False = Normal connection thickness.
    /// </summary>
    public class BoolToSelectedConnectionThicknessConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue && boolValue)
            {
                // Selected state - thicker
                return 3.0;
            }
            
            // Normal state
            return 1.5;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return DependencyProperty.UnsetValue;
        }
    }
}