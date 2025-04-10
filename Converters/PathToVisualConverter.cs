using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a file path to a visual image.
    /// </summary>
    public class PathToVisualConverter : IValueConverter
    {
        /// <summary>
        /// Converts a file path to a visual image.
        /// </summary>
        /// <param name="value">The file path to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// An ImageSource if the input is a valid file path; otherwise a default image or null.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is string path && !string.IsNullOrEmpty(path))
            {
                try
                {
                    var uri = new Uri(path, UriKind.RelativeOrAbsolute);
                    return new BitmapImage(uri);
                }
                catch (Exception)
                {
                    return DependencyProperty.UnsetValue;
                }
            }

            return DependencyProperty.UnsetValue;
        }

        /// <summary>
        /// Converts a visual image back to a file path.
        /// </summary>
        /// <param name="value">The visual image to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// A file path if the input is a valid BitmapImage; otherwise null.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            // Conversion de retour non prise en charge
            return null;
        }
    }
}
