   M S B A M L   `   `   `  ����B  >TNP.App, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null-clr-namespace:TNP.App.ViewsTNP.App.Views  5      7 clr-namespace:TNP.App.ConvertersTNP.App.Converters  5      )    #TNP.App.Views.ElementPropertiesView   5      W SPresentationCore, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35e aSystem.Windows.Controls.Ribbon, Version=6.0.2.0, Culture=neutral, PublicKeyToken=b77a5c561934e089R NWindowsBase, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35F 9http://schemas.microsoft.com/winfx/2006/xaml/presentation     5      R NSystem.Xaml, Version=6.0.2.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     5      Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     5      @d2http://schemas.microsoft.com/expression/blend/2008     5      -localclr-namespace:TNP.App.Views     5      7
converters clr-namespace:TNP.App.Converters     5        � 	Resources  5      2   ,TNP.App.Converters.BoolToVisibilityConverter   BoolToVisibilityConverter9   3TNP.App.Converters.InverseBoolToVisibilityConverter $  InverseBoolToVisibilityConverter��%:   &	        &	        5   
   6/     5   
   66   5      .��5      �  � RowDefinitions 5   
   �� 5      $
��Auto �6   �� 5      $��* �6   .+�5   
   �� $��0��6   ���#F0F0F0���#CCC$��0,0,0,1q�6F    �� Padding$
 10,5q�6`   ._�5      �  � ColumnDefinitions 5      �� 5      $��* �6'   �� 5      $
��Auto �6'   .+�5      �� $��0��6   $
��Bold"�6K   $��14(�6]   $��Center=�5      ��5   ,   �� *SelectionName+5      �� 5!      $��1��6   ���#666$��Center=�5"      $��5,0,0,0q�65   ��5!   ,   �� *SelectionType+5"   5   5#      5$      � 5'   
   $��1��6   y�6   �� *HasSelection+ �� 	Converter# [  .+�5(      &   System.Windows.Controls.DataGrid     AutoGenerateColumns$ False��6:      CanUserAddRows$ False��5)         CanUserDeleteRows$ False��6,      HeadersVisibility$ Column=�5*      	   GridLinesVisibility$	 
Horizontal=�60   ��5(      �� *
Properties+
 �� SelectedItem
 5+      �� *SelectedProperty+   Columns 5,      0  *System.Windows.Controls.DataGridTextColumn  5-      ,  &System.Windows.Controls.DataGridColumn   Header$ Property��6)      Width5  /System.Windows.Controls.DataGridLengthConverter$ * 6S      
IsReadOnly$
 True��6]   1  +System.Windows.Controls.DataGridBoundColumn   Binding 6;   �� *Key+6]     5.      $ Value��6)   $ * 6R    68   �� *Value+6R   50      51      �� 54   
   $��1��6   ���#FAFAFA���#EEE$��1q�55   7   $��10q�6K   y�54      �� *HasSelection+# [ ._�56      �� $'��!No element or connection selected��6   $��Center=�6A   $��Center=�57      ���#99961   58      59      5:      