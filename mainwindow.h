#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QListWidgetItem>
#include <QGraphicsScene>
#include <QMap>

#include "Fixture.h"
#include "stagelayout.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ArtNetSender;
class ArtNetReceiver;
class Universe;
class DMXUSBWidget;
class FixtureItem;
class AddressPage;
class LibraryPage;
class Globe3D;
class QStackedWidget;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // 灯库
    void on_librarySearch_textChanged(const QString &text);
    void on_libraryList_itemDoubleClicked(QListWidgetItem *item);
    void on_addFixtureBtn_clicked();
    void on_removeFixtureBtn_clicked();

    // 已添加灯具
    void on_fixtureList_itemSelectionChanged();
    void on_fixtureList_itemDoubleClicked(QListWidgetItem *item);

    // 控制
    void on_dimmerSlider_valueChanged(int value);
    void on_redSlider_valueChanged(int value);
    void on_greenSlider_valueChanged(int value);
    void on_blueSlider_valueChanged(int value);
    void on_fullButton_clicked();
    void on_blackoutButton_clicked();
    void on_backButton_clicked();

    // 2D 视图
    void onFixtureItemSelected(FixtureItem *item);

    // 输出
    void on_artnetRadio_toggled(bool checked);
    void on_dmxRadio_toggled(bool checked);
    void on_connectButton_clicked();
    void on_manualIpBtn_clicked();

    // 菜单
    void on_actionNew_triggered();
    void on_actionSave_triggered();
    void on_actionExit_triggered();
    void on_actionAddressPage_triggered();

private:
    void initFixtureLibrary();
    void addFixtureToCurrent(const FixtureDef &def, const QPointF &pos = QPointF());
    void removeFixture(Fixture *f);
    void selectFixture(Fixture *f);
    void sendDmx();
    void sendDmxRaw(const QByteArray &data);
    void onArtNetReceived(uint16_t universe, const QByteArray &data);
    void saveAllData();
    void loadAllData();
    void updateChannelControls();
    void refreshFixtureList();
    void showControlMode(Fixture *f);
    void showLibraryMode();
    void switchDomain(int domainIndex);
    void rebuildControlPanel(Fixture *f);
    void refreshLibraryPage();
    Universe *currentUniverse() const;
    void updateProgramPageInfo();

    Ui::MainWindow       *ui;
    QGraphicsScene       *m_scene       = nullptr;
    QStackedWidget       *m_rightStack  = nullptr;
    QStackedWidget       *m_viewStack   = nullptr;
    AddressPage          *m_addressPage = nullptr;
    LibraryPage          *m_libraryPage = nullptr;
    class ProgramPage    *m_programPage = nullptr;
    class DmxMonitor     *m_dmxMonitor  = nullptr;
    Globe3D              *m_globe3D     = nullptr;
    StageLayout          *m_stageLayout = nullptr;
    QPushButton          *m_prevBtn     = nullptr;
    QPushButton          *m_playBtn     = nullptr;
    QPushButton          *m_stopBtn     = nullptr;
    QPushButton          *m_nextBtn     = nullptr;
    bool                  m_timelinePlaying = false;
    void syncPlayBtn();
    QPushButton          *m_btn2D       = nullptr;
    QPushButton          *m_btn3D       = nullptr;
    QPushButton          *m_colorBtn    = nullptr;
    QWidget              *m_beamWidget  = nullptr;
    QStackedWidget       *m_masterStack = nullptr;
    class Timeline       *m_timeline    = nullptr;
    ArtNetSender         *m_artnet      = nullptr;
    ArtNetReceiver       *m_receiver    = nullptr;
    DMXUSBWidget         *m_dmxDevice   = nullptr;
    bool                  m_connected   = false;

    QList<Universe *>     m_universes;         // 域1, 域2, ...
    int                   m_currentDomain = 0;
    Fixture              *m_selected     = nullptr;
    QList<FixtureDef>     m_library;
    QMap<Fixture *, FixtureItem *> m_fixtureItems;
    QMap<Fixture *, QString> m_stageMap;  // fixture → stage position name

    bool                  m_useArtnet    = true;
    bool                  m_deleteMode   = false;
    bool                  m_switchingDomain = false;
};

#endif // MAINWINDOW_H
