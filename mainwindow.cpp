#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ArtNetSender.h"
#include "Universe.h"
#include "fixtureitem.h"
#include "dropview.h"
#include "draglibrarylist.h"
#include "fixturedialog.h"
#include "addresspage.h"
#include "librarypage.h"
#include "globe3d.h"
#include "colorwheel.h"
#include "beamwidget.h"
#include "timeline.h"
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

    // 场景选择变化
    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (m_syncOnly) return;
        auto sel = m_scene->selectedItems();
        if (!sel.isEmpty()) {
            auto *item = dynamic_cast<FixtureItem *>(sel.first());
            if (item) onFixtureItemSelected(item);
        }
    });

    // ===== 3D 视图 + 切换 =====
    m_globe3D = new Globe3D;
    m_viewStack = new QStackedWidget;
    // graphicsView 原本在 viewFrame 布局中，取出来放到 viewStack 里
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
    m_btn2D->setStyleSheet("background:#335;color:#fff;font-weight:bold"); // 默认选中
    m_btn3D->setStyleSheet("background:#222;color:#888");
    btnLayout->addWidget(m_btn2D);
    btnLayout->addWidget(m_btn3D);
    btnLayout->addStretch();
    ui->viewLayout->insertWidget(0, btnBar);  // 插入到最顶部

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
    // 全屏叠加层
    m_masterStack = new QStackedWidget;
    m_masterStack->addWidget(ui->centralwidget);   // 0 = 正常布局
    m_masterStack->addWidget(m_libraryPage);       // 1 = 全屏灯库
    m_masterStack->addWidget(m_addressPage);       // 2 = 域管理
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

    // ===== 域1（必须在 m_addressPage 之后）=====
    m_universes << new Universe(0, this);
    switchDomain(0);  // 初始化视图

    // ===== Art-Net =====
    m_artnet = new ArtNetSender("192.168.1.255", this);

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

    // 替换占位时间线
    m_timeline = new Timeline;
    ui->timelineLayout->addWidget(m_timeline);
    ui->timelinePlaceholder->hide();
    ui->timelineHeader->hide();

    // 已添加灯具右键菜单
    ui->fixtureList->setContextMenuPolicy(Qt::CustomContextMenu);
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

        QMenu menu;
        QAction *addAction = menu.addAction("添加到时间线");
        QAction *switchAction = menu.addAction("切换域");
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
            for (int di = 0; di < m_universes.size(); di++) {
                QString label = QString("域 %1").arg(di + 1);
                if (di < doms.size())
                    label += QString(" — %1 (%2ch)").arg(doms[di].model).arg(doms[di].channels);
                combo->addItem(label, di);
            }
            dl->addWidget(combo);
            auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            dl->addWidget(btns);
            if (dlg.exec() == QDialog::Accepted) {
                int targetDi = combo->currentData().toInt();
                if (targetDi >= 0 && targetDi < m_universes.size()) {
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
        }
    });

    // 隐藏主界面灯库的添加/删除按钮（改用全屏灯库页管理）
    ui->addFixtureBtn->hide();
    ui->removeFixtureBtn->hide();

    // 默认显示灯库模式
    ui->leftStack->setCurrentIndex(0);
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
        for (auto *it : items) {
            int row = ui->fixtureList->row(it);
            auto list = currentUniverse()->fixtures();
            if (row >= 0 && row < list.size()) removeFixture(list[row]);
        }
        return;
    }
    auto sel = m_scene->selectedItems();
    if (!sel.isEmpty()) {
        for (auto *si : sel) {
            auto *fi = dynamic_cast<FixtureItem *>(si);
            if (fi) removeFixture(fi->fixture());
        }
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

void MainWindow::switchDomain(int domainIndex)
{
    if (domainIndex < 0) return;
    while (domainIndex >= m_universes.size())
        m_universes << new Universe(m_universes.size(), this);

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
    while (m_universes.size() <= domIdx)
        m_universes << new Universe(m_universes.size(), this);

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
    auto list = currentUniverse()->fixtures();
    if (row < 0 || row >= list.size()) return;
    Fixture *f = list[row];
    m_syncOnly = true;
    m_scene->clearSelection();
    if (auto *it = m_fixtureItems.value(f)) it->setSelected(true);
    m_syncOnly = false;
}

void MainWindow::on_fixtureList_itemDoubleClicked(QListWidgetItem *)
{
    auto items = ui->fixtureList->selectedItems();
    if (items.isEmpty()) return;
    int row = ui->fixtureList->row(items.first());
    auto list = currentUniverse()->fixtures();
    if (row >= 0 && row < list.size()) selectFixture(list[row]);
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
        // 颜色标记（统一占位，保证对齐）
        auto *mark = new QLabel;
        mark->setFixedSize(12,12);
        if (chName.contains("红") || chName == "R")
            mark->setStyleSheet("background:#dc3c28;border-radius:6px;border:1px solid #999");
        else if (chName.contains("绿") || chName == "G")
            mark->setStyleSheet("background:#28b43c;border-radius:6px;border:1px solid #999");
        else if (chName.contains("蓝") || chName == "B")
            mark->setStyleSheet("background:#2850dc;border-radius:6px;border:1px solid #999");
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

void MainWindow::on_connectButton_clicked()
{
    if (m_useArtnet) {
        QString ip = ui->artnetIpEdit->text();
        if (m_artnet) delete m_artnet;
        m_artnet = new ArtNetSender(ip, this);
        ui->statusLabel->setText("已连接 - Art-Net → " + ip);
    } else {
        QList<DMXUSBWidget *> devs = DMXUSBWidget::widgets();
        if (devs.isEmpty()) { QMessageBox::warning(this, "错误", "未检测到 USB DMX 设备！"); return; }
        m_dmxDevice = devs.first();
        m_dmxDevice->open(0, false);
        m_dmxDevice->setOutputFrequency(44);
        ui->statusLabel->setText("已连接 - USB DMX");
    }
}

// =====================================================================
//  发送
// =====================================================================

void MainWindow::sendDmx()
{
    Universe *u = currentUniverse();
    if (!u) return;
    u->render();

    if (m_useArtnet && m_artnet) {
        m_artnet->setChannels(u->data());
        m_artnet->sendDmx();
    } else if (m_dmxDevice) {
        m_dmxDevice->writeUniverse(0, 0, u->data(), false);
    }

    // 性能：只在地址码页可见时刷新 UI
    if (m_addressPage->isVisible()) {
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
    QMainWindow::closeEvent(event);
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
