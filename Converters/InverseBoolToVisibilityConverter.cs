using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a Visibility value with inverse logic.
    /// True = Collapsed, False = Visible.
    /// </summary>
    public class InverseBoolToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? Visibility.Collapsed : Visibility.Visible;
            }
            
            return Visibility.Visible;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is Visibility visibility)
            {
                return visibility != Visibility.Visible;
            }
            
            return true;
        }
    }
}