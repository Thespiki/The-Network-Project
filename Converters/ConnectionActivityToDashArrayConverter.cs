using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean IsActive value to a dash array for connection lines.
    /// True = Solid line, False = Dashed line.
    /// </summary>
    public class ConnectionActivityToDashArrayConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool isActive)
            {
                if (isActive)
                {
                    // Active connection - solid line
                    return null;
                }
                else
                {
                    // Inactive connection - dashed line
                    return new DoubleCollection(new[] { 4.0, 2.0 });
                }
            }
            
            return null;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return DependencyProperty.UnsetValue;
        }
    }
}