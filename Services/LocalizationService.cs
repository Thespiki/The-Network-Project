using System;
using System.Globalization;
using System.Windows;
using System.Threading;

namespace TNP.App.Services
{
    /// <summary>
    /// Service for handling application localization
    /// </summary>
    public class LocalizationService
    {
        /// <summary>
        /// Event that occurs when the language changes
        /// </summary>
        public event EventHandler LanguageChanged;

        private string _currentLanguage;

        /// <summary>
        /// Gets the current language code
        /// </summary>
        public string CurrentLanguage => _currentLanguage;

        /// <summary>
        /// Constructor
        /// </summary>
        public LocalizationService()
        {
            _currentLanguage = GetSystemLanguage();
        }

        /// <summary>
        /// Sets the application language
        /// </summary>
        /// <param name="languageCode">The language code (e.g., "en", "fr")</param>
        public void SetLanguage(string languageCode)
        {
            if (string.IsNullOrEmpty(languageCode))
                throw new ArgumentNullException(nameof(languageCode));

            // Normalize the language code
            languageCode = languageCode.ToLowerInvariant();
            if (languageCode.Length > 2)
                languageCode = languageCode.Substring(0, 2);

            // If the language hasn't changed, do nothing
            if (_currentLanguage == languageCode)
                return;

            _currentLanguage = languageCode;

            // Set the current thread culture
            Thread.CurrentThread.CurrentCulture = new CultureInfo(languageCode);
            Thread.CurrentThread.CurrentUICulture = new CultureInfo(languageCode);

            // Update resource dictionaries
            UpdateResourceDictionaries(languageCode);

            // Raise the LanguageChanged event
            OnLanguageChanged();
        }

        /// <summary>
        /// Gets the system language from the current culture
        /// </summary>
        /// <returns>The language code (e.g., "en", "fr")</returns>
        public static string GetSystemLanguage()
        {
            // Get the current culture's language
            string language = CultureInfo.CurrentCulture.TwoLetterISOLanguageName.ToLowerInvariant();

            // If the language is not supported, use English as default
            if (language != "en" && language != "fr")
                language = "en";

            return language;
        }

        /// <summary>
        /// Gets the localized string resource
        /// </summary>
        /// <param name="key">The resource key</param>
        /// <returns>The localized string or the key if not found</returns>
        public string GetString(string key)
        {
            if (string.IsNullOrEmpty(key))
                return string.Empty;

            // Try to get the resource from the application resources
            if (Application.Current.Resources.Contains(key))
                return Application.Current.Resources[key] as string;

            // Return the key as fallback
            return key;
        }

        /// <summary>
        /// Raises the LanguageChanged event
        /// </summary>
        protected virtual void OnLanguageChanged()
        {
            LanguageChanged?.Invoke(this, EventArgs.Empty);
        }

        /// <summary>
        /// Updates the application resource dictionaries based on the language
        /// </summary>
        /// <param name="languageCode">The language code</param>
        private void UpdateResourceDictionaries(string languageCode)
        {
            // Get the application resources
            var resources = Application.Current.Resources;
            var mergedDictionaries = resources.MergedDictionaries;

            // Find and remove the current language dictionary
            for (int i = mergedDictionaries.Count - 1; i >= 0; i--)
            {
                var dictionary = mergedDictionaries[i];
                string source = dictionary.Source?.ToString() ?? string.Empty;

                if (source.Contains("StringResources.") && source.EndsWith(".xaml"))
                {
                    mergedDictionaries.RemoveAt(i);
                }
            }

            // Add the new language dictionary
            var languageDictionaryUri = new Uri($"/Resources/StringResources.{languageCode}.xaml", UriKind.Relative);
            try
            {
                var languageDictionary = new ResourceDictionary() { Source = languageDictionaryUri };
                mergedDictionaries.Add(languageDictionary);
            }
            catch (Exception ex)
            {
                // If the language dictionary is not found, fall back to English
                if (languageCode != "en")
                {
                    System.Diagnostics.Debug.WriteLine($"Failed to load language dictionary for '{languageCode}': {ex.Message}");
                    var fallbackUri = new Uri("/Resources/StringResources.en.xaml", UriKind.Relative);
                    mergedDictionaries.Add(new ResourceDictionary() { Source = fallbackUri });
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine($"Failed to load language dictionary: {ex.Message}");
                }
            }
        }
    }
}
