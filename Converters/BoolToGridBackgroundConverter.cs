using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Media;

namespace TNP.App.Converters
{
    /// <summary>
    /// Converts a boolean value to a grid background brush.
    /// True = Grid pattern, False = Transparent.
    /// </summary>
    public class BoolToGridBackgroundConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue && boolValue)
            {
                // Create a grid pattern brush
                var gridBrush = new DrawingBrush
                {
                    TileMode = TileMode.Tile,
                    ViewportUnits = BrushMappingMode.Absolute,
                    Viewport = new Rect(0, 0, 20, 20),
                    Opacity = 0.3
                };

                var gridGeometry = new GeometryGroup();
                gridGeometry.Children.Add(new LineGeometry(new Point(0, 0), new Point(0, 20)));
                gridGeometry.Children.Add(new LineGeometry(new Point(0, 0), new Point(20, 0)));

                var gridDrawing = new GeometryDrawing
                {
                    Geometry = gridGeometry,
                    Pen = new Pen(new SolidColorBrush(Colors.LightGray), 0.5)
                };

                gridBrush.Drawing = gridDrawing;
                return gridBrush;
            }
            
            return Brushes.Transparent;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return DependencyProperty.UnsetValue;
        }
    }
}