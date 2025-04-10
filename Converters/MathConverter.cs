using System;
using System.Globalization;
using System.Windows.Data;

namespace TNP.App.Converters
{
    public class MathConverter : IValueConverter
    {
        public static MathConverter Add { get; } = new MathConverter { Operation = MathOperation.Add };
        public static MathConverter Subtract { get; } = new MathConverter { Operation = MathOperation.Subtract };
        public static MathConverter Multiply { get; } = new MathConverter { Operation = MathOperation.Multiply };
        public static MathConverter Divide { get; } = new MathConverter { Operation = MathOperation.Divide };
        
        public enum MathOperation
        {
            Add,
            Subtract,
            Multiply,
            Divide
        }
        
        public MathOperation Operation { get; set; } = MathOperation.Add;
        
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            double param;
            
            if (parameter is double doubleParam)
            {
                param = doubleParam;
            }
            else if (parameter is int intParam)
            {
                param = intParam;
            }
            else if (parameter is string stringParam && double.TryParse(stringParam, out double parsedParam))
            {
                param = parsedParam;
            }
            else
            {
                return value;
            }
            
            if (value is double doubleValue)
            {
                switch (Operation)
                {
                    case MathOperation.Add: return doubleValue + param;
                    case MathOperation.Subtract: return doubleValue - param;
                    case MathOperation.Multiply: return doubleValue * param;
                    case MathOperation.Divide: return param != 0 ? doubleValue / param : double.NaN;
                    default: return value;
                }
            }
            else if (value is int intValue)
            {
                switch (Operation)
                {
                    case MathOperation.Add: return intValue + param;
                    case MathOperation.Subtract: return intValue - param;
                    case MathOperation.Multiply: return intValue * param;
                    case MathOperation.Divide: return param != 0 ? intValue / param : double.NaN;
                    default: return value;
                }
            }
            
            return value;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
