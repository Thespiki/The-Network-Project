   M S B A M L   `   `   `  ����B  >TNP.App, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null-clr-namespace:TNP.App.ViewsTNP.App.Views  5      7 clr-namespace:TNP.App.ConvertersTNP.App.Converters  5      /clr-namespace:TNP.App.ModelsTNP.App.Models  5      '    !TNP.App.Views.NetworkDesignerView   5      W SPresentationCore, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35e aSystem.Windows.Controls.Ribbon, Version=6.0.2.0, Culture=neutral, PublicKeyToken=b77a5c561934e089R NWindowsBase, Version=6.0.2.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35F 9http://schemas.microsoft.com/winfx/2006/xaml/presentation     5      R NSystem.Xaml, Version=6.0.2.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     5      Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     5      @d2http://schemas.microsoft.com/expression/blend/2008     5      -localclr-namespace:TNP.App.Views     5      7
converters clr-namespace:TNP.App.Converters     5      /modelsclr-namespace:TNP.App.Models     5        � 	Resources  5      6   0TNP.App.Converters.BoolToGridBackgroundConverter !  BoolToGridBackgroundConverter5   /TNP.App.Converters.BoolToSelectedBrushConverter   BoolToSelectedBrushConverter9   3TNP.App.Converters.BoolToSelectedThicknessConverter $  BoolToSelectedThicknessConverterC   =TNP.App.Converters.BoolToSelectedConnectionThicknessConverter . *BoolToSelectedConnectionThicknessConverter7   1TNP.App.Converters.ConnectionTypeToColorConverter " ConnectionTypeToColorConverter?   9TNP.App.Converters.ConnectionActivityToDashArrayConverter * &ConnectionActivityToDashArrayConverter  NetworkElementTemplate �� DataType#   TNP.App.Models.NetworkElement � ToolTip �� 	Converter  ConnectionTemplate   TNP.App.Models.Connection	 �� X1	 �� Y1	 �� X2	 �� Y2 �� StrokeDashArray��%�  &	        &	      &	 &     &	 9     &	 L     &	 _     &	 r     7  7  &	 �    7  7  7    5   
   63     5   
   62     5   
   66     5   
   6@     5   
   64     5   
   6<   �� 5   
   "  66   .R�5      � $��60��6   $��60��6    6*   �� *Name+.+�5      A� 
���������5      �� *
IsSelected+8   ��5      �� *
IsSelected+8  �� 5      $��40��6;   $��40��6F   ��6   �� *ElementTypeIcon+6F   �� 5      $��Center=�62   $��Bottom=�6O   $��0,0,0,2q�6j   ��6   �� *Name+6j   5      5      �� 5    
   "  62   .R�5!      ��  6   �� *SourcePoint.X+ 60   �� *SourcePoint.Y+ 5"      �� *TargetPoint.X+ 60   �� *TargetPoint.Y+��5#      �� *ConnectionType+8   ��5$      �� *
IsSelected+8   5%      �� *
IsActive+8  5&      5'      .��5)      � ��6   �� *
ShowGrid+# [  .+�5+   
   �� -   5        	 ��Name$	 NetworkCanvas��5+      
������ $
}�True��6A   .+�52      �� #��[ 6?   ��6   �� *Connections+��53      �� 54      .R�55      �� 
 K� IsItemsHost$

 True��6!   56      ��58      �� 59       �� 
TargetType" ��6   .�5:      ��  �� Property �@��Left �� Value$ 0��68   �� 5;       �@��Top$ 0��67   5<      5>      �� 5A      #��[ 6>   ��6   �� *
Components+��5B      �� 5C      .R�5D      �� $

 True��6!   5E      ��5G      �� 5H      " ��6   .�5I      ��  �@��Left 68   �� *
Position.X+�� 5J       �@��Top 67   �� *
Position.Y+5K      5M      �� 5P      -   5         ��Name$ ConnectionPreview��5P      $y�	Collapsed=�6.   
�������$	 4,2R�6S   $��2��6i   �� 5S      -   5         ��Name$ SelectionRectangle��5S      $y�	Collapsed=�64   
����  �$��1��6Y   ���	#3000BFFF6m   5T      5U      5V      