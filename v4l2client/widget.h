#ifndef WIDGET_H
#define WIDGET_H

#include <QMainWindow>
#include <QTcpSocket>

/*视频buffer*/
struct bufferrecv{
    unsigned char rgb24[640*480*3];
    int height;
    int width;
};
namespace Ui {
class Widget;
}

class Widget : public QMainWindow
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private slots:

    //void on_connected_video();
    void on_connected();

    void recv_Video();

    void error_video();


private:
     qint64 imageBlockSize;
    QTcpSocket ptcpSocket_video;   //视频套接字
    Ui::Widget *ui;
};

#endif // WIDGET_H
