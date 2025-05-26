#pragma once

#include <QString>

// Light theme stylesheet (as a raw string)
const QString lightStyleSheet = R"(
    QWidget {
        background-color:rgb(208, 208, 208);
        color: #000000;
    }
    QPushButton {
        background-color: #e0e0e0;
        color: #000000;
        border: 1px solid #cccccc;
        padding: 5px;
    }
    QPushButton:hover {
        background-color: #d0d0d0;
    }
)";

// Dark theme stylesheet
const QString darkStyleSheet = R"(
    QWidget {
        background-color:rgb(44, 44, 44);
        color: #e0e0e0;
    }
    QPushButton {
        background-color: #333333;
        color: #e0e0e0;
        border: 1px solid #555555;
        padding: 5px;
    }
    QPushButton:hover {
        background-color: #444444;
    }
)";