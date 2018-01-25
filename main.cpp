#include "mainwindow.h"
#include <QApplication>
#include <RunGuard.h>
#include <QMessageBox>

#include <iostream>

int main(int argc, char *argv[])
{
    //std::cout << "main()" << std::endl;

    QApplication a(argc, argv);

    RunGuard guard( "am_motion_correction_runguard" );
    if (!guard.tryToRun() ){
        QMessageBox* msgBox = new QMessageBox;
        msgBox->setWindowTitle("Error");
        msgBox->setText("Other instance is still running. Please close it first.");
        msgBox->setIcon(QMessageBox::Critical);
        msgBox->setWindowFlags(Qt::WindowStaysOnTopHint);
        msgBox->show();
        a.exec();
        return -1;
    }
    MainWindow w;
    w.setFixedSize(w.size());
    w.show();

    return a.exec();
}
