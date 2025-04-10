using System.Windows.Controls;
using TNP.App.ViewModels;

namespace TNP.App.Views
{
    /// <summary>
    /// Interaction logic for PluginView.xaml
    /// </summary>
    public partial class PluginView : UserControl
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="PluginView"/> class.
        /// </summary>
        public PluginView()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Sets the view model.
        /// </summary>
        /// <param name="viewModel">The view model.</param>
        public void SetViewModel(PluginViewModel viewModel)
        {
            DataContext = viewModel;
        }
    }
}