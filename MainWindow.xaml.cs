using System;
using System.Windows;
using System.Windows.Threading;
using System.Collections.ObjectModel;

namespace TheNetworkProject
{
    public partial class MainWindow : Window
    {
        private readonly DispatcherTimer networkSearchTimer;
        private string currentPhysicalNetwork = "Recherche...";
        // Collection pour les réseaux virtuels (commence vide)
        public ObservableCollection<VirtualNetwork> VirtualNetworks { get; set; } = new ObservableCollection<VirtualNetwork>();

        public MainWindow()
        {
            InitializeComponent();
            // Lier la liste des réseaux virtuels à la collection
            VirtualNetworksList.ItemsSource = VirtualNetworks;

            // Initialisation de la recherche du réseau physique
            PhysicalNetworkStatus.Text = currentPhysicalNetwork;
            networkSearchTimer = new DispatcherTimer();
            networkSearchTimer.Interval = TimeSpan.FromSeconds(5);
            networkSearchTimer.Tick += NetworkSearchTimer_Tick;
            networkSearchTimer.Start();
        }

        // Simulation de la recherche de réseau physique
        private void NetworkSearchTimer_Tick(object? sender, EventArgs e)
        {
            // Ici, on simule la détection : s'il n'y a pas de réseau, on affiche "Réseau inconnu"
            currentPhysicalNetwork = "Réseau inconnu";
            PhysicalNetworkStatus.Text = currentPhysicalNetwork;
        }

        // Création d'un réseau virtuel via la fenêtre de configuration
        private void CreateVirtualNetwork_Click(object sender, RoutedEventArgs e)
        {
            // Assure-toi que VirtualNetworkConfigurationWindow existe et est bien dans le bon namespace.
            VirtualNetworkConfigurationWindow configWindow = new VirtualNetworkConfigurationWindow();
            if (configWindow.ShowDialog() == true)
            {
                // Utilise les paramètres renvoyés par la fenêtre de configuration
                VirtualNetworks.Add(new VirtualNetwork { Name = "Nouveau Réseau Virtuel" });
            }
            else
            {
                // Simulation de création directe
                VirtualNetworks.Add(new VirtualNetwork { Name = "Nouveau Réseau Virtuel" });
            }
        }

        // Affiche ou masque le volet d'options pour un réseau virtuel
        private void VirtualNetworkOptions_Click(object sender, RoutedEventArgs e)
        {
            if (sender is FrameworkElement fe && fe.DataContext is VirtualNetwork vn)
            {
                // Bascule la visibilité des options
                vn.OptionsVisible = !vn.OptionsVisible;
            }
        }

        // Renommage (le TextBox étant en liaison bidirectionnelle, le changement est immédiat)
        private void RenameVirtualNetwork_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Le réseau a été renommé !");
            if (sender is FrameworkElement fe && fe.DataContext is VirtualNetwork vn)
            {
                vn.OptionsVisible = false;
            }
        }

        // Suppression d'un réseau virtuel
        private void DeleteVirtualNetwork_Click(object sender, RoutedEventArgs e)
        {
            if (sender is FrameworkElement fe && fe.DataContext is VirtualNetwork vn)
            {
                VirtualNetworks.Remove(vn);
            }
        }
    }
}