using System;
using System.Windows;

namespace TNP.App
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="App"/> class.
        /// </summary>
        public App()
        {
            // Application initialization code
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
        }

        private void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            var exception = e.ExceptionObject as Exception;
            MessageBox.Show(
                $"An unhandled exception occurred: {exception?.Message}\n\n{exception?.StackTrace}",
                "Error",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }
}
