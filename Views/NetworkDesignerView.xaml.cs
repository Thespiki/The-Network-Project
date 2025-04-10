using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using TNP.App.ViewModels;

namespace TNP.App.Views
{
    /// <summary>
    /// Interaction logic for NetworkDesignerView.xaml
    /// </summary>
    public partial class NetworkDesignerView : UserControl
    {
        private NetworkDesignerViewModel? _viewModel;
        private Point _startPoint;
        private bool _isDragging;

        /// <summary>
        /// Initializes a new instance of the <see cref="NetworkDesignerView"/> class.
        /// </summary>
        public NetworkDesignerView()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Sets the view model.
        /// </summary>
        /// <param name="viewModel">The view model.</param>
        public void SetViewModel(NetworkDesignerViewModel viewModel)
        {
            _viewModel = viewModel;
            DataContext = viewModel;
        }

        /// <summary>
        /// Handles mouse left button down event.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The event args.</param>
        private void NetworkCanvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            _startPoint = e.GetPosition(NetworkCanvas);
            _isDragging = true;
            NetworkCanvas.CaptureMouse();
        }

        /// <summary>
        /// Handles mouse move event.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The event args.</param>
        private void NetworkCanvas_MouseMove(object sender, MouseEventArgs e)
        {
            if (_isDragging && e.LeftButton == MouseButtonState.Pressed)
            {
                var currentPosition = e.GetPosition(NetworkCanvas);
                // Handle element drag or selection rectangle drawing
                // Implementation depends on the current editor mode
            }
        }

        /// <summary>
        /// Handles mouse left button up event.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The event args.</param>
        private void NetworkCanvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (_isDragging)
            {
                _isDragging = false;
                NetworkCanvas.ReleaseMouseCapture();
                
                // Handle element placement or selection completion
                // Implementation depends on the current editor mode
            }
        }

        /// <summary>
        /// Handles mouse wheel event for zooming.
        /// </summary>
        /// <param name="sender">The sender.</param>
        /// <param name="e">The event args.</param>
        private void NetworkCanvas_PreviewMouseWheel(object sender, MouseWheelEventArgs e)
        {
            if (_viewModel == null) return;
            
            const double zoomIncrement = 0.1;
            
            if (e.Delta > 0)
            {
                // Zoom in
                _viewModel.Zoom = System.Math.Min(_viewModel.Zoom + zoomIncrement, 3.0);
            }
            else
            {
                // Zoom out
                _viewModel.Zoom = System.Math.Max(_viewModel.Zoom - zoomIncrement, 0.3);
            }
            
            e.Handled = true;
        }
    }
}