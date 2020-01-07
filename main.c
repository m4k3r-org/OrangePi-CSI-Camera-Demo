/*
 * main.c
 *
 *  Created on: 2019年12月12日
 *      Author: tom
 *      https://github.com/macos2/MyCamera
 *		https://gitee.com/macos2/MyCamera
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <getopt.h>
#include <libv4l2.h>
#include <libv4lconvert.h>
#include <cairo.h>
#include "video_unit.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

typedef struct {
	void *offset;
	size_t len;
} Frame_buf;

static char *default_device = "/dev/video0\0";
static char *default_pix_format = "YU12\0";
unsigned int n, width, height, input, count;
char *device_path, *pix_format;

static int xioctl(int fd, int req, void *arg) {
	int r;
	do {
		r = ioctl(fd, req, arg);
	} while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));
	if (r == -1) {
		fprintf(stderr, "error %d, %s\\n", errno, strerror(errno));
	}
	return r;
}

void write_image_to_png(char *filename, char *argb_data) {
	if (argb_data == NULL || filename == NULL)
		return;
	cairo_status_t  status;
	cairo_surface_t *img = cairo_image_surface_create_for_data(argb_data,
			CAIRO_FORMAT_ARGB32, width, height,
			cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));
	status=cairo_surface_write_to_png(img, filename);
	if(status!=CAIRO_STATUS_SUCCESS){
		perror("Write image to png file fail");
	}
	cairo_surface_destroy(img);
	free(argb_data);
}

static int get_image_mmap(int fd, int w, int h) {
	struct v4l2_buffer buf;
	struct v4l2_requestbuffers req;
	FILE *image;
	Frame_buf *frm_buf = malloc(4 * sizeof(Frame_buf));
	void *temp;
	int i = 0, index;
	unsigned char *argb = NULL, filename[256];

	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	CLEAR(req);
	req.count = 4;
	req.memory = V4L2_MEMORY_MMAP;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(fd, VIDIOC_REQBUFS, &req);

	for (i = 0; i < 4; i++) {
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.index = i;
		buf.memory = V4L2_MEMORY_MMAP;
		xioctl(fd, VIDIOC_QUERYBUF, &buf);
		frm_buf[i].len = buf.length;
		frm_buf[i].offset = mmap(NULL, buf.length, PROT_READ, MAP_SHARED, fd,
				buf.m.offset);
		if (frm_buf[i].offset == NULL) {
			fprintf(stderr, "mmap fail\n");
			xioctl(fd, VIDIOC_STREAMOFF, &type);
			return -1;
		}
	}
	temp=malloc(frm_buf[0].len);
	xioctl(fd, VIDIOC_STREAMON, &type);

	for (i = 0; i < 4; i++) {
		CLEAR(buf);
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		xioctl(fd, VIDIOC_QBUF, &buf);
	}

	for (i = 0; i < count; i++) {
		CLEAR(buf);
		index = count % 4;
		buf.index = index;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		xioctl(fd, VIDIOC_DQBUF, &buf);
		//copy the image to a temp buffer since the image data can be read only once times.
		memcpy(temp,frm_buf[index].offset,buf.bytesused);

		sprintf(filename, "image.%02d.%s\0", i, pix_format);
		image = fopen(filename, "w+");
		//fwrite(frm_buf[index].offset, buf.bytesused, 1, image);
		fwrite(temp, buf.bytesused, 1, image);
		fclose(image);
		argb=NULL;
		if (strncmp(pix_format, default_pix_format, 4) == 0) {
			//argb = yu12_to_argb(frm_buf[index].offset, w, h, 255);
			argb = yu12_to_argb(temp, w, h, 255);
		}
		if (strncmp(pix_format, "YUYV\0", 4) == 0) {
			//argb = yuyv_to_argb(frm_buf[index].offset, w, h, 255);
			argb = yuyv_to_argb(temp, w, h, 255);
		}
		if (strncmp(pix_format, "422P\0", 4) == 0) {
			//argb = yuv422p_to_argb(frm_buf[index].offset, w, h, 255);
			argb = yuv422p_to_argb(temp, w, h, 255);
		}
		sprintf(filename, "image.%02d.png\0", i);
		write_image_to_png(filename, argb);

		xioctl(fd, VIDIOC_QBUF, &buf);
		printf("\t\e[1;32m%2d images captured\e[0m\n",i+1);
	}
	xioctl(fd, VIDIOC_STREAMOFF, &type);
	for (i = 0; i < 4; i++) {
		munmap(frm_buf[i].offset, frm_buf[i].len);
	}
	free(temp);
	return 0;
}

char *short_opt = "c:d:w:h:i:p:";
struct option long_opt[] =
		{ { "count", optional_argument, NULL, 'c' }, { "device",
				optional_argument, NULL, 'd' }, { "width", optional_argument,
		NULL, 'w' }, { "height", optional_argument, NULL, 'h' }, { "input",
		optional_argument, NULL, 'i' }, { "pix_format", optional_argument, NULL,
				'p' }, };

void print_help(char *program_name) {
	printf("%s [options]\n"
			"-c|--count=capture count	\tdefault:4\n"
			"-d|--device=Path to device	\tdefault:'/dev/video0'\n"
			"-w|--width=capture width	\tdefault:800\n"
			"-h|--height=capture height	\tdefault:600\n"
			"-i|--input=device input channel	\tdefault:0\n"
			"-f|--pix_format=fourcc pix format	\tdefault:'YU12'\n"
			"Becare\n"
			"Only YUYV,YU12,422P pix format could be write to png file\n"
			"width,height and pix_foramt could be modified by the camera driver to make it compatible\n"
			"example:'%s -d /dev/video1 -w 800 -h 600 -p YU12 -c 10'\n",
			program_name,program_name);
	return;
}

int main(int argc, char *argv[]) {
	struct v4l2_format fmt;
	char fourcc[8];
	char opt,*c;
	int index, ret=7;
	input = 0;
	count = 4;
	device_path = default_device;
	pix_format = calloc(1, 5);
	memcpy(pix_format,default_pix_format,4);
	do{
		opt = getopt_long(argc, argv, short_opt, long_opt, &index);
		if (opt != -1 && opt!= 255)
			switch (opt) {
			case 'd':
				device_path = calloc(1, strlen(optarg));
				memcpy(device_path, optarg, strlen(optarg));
				break;
			case 'p':
				if (strlen(optarg) != 4) {
					perror("Unknow Pix Format");
					return -1;
				}
				memcpy(pix_format, optarg, strlen(optarg));
				pix_format[0] = toupper(pix_format[0]);
				pix_format[1] = toupper(pix_format[1]);
				pix_format[2] = toupper(pix_format[2]);
				pix_format[3] = toupper(pix_format[3]);
				break;
			case 'w':
				width = atoi(optarg);
				break;
			case 'h':
				height = atoi(optarg);
				break;
			case 'i':
				input = atoi(optarg);
				break;
			case 'c':
				count = atoi(optarg);
				if (count == 0)
					count = 1;
				break;
			default:
				print_help(argv[0]);
				exit(0);
			}
	}while (ret--!=0 && opt != -1 && opt!= 255) ;
	if (width == 0)
		width = 800;
	if (height == 0)
		height = 600;
	//carefully,nonblock open the device.
	int fd = open(device_path, O_RDWR | O_NONBLOCK, 0);
	if (fd < 0) {
		perror("Could not open the device");
		return -1;
	}
	//Select the O input channel of the Camera.
	xioctl(fd, VIDIOC_S_INPUT, &input);

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width; //Capture Image Width
	fmt.fmt.pix.height = height; //Capture Image Height
	fmt.fmt.pix.pixelformat = v4l2_fourcc(pix_format[0], pix_format[1],
			pix_format[2], pix_format[3]); //Set Pix Format.
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	//Let's the camera driver to correct the error settings of the fmt struct.
	xioctl(fd, VIDIOC_TRY_FMT, &fmt);
	width=fmt.fmt.pix.width;
	height=fmt.fmt.pix.height;
	c=&fmt.fmt.pix.pixelformat;
	memcpy(pix_format,c,4);

	//set the camera capture format settings.
	xioctl(fd, VIDIOC_S_FMT, &fmt);
	printf("\e[1;31mDevice:%s\n",device_path);
	printf("VIDIOC_S_FMT:%4dx%4d\t|Pix-format:%s\t|Image Size:%d Byte\e[0m\n", fmt.fmt.pix.width,
			fmt.fmt.pix.height, pix_format,fmt.fmt.pix.sizeimage);
	if(access("image",F_OK)!=0)
	mkdir("image",S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH);
	if(access("image",F_OK)==0){
		chdir("image");
	}else{
		perror("chdir 'image' Fail,capture images are saved in current directory");
	}

	printf("\e[1;34mCapture %d images\e[0m\n ",count);
	get_image_mmap(fd, fmt.fmt.pix.width, fmt.fmt.pix.height);
	close(fd);
	return 0;
}

