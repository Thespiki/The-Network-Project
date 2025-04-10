using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a brush for selection state.
    /// True = Selection brush, False = Normal brush.
    /// </summary>
    public class BoolToSelectedBrushConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue && boolValue)
            {
                // Selected state
                return new SolidColorBrush(Colors.Blue);
            }
            
            // Normal state
            return new SolidColorBrush(Colors.Gray);
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return DependencyProperty.UnsetValue;
        }
    }
}