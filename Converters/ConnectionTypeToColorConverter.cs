using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;
using TNP.App.Models;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a ConnectionType enum value to a brush color.
    /// </summary>
    public class ConnectionTypeToColorConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is ConnectionType connectionType)
            {
                switch (connectionType)
                {
                    case ConnectionType.Ethernet:
                        return new SolidColorBrush(Colors.Blue);
                    case ConnectionType.WiFi:
                        return new SolidColorBrush(Colors.Green);
                    case ConnectionType.Fiber:
                        return new SolidColorBrush(Colors.Orange);
                    case ConnectionType.Serial:
                        return new SolidColorBrush(Colors.Purple);
                    case ConnectionType.USB:
                        return new SolidColorBrush(Colors.Red);
                    case ConnectionType.Bluetooth:
                        return new SolidColorBrush(Colors.DodgerBlue);
                    default:
                        return new SolidColorBrush(Colors.Black);
                }
            }
            
            return new SolidColorBrush(Colors.Black);
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return DependencyProperty.UnsetValue;
        }
    }
}