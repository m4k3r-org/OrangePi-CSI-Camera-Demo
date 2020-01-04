# OrangePi-CSI-Camera-Demo
a small program used for Demo to Capture Picture with OrangePi PC2 official CSI Camera.

Depend
-----
cairo v4l2  

Build
-----
install all the depend package and make sure pkg-config work fine
Just `make` in the working directory and a program named `demo.run` would be maked for you.

Use Argument
-----
demo.run [options]  
	-c|--count=capture count	default:4  
	-d|--device=Path to device	default:'/dev/video0'  
	-w|--width=capture width	default:800  
	-h|--height=capture height	default:600  
	-i|--input=device input channel	default:0  
	-f|--pix_format=fourcc pix format	default:'YU12'  
Becare:  
	Only YUYV,YU12,422P pix format could be write to png file  
	width,height and pix_foramt could be modified by the camera driver to make it compatible  
	example:'%s -d /dev/video1 -w 800 -h 600 -p YU12 -c 10'  

