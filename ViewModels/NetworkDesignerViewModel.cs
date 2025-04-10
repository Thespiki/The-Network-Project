using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Input;
using TNP.App.Models;

namespace TNP.App.ViewModels
{
    /// <summary>
    /// ViewModel for network designer.
    /// </summary>
    public class NetworkDesignerViewModel : INotifyPropertyChanged
    {
        private bool _showGrid = true;
        private bool _snapToGrid = true;
        private double _zoom = 1.0;
        private NetworkComponent? _selectedComponent;
        private Connection? _selectedConnection;

        /// <summary>
        /// Gets or sets a value indicating whether the grid should be shown.
        /// </summary>
        public bool ShowGrid
        {
            get => _showGrid;
            set
            {
                if (_showGrid != value)
                {
                    _showGrid = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets a value indicating whether elements should snap to grid.
        /// </summary>
        public bool SnapToGrid
        {
            get => _snapToGrid;
            set
            {
                if (_snapToGrid != value)
                {
                    _snapToGrid = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the zoom level.
        /// </summary>
        public double Zoom
        {
            get => _zoom;
            set
            {
                if (_zoom != value)
                {
                    _zoom = value;
                    OnPropertyChanged();
                }
            }
        }

        /// <summary>
        /// Gets or sets the selected component.
        /// </summary>
        public NetworkComponent? SelectedComponent
        {
            get => _selectedComponent;
            set
            {
                if (_selectedComponent != value)
                {
                    if (_selectedComponent != null)
                    {
                        _selectedComponent.IsSelected = false;
                    }

                    _selectedComponent = value;

                    if (_selectedComponent != null)
                    {
                        _selectedComponent.IsSelected = true;
                    }

                    OnPropertyChanged();
                    ComponentSelected?.Invoke(this, _selectedComponent);
                }
            }
        }

        /// <summary>
        /// Gets or sets the selected connection.
        /// </summary>
        public Connection? SelectedConnection
        {
            get => _selectedConnection;
            set
            {
                if (_selectedConnection != value)
                {
                    if (_selectedConnection != null)
                    {
                        _selectedConnection.IsSelected = false;
                    }

                    _selectedConnection = value;

                    if (_selectedConnection != null)
                    {
                        _selectedConnection.IsSelected = true;
                    }

                    OnPropertyChanged();
                    ConnectionSelected?.Invoke(this, _selectedConnection);
                }
            }
        }

        /// <summary>
        /// Gets the collection of network components.
        /// </summary>
        public ObservableCollection<NetworkComponent> Components { get; } = new ObservableCollection<NetworkComponent>();

        /// <summary>
        /// Gets the collection of connections.
        /// </summary>
        public ObservableCollection<Connection> Connections { get; } = new ObservableCollection<Connection>();

        /// <summary>
        /// Event raised when a component is selected.
        /// </summary>
        public event EventHandler<NetworkComponent?> ComponentSelected;

        /// <summary>
        /// Event raised when a connection is selected.
        /// </summary>
        public event EventHandler<Connection?> ConnectionSelected;

        /// <summary>
        /// Event raised when a property value changes.
        /// </summary>
        public event PropertyChangedEventHandler? PropertyChanged;

        /// <summary>
        /// Adds a component to the design.
        /// </summary>
        /// <param name="component">The component to add.</param>
        public void AddComponent(NetworkComponent component)
        {
            Components.Add(component);
            SelectedComponent = component;
        }

        /// <summary>
        /// Removes a component from the design.
        /// </summary>
        /// <param name="component">The component to remove.</param>
        public void RemoveComponent(NetworkComponent component)
        {
            if (Components.Contains(component))
            {
                // Remove any connections to/from this component
                for (int i = Connections.Count - 1; i >= 0; i--)
                {
                    var connection = Connections[i];
                    if (connection.Source == component || connection.Target == component)
                    {
                        Connections.RemoveAt(i);
                    }
                }

                Components.Remove(component);
                
                if (SelectedComponent == component)
                {
                    SelectedComponent = null;
                }
            }
        }

        /// <summary>
        /// Adds a connection to the design.
        /// </summary>
        /// <param name="connection">The connection to add.</param>
        public void AddConnection(Connection connection)
        {
            Connections.Add(connection);
            SelectedConnection = connection;
        }

        /// <summary>
        /// Removes a connection from the design.
        /// </summary>
        /// <param name="connection">The connection to remove.</param>
        public void RemoveConnection(Connection connection)
        {
            if (Connections.Contains(connection))
            {
                Connections.Remove(connection);
                
                if (SelectedConnection == connection)
                {
                    SelectedConnection = null;
                }
            }
        }

        /// <summary>
        /// Sets the grid snapping for a position.
        /// </summary>
        /// <param name="position">The position to snap.</param>
        /// <returns>The snapped position.</returns>
        public Point SnapToGridPosition(Point position)
        {
            if (!SnapToGrid)
            {
                return position;
            }

            // Snap to 20-pixel grid
            const int gridSize = 20;
            
            double x = Math.Round(position.X / gridSize) * gridSize;
            double y = Math.Round(position.Y / gridSize) * gridSize;
            
            return new Point(x, y);
        }

        /// <summary>
        /// Creates a new component of the specified type.
        /// </summary>
        /// <param name="elementType">The element type.</param>
        /// <param name="position">The position.</param>
        /// <returns>The new component.</returns>
        public NetworkComponent CreateComponent(NetworkElementType elementType, Point position)
        {
            NetworkComponent component;
            
            switch (elementType)
            {
                case NetworkElementType.Router:
                    component = new Router();
                    break;
                case NetworkElementType.Switch:
                    component = new Switch();
                    break;
                case NetworkElementType.Server:
                    component = new Server();
                    break;
                case NetworkElementType.Computer:
                    component = new Computer();
                    break;
                case NetworkElementType.Modem:
                    component = new Modem();
                    break;
                case NetworkElementType.Printer:
                    component = new Printer();
                    break;
                case NetworkElementType.Firewall:
                    component = new Firewall();
                    break;
                case NetworkElementType.AccessPoint:
                    component = new AccessPoint();
                    break;
                default:
                    throw new ArgumentException($"Unknown element type: {elementType}");
            }
            
            component.Position = SnapToGridPosition(position);
            
            return component;
        }

        /// <summary>
        /// Finds the component at the specified position.
        /// </summary>
        /// <param name="position">The position.</param>
        /// <returns>The component, or null if not found.</returns>
        public NetworkComponent? FindComponentAt(Point position)
        {
            // Find the component at the specified position
            // Simple hit-testing logic
            const double hitTestRadius = 30; // Half of the component's visual size
            
            foreach (var component in Components)
            {
                double dx = position.X - component.Position.X;
                double dy = position.Y - component.Position.Y;
                double distanceSquared = dx * dx + dy * dy;
                
                if (distanceSquared <= hitTestRadius * hitTestRadius)
                {
                    return component;
                }
            }
            
            return null;
        }

        /// <summary>
        /// Called when a property value changes.
        /// </summary>
        /// <param name="propertyName">The name of the property.</param>
        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}