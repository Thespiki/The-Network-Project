using System;
using System.Windows;
using TNP.App.Services;
using TNP.App.ViewModels;
using TNP.App.Plugins;

namespace TNP.App
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="MainWindow"/> class.
        /// </summary>
        public MainWindow()
        {
            InitializeComponent();

            // Create services
            var simulationService = new SimulationService();
            var networkConfigService = new NetworkConfigService();
            
            // Create view models
            var networkDesignerViewModel = new NetworkDesignerViewModel();
            var elementPropertiesViewModel = new ElementPropertiesViewModel();
            var simulationViewModel = new SimulationViewModel();
            var pluginViewModel = new PluginViewModel(new Plugins.PluginManager("Plugins"));
            
            // Connect view models to views
            NetworkDesignerViewControl.SetViewModel(networkDesignerViewModel);
            ElementPropertiesViewControl.SetViewModel(elementPropertiesViewModel);
            SimulationViewControl.SetViewModel(simulationViewModel);
            PluginViewControl.SetViewModel(pluginViewModel);
            
            // Set up main view model
            var mainViewModel = new MainViewModel(
                networkDesignerViewModel, 
                elementPropertiesViewModel, 
                simulationViewModel, 
                pluginViewModel, 
                networkConfigService as INetworkConfigService);
            
            DataContext = mainViewModel;
        }
    }
}
