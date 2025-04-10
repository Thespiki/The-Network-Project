using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a thickness value based on selection state.
    /// True = Selected thickness (thicker), False = Normal thickness.
    /// </summary>
    public class BoolToSelectedThicknessConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue && boolValue)
            {
                // Selected state - thicker
                return 2.0;
            }
            
            // Normal state
            return 1.0;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return DependencyProperty.UnsetValue;
        }
    }
}