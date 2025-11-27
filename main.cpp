#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Hello Qt");
    window.resize(320, 200);

    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel("Qt is up and running!");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    window.show();
    return app.exec();
}
