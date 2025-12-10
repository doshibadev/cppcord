#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Discord Client");
    window.resize(800, 600);

    QWidget *centralWidget = new QWidget(&window);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    QPushButton *button = new QPushButton("Login to Discord", centralWidget);
    layout->addWidget(button);

    QObject::connect(button, &QPushButton::clicked, []()
                     { qDebug() << "Login button clicked!"; });

    window.setCentralWidget(centralWidget);
    window.show();

    return app.exec();
}
