using System.Windows.Controls;
using TNP.App.ViewModels;

namespace TNP.App.Views
{
    /// <summary>
    /// Interaction logic for SimulationView.xaml
    /// </summary>
    public partial class SimulationView : UserControl
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="SimulationView"/> class.
        /// </summary>
        public SimulationView()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Sets the view model.
        /// </summary>
        /// <param name="viewModel">The view model.</param>
        public void SetViewModel(SimulationViewModel viewModel)
        {
            DataContext = viewModel;
        }
    }
}