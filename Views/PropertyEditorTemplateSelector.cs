using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;

namespace TNP.App.Views
{
    /// <summary>
    /// Template selector for property editor that chooses the appropriate template
    /// based on property type.
    /// </summary>
    public class PropertyEditorTemplateSelector : DataTemplateSelector
    {
        /// <summary>
        /// Gets or sets the template for string properties.
        /// </summary>
        public DataTemplate StringTemplate { get; set; } = null!;
        
        /// <summary>
        /// Gets or sets the template for boolean properties.
        /// </summary>
        public DataTemplate BooleanTemplate { get; set; } = null!;
        
        /// <summary>
        /// Gets or sets the template for numeric properties.
        /// </summary>
        public DataTemplate NumericTemplate { get; set; } = null!;
        
        /// <summary>
        /// Gets or sets the template for enum properties.
        /// </summary>
        public DataTemplate EnumTemplate { get; set; } = null!;
        
        /// <summary>
        /// Gets or sets the template for color properties.
        /// </summary>
        public DataTemplate ColorTemplate { get; set; } = null!;
        
        /// <summary>
        /// Gets or sets the template for read-only properties.
        /// </summary>
        public DataTemplate ReadOnlyTemplate { get; set; } = null!;
        
        /// <summary>
        /// Selects a template based on the property type.
        /// </summary>
        /// <param name="item">The property item.</param>
        /// <param name="container">The container.</param>
        /// <returns>The selected template.</returns>
        public override DataTemplate SelectTemplate(object item, DependencyObject container)
        {
            if (item is not KeyValuePair<string, object> property)
            {
                return base.SelectTemplate(item, container);
            }

            // Check for read-only properties
            if (property.Key.StartsWith("_"))
            {
                return ReadOnlyTemplate;
            }
            
            // Select template based on property value type
            if (property.Value == null)
            {
                return StringTemplate;
            }
            
            Type type = property.Value.GetType();
            
            if (type == typeof(bool))
            {
                return BooleanTemplate;
            }
            else if (type == typeof(int) || type == typeof(double) || type == typeof(float) || 
                     type == typeof(decimal) || type == typeof(long) || type == typeof(short))
            {
                return NumericTemplate;
            }
            else if (type.IsEnum)
            {
                return EnumTemplate;
            }
            else if (type.Name.Contains("Color"))
            {
                return ColorTemplate;
            }
            
            // Default to string template
            return StringTemplate;
        }
    }
}