#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ArtNetSender.h"
#include "ArtNetReceiver.h"
#include "Universe.h"
#include "fixtureitem.h"
#include "dropview.h"
#include "draglibrarylist.h"
#include "fixturedialog.h"
#include "addresspage.h"
#include "librarypage.h"
#include "programpage.h"
#include "globe3d.h"
#include "colorwheel.h"

#include <QNetworkInterface>
#include "beamwidget.h"
#include "timeline.h"
#include "dmxmonitor.h"
#include "dmxusbwidget.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QMenu>
#include <QRegularExpression>
#include <QMessageBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollArea>
#include <QStyle>
#include <QApplication>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ===== 2D 视图场景 =====
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-400, -300, 800, 600);
    m_scene->setBackgroundBrush(QBrush(QColor(0xf5, 0xf5, 0xf5)));
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);

    // 拖拽到 2D 视图
    connect(ui->graphicsView, &DropView::fixtureDropped, this,
        [this](int idx, const QPointF &pos) {
            if (idx >= 0 && idx < m_library.size())
                addFixtureToCurrent(m_library[idx], pos);
        });

    // 场景选择变化（仅在用户主动点击 2D 场景时触发）
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        auto sel = m_scene->selectedItems();
        if (!sel.isEmpty()) {
            auto *item = dynamic_cast<FixtureItem *>(sel.first());
            if (item) onFixtureItemSelected(item);
        }
    });

    // ===== 3D 视图 + 切换 =====
    m_globe3D = new Globe3D;
    m_viewStack = new QStackedWidget;
    ui->viewLayout->removeWidget(ui->graphicsView);
    m_viewStack->addWidget(ui->graphicsView);   // index 0 = 2D
    m_viewStack->addWidget(m_globe3D);          // index 1 = 3D
    ui->viewLayout->insertWidget(0, m_viewStack);

    // 2D/3D 切换按钮
    auto *btnBar = new QWidget;
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(2, 2, 2, 2); btnLayout->setSpacing(2);
    m_btn2D = new QPushButton("2D");
    m_btn3D = new QPushButton("3D");
    m_btn2D->setFixedSize(40, 24); m_btn3D->setFixedSize(40, 24);
    m_btn2D->setStyleSheet("background:#335;color:#fff;font-weight:bold");
    m_btn3D->setStyleSheet("background:#222;color:#888");
    btnLayout->addWidget(m_btn2D);
    btnLayout->addWidget(m_btn3D);
    btnLayout->addStretch();
    ui->viewLayout->insertWidget(0, btnBar);

    connect(m_btn2D, &QPushButton::clicked, this, [this]() {
        m_viewStack->setCurrentIndex(0);
        m_btn2D->setStyleSheet("background:#335;color:#fff;font-weight:bold");
        m_btn3D->setStyleSheet("background:#222;color:#888");
    });
    connect(m_btn3D, &QPushButton::clicked, this, [this]() {
        m_viewStack->setCurrentIndex(1);
        m_btn3D->setStyleSheet("background:#335;color:#fff;font-weight:bold");
        m_btn2D->setStyleSheet("background:#222;color:#888");
    });

    // 3D 视图选中灯具 → 同步控制面板
    connect(m_globe3D, &Globe3D::fixtureSelected, this, [this](Fixture *f) {
        if (f) { m_selected = f; showControlMode(f); }
    });
    connect(m_globe3D, &Globe3D::fixtureDeselected, this, [this]() {
        m_selected = nullptr; showLibraryMode();
    });
    // 3D 拖拽灯具 → 同步 2D 位置
    connect(m_globe3D, &Globe3D::fixtureMoved3D, this, [this](Fixture *f, QPointF pos2D) {
        if (auto *it = m_fixtureItems.value(f))
            it->setPos(pos2D);
    });

    // ===== 灯库 =====
    initFixtureLibrary();

    // ===== 右侧 QStackedWidget（先创建，switchDomain 要用）=====
    m_rightStack = new QStackedWidget;
    m_rightStack->addWidget(ui->rightSplitter);
    m_libraryPage = new LibraryPage;
    m_libraryPage->setLibrary(m_library);
    m_addressPage = new AddressPage;
    m_programPage = new ProgramPage;
    m_dmxMonitor  = new DmxMonitor;

    // 舞台选择页（wrapper：StageLayout + 返回按钮）
    auto *stagePage = new QWidget;
    auto *stagePageLayout = new QVBoxLayout(stagePage);
    stagePageLayout->setContentsMargins(0, 0, 0, 0);
    m_stageLayout = new StageLayout;
    stagePageLayout->addWidget(m_stageLayout, 1);

    // 底部栏：返回 + 提示
    auto *stageBottom = new QWidget;
    stageBottom->setFixedHeight(36);
    stageBottom->setStyleSheet("background:#f0f0f0; border-top:1px solid #ddd;");
    auto *sbl = new QHBoxLayout(stageBottom);
    sbl->setContentsMargins(12, 4, 12, 4);
    auto *stageBackBtn = new QPushButton("← 返回");
    stageBackBtn->setFixedHeight(28);
    stageBackBtn->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:3px;padding:0 12px;");
    connect(stageBackBtn, &QPushButton::clicked, this, [this]() { m_masterStack->setCurrentIndex(0); });
    sbl->addWidget(stageBackBtn);
    sbl->addStretch();
    auto *stageHint = new QLabel("选择灯具 → 点击右下角「进入编程」");
    stageHint->setStyleSheet("color:#888; font-size:11px; border:none;");
    sbl->addWidget(stageHint);
    stagePageLayout->addWidget(stageBottom);

    // Stage "进入编程" → 打开编程编辑页
    connect(m_stageLayout, &StageLayout::enterProgramRequested, this, [this]() {
        // Pass fixture info from selected/assigned stage positions
        QStringList selNames = m_stageLayout->selectedNames();
        if (selNames.isEmpty()) {
            for (auto it = m_stageMap.begin(); it != m_stageMap.end(); ++it)
                selNames << it.value();
            selNames.removeDuplicates();
        }
        if (selNames.isEmpty()) {
            QMessageBox::information(this, "提示", "请先在舞台视图中选择灯具");
            return;
        }
        m_programPage->setBoundPositions(selNames);
        m_programPage->setStageLayout(m_stageLayout);

        // Find fixture model from assigned fixtures
        QString model = "--";
        int channels = 0;
        for (auto it = m_stageMap.begin(); it != m_stageMap.end(); ++it) {
            if (selNames.contains(it.value())) {
                model = it.key()->name();
                channels = it.key()->channelCount();
                break;
            }
        }
        m_programPage->setFixtureInfo(model, channels);
        // 查找绑定到这些舞台位置的灯具，传给编程页
        QList<Fixture *> progFixtures;
        for (auto it = m_stageMap.begin(); it != m_stageMap.end(); ++it) {
            if (selNames.contains(it.value())) progFixtures << it.key();
        }
        m_programPage->setFixtures(progFixtures);
        updateProgramPageInfo();
        m_stageLayout->clearSelection();  // clear blue highlight, keep green occupied
        m_masterStack->setCurrentIndex(4);
    });

    // Program page save → update stage labels
    connect(m_programPage, &ProgramPage::saved, this, [this](const QString &groupName, const QStringList &positions) {
        for (const auto &pos : positions)
            m_stageLayout->setPositionLabel(pos, groupName);
    });

    // 全屏叠加层
    m_masterStack = new QStackedWidget;
    m_masterStack->addWidget(ui->centralwidget);   // 0 = 正常布局
    m_masterStack->addWidget(m_libraryPage);       // 1 = 全屏灯库
    m_masterStack->addWidget(m_addressPage);       // 2 = 域管理
    m_masterStack->addWidget(stagePage);           // 3 = 舞台选择
    m_masterStack->addWidget(m_programPage);       // 4 = 编程编辑
    m_masterStack->addWidget(m_dmxMonitor);        // 5 = DMX 监视器
    setCentralWidget(m_masterStack);
    connect(m_libraryPage, &LibraryPage::goBackRequested, this, [this]() { m_masterStack->setCurrentIndex(0); });
    connect(m_libraryPage, &LibraryPage::libraryUpdated, this, [this](const QList<FixtureDef> &lib) {
        m_library = lib;
        ui->libraryList->clear();
        for (const auto &d : m_library)
            ui->libraryList->addItem(QString("%1 [%2] %3ch").arg(d.name, d.manufacturer).arg(d.channels));
        refreshLibraryPage();
    });
    int idx = ui->rootLayout->indexOf(ui->rightSplitter);
    auto *item = ui->rootLayout->takeAt(idx);
    delete item;
    ui->rootLayout->insertWidget(idx, m_rightStack);

    connect(m_addressPage, &AddressPage::backRequested, this, [this]() { m_masterStack->setCurrentIndex(0); });
    connect(m_addressPage, &AddressPage::domainSwitched, this, [this](int idx) { switchDomain(idx); });
    connect(m_programPage, &ProgramPage::backRequested, this, [this]() { m_masterStack->setCurrentIndex(0); });
    connect(m_programPage, &ProgramPage::backToStageRequested, this, [this]() { m_masterStack->setCurrentIndex(3); });
    connect(m_dmxMonitor, &DmxMonitor::goBackRequested, this, [this]() { m_masterStack->setCurrentIndex(0); });
    // ProgramPage 模板编辑 → DMX 输出
    connect(m_programPage, &ProgramPage::dmxOutputRequested, this, &MainWindow::sendDmx);
    // 地址页：新建域
    connect(m_addressPage, &AddressPage::newDomainRequested, this, [this](const QString &model, int ch) {
        while (m_universes.size() <= m_addressPage->curDomain())
            m_universes << new Universe(m_universes.size(), this);
    });
    // 地址页：删除域
    connect(m_addressPage, &AddressPage::deleteDomainRequested, this, [this](const QString &model) {
        // 删除匹配的所有灯具
        auto fixtures = currentUniverse()->fixtures();
        for (int i = fixtures.size() - 1; i >= 0; i--)
            if (fixtures[i]->name() == model) removeFixture(fixtures[i]);
    });

    // 菜单：灯库 → 灯库详情页（替换空子菜单为直触点）
    {
        ui->menuFixture->menuAction()->setVisible(false);
        auto *actLib = new QAction("灯库", this);
        connect(actLib, &QAction::triggered, this, [this]() {
            m_libraryPage->setLibrary(m_library);
            m_masterStack->setCurrentIndex(1);
        });
        ui->menubar->insertAction(ui->menuProgram->menuAction(), actLib);
    }
    // 菜单：地址码 → 直接显示
    {
        ui->menuAddress->menuAction()->setVisible(false);
        auto *actAddr = new QAction("地址码", this);
        connect(actAddr, &QAction::triggered, this, [this]() {
            on_actionAddressPage_triggered();
        });
        ui->menubar->insertAction(ui->menuProgram->menuAction(), actAddr);
    }
    // 菜单"编程" → 先进入舞台选择页
    ui->menuProgram->menuAction()->setVisible(false);
    auto *actProg = new QAction("编程", this);
    connect(actProg, &QAction::triggered, this, [this]() {
        m_masterStack->setCurrentIndex(3);
    });
    ui->menubar->insertAction(ui->menuWindow->menuAction(), actProg);

    // 菜单：DMX 监视器
    auto *actDmxMon = new QAction("DMX监视", this);
    connect(actDmxMon, &QAction::triggered, this, [this]() {
        m_masterStack->setCurrentIndex(5);
    });
    ui->menubar->insertAction(ui->menuWindow->menuAction(), actDmxMon);

    // ===== 域1（必须在 m_addressPage 之后）=====
    m_universes << new Universe(0, this);
    switchDomain(0);  // 初始化视图

    // Art-Net 发送器在用户点击"连接"时创建，此处不再预创建

    // 调色盘按钮（控制页左上角，放在返回按钮右边）
    m_colorBtn = new QPushButton;
    m_colorBtn->setFixedSize(28, 28);
    m_colorBtn->setToolTip("调色盘");
    m_colorBtn->setStyleSheet(
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 red,stop:0.17 yellow,stop:0.33 lime,"
        "stop:0.5 cyan,stop:0.67 blue,stop:0.83 magenta,stop:1 red);"
        "border-radius:14px;border:1px solid #555");
    // 找到 backButton 所在的 layout 并把调色盘按钮加进去
    if (auto *headerLayout = ui->backButton->parentWidget()->layout()) {
        auto *ql = qobject_cast<QBoxLayout *>(headerLayout);
        if (ql) ql->insertWidget(2, m_colorBtn); // backButton(0), controlTitle(1), colorBtn(2)
    }

    connect(m_colorBtn, &QPushButton::clicked, this, [this]() {
        if (!m_selected) return;
        ColorPickerDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            QColor c = dlg.selectedColor();
            // 找 R/G/B 通道并设置
            for (int ch = 0; ch < m_selected->channelCount(); ch++) {
                QString name = m_selected->channelName(ch).toLower();
                if (name == "r" || name == "red")
                    m_selected->setChannel(ch, c.red());
                else if (name == "g" || name == "green")
                    m_selected->setChannel(ch, c.green());
                else if (name == "b" || name == "blue")
                    m_selected->setChannel(ch, c.blue());
            }
            sendDmx();
            rebuildControlPanel(m_selected);
        }
    });

    // 时间线播放控制栏
    auto *tlBar = new QWidget;
    tlBar->setFixedHeight(30);
    tlBar->setStyleSheet("background:#e8e8e8; border-bottom:1px solid #ccc;");
    auto *tlBarL = new QHBoxLayout(tlBar);
    tlBarL->setContentsMargins(6, 2, 6, 2);
    tlBarL->setSpacing(2);

    auto makeTlBtn = [](QStyle::StandardPixmap icon, const QString &tip) {
        auto *b = new QPushButton;
        b->setFixedSize(32, 24);
        b->setToolTip(tip);
        b->setIcon(QApplication::style()->standardIcon(icon));
        b->setIconSize(QSize(16, 16));
        b->setStyleSheet(
            "QPushButton { background:#ddd; border:1px solid #bbb; border-radius:3px; }"
            "QPushButton:hover { background:#e0e0e0; border-color:#99b; }");
        return b;
    };
    auto setIcon = [](QPushButton *b, QStyle::StandardPixmap icon) {
        b->setIcon(QApplication::style()->standardIcon(icon));
    };

    m_prevBtn  = makeTlBtn(QStyle::SP_MediaSeekBackward, "上一帧");
    m_playBtn  = makeTlBtn(QStyle::SP_MediaPlay,        "播放");
    m_stopBtn  = makeTlBtn(QStyle::SP_MediaStop,         "停止");
    m_nextBtn  = makeTlBtn(QStyle::SP_MediaSeekForward,  "下一帧");

    connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_timeline) { m_timeline->stepPrev(); m_timelinePlaying = false; syncPlayBtn(); }
    });
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (!m_timeline) return;
        if (m_timelinePlaying) { m_timeline->pause(); m_timelinePlaying = false; }
        else                   { m_timeline->play();  m_timelinePlaying = true;  }
        syncPlayBtn();
    });
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        if (m_timeline) { m_timeline->pause(); m_timeline->resetPlayhead(); }
        m_timelinePlaying = false;
        syncPlayBtn();
        // 停止时发送全零 DMX（黑场）
        QByteArray blackout(512, '\0');
        sendDmxRaw(blackout);
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        if (m_timeline) { m_timeline->stepNext(); m_timelinePlaying = false; syncPlayBtn(); }
    });

    tlBarL->addStretch();
    tlBarL->addWidget(m_prevBtn);
    tlBarL->addWidget(m_playBtn);
    tlBarL->addWidget(m_stopBtn);
    tlBarL->addWidget(m_nextBtn);
    tlBarL->addStretch();

    // 替换占位时间线
    m_timeline = new Timeline;
    ui->timelineLayout->addWidget(tlBar);
    ui->timelineLayout->addWidget(m_timeline);
    ui->timelinePlaceholder->hide();
    ui->timelineHeader->hide();

    // 时间线 → DMX 输出
    connect(m_timeline, &Timeline::playheadDmxReady, this,
        [this](double, const QByteArray &mergedDmx) {
            sendDmxRaw(mergedDmx);
        });
    // 时间线 → 捕获 DMX
    connect(m_timeline, &Timeline::captureRequested, this, [this]() {
        Universe *u = currentUniverse();
        if (u) {
            u->render();
            m_timeline->captureBlockDmx(u->data(), m_currentDomain);
        }
    });

    // 已添加灯具右键菜单
    ui->fixtureList->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->fixtureList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->fixtureList->setFocusPolicy(Qt::StrongFocus);
    connect(ui->fixtureList, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = ui->fixtureList->itemAt(pos);
        if (!item) return;
        int row = ui->fixtureList->row(item);

        // Find which fixture this row belongs to
        Fixture *targetFx = nullptr;
        Universe *srcUniv = nullptr;
        int cnt = 0;
        for (int di = 0; di < m_universes.size() && !targetFx; di++) {
            auto *u = m_universes[di];
            if (!u) continue;
            for (auto *f : u->fixtures()) {
                if (cnt == row) { targetFx = f; srcUniv = u; break; }
                cnt++;
            }
        }
        if (!targetFx) return;

        bool hasBinding = m_stageMap.contains(targetFx);
        QMenu menu;
        QAction *addAction = menu.addAction("添加到时间线");
        QAction *switchAction = menu.addAction("切换域");
        QAction *progAction = menu.addAction("添加编程");
        QAction *editBindAction = nullptr;
        QAction *removeBindAction = nullptr;
        if (hasBinding) {
            menu.addSeparator();
            editBindAction = menu.addAction("修改绑定");
            removeBindAction = menu.addAction("移除绑定");
        }
        QAction *chosen = menu.exec(ui->fixtureList->mapToGlobal(pos));
        if (chosen == addAction && m_timeline) {
            QString name = item->text();
            name = name.section("  ", 1, 1);
            if (name.isEmpty()) name = "测试灯";
            m_timeline->addBlockToFirstTrack(name);
        } else if (chosen == switchAction) {
            // Show domain picker dialog
            QDialog dlg(this);
            dlg.setWindowTitle("切换域");
            dlg.setStyleSheet("background:#fff");
            auto *dl = new QVBoxLayout(&dlg);
            dl->addWidget(new QLabel(QString("将 %1 切换到：").arg(targetFx->name())));
            auto *combo = new QComboBox;
            auto doms = m_addressPage->getDomains();
            // Ensure universes for all domains
            while (m_universes.size() < doms.size())
                m_universes << new Universe(m_universes.size(), this);
            for (int di = 0; di < doms.size(); di++) {
                QString label = QString("域 %1 — %2（%3通道）")
                    .arg(di + 1).arg(doms[di].model).arg(doms[di].channels);
                combo->addItem(label, di);
            }
            dl->addWidget(combo);
            auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            dl->addWidget(btns);
            if (dlg.exec() == QDialog::Accepted) {
                int targetDi = combo->currentData().toInt();
                if (targetDi >= 0 && targetDi < doms.size()) {
                    // Ensure universe exists
                    while (m_universes.size() <= targetDi)
                        m_universes << new Universe(m_universes.size(), this);
                    Universe *dstUniv = m_universes[targetDi];
                    if (dstUniv && dstUniv != srcUniv) {
                        // Get target domain info
                        auto doms = m_addressPage->getDomains();
                        if (targetDi < doms.size()) {
                            targetFx->setName(doms[targetDi].model);
                            // Update fixture channels to match new domain if needed
                            // (keep existing channel count for now)
                        }
                        srcUniv->removeFixture(targetFx);
                        targetFx->setUniverse(targetDi);
                        dstUniv->addFixture(targetFx);
                        if (auto *it = m_fixtureItems.value(targetFx))
                            it->updateFromData();
                        m_globe3D->updateFixture(targetFx);
                        refreshFixtureList();
                        sendDmx();
                        QList<Fixture *> allFx;
                        for (auto *uv : m_universes) allFx << uv->fixtures();
                        m_addressPage->updateFixtures(allFx);
                    }
                }
            }
        } else if (chosen == progAction && m_stageLayout) {
            QDialog dlg(this);
            dlg.setWindowTitle("添加编程 — " + targetFx->name());
            dlg.setFixedSize(300, 160);
            dlg.setStyleSheet("background:#fff");
            auto *pl = new QVBoxLayout(&dlg);
            pl->setSpacing(10);
            pl->setContentsMargins(16, 16, 16, 16);

            auto *row1 = new QHBoxLayout;
            row1->addWidget(new QLabel("添加为："));
            auto *typeCombo = new QComboBox(&dlg);
            typeCombo->setMinimumWidth(140);
            typeCombo->addItems({"面光", "逆光", "侧光", "顶光"});
            row1->addWidget(typeCombo);
            row1->addStretch();
            pl->addLayout(row1);

            auto *row2 = new QHBoxLayout;
            row2->addWidget(new QLabel("编号："));
            auto *numCombo = new QComboBox(&dlg);
            numCombo->setMinimumWidth(140);
            row2->addWidget(numCombo);
            row2->addStretch();
            pl->addLayout(row2);

            auto refreshNumbers = [&]() {
                QString group = typeCombo->currentText();
                numCombo->clear();
                int maxN = m_stageLayout->maxNumber(group);
                for (int n = 1; n <= maxN; n++) {
                    QString fullName = group + QString::number(n);
                    bool occ = m_stageLayout->isOccupied(fullName);
                    numCombo->addItem(occ ? QString::number(n) + "（占用）" : QString::number(n), fullName);
                }
            };
            refreshNumbers();
            connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    &dlg, refreshNumbers);

            pl->addStretch();
            auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            pl->addWidget(btns);

            if (dlg.exec() == QDialog::Accepted && numCombo->count() > 0) {
                QString posName = numCombo->currentData().toString();
                bool doAssign = true;
                if (m_stageLayout->isOccupied(posName)) {
                    auto answer = QMessageBox::question(&dlg, "覆盖确认",
                        posName + " 已被占用，是否覆盖？",
                        QMessageBox::Yes | QMessageBox::No);
                    doAssign = (answer == QMessageBox::Yes);
                }
                if (doAssign) {
                    if (m_stageMap.contains(targetFx))
                        m_stageLayout->unassignPosition(m_stageMap[targetFx]);
                    m_stageLayout->assignPosition(posName);
                    m_stageMap[targetFx] = posName;
                }
            }
        } else if (editBindAction && chosen == editBindAction && m_stageLayout) {
            // 修改绑定：打开相同对话框，预选当前绑定
            QString curName = m_stageMap.value(targetFx);
            QString curGroup;
            int curNum = 0;
            for (const auto &g : QStringList{"面光", "逆光", "侧光", "顶光"}) {
                if (curName.startsWith(g)) { curGroup = g; curNum = curName.mid(g.length()).toInt(); break; }
            }

            QDialog dlg(this);
            dlg.setWindowTitle("修改绑定 — " + targetFx->name());
            dlg.setFixedSize(300, 160);
            dlg.setStyleSheet("background:#fff");
            auto *pl = new QVBoxLayout(&dlg);
            pl->setSpacing(10); pl->setContentsMargins(16, 16, 16, 16);

            auto *row1 = new QHBoxLayout;
            row1->addWidget(new QLabel("添加为："));
            auto *typeCombo = new QComboBox(&dlg);
            typeCombo->setMinimumWidth(140);
            typeCombo->addItems({"面光", "逆光", "侧光", "顶光"});
            if (!curGroup.isEmpty()) typeCombo->setCurrentText(curGroup);
            row1->addWidget(typeCombo); row1->addStretch();
            pl->addLayout(row1);

            auto *row2 = new QHBoxLayout;
            row2->addWidget(new QLabel("编号："));
            auto *numCombo = new QComboBox(&dlg);
            numCombo->setMinimumWidth(140);
            row2->addWidget(numCombo); row2->addStretch();
            pl->addLayout(row2);

            auto refreshNumbers = [&]() {
                QString group = typeCombo->currentText();
                numCombo->clear();
                int selIdx = 0, i = 0;
                for (int n = 1; n <= m_stageLayout->maxNumber(group); n++) {
                    QString fn = group + QString::number(n);
                    bool occ = m_stageLayout->isOccupied(fn);
                    numCombo->addItem(occ ? QString::number(n) + "（占用）" : QString::number(n), fn);
                    if (fn == curName) selIdx = i;
                    i++;
                }
                if (selIdx < numCombo->count()) numCombo->setCurrentIndex(selIdx);
            };
            refreshNumbers();
            connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, refreshNumbers);

            pl->addStretch();
            auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            pl->addWidget(btns);

            if (dlg.exec() == QDialog::Accepted && numCombo->count() > 0) {
                QString posName = numCombo->currentData().toString();
                if (posName != curName && m_stageLayout->isOccupied(posName)) {
                    if (QMessageBox::question(&dlg, "覆盖确认",
                            posName + " 已被占用，是否覆盖？",
                            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                        return; // stay in lambda, skip assign
                }
                m_stageLayout->unassignPosition(curName);
                m_stageLayout->assignPosition(posName);
                m_stageMap[targetFx] = posName;
            }
        } else if (removeBindAction && chosen == removeBindAction && m_stageLayout) {
            QString curName = m_stageMap.value(targetFx);
            if (QMessageBox::question(this, "移除绑定",
                    "确认移除 " + targetFx->name() + " 的舞台绑定「" + curName + "」？",
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                m_stageLayout->unassignPosition(curName);
                m_stageMap.remove(targetFx);
            }
        }
    });

    // 隐藏主界面灯库的添加/删除按钮（改用全屏灯库页管理）
    ui->addFixtureBtn->hide();
    ui->removeFixtureBtn->hide();

    // 默认显示灯库模式
    ui->leftStack->setCurrentIndex(0);

    // 加载持久化数据
    loadAllData();
}

void MainWindow::syncPlayBtn()
{
    if (m_timelinePlaying) {
        m_playBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPause));
        m_playBtn->setToolTip("暂停");
    } else {
        m_playBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
        m_playBtn->setToolTip("播放");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Delete || m_deleteMode) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    // Timeline block selected → delete it first
    if (m_timeline && m_timeline->hasBlockSelected()) {
        m_timeline->deleteSelectedBlock();
        return;
    }
    if (ui->fixtureList->hasFocus()) {
        auto items = ui->fixtureList->selectedItems();
        if (items.isEmpty()) return;
        // 确认删除
        QStringList names;
        for (auto *it : items) {
            int row = ui->fixtureList->row(it);
            // 遍历所有域找到对应灯具（refreshFixtureList 逻辑）
            for (auto *uv : m_universes) {
                if (!uv) continue;
                auto list = uv->fixtures();
                if (row >= 0 && row < list.size()) {
                    names << list[row]->name();
                    row -= list.size();
                } else {
                    row -= uv->fixtures().size();
                }
            }
        }
        if (names.isEmpty()) return;
        auto answer = QMessageBox::question(this, "确认删除",
            QString("确定要删除 %1 个灯具吗？\n%2")
            .arg(names.size()).arg(names.join(", ")),
            QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        // 执行删除
        for (auto *it : items) {
            int row = ui->fixtureList->row(it);
            for (auto *uv : m_universes) {
                if (!uv) continue;
                auto list = uv->fixtures();
                if (row >= 0 && row < list.size()) {
                    removeFixture(list[row]);
                    break;
                }
                row -= uv->fixtures().size();
            }
        }
        return;
    }
    auto sel = m_scene->selectedItems();
    if (!sel.isEmpty()) {
        QStringList sceneNames;
        for (auto *si : sel)
            if (auto *fi = dynamic_cast<FixtureItem *>(si))
                sceneNames << fi->fixture()->name();
        if (sceneNames.isEmpty()) { QMainWindow::keyPressEvent(event); return; }
        auto answer = QMessageBox::question(this, "确认删除",
            QString("确定要删除 %1 个灯具吗？\n%2")
            .arg(sceneNames.size()).arg(sceneNames.join(", ")),
            QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        for (auto *si : sel) {
            auto *fi = dynamic_cast<FixtureItem *>(si);
            if (fi) removeFixture(fi->fixture());
        }
        return;
    }
    QMainWindow::keyPressEvent(event);
}

// =====================================================================
//  Universe 管理
// =====================================================================

Universe *MainWindow::currentUniverse() const
{
    return m_universes.value(m_currentDomain, nullptr);
}

void MainWindow::updateProgramPageInfo()
{
    if (!m_programPage) return;
    Universe *u = currentUniverse();
    int sel = m_stageLayout ? m_stageLayout->selectedCount() : 0;
    m_programPage->setProgramInfo(
        QString("域 %1").arg(m_currentDomain + 1),
        u ? 512 : 0,
        sel > 0 ? sel : (u ? u->fixtures().size() : 0));
    m_programPage->setArtNetStatus(m_connected);
}

void MainWindow::switchDomain(int domainIndex)
{
    if (domainIndex < 0 || domainIndex == m_currentDomain) return;
    if (m_switchingDomain) return;  // 防重入
    m_switchingDomain = true;
    while (domainIndex >= m_universes.size()) {
        auto *newUv = new Universe(m_universes.size(), this);
        if (m_artnet) newUv->bindSender(m_artnet);
        m_universes << newUv;
    }

    m_scene->clear();
    m_globe3D->clear();
    m_fixtureItems.clear();
    m_selected = nullptr;

    m_currentDomain = domainIndex;

    Universe *u = currentUniverse();
    if (!u) return;

    for (auto *f : u->fixtures()) {
        auto *fitm = new FixtureItem(f);
        m_scene->addItem(fitm);
        fitm->updateFromData();
        m_fixtureItems.insert(f, fitm);
        m_globe3D->addFixture(f);
        m_globe3D->updateFixture(f);
        connect(fitm, &FixtureItem::positionChanged, this, [this](FixtureItem *item) {
            if (item && item->fixture())
                m_globe3D->updateFixturePosition(item->fixture(), item->pos());
        });
    }

    refreshFixtureList();
    showLibraryMode();

    // 刷新地址码页面（所有宇宙）
    QList<Fixture *> allFx;
    for (auto *uv : m_universes) allFx << uv->fixtures();
    m_addressPage->updateFixtures(allFx);
    updateProgramPageInfo();
    m_switchingDomain = false;
}

// =====================================================================
//  灯库
// =====================================================================

void MainWindow::initFixtureLibrary()
{
    m_library << FixtureDef("8CH PAR 灯", "", 8,
        {"亮度","红","绿","蓝","频闪","宏","水平","垂直"});

    for (const auto &def : m_library)
        ui->libraryList->addItem(QString("%1 [%2] %3ch").arg(def.name, def.manufacturer).arg(def.channels));
}

void MainWindow::on_librarySearch_textChanged(const QString &text)
{
    for (int i = 0; i < ui->libraryList->count(); i++)
        ui->libraryList->item(i)->setHidden(!ui->libraryList->item(i)->text().contains(text, Qt::CaseInsensitive));
}

void MainWindow::on_libraryList_itemDoubleClicked(QListWidgetItem *) { if (!m_deleteMode) on_addFixtureBtn_clicked(); }

void MainWindow::on_addFixtureBtn_clicked()
{
    FixtureDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    FixtureDef def = dlg.getFixtureDef();
    if (def.name.isEmpty()) { QMessageBox::warning(this, "提示", "灯具名称不能为空"); return; }
    m_library << def;
    ui->libraryList->clear();
    for (const auto &d : m_library)
        ui->libraryList->addItem(QString("%1 [%2] %3ch").arg(d.name, d.manufacturer).arg(d.channels));
    refreshLibraryPage();
}

void MainWindow::on_removeFixtureBtn_clicked()
{
    m_deleteMode = !m_deleteMode;
    if (m_deleteMode) {
        ui->removeFixtureBtn->setText("完成删除");
        ui->removeFixtureBtn->setStyleSheet("background:#c44;color:#fff");
        for (int i = 0; i < ui->libraryList->count(); i++) {
            auto *litm = ui->libraryList->item(i);
            auto *container = new QWidget;
            auto *hl = new QHBoxLayout(container);
            hl->setContentsMargins(0,0,0,0); hl->setSpacing(0); hl->addStretch();
            auto *btn = new QPushButton("×");
            btn->setFixedSize(24,24);
            btn->setStyleSheet("background:transparent;color:red;font-weight:bold;border:none;font-size:16px");
            hl->addWidget(btn);
            connect(btn, &QPushButton::clicked, this, [this, litm]() {
                int row = ui->libraryList->row(litm);
                if (row >= 0 && row < m_library.size()) {
                    m_library.removeAt(row);
                    delete ui->libraryList->takeItem(row);
                }
                if (ui->libraryList->count() == 0) on_removeFixtureBtn_clicked();
                refreshLibraryPage();
            });
            ui->libraryList->setItemWidget(litm, container);
        }
    } else {
        ui->removeFixtureBtn->setText("- 删除");
        ui->removeFixtureBtn->setStyleSheet("");
        for (int i = 0; i < ui->libraryList->count(); i++)
            ui->libraryList->removeItemWidget(ui->libraryList->item(i));
    }
}

// =====================================================================
//  添加/删除灯具
// =====================================================================

void MainWindow::addFixtureToCurrent(const FixtureDef &def, const QPointF &pos)
{
    // 找到/创建对应型号的域
    int domIdx = -1;
    auto doms = m_addressPage->getDomains();
    for (int i = 0; i < doms.size(); i++) {
        if (doms[i].model == def.name) { domIdx = i; break; }
    }
    if (domIdx < 0) {
        domIdx = doms.size();
        m_addressPage->addDomain(def.name, def.channels);
    }
    while (m_universes.size() <= domIdx) {
        auto *newUv = new Universe(m_universes.size(), this);
        if (m_artnet) newUv->bindSender(m_artnet);
        m_universes << newUv;
    }

    Universe *u = m_universes[domIdx];
    if (!u) return;

    // 512 限制（仅该域内）
    int nextAddr = 1;
    for (auto *fx : u->fixtures()) nextAddr += fx->channelCount();
    if (nextAddr + def.channels - 1 > 512) {
        QMessageBox::warning(this, "地址码超限",
            QString("域 %1 已占用到 %2，此灯(%3通道)将超512")
                .arg(domIdx + 1).arg(nextAddr - 1).arg(def.channels));
        return;
    }

    auto *f = new Fixture(def, this);
    f->setName(def.name);
    f->setUniverse(domIdx);
    u->addFixture(f);

    auto *fitm = new FixtureItem(f);
    m_scene->addItem(fitm);
    fitm->setPos(pos.isNull() ? QPointF(m_fixtureItems.size() * 60 - 200, 0) : pos);
    m_fixtureItems.insert(f, fitm);

    fitm->updateFromData();
    m_globe3D->addFixture(f, pos);
    connect(fitm, &FixtureItem::positionChanged, this, [this](FixtureItem *item) {
        if (item && item->fixture())
            m_globe3D->updateFixturePosition(item->fixture(), item->pos());
    });
    refreshFixtureList();
    sendDmx();
    // 收集所有宇宙的灯具更新地址码页面
    QList<Fixture *> allFixtures;
    for (auto *uv : m_universes)
        allFixtures << uv->fixtures();
    m_addressPage->updateFixtures(allFixtures);
}

void MainWindow::removeFixture(Fixture *f)
{
    Universe *u = currentUniverse();
    if (!u) return;
    u->removeFixture(f);

    if (auto *fitm = m_fixtureItems.take(f)) {
        m_scene->removeItem(fitm);
        delete fitm;
    }
    m_globe3D->removeFixture(f);
    if (m_stageMap.contains(f)) {
        if (m_stageLayout) m_stageLayout->unassignPosition(m_stageMap[f]);
        m_stageMap.remove(f);
    }
    bool wasSelected = (m_selected == f);
    if (wasSelected) m_selected = nullptr;
    delete f;
    refreshFixtureList();
    if (wasSelected) showLibraryMode();
    sendDmx();
}

void MainWindow::refreshFixtureList()
{
    ui->fixtureList->clear();
    for (int di = 0; di < m_universes.size(); di++) {
        auto *u = m_universes[di];
        if (!u) continue;
        int addr = 1;
        for (auto *f : u->fixtures()) {
            f->setAddress(addr);
            addr += f->channelCount();
            ui->fixtureList->addItem(QString("域%1  %2  [%3]  %4通道")
                .arg(di + 1).arg(f->name())
                .arg(f->address(), 3, 10, QChar('0')).arg(f->channelCount()));
        }
    }
}

// =====================================================================
//  已添加灯具
// =====================================================================

void MainWindow::on_fixtureList_itemSelectionChanged()
{
    auto items = ui->fixtureList->selectedItems();
    if (items.isEmpty()) return;
    int row = ui->fixtureList->row(items.first());
    // 遍历所有域找到对应灯具（多域支持）
    Fixture *f = nullptr;
    int r = row;
    for (auto *uv : m_universes) {
        if (!uv) continue;
        auto list = uv->fixtures();
        if (r >= 0 && r < list.size()) { f = list[r]; break; }
        r -= uv->fixtures().size();
    }
    if (!f) return;
    // 用 blockSignals 防止 selectionChanged 触发 showControlMode（会隐藏列表）
    m_scene->blockSignals(true);
    m_scene->clearSelection();
    if (auto *it = m_fixtureItems.value(f)) it->setSelected(true);
    m_scene->blockSignals(false);
}

void MainWindow::on_fixtureList_itemDoubleClicked(QListWidgetItem *)
{
    auto items = ui->fixtureList->selectedItems();
    if (items.isEmpty()) return;
    int row = ui->fixtureList->row(items.first());
    Fixture *f = nullptr;
    int r = row;
    for (auto *uv : m_universes) {
        if (!uv) continue;
        auto list = uv->fixtures();
        if (r >= 0 && r < list.size()) { f = list[r]; break; }
        r -= uv->fixtures().size();
    }
    if (f) selectFixture(f);
}

// =====================================================================
//  控制
// =====================================================================

void MainWindow::selectFixture(Fixture *f) { m_selected = f; showControlMode(f); }

void MainWindow::onFixtureItemSelected(FixtureItem *item) { if (item) selectFixture(item->fixture()); }

void MainWindow::refreshLibraryPage()
{
    m_libraryPage->setLibrary(m_library);
}

void MainWindow::showControlMode(Fixture *f)
{
    ui->controlTitle->setText(f->name());
    ui->leftStack->setCurrentIndex(1);

    // 光束朝向（如果还没创建）
    if (!m_beamWidget) {
        m_beamWidget = new BeamWidget;
        auto *pageLayout = qobject_cast<QVBoxLayout *>(ui->pageControl->layout());
        if (pageLayout) pageLayout->insertWidget(1, m_beamWidget); // controlHeader(0), beamWidget(1), paramGroup(2)
    }
    m_beamWidget->setVisible(true);

    rebuildControlPanel(f);

    // 只有含 R/G/B 通道的才显示调色盘
    bool hasR = false, hasG = false, hasB = false;
    for (int ch = 0; ch < f->channelCount(); ch++) {
        QString n = f->channelName(ch).toLower();
        if (n == "r" || n == "red")   hasR = true;
        if (n == "g" || n == "green") hasG = true;
        if (n == "b" || n == "blue")  hasB = true;
    }
    m_colorBtn->setVisible(hasR && hasG && hasB);
}

void MainWindow::showLibraryMode() {
    m_selected = nullptr; m_scene->clearSelection(); ui->leftStack->setCurrentIndex(0);
    if (m_beamWidget) m_beamWidget->setVisible(false);
    ui->fixtureList->setFocus();  // 返回灯库模式时自动聚焦灯具列表
}
void MainWindow::on_backButton_clicked() { showLibraryMode(); }

void MainWindow::rebuildControlPanel(Fixture *f)
{
    QLayoutItem *child;
    while ((child = ui->paramLayout->takeAt(0)) != nullptr) {
        if (child->layout()) {
            // 嵌套 layout，清空子控件
            QLayout *sub = child->layout();
            QLayoutItem *subChild;
            while ((subChild = sub->takeAt(0)) != nullptr) {
                if (subChild->widget()) subChild->widget()->deleteLater();
                delete subChild;
            }
        }
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    for (int ch = 0; ch < f->channelCount(); ch++) {
        auto ranges = f->channelRanges(ch);
        auto *row = new QHBoxLayout;
        QString chName = f->channelName(ch);
        // 图标/颜色标记（优先显示导入图标，其次 R/G/B 色标）
        const QStringList &icons = f->definition().channelIcons;
        bool hasChIcon = ch < icons.size() && !icons[ch].isEmpty();
        const int markSz = 18;
        auto *mark = new QLabel;
        mark->setFixedSize(markSz, markSz);
        if (hasChIcon) {
            QImage img(icons[ch]);
            if (!img.isNull()) {
                int sq = qMin(img.width(), img.height());
                int cx = (img.width() - sq) / 2, cy = (img.height() - sq) / 2;
                QPixmap pm = QPixmap::fromImage(
                    img.copy(cx, cy, sq, sq).scaled(markSz, markSz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                QPixmap circle(markSz, markSz); circle.fill(Qt::transparent);
                QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
                pp.setBrush(pm); pp.setPen(QPen(QColor(0x99,0x99,0x99), 1));
                pp.drawEllipse(1, 1, markSz - 2, markSz - 2); pp.end();
                mark->setPixmap(circle);
            }
        } else if (chName.contains("红") || chName.toLower() == "r")
            mark->setStyleSheet(QString("background:#dc3c28;border-radius:%1px;border:1px solid #999").arg(markSz/2));
        else if (chName.contains("绿") || chName.toLower() == "g")
            mark->setStyleSheet(QString("background:#28b43c;border-radius:%1px;border:1px solid #999").arg(markSz/2));
        else if (chName.contains("蓝") || chName.toLower() == "b")
            mark->setStyleSheet(QString("background:#2850dc;border-radius:%1px;border:1px solid #999").arg(markSz/2));
        else
            mark->setStyleSheet("background:transparent;border:none");
        row->addWidget(mark);
        auto *label = new QLabel(chName);
        label->setMinimumWidth(55);
        row->addWidget(label);

        if (ranges.isEmpty()) {
            auto *slider = new QSlider(Qt::Horizontal);
            slider->setRange(0, 255);
            slider->setValue(f->channelValue(ch));
            auto *valEdit = new QLineEdit(QString::number(f->channelValue(ch)));
            valEdit->setMaxLength(3);
            valEdit->setMaximumWidth(38);
            valEdit->setAlignment(Qt::AlignCenter);
            valEdit->setStyleSheet("color:#000;border:1px solid #888;border-radius:3px;padding:1px 3px;font-size:11px");
            connect(slider, &QSlider::valueChanged, this, [valEdit, this, f, ch](int v) {
                valEdit->setText(QString::number(v));
                f->setChannel(ch, v); sendDmx();
                if (auto *it = m_fixtureItems.value(f)) it->updateFromData();
                if (f == m_selected) m_globe3D->updateFixture(f);
            });
            connect(valEdit, &QLineEdit::editingFinished, this, [slider, f, ch, valEdit, this]() {
                bool ok; int v = valEdit->text().toInt(&ok);
                if (ok && v >= 0 && v <= 255) { slider->setValue(v); f->setChannel(ch, v); sendDmx(); }
            });
            row->addWidget(slider);
            row->addWidget(valEdit);
        } else {
            auto *combo = new QComboBox;
            int matchIdx = -1; int cv = f->channelValue(ch);
            for (int r = 0; r < ranges.size(); r++) {
                combo->addItem(ranges[r].name);
                if (cv >= ranges[r].minValue && cv <= ranges[r].maxValue) matchIdx = r;
            }
            if (matchIdx >= 0) combo->setCurrentIndex(matchIdx);
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this, f, ch, ranges](int idx) {
                if (idx >= 0 && idx < ranges.size())
                    f->setChannel(ch, static_cast<uint8_t>(ranges[idx].minValue));
                sendDmx();
            });
            row->addWidget(combo, 1);
        }
        ui->paramLayout->addLayout(row);
    }
    ui->paramLayout->addStretch();
}

void MainWindow::updateChannelControls() {}
void MainWindow::on_dimmerSlider_valueChanged(int) {}
void MainWindow::on_redSlider_valueChanged(int)   {}
void MainWindow::on_greenSlider_valueChanged(int)  {}
void MainWindow::on_blueSlider_valueChanged(int)   {}

void MainWindow::on_fullButton_clicked()
{
    for (auto *f : currentUniverse()->fixtures()) { f->setDimmer(255); if (auto *it = m_fixtureItems.value(f)) it->updateFromData(); m_globe3D->updateFixture(f); }
    sendDmx(); if (m_selected) rebuildControlPanel(m_selected);
}
void MainWindow::on_blackoutButton_clicked()
{
    for (auto *f : currentUniverse()->fixtures()) { f->setDimmer(0); if (auto *it = m_fixtureItems.value(f)) it->updateFromData(); m_globe3D->updateFixture(f); }
    sendDmx(); if (m_selected) rebuildControlPanel(m_selected);
}

// =====================================================================
//  输出
// =====================================================================

void MainWindow::on_artnetRadio_toggled(bool checked) { m_useArtnet = checked; ui->artnetIpEdit->setEnabled(checked); }
void MainWindow::on_dmxRadio_toggled(bool checked)   { if (checked) m_useArtnet = false; }

void MainWindow::on_manualIpBtn_clicked()
{
    // 切换 IP 编辑框的只读状态
    bool wasReadOnly = ui->artnetIpEdit->isReadOnly();
    ui->artnetIpEdit->setReadOnly(!wasReadOnly);
    if (!wasReadOnly) {
        // 重新锁定 → 重新自动计算广播地址
        ui->manualIpBtn->setText("✎");
        QHostAddress cur(ui->artnetIpEdit->text());
        if (!cur.isNull()) {
            // 根据当前 IP 找到匹配网卡，重新算广播
            const auto ifaces = QNetworkInterface::allInterfaces();
            for (const auto &iface : ifaces) {
                if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
                for (const auto &entry : iface.addressEntries()) {
                    QHostAddress lip = entry.ip();
                    if (lip.protocol() != QAbstractSocket::IPv4Protocol || lip.isLoopback() || lip.isLinkLocal()) continue;
                    quint32 mask = entry.netmask().toIPv4Address();
                    if (mask == 0) continue;
                    if ((cur.toIPv4Address() & mask) == (lip.toIPv4Address() & mask)) {
                        quint32 bcast = (lip.toIPv4Address() & mask) | (~mask);
                        ui->artnetIpEdit->setText(QHostAddress(bcast).toString());
                        goto manualDone;
                    }
                }
            }
            manualDone:;
        }
    } else {
        // 解锁手动输入
        ui->manualIpBtn->setText("🔒");
    }
}

void MainWindow::on_connectButton_clicked()
{
    // 如果已连接 → 断开
    if (m_connected) {
        if (m_artnet) {
            delete m_artnet;
            m_artnet = nullptr;
        }
        if (m_receiver) {
            delete m_receiver;
            m_receiver = nullptr;
        }
        if (m_dmxDevice) {
            m_dmxDevice->close();
            m_dmxDevice = nullptr;
        }
        m_connected = false;
        ui->statusLabel->setText("已断开");
        ui->connectButton->setText("连接");
        if (m_programPage) { m_programPage->setArtNetStatus(false); updateProgramPageInfo(); }
        if (m_libraryPage) m_libraryPage->setArtNetStatus(false);
        return;
    }

    if (m_useArtnet) {
        // 自动模式（只读）→ 自动计算广播；手动模式 → 用用户输入的 IP
        if (ui->artnetIpEdit->isReadOnly()) {
            QString ip;
            const auto ifaces = QNetworkInterface::allInterfaces();
            // 第一遍：找无线网卡
            for (const auto &iface : ifaces) {
                if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
                QString name = iface.name().toLower();
                bool isWireless = name.contains("wireless") || name.contains("wlan") || name.contains("wi-fi");
                if (!isWireless) continue;
                for (const auto &entry : iface.addressEntries()) {
                    QHostAddress lip = entry.ip();
                    if (lip.protocol() != QAbstractSocket::IPv4Protocol || lip.isLoopback() || lip.isLinkLocal()) continue;
                    quint32 mask = entry.netmask().toIPv4Address();
                    if (mask == 0) continue;
                    quint32 bcast = (lip.toIPv4Address() & mask) | (~mask);
                    ip = QHostAddress(bcast).toString();
                    ui->artnetIpEdit->setText(ip);
                    goto found;
                }
            }
            // 第二遍：没无线网卡就用有线（排除虚拟网卡 VMware/VirtualBox/Hyper-V）
            for (const auto &iface : ifaces) {
                if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
                QString name = iface.name().toLower();
                if (name.contains("vmware") || name.contains("virtualbox") || name.contains("hyper-v")) continue;
                for (const auto &entry : iface.addressEntries()) {
                    QHostAddress lip = entry.ip();
                    if (lip.protocol() != QAbstractSocket::IPv4Protocol || lip.isLoopback() || lip.isLinkLocal()) continue;
                    quint32 mask = entry.netmask().toIPv4Address();
                    if (mask == 0) continue;
                    quint32 bcast = (lip.toIPv4Address() & mask) | (~mask);
                    ip = QHostAddress(bcast).toString();
                    ui->artnetIpEdit->setText(ip);
                    goto found;
                }
            }
            found:;
            if (ip.isEmpty()) { ip = "255.255.255.255"; ui->artnetIpEdit->setText(ip); }
        }
        QString ip = ui->artnetIpEdit->text().trimmed();
        if (ip.isEmpty()) ip = "255.255.255.255";
        m_artnet = new ArtNetSender(ip, this);
        // 重新绑定所有 Universe
        for (auto *uv : m_universes)
            uv->bindSender(m_artnet);
        // 创建 ArtNet 接收器（监听输入）
        if (!m_receiver) {
            m_receiver = new ArtNetReceiver(this);
            m_receiver->bind(ARTNET_PORT);
            connect(m_receiver, &ArtNetReceiver::dmxReceived,
                    this, &MainWindow::onArtNetReceived);
        }
        m_connected = true;
        ui->statusLabel->setText("已连接 - Art-Net → " + ip);
        ui->connectButton->setText("断开连接");
        if (m_programPage) { m_programPage->setArtNetStatus(true); updateProgramPageInfo(); }
        if (m_libraryPage) m_libraryPage->setArtNetStatus(true);
    } else {
        QList<DMXUSBWidget *> devs = DMXUSBWidget::widgets();
        if (devs.isEmpty()) { QMessageBox::warning(this, "错误", "未检测到 USB DMX 设备！"); return; }
        m_dmxDevice = devs.first();
        m_dmxDevice->open(0, false);
        m_dmxDevice->setOutputFrequency(44);
        m_connected = true;
        ui->statusLabel->setText("已连接 - USB DMX");
        ui->connectButton->setText("断开连接");
        if (m_programPage) { m_programPage->setArtNetStatus(true); updateProgramPageInfo(); }
        if (m_libraryPage) m_libraryPage->setArtNetStatus(true);
    }
}

// =====================================================================
//  发送
// =====================================================================

void MainWindow::sendDmx()
{
    if (m_useArtnet && m_artnet) {
        // 遍历所有域，每个域以独立的 Art-Net Universe 发送
        for (int i = 0; i < m_universes.size(); i++) {
            Universe *u = m_universes[i];
            if (!u) continue;
            u->render();
            m_artnet->setUniverse(i);
            m_artnet->setChannels(u->data());
            m_artnet->sendDmx();
            // 更新 DMX 监视器
            if (m_dmxMonitor) m_dmxMonitor->updateData(u->data(), i);
        }
    } else if (m_dmxDevice) {
        // USB DMX 只有一路物理输出，发送当前域
        Universe *u = currentUniverse();
        if (u) {
            u->render();
            m_dmxDevice->writeUniverse(0, 0, u->data(), true);
        }
    }

    // 性能：只在地址码页可见时刷新 UI
    if (m_addressPage->isVisible()) {
        QList<Fixture *> allFx;
        for (auto *uv : m_universes) allFx << uv->fixtures();
        m_addressPage->updateFixtures(allFx);
    }
}

void MainWindow::sendDmxRaw(const QByteArray &data)
{
    if (m_useArtnet && m_artnet) {
        m_artnet->setChannels(data);
        m_artnet->sendDmx();
    } else if (m_dmxDevice) {
        m_dmxDevice->writeUniverse(0, 0, data, true);
    }
    if (m_dmxMonitor) m_dmxMonitor->updateData(data, 0);
}

void MainWindow::onArtNetReceived(uint16_t universe, const QByteArray &data)
{
    // 将收到的数据写入对应 Universe
    if (universe < m_universes.size()) {
        Universe *u = m_universes[universe];
        if (u) {
            for (int i = 0; i < qMin(data.size(), 512); i++)
                u->setChannel(i, static_cast<uint8_t>(data[i]));
            u->render();
        }
    }
    // 更新地址码页 UI
    if (m_addressPage && m_addressPage->isVisible()) {
        QList<Fixture *> allFx;
        for (auto *uv : m_universes) allFx << uv->fixtures();
        m_addressPage->updateFixtures(allFx);
    }
}

// =====================================================================
//  菜单
// =====================================================================

void MainWindow::on_actionAddressPage_triggered()
{
    QList<Fixture *> allFx;
    for (auto *uv : m_universes) allFx << uv->fixtures();
    m_addressPage->updateFixtures(allFx);
    m_masterStack->setCurrentIndex(2);
}

void MainWindow::on_actionNew_triggered()
{
    Universe *u = currentUniverse();
    if (!u) return;
    for (auto *f : u->fixtures()) {
        delete m_fixtureItems.take(f);
        delete f;
    }
    u->clear();
    m_scene->clear();
    m_fixtureItems.clear();
    ui->fixtureList->clear();
    m_selected = nullptr;
    showLibraryMode();
    sendDmx();
}

void MainWindow::on_actionSave_triggered() { ui->statusLabel->setText("保存（功能待实现）"); }
void MainWindow::on_actionExit_triggered() { close(); }

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_libraryPage && m_libraryPage->isDirty()) {
        if (!m_libraryPage->maybeSave()) {
            event->ignore();
            return;
        }
    }
    if (m_programPage && m_programPage->isDirty()) {
        if (!m_programPage->maybeSave()) {
            event->ignore();
            return;
        }
    }
    saveAllData();
    QMainWindow::closeEvent(event);
}

// =====================================================================
//  持久化：自动保存/加载灯库、域、模板到 AppData
// =====================================================================

static QString dataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

void MainWindow::saveAllData()
{
    QString dir = dataDir();

    // === 保存灯库 ===
    {
        QJsonArray arr;
        for (const auto &def : m_library) {
            QJsonObject o;
            o["name"] = def.name;
            o["manufacturer"] = def.manufacturer;
            o["channels"] = def.channels;
            if (!def.iconPath.isEmpty()) o["iconPath"] = def.iconPath;
            if (!def.channelIcons.isEmpty()) {
                QJsonArray ciArr;
                for (const auto &ci : def.channelIcons) ciArr << ci;
                o["channelIcons"] = ciArr;
            }
            QJsonArray chArr;
            for (int i = 0; i < def.channelNames.size(); i++) {
                QJsonObject co;
                co["name"] = def.channelNames[i];
                QJsonArray rArr;
                if (i < def.ranges.size()) {
                    for (const auto &r : def.ranges[i]) {
                        QJsonObject ro;
                        ro["label"] = r.name;
                        ro["min"] = r.minValue;
                        ro["max"] = r.maxValue;
                        if (!r.iconPath.isEmpty()) ro["icon"] = r.iconPath;
                        rArr << ro;
                    }
                }
                co["ranges"] = rArr;
                chArr << co;
            }
            o["channelDefs"] = chArr;
            arr << o;
        }
        QFile f(dir + "/library.json");
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(arr).toJson());
    }

    // === 保存域 ===
    QList<AddrEntry> doms = m_addressPage->getDomains();
    {
        QJsonArray arr;
        for (const auto &d : doms) {
            QJsonObject o;
            o["model"] = d.model;
            o["channels"] = d.channels;
            arr << o;
        }
        QFile f(dir + "/domains.json");
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(arr).toJson());
    }

}

void MainWindow::loadAllData()
{
    QString dir = dataDir();

    // === 加载灯库 ===
    {
        QFile f(dir + "/library.json");
        if (f.open(QIODevice::ReadOnly)) {
            QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
            m_library.clear();
            for (const auto &v : arr) {
                QJsonObject o = v.toObject();
                FixtureDef def;
                def.name = o["name"].toString();
                def.manufacturer = o["manufacturer"].toString();
                def.channels = o["channels"].toInt();
                def.iconPath = o["iconPath"].toString();
                QJsonArray ciArr = o["channelIcons"].toArray();
                for (const auto &civ : ciArr) def.channelIcons << civ.toString();
                QJsonArray chArr = o["channelDefs"].toArray();
                for (const auto &cv : chArr) {
                    QJsonObject co = cv.toObject();
                    def.channelNames << co["name"].toString();
                    QJsonArray rArr = co["ranges"].toArray();
                    QList<ChannelRange> ranges;
                    for (const auto &rv : rArr) {
                        QJsonObject ro = rv.toObject();
                        ChannelRange cr;
                        cr.name = ro["label"].toString();
                        cr.minValue = ro["min"].toInt();
                        cr.maxValue = ro["max"].toInt();
                        cr.iconPath = ro["icon"].toString();
                        ranges << cr;
                    }
                    def.ranges << ranges;
                }
                m_library << def;
            }
            f.close();
        }
    }

    // === 加载域 ===
    {
        QFile f(dir + "/domains.json");
        if (f.open(QIODevice::ReadOnly)) {
            QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
            for (const auto &v : arr) {
                QJsonObject o = v.toObject();
                m_addressPage->addDomain(o["model"].toString(), o["channels"].toInt());
            }
            f.close();
        }
    }

    // 刷新 UI
    ui->libraryList->clear();
    for (const auto &d : m_library)
        ui->libraryList->addItem(QString("%1 [%2] %3ch").arg(d.name, d.manufacturer).arg(d.channels));
    refreshLibraryPage();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->fixtureList && event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (!(me->buttons() & Qt::LeftButton)) return false;
        QListWidgetItem *item = ui->fixtureList->itemAt(me->pos());
        if (!item) return false;
        // 开始拖拽
        auto *drag = new QDrag(ui->fixtureList);
        auto *mime = new QMimeData;
        mime->setText(item->text());
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}
