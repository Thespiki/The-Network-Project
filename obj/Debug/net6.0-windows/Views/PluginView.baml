   M S B A M L   `   `   `  ����B  >TNP.App, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null-clr-namespace:TNP.App.ViewsTNP.App.Views  5      7 clr-namespace:TNP.App.ConvertersTNP.App.Converters  5          TNP.App.Views.PluginView   5      W SPresentationCore, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35e aSystem.Windows.Controls.Ribbon, Version=6.0.2.0, Culture=neutral, PublicKeyToken=b77a5c561934e089R NWindowsBase, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35F 9http://schemas.microsoft.com/winfx/2006/xaml/presentation     5      R NSystem.Xaml, Version=6.0.2.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     5      Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     5      @d2http://schemas.microsoft.com/expression/blend/2008     5      -localclr-namespace:TNP.App.Views     5      7
converters clr-namespace:TNP.App.Converters     5        � 	Resources  5      2   ,TNP.App.Converters.BoolToVisibilityConverter   BoolToVisibilityConverter��%   &	          5   
   6/   5      .��5      �  � RowDefinitions 5   
   �� 5      $
��Auto �6   �� 5      $��* �6   �� 5      $
��Auto �6   .+�5   
   �� $��0��6   ���#F0F0F0���#CCC$��0,0,0,1q�6F    �� Padding$ 10q�6`   ._�5      �  � ColumnDefinitions 5      �� 5      $��* �6'   �� 5      $
��Auto �6'   .+�5      �� $��0��6   $��Plugin Manager��6,   $
��Bold"�6B   $��16(�6T   �� 5       $��1��6    �� Orientation$ 
Horizontal=�6-   .+�5!      �� $	��200��6   $��0,0,10,0q�6*   $��Center=�5"       � ToolTip$ Search plugins��69   ��5!   <   �� *PluginSearchText+ �� UpdateSourceTrigger$ PropertyChanged=�5"   9   �� 5#      $��Refresh��6   $	��8,3q�6Y   ��6/   �� *RefreshPluginsCommand+6Y   5$      5%      5&      � 5)   
   $��1��6    5*      �� 5+      $��* �6#   �� 5,      $
��Auto �6#   �� 5-      $��* �6#   .+�51      � $��0��6    52      �� 53      $
��Auto �6$   �� 54      $��* �6$   .+�57      �� $��0��6   ���#F5F5F5���#DDD$��0,0,0,1q�6N   $
 10,5q�6h   ._�58      �� $��Available Plugins��6    $��SemiBold"�69   59      �� 5;      $��1��6   $��5q�5<   B   ��5;   (   �� *AvailablePlugins+ �� SelectedItem 5<      �� *SelectedPlugin+��5=      �� 5>      .R�5?      � $	��0,5q�6#    5@   "   �� 5A   &   $
��Auto �64   �� 5B   &   $
��Auto �64   .+�5E   "   �� $��0��6,   $
��Bold"�6O   ��69   �� *Name+6O   �� 5F   "   $��1��6,   $
��Wrap=�6V   ��69   �� *Description+6V   5G      5H      5J      5K      �� 5N      $��1��6   $��Center=�6)   $
��10,0q�6D   .+�5O      �� $��Activate ▶��6   $	��8,3q�6X   $��0,0,0,10q�6f   ��6.   �� *ActivatePluginCommand+{�5P      �� *CanActivatePlugin+�� 5Q      $��◀ Deactivate��6   $	��8,3q�6\   ��60   �� *DeactivatePluginCommand+{�5R      �� *CanDeactivatePlugin+5S      � 5V      $��2��6    5W      �� 5X      $
��Auto �6$   �� 5Y      $��* �6$   .+�5\      �� $��0��6   ���#F5F5F5���#DDD$��0,0,0,1q�6N   $
 10,5q�6h   ._�5]      �� $��Active Plugins��6    $��SemiBold"�66   5^      �� 5`      $��1��6   $��5q�5a   B   ��5`   (   �� *ActivePlugins+ 5a      �� *SelectedPlugin+��5b      �� 5c      .R�5d      � $	��0,5q�6#    5e   "   �� 5f   &   $
��Auto �64   �� 5g   &   $
��Auto �64   �� 5h   &   $
��Auto �64   .+�5k   "   �� $��0��6,   $
��Bold"�6O   ��69   �� *Name+6O   �� 5l   "   $��1��6,   ���#666��69   �� *	Version+ �� StringFormat$ Version: {0}��6m   �� 5m   "   $��2��6,   $
��Wrap=�6V   ��69   �� *Description+6V   5n      5o      5q      5r      5s      �� 5v   
   $��2��6   ���#F0F0F0���#CCC$��0,1,0,0q�6F   $ 10q�6`   ._�5w      �  5x      �� 5y      $��* �6'   �� 5z      $
��Auto �6'   .+�5}      �� $��0��6   ��6,   �� *InstallationStatus+y�6P   �� *IsInstalling+	 �� 	Converter#	 [  �� 5      $��1��6   $��Install Plugin...��6)   $	��8,3q�6n   ��6E   �� *InstallPluginCommand+{�5�      �� *IsInstalling+  InverseBoolConverter#	 [ 5�      5�      5�      5�      