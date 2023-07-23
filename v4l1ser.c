#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>            
#include <fcntl.h>             
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <asm/types.h>         
#include <linux/videodev2.h>
#include <linux/fb.h>

/*
*  socket 所用头文件
*/

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define SERVER_PORT		6000
#define CLEAR(x) memset (&(x), 0, sizeof (x))
 
struct mybuffersend{
	unsigned char rgb24[640*480*3];
	int height;
	int width;
};
//视频格式转换
static int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;
    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r ;
    pixel[1] = g ;
    pixel[2] = b ;
    return pixel32;
}

static int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;

    for(in = 0; in < width * height * 2; in += 4)
    {
        pixel_16 =
                yuv[in + 3] << 24 |
                               yuv[in + 2] << 16 |
                                              yuv[in + 1] <<  8 |
                                                              yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }
    return 0;
}


int main(void)
{
	int socket_fd;
	int connect_fd;
	struct mybuffersend sendBuffer;
	struct sockaddr_in servaddr;
	struct sockaddr_in client;
	struct in_addr client_in_a;
	socklen_t caddrlen = sizeof(client);
	 //初始化Socket
    if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
    	printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
    	exit(0);
    }

	   //初始化
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。
    servaddr.sin_port = htons(SERVER_PORT);//设置的端口为DEFAULT_PORT

	 //将本地地址绑定到所创建的套接字上
    if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
    	printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
    	exit(0);
    }

  	//开始监听是否有客户端连接
    if( listen(socket_fd, 10) == -1){
    	printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
    	exit(0);
    }

	if( (connect_fd = accept(socket_fd, (struct sockaddr*)&client, &caddrlen)) == -1){
		printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
	}

	printf("new client connected.IP:%s,port:%u\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

	//1.打开设备
	int fd = open("/dev/video0", O_RDWR);
	if(fd < 0)
	{
		perror("打开设备失败");
		return -1;
	}

	//3.设置采集格式
	struct v4l2_format vfmt;
	vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//摄像头采集
	vfmt.fmt.pix.width = 640;//设置宽（不能任意）
	vfmt.fmt.pix.height = 480;//设置高
	vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//设置视频采集格式
	int ret = ioctl(fd, VIDIOC_S_FMT, &vfmt);
	if(ret < 0)
	{
		perror("设置格式失败");
	}

	//4.申请内核空间
	struct v4l2_requestbuffers reqbuffer;
	reqbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuffer.count = 4; //申请4个缓冲区
	reqbuffer.memory = V4L2_MEMORY_MMAP ;//映射方式
	ret  = ioctl(fd, VIDIOC_REQBUFS, &reqbuffer);
	if(ret < 0)
	{
		perror("申请队列空间失败");
	}


	//5.映射
	unsigned char *mptr[4];//保存映射后用户空间的首地址
    unsigned int  size[4];
	struct v4l2_buffer mapbuffer;
	//初始化type, index
	mapbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i;
	for(i=0; i<4; i++)
	{
		mapbuffer.index = i;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &mapbuffer);//从内核空间中查询一个空间做映射
		if(ret < 0)
		{
			perror("查询内核空间队列失败");
		}
		mptr[i] = (unsigned char *)mmap(NULL, mapbuffer.length, PROT_READ|PROT_WRITE, 
                                            MAP_SHARED, fd, mapbuffer.m.offset);
        size[i] = mapbuffer.length;

		//通知使用完毕--‘放回去’
		ret  = ioctl(fd, VIDIOC_QBUF, &mapbuffer);
		if(ret < 0)
		{
			perror("放回失败");
		}
	}
while (1)
{
	//6.开始采集
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_STREAMON, &type);
	if(ret < 0)
	{
		perror("开启失败");
	}

    //从队列中提取一帧数据
	struct v4l2_buffer  readbuffer;
	readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_DQBUF, &readbuffer);
	if(ret < 0)
	{
		perror("提取数据失败");
	}

	CLEAR (sendBuffer);
	sendBuffer.width = vfmt.fmt.pix.width;
	sendBuffer.height = vfmt.fmt.pix.height;
	convert_yuv_to_rgb_buffer((unsigned char*)mptr[readbuffer.index],sendBuffer.rgb24, sendBuffer.width, sendBuffer.height);

	int num = send(connect_fd, (void *)&sendBuffer,sizeof(sendBuffer),0);
	if(	num < 0)
	{
		printf("error: SEND_BUFF_ERR %s\n", __func__);
		return 3;
	}

	// FILE *file=fopen("my.yuyv", "w+");
	// fwrite(mptr[readbuffer.index], readbuffer.length, 1, file);
	// fclose(file);
	//通知内核已经使用完毕
	ret = ioctl(fd, VIDIOC_QBUF, &readbuffer);

	if(ret < 0)
	{
		perror("放回队列失败");
	}
}

   
    //8. 停止采集
    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);

    //9.释放映射
  //  for(i=0; i<4; i)
  //      munmap(mptr[i], size[i]);
	//9.关闭设备

	close(fd);
	return 0;
}
