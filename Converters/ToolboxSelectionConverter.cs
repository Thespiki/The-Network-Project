using System;
using System.Globalization;
using System.Windows.Data;
using TNP.App.Models;

namespace TNP.App.Converters
{
    /// <summary>
    /// Base class for toolbox selection converters.
    /// </summary>
    public abstract class ToolboxSelectionConverterBase : IValueConverter
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected abstract NetworkElementType ElementType { get; }

        /// <summary>
        /// Converts a NetworkElementType value to a boolean value indicating whether the associated toolbox item is selected.
        /// </summary>
        /// <param name="value">The NetworkElementType value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// true if the input value equals the ElementType property value; otherwise false.
        /// </returns>
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is NetworkElementType elementType)
            {
                return elementType == ElementType;
            }

            return false;
        }

        /// <summary>
        /// Converts a boolean value back to a NetworkElementType value.
        /// </summary>
        /// <param name="value">The boolean value to convert.</param>
        /// <param name="targetType">The type of the binding target property.</param>
        /// <param name="parameter">The converter parameter to use.</param>
        /// <param name="culture">The culture to use in the converter.</param>
        /// <returns>
        /// The ElementType property value if the input value is true; otherwise the default NetworkElementType value.
        /// </returns>
        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue && boolValue)
            {
                return ElementType;
            }

            return NetworkElementType.Generic;
        }
    }

    /// <summary>
    /// Converter for the selection toolbox item.
    /// </summary>
    public class ToolboxSelectionConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Generic;
    }

    /// <summary>
    /// Converter for the router toolbox item.
    /// </summary>
    public class ToolboxRouterConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Router;
    }

    /// <summary>
    /// Converter for the switch toolbox item.
    /// </summary>
    public class ToolboxSwitchConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Switch;
    }

    /// <summary>
    /// Converter for the server toolbox item.
    /// </summary>
    public class ToolboxServerConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Server;
    }

    /// <summary>
    /// Converter for the computer toolbox item.
    /// </summary>
    public class ToolboxComputerConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Computer;
    }

    /// <summary>
    /// Converter for the modem toolbox item.
    /// </summary>
    public class ToolboxModemConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Modem;
    }

    /// <summary>
    /// Converter for the firewall toolbox item.
    /// </summary>
    public class ToolboxFirewallConverter : ToolboxSelectionConverterBase
    {
        /// <summary>
        /// Gets the network element type that this converter is associated with.
        /// </summary>
        protected override NetworkElementType ElementType => NetworkElementType.Firewall;
    }
}
