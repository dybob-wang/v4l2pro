#include "widget.h"
#include "ui_widget.h"
#include <QHostAddress>
#include <QString>
#include <QDebug>
#include <QByteArray>
#include <QImage>
#include <QMessageBox>
#include <QTcpSocket>
Widget::Widget(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Widget)
{
     this->setWindowTitle("客户端");

     ptcpSocket_video.connectToHost(QHostAddress("192.168.110.188"),6000);
   //  connect(&ptcpSocket_video,SIGNAL(connected()),this,SLOT(on_connected_video()));
     connect(&ptcpSocket_video,SIGNAL(readyRead()),this,SLOT(recv_Video()));
     connect(&ptcpSocket_video,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(error_video()));
     ui->setupUi(this);

}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_connected()
{
    qDebug()<<"视频连接成功 41";
    QMessageBox::information(this,"确认","视频连接成功");
}

void Widget::recv_Video()
{
    bufferrecv  recvbuf;
    if (ptcpSocket_video.bytesAvailable() < sizeof(recvbuf))
    {
            return;
    }

    memset(&recvbuf,0,sizeof(recvbuf));
    imageBlockSize = ptcpSocket_video.read((char *)&recvbuf,sizeof(recvbuf));

    qDebug() <<"readsize "<<imageBlockSize<<"width:"<<recvbuf.width<<" height:"<<recvbuf.height;

    QImage img = QImage((unsigned char *)(recvbuf.rgb24),recvbuf.width,recvbuf.height,QImage::Format_RGB888);

    ui->label->clear();

    ui->label->setPixmap(QPixmap::fromImage(img.scaled(ui->label->size())));
}

void Widget::error_video()
{
    ptcpSocket_video.disconnectFromHost();
    ptcpSocket_video.close();
}
