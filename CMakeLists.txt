cmake_minimum_required(VERSION 3.25)

project(poisonlog VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Sql
    Network
    Test
)

qt_standard_project_setup(REQUIRES 6.12)

qt_add_library(poisonlog_core STATIC
    src/database.h
    src/database.cpp
    src/recordlistmodel.h
    src/recordlistmodel.cpp
    src/rectanglelistmodel.h
    src/rectanglelistmodel.cpp
    src/geocoder.h
    src/geocoder.cpp
    src/tileimageprovider.h
    src/tileimageprovider.cpp
    src/appstore.h
    src/appstore.cpp
)

target_include_directories(poisonlog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(poisonlog_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Sql
    Qt6::Network
)

qt_add_executable(poisonlog src/main.cpp)

qt_add_qml_module(poisonlog
    URI PoisonLog
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/MapSetupPage.qml
        qml/MapCanvas.qml
        qml/MapView.qml
        qml/RecordList.qml
        qml/EditorPanel.qml
)

target_link_libraries(poisonlog PRIVATE poisonlog_core Qt6::QuickControls2)

enable_testing()

qt_add_executable(tst_database tests/tst_database.cpp)
target_link_libraries(tst_database PRIVATE poisonlog_core Qt6::Test)
add_test(NAME tst_database COMMAND tst_database)

qt_add_executable(tst_appstore tests/tst_appstore.cpp)
target_link_libraries(tst_appstore PRIVATE poisonlog_core Qt6::Test)
add_test(NAME tst_appstore COMMAND tst_appstore)
