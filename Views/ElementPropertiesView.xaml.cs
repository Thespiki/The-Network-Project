using System.Windows.Controls;
using TNP.App.ViewModels;

namespace TNP.App.Views
{
    /// <summary>
    /// Interaction logic for ElementPropertiesView.xaml
    /// </summary>
    public partial class ElementPropertiesView : UserControl
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ElementPropertiesView"/> class.
        /// </summary>
        public ElementPropertiesView()
        {
            InitializeComponent();
        }

        /// <summary>
        /// Sets the view model.
        /// </summary>
        /// <param name="viewModel">The view model.</param>
        public void SetViewModel(ElementPropertiesViewModel viewModel)
        {
            DataContext = viewModel;
        }
    }
}