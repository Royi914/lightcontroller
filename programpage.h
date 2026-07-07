#ifndef PROGRAMPAGE_H
#define PROGRAMPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QSlider>
#include <QLineEdit>
#include <QStackedWidget>
#include <QList>
#include <QMap>
#include <QColorDialog>
#include <QMessageBox>
#include <QMenu>
#include <QInputDialog>
#include <QTimer>

// Per-template stored values
struct TemplateData {
    QMap<QString, int> colorValues;      // color name → 0-255
    QMap<QString, int> effectValues;     // effect key → slider value
    QMap<QString, QString> effectLabels; // effect key → display label (for custom effects)
    QColor customColor = QColor(200, 200, 200);
    QString activeColor;
};

class StageLayout;
class ProgramPage : public QWidget
{
    Q_OBJECT
public:
    explicit ProgramPage(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        // Init 7 templates with defaults, then apply per-template presets
        m_templateNames = {"开场暖光", "前奏渐亮", "频闪效果", "左-右", "渐变呼吸", "快速跑灯", "收光"};
        for (int i = 0; i < 7; i++) {
            TemplateData td;
            for (auto &c : m_colors) td.colorValues[c] = 255;
            for (auto &e : m_effects) {
                td.effectValues[e] = (e == "主控") ? 255 : 0;
                if (e.startsWith("自定义")) td.effectLabels[e] = "自定义";
            }
            m_templateData << td;
        }
        m_templateCount = 7;
        for (int i = 0; i < 7; i++) initTemplatePreset(i);
        m_savedTemplateData = m_templateData;
        m_savedTemplateNames = m_templateNames;
        m_dirty = false; // presets are the baseline, not unsaved changes

        auto *oroot = new QHBoxLayout(this);
        oroot->setContentsMargins(0, 0, 0, 0);

        // === 左侧工具栏 ===
        auto *tb = new QFrame;
        tb->setFixedWidth(140);
        tb->setStyleSheet("background:#e8e8e8;border-right:1px solid #ccc");
        auto *tbl = new QVBoxLayout(tb);
        tbl->setSpacing(4);

        auto makeBtn = [&](const QString &text) {
            auto *b = new QPushButton("  " + text);
            b->setFixedHeight(36);
            b->setStyleSheet("background:#f5f5f5;color:#222;border:1px solid #ccc;border-radius:4px;text-align:left;padding-left:12px");
            tbl->addWidget(b);
        };
        makeBtn("新建");
        makeBtn("打开");
        auto *saveBtn = new QPushButton("  保存");
        saveBtn->setFixedHeight(36);
        saveBtn->setStyleSheet("background:#f5f5f5;color:#222;border:1px solid #ccc;border-radius:4px;text-align:left;padding-left:12px");
        connect(saveBtn, &QPushButton::clicked, this, [this]() { onSave(); });
        tbl->addWidget(saveBtn);
        makeBtn("另存为");

        // 组名编辑
        auto *grpRow = new QHBoxLayout;
        grpRow->setSpacing(4);
        auto *grpLbl = new QLabel("组名");
        grpLbl->setStyleSheet("color:#555;font-size:11px;border:none;");
        grpRow->addWidget(grpLbl);
        m_groupEdit = new QLineEdit("组1");
        m_groupEdit->setMaxLength(12);
        m_groupEdit->setStyleSheet("color:#222;border:1px solid #bbb;border-radius:2px;padding:2px 4px;font-size:11px;");
        m_groupEdit->setFixedWidth(60);
        connect(m_groupEdit, &QLineEdit::textChanged, this, [this]() { m_dirty = true; });
        grpRow->addWidget(m_groupEdit);
        grpRow->addStretch();
        tbl->addLayout(grpRow);

        tbl->addStretch();

        // Play / Stop at bottom of toolbar
        auto *playBtn = new QPushButton("▶ 播放");
        playBtn->setFixedHeight(40);
        playBtn->setCursor(Qt::PointingHandCursor);
        playBtn->setStyleSheet(
            "QPushButton { background:#3a8; color:#fff; border:1px solid #297; border-radius:4px; font-size:13px; font-weight:bold; }"
            "QPushButton:hover { background:#4b9; }");
        connect(playBtn, &QPushButton::clicked, this, [this]() {
            // TODO: start playback
        });
        tbl->addWidget(playBtn);

        auto *stopBtn = new QPushButton("■ 停止");
        stopBtn->setFixedHeight(40);
        stopBtn->setCursor(Qt::PointingHandCursor);
        stopBtn->setStyleSheet(
            "QPushButton { background:#a44; color:#fff; border:1px solid #933; border-radius:4px; font-size:13px; font-weight:bold; }"
            "QPushButton:hover { background:#b55; }");
        connect(stopBtn, &QPushButton::clicked, this, [this]() {
            // TODO: stop playback
        });
        tbl->addWidget(stopBtn);

        auto *backStageBtn = new QPushButton("  返回舞台视图");
        backStageBtn->setFixedHeight(36);
        backStageBtn->setStyleSheet("background:#e8e0d0;color:#333;border:1px solid #ccb;border-radius:4px;text-align:left;padding-left:12px");
        connect(backStageBtn, &QPushButton::clicked, this, [this]() {
            if (maybeSave()) emit backToStageRequested();
        });
        tbl->addWidget(backStageBtn);

        auto *bk = new QPushButton("  返回主界面");
        bk->setFixedHeight(36);
        bk->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px");
        connect(bk, &QPushButton::clicked, this, [this]() {
            if (maybeSave()) emit backRequested();
        });
        tbl->addWidget(bk);
        oroot->addWidget(tb);

        // === 右侧大空间 ===
        auto *main = new QFrame;
        main->setStyleSheet("background:#fff");
        auto *mainLayout = new QVBoxLayout(main);
        mainLayout->setContentsMargins(0, 0, 0, 0);

        // ---- 信息栏 ----
        auto *infoBar = new QFrame;
        infoBar->setStyleSheet("background:#fafafa; border-bottom:1px solid #e8e8e8;");
        infoBar->setFixedHeight(28);
        auto *infoLayout = new QHBoxLayout(infoBar);
        infoLayout->setContentsMargins(12, 2, 12, 2);
        infoLayout->setSpacing(20);

        m_infoName = new QLabel("名称：--");
        m_infoName->setStyleSheet("color:#555; font-size:11px; border:none;");
        infoLayout->addWidget(m_infoName);

        m_infoChannels = new QLabel("通道：--");
        m_infoChannels->setStyleSheet("color:#555; font-size:11px; border:none;");
        infoLayout->addWidget(m_infoChannels);

        m_infoFixtures = new QLabel("灯具数量：--");
        m_infoFixtures->setStyleSheet("color:#555; font-size:11px; border:none;");
        infoLayout->addWidget(m_infoFixtures);

        infoLayout->addStretch();

        m_artnetStatus = new QLabel("○ ArtNet 未连接");
        m_artnetStatus->setStyleSheet("color:#999; font-size:11px; font-weight:bold; border:none;");
        infoLayout->addWidget(m_artnetStatus);

        mainLayout->addWidget(infoBar);

        // ---- 顶部模板条 ----
        auto *tmplBar = new QFrame;
        tmplBar->setStyleSheet("background:#f0f0f0;border-bottom:1px solid #ddd");
        tmplBar->setFixedHeight(72);
        auto *tbar = new QHBoxLayout(tmplBar);
        tbar->setContentsMargins(8, 8, 8, 8);
        tbar->setSpacing(4);

        auto *lbl = new QLabel("模板");
        lbl->setStyleSheet("color:#333;font-weight:bold;font-size:13px;border:none;background:transparent");
        tbar->addWidget(lbl);
        tbar->addSpacing(8);

        m_leftArrow = new QPushButton("<");
        m_leftArrow->setFixedSize(28, 56);
        m_leftArrow->setStyleSheet(arrowStyle());
        m_leftArrow->hide();
        connect(m_leftArrow, &QPushButton::clicked, this, [this]() { flipPage(-1); });
        tbar->addWidget(m_leftArrow);

        for (int i = 0; i < 7; i++) {
            auto *blk = new QPushButton;
            blk->setFixedSize(72, 56);
            blk->setStyleSheet(
                "QPushButton { background:#fff; border:2px solid #ccc; border-radius:4px; color:#555; font-size:11px; }"
                "QPushButton:hover { border-color:#88b; background:#eef; }");
            connect(blk, &QPushButton::clicked, this, [this, i]() { openTemplate(i); });
            // Right-click to rename
            blk->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(blk, &QPushButton::customContextMenuRequested, this, [this, i](const QPoint &pos) {
                QMenu menu;
                QAction *renameAct = menu.addAction("重命名");
                QAction *chosen = menu.exec(m_blocks[i]->mapToGlobal(pos));
                if (chosen == renameAct) {
                    bool ok;
                    QString newName = QInputDialog::getText(this, "重命名模板", "模板名称:",
                                                             QLineEdit::Normal, m_templateNames[i], &ok);
                    if (ok && !newName.isEmpty()) {
                        m_templateNames[i] = newName;
                        m_dirty = true;
                        refreshTemplates();
                    }
                }
            });
            tbar->addWidget(blk);
            m_blocks << blk;
        }

        m_rightArrow = new QPushButton(">");
        m_rightArrow->setFixedSize(28, 56);
        m_rightArrow->setStyleSheet(arrowStyle());
        connect(m_rightArrow, &QPushButton::clicked, this, [this]() { flipPage(1); });
        tbar->addWidget(m_rightArrow);

        auto *addBtn = new QPushButton("+");
        addBtn->setFixedSize(32, 56);
        addBtn->setStyleSheet(
            "QPushButton { background:#e8e8ff; border:2px dashed #aab; border-radius:4px; color:#558; font-size:18px; font-weight:bold; }"
            "QPushButton:hover { background:#d0d0ff; border-color:#88b; }");
        connect(addBtn, &QPushButton::clicked, this, [this]() {
            bool ok;
            QString defName = QString("模板%1").arg(m_templateCount + 1);
            QString name = QInputDialog::getText(this, "新建模板", "模板名称:",
                                                  QLineEdit::Normal, defName, &ok);
            if (!ok || name.isEmpty()) return;
            TemplateData td;
            for (auto &c : m_colors) td.colorValues[c] = 255;
            for (auto &e : m_effects) {
                td.effectValues[e] = (e == "主控") ? 255 : 0;
                if (e.startsWith("自定义")) td.effectLabels[e] = "自定义";
            }
            m_templateData << td;
            m_templateCount++;
            m_templateNames << name;
            m_page = (m_templateCount - 1) / 7;
            // Snapshot saved baseline for the new template too
            m_savedTemplateData = m_templateData;
            m_savedTemplateNames = m_templateNames;
            m_dirty = false;
            refreshTemplates();
        });
        tbar->addWidget(addBtn);

        tbar->addStretch();
        mainLayout->addWidget(tmplBar);

        // ---- 编辑区 (stacked) ----
        m_editStack = new QStackedWidget;
        // Page 0: placeholder
        auto *ph = new QLabel("双击模板进行编辑");
        ph->setAlignment(Qt::AlignCenter);
        ph->setStyleSheet("color:#aaa;font-size:18px;border:none");
        m_editStack->addWidget(ph);
        // Page 1: template editor (built on demand)
        m_editStack->addWidget(new QWidget);
        mainLayout->addWidget(m_editStack, 1);

        oroot->addWidget(main, 1);

        refreshTemplates();
        openTemplate(0);
    }

    bool isDirty() const { return m_dirty; }

    /// Returns true if safe to navigate away; false if user cancelled
    bool maybeSave()
    {
        if (!m_dirty) return true;

        QMessageBox mb(this);
        mb.setWindowTitle("未保存的修改");
        mb.setText("编程模板数据修改尚未保存，是否保存？");
        mb.setIcon(QMessageBox::Question);
        QPushButton *saveBtn   = mb.addButton("保存", QMessageBox::AcceptRole);
        QPushButton *ignoreBtn = mb.addButton("忽略", QMessageBox::DestructiveRole);
        mb.setDefaultButton(saveBtn);
        mb.setEscapeButton(nullptr);
        mb.exec();

        if (mb.clickedButton() == saveBtn) {
            onSave();
            return true;
        } else if (mb.clickedButton() == ignoreBtn) {
            m_templateData = m_savedTemplateData;
            m_templateNames = m_savedTemplateNames;
            m_templateCount = m_templateData.size();
            m_dirty = false;
            if (m_currentTemplate >= 0 && m_currentTemplate < m_templateData.size())
                buildTemplateEditor(m_currentTemplate);
            refreshTemplates();
            return true;
        }
        return false; // X clicked — stay on page
    }

    void onSave()
    {
        m_savedTemplateData = m_templateData;
        m_savedTemplateNames = m_templateNames;
        m_dirty = false;
        if (m_stageLayout) {
            QString gName = groupName();
            for (auto &n : m_boundPositions)
                m_stageLayout->setPositionLabel(n, gName);
        }
        emit saved(groupName(), m_boundPositions);
        m_infoName->setText(m_infoName->text() + "  ✓已保存");
        QTimer::singleShot(2000, this, [this]() {
            updateInfoDisplay();
        });
    }

    void setBoundPositions(const QStringList &positions) { m_boundPositions = positions; }
    void setStageLayout(StageLayout *sl) { m_stageLayout = sl; }

    void setProgramInfo(const QString &name, int channels, int fixtureCount)
    {
        Q_UNUSED(name);
        Q_UNUSED(channels);
        updateInfoDisplay();
        m_infoFixtures->setText("灯具数量：" + QString::number(fixtureCount));
    }

    void setArtNetStatus(bool connected)
    {
        if (connected) {
            m_artnetStatus->setText("● ArtNet 已连接");
            m_artnetStatus->setStyleSheet("color:#2a2; font-size:11px; font-weight:bold; border:none;");
        } else {
            m_artnetStatus->setText("○ ArtNet 未连接");
            m_artnetStatus->setStyleSheet("color:#999; font-size:11px; font-weight:bold; border:none;");
        }
    }

    void setFixtureInfo(const QString &model, int channels)
    {
        m_fixtureModel = model;
        m_fixtureChannels = channels;
        updateInfoDisplay();
    }

    QString groupName() const { return m_groupEdit ? m_groupEdit->text() : "组1"; }

    void updateInfoDisplay()
    {
        bool hasFixture = !m_fixtureModel.isEmpty() && m_fixtureChannels > 0;
        m_infoName->setVisible(hasFixture);
        m_infoChannels->setVisible(hasFixture);
        if (hasFixture) {
            m_infoName->setText("名称：" + m_fixtureModel + " " + groupName());
            m_infoChannels->setText("通道：" + QString::number(m_fixtureChannels));
        }
    }

signals:
    void backRequested();
    void backToStageRequested();
    void saved(const QString &groupName, const QStringList &positions);

private:
    void openTemplate(int pageBlockIndex)
    {
        int tIdx = m_page * 7 + pageBlockIndex;
        if (tIdx < 0 || tIdx >= m_templateData.size()) return;
        m_currentTemplate = tIdx;
        buildTemplateEditor(tIdx);
        m_editStack->setCurrentIndex(1);
    }

    void buildTemplateEditor(int tIdx)
    {
        // Remove old editor widget
        QWidget *old = m_editStack->widget(1);
        if (old) old->deleteLater();

        auto *editor = new QWidget;
        auto *hl = new QHBoxLayout(editor);
        hl->setContentsMargins(12, 12, 12, 12);
        hl->setSpacing(12);

        auto &data = m_templateData[tIdx];

        // === 氛围面板 ===
        auto *atmoPanel = new QFrame;
        atmoPanel->setFixedWidth(90);
        atmoPanel->setStyleSheet("background:#fafafa; border:1px solid #e0e0e0; border-radius:4px;");
        auto *atmoLayout = new QVBoxLayout(atmoPanel);
        atmoLayout->setContentsMargins(4, 6, 4, 6);
        atmoLayout->setSpacing(3);
        atmoLayout->setAlignment(Qt::AlignTop);

        auto *atmoTitle = new QLabel("氛围");
        atmoTitle->setStyleSheet("color:#333;font-weight:bold;font-size:13px;padding:2px 0;border:none;background:transparent;");
        atmoTitle->setAlignment(Qt::AlignCenter);
        atmoLayout->addWidget(atmoTitle);

        auto makeAtmoBtn = [&](const QString &name) {
            auto *btn = new QPushButton(name);
            btn->setFixedHeight(28);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(
                "QPushButton { background:#fff; color:#444; border:1px solid #ddd; border-radius:3px; font-size:11px; }"
                "QPushButton:hover { background:#e8e8f8; border-color:#99b; color:#225; }");
            connect(btn, &QPushButton::clicked, this, [this, tIdx, name]() {
                applyAtmosphere(tIdx, name);
                buildTemplateEditor(tIdx);
            });
            atmoLayout->addWidget(btn);
        };
        makeAtmoBtn("暖调");
        makeAtmoBtn("冷调");
        makeAtmoBtn("对比撞色");
        makeAtmoBtn("低饱和高级");
        makeAtmoBtn("中性");
        makeAtmoBtn("国风");
        atmoLayout->addStretch();
        hl->addWidget(atmoPanel);

        // === 左列：颜色 ===
        auto *leftScroll = new QScrollArea;
        leftScroll->setWidgetResizable(true);
        auto *leftW = new QWidget;
        auto *leftL = new QVBoxLayout(leftW);
        leftL->setSpacing(2);
        leftL->setAlignment(Qt::AlignTop);

        auto *leftTitle = new QLabel("颜色");
        leftTitle->setStyleSheet("color:#333;font-weight:bold;font-size:14px;padding:4px 0;border:none");
        leftL->addWidget(leftTitle);

        for (auto &name : m_colors) {
            auto *row = new QHBoxLayout;
            row->setAlignment(Qt::AlignVCenter);
            // Color name
            auto *nameLbl = new QLabel(name);
            nameLbl->setFixedWidth(48);
            bool active = data.activeColor.isEmpty() || (name == data.activeColor);
            nameLbl->setStyleSheet(
                active ? "color:#000;font-weight:bold;font-size:11px;border:none"
                       : "color:#bbb;font-size:11px;border:none");
            row->addWidget(nameLbl);
            // Color swatch (all clickable)
            QColor swColor = (name == "自定义") ? data.customColor : colorByName(name);
            auto *swBtn = new QPushButton;
            swBtn->setFixedSize(18, 18);
            swBtn->setCursor(Qt::PointingHandCursor);
            swBtn->setStyleSheet(QString("background:%1;border:1px solid #999;border-radius:2px").arg(swColor.name()));
            connect(swBtn, &QPushButton::clicked, this, [this, tIdx, name, swBtn]() {
                if (name == "自定义") {
                    QColor c = QColorDialog::getColor(m_templateData[tIdx].customColor, this, "选择自定义颜色");
                    if (c.isValid()) {
                        m_templateData[tIdx].customColor = c;
                        swBtn->setStyleSheet(QString("background:%1;border:1px solid #999;border-radius:2px").arg(c.name()));
                    }
                }
                m_templateData[tIdx].activeColor = name;
                m_dirty = true;
                // Rebuild to refresh visual state
                buildTemplateEditor(tIdx);
            });
            swBtn->setToolTip("点击切换当前颜色");
            row->addWidget(swBtn);

            // Slider
            auto *sl = new QSlider(Qt::Horizontal);
            sl->setRange(0, 255);
            sl->setValue(data.colorValues.value(name, 255));
            sl->setMaximumWidth(160);
            auto *val = new QLineEdit(QString::number(sl->value()));
            val->setMaxLength(3); val->setMaximumWidth(32); val->setAlignment(Qt::AlignCenter);
            val->setStyleSheet("color:#000;border:1px solid #888;border-radius:2px;padding:1px 2px;font-size:11px");
            connect(sl, &QSlider::valueChanged, val, [val](int v) { val->setText(QString::number(v)); });
            connect(sl, &QSlider::valueChanged, this, [this, tIdx, name](int v) {
                if (tIdx < m_templateData.size()) { m_templateData[tIdx].colorValues[name] = v; m_dirty = true; }
            });
            row->addWidget(sl);
            row->addWidget(val);
            leftL->addLayout(row);
        }
        leftL->addStretch();
        leftScroll->setWidget(leftW);
        hl->addWidget(leftScroll, 1);

        // === 右列：效果 ===
        auto *rightScroll = new QScrollArea;
        rightScroll->setWidgetResizable(true);
        auto *rightW = new QWidget;
        auto *rightL = new QVBoxLayout(rightW);
        rightL->setSpacing(2);
        rightL->setAlignment(Qt::AlignTop);

        // Title row: "效果" on left, "效果演示" on right
        auto *rightTitleRow = new QHBoxLayout;
        auto *rightTitle = new QLabel("效果");
        rightTitle->setStyleSheet("color:#333;font-weight:bold;font-size:14px;padding:4px 0;border:none");
        rightTitleRow->addWidget(rightTitle);
        rightTitleRow->addStretch();
        auto *demoHeader = new QLabel("效果演示");
        demoHeader->setStyleSheet("color:#999;font-size:10px;border:none;padding-right:6px");
        rightTitleRow->addWidget(demoHeader);
        rightL->addLayout(rightTitleRow);

        for (auto &name : m_effects) {
            auto *row = new QHBoxLayout;
            row->setAlignment(Qt::AlignVCenter);

            bool isCustom = name.startsWith("自定义");

            if (isCustom) {
                // Editable label for custom effects
                QString displayLabel = data.effectLabels.value(name, "自定义");
                auto *nameEdit = new QLineEdit(displayLabel);
                nameEdit->setFixedWidth(100);
                nameEdit->setMaxLength(12);
                nameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                nameEdit->setStyleSheet("color:#222;font-size:11px;border:1px solid #bbb;border-radius:2px;padding:1px 3px;");
                connect(nameEdit, &QLineEdit::textChanged, this, [this, tIdx, name](const QString &text) {
                    if (tIdx < m_templateData.size()) { m_templateData[tIdx].effectLabels[name] = text; m_dirty = true; }
                });
                row->addWidget(nameEdit);
            } else {
                auto *nameLbl = new QLabel(name);
                nameLbl->setFixedWidth(100);
                nameLbl->setStyleSheet("color:#222;font-size:11px;border:none");
                row->addWidget(nameLbl);
            }

            auto *sl = new QSlider(Qt::Horizontal);
            int maxVal = (name == "主控") ? 255 : 127;
            sl->setRange(0, maxVal);
            sl->setValue(data.effectValues.value(name, (name == "主控") ? 255 : 0));
            sl->setMaximumWidth(160);

            // Value display
            QString valText;
            if (name == "主控") {
                valText = QString::number(sl->value());
            } else if (name == "主渐暗") {
                valText = "慢";
            } else {
                valText = "快";
            }
            auto *val = new QLineEdit(valText);
            val->setMaximumWidth(48); val->setAlignment(Qt::AlignCenter);
            val->setStyleSheet("color:#000;border:1px solid #888;border-radius:2px;padding:1px 2px;font-size:11px");
            val->setReadOnly(name != "主控");

            if (name == "主控") {
                connect(sl, &QSlider::valueChanged, val, [val](int v) { val->setText(QString::number(v)); });
            }
            connect(sl, &QSlider::valueChanged, this, [this, tIdx, name](int v) {
                if (tIdx < m_templateData.size()) { m_templateData[tIdx].effectValues[name] = v; m_dirty = true; }
            });
            row->addWidget(sl);
            row->addWidget(val);

            // ▶ triangle button for effect demo video
            auto *demoBtn = new QPushButton("▶");
            demoBtn->setFixedSize(22, 22);
            demoBtn->setCursor(Qt::PointingHandCursor);
            demoBtn->setStyleSheet(
                "QPushButton { color:#999; border:none; background:transparent; font-size:9px; }"
                "QPushButton:hover { color:#336; background:#e8e8e8; border-radius:3px; }");
            QString btnLabel = isCustom ? data.effectLabels.value(name, "自定义") : name;
            demoBtn->setToolTip("点击查看「" + btnLabel + "」效果演示");
            connect(demoBtn, &QPushButton::clicked, this, [this, name, tIdx]() {
                // TODO: 弹出对应效果的视频演示
                QString effectLabel = m_templateData[tIdx].effectLabels.value(name, name);
                // showEffectDemo(effectLabel);
            });
            row->addWidget(demoBtn);

            rightL->addLayout(row);
        }
        rightL->addStretch();
        rightScroll->setWidget(rightW);
        hl->addWidget(rightScroll, 1);

        m_editStack->insertWidget(1, editor);
        m_editStack->setCurrentIndex(1);
    }

    void applyAtmosphere(int tIdx, const QString &name)
    {
        if (tIdx < 0 || tIdx >= m_templateData.size()) return;
        auto &d = m_templateData[tIdx].colorValues;

        if (name == "暖调") {
            d["红"]=255; d["绿"]=60;  d["蓝"]=30; d["黄"]=255; d["品红"]=160; d["青"]=30;
            d["紫"]=100; d["天蓝"]=30; d["粉"]=220; d["浅绿"]=60; d["浅紫"]=120; d["橙"]=255;
            d["深蓝"]=20; d["深绿"]=25; d["深紫"]=80; d["金"]=240;
        } else if (name == "冷调") {
            d["红"]=30;  d["绿"]=70;  d["蓝"]=255; d["黄"]=50; d["品红"]=90;  d["青"]=255;
            d["紫"]=140; d["天蓝"]=240; d["粉"]=50; d["浅绿"]=160; d["浅紫"]=170; d["橙"]=30;
            d["深蓝"]=240; d["深绿"]=50; d["深紫"]=150; d["金"]=40;
        } else if (name == "对比撞色") {
            d["红"]=255; d["绿"]=220; d["蓝"]=255; d["黄"]=220; d["品红"]=255; d["青"]=255;
            d["紫"]=220; d["天蓝"]=200; d["粉"]=200; d["浅绿"]=200; d["浅紫"]=220; d["橙"]=240;
            d["深蓝"]=220; d["深绿"]=200; d["深紫"]=255; d["金"]=220;
        } else if (name == "低饱和高级") {
            d["红"]=140; d["绿"]=150; d["蓝"]=160; d["黄"]=130; d["品红"]=140; d["青"]=150;
            d["紫"]=130; d["天蓝"]=150; d["粉"]=160; d["浅绿"]=140; d["浅紫"]=150; d["橙"]=130;
            d["深蓝"]=120; d["深绿"]=130; d["深紫"]=140; d["金"]=140;
        } else if (name == "中性") {
            d["红"]=128; d["绿"]=128; d["蓝"]=128; d["黄"]=128; d["品红"]=128; d["青"]=128;
            d["紫"]=128; d["天蓝"]=128; d["粉"]=128; d["浅绿"]=128; d["浅紫"]=128; d["橙"]=128;
            d["深蓝"]=128; d["深绿"]=128; d["深紫"]=128; d["金"]=128;
        } else if (name == "国风") {
            d["红"]=255; d["绿"]=50;  d["蓝"]=40;  d["黄"]=160; d["品红"]=130; d["青"]=50;
            d["紫"]=110; d["天蓝"]=40;  d["粉"]=110; d["浅绿"]=70; d["浅紫"]=130; d["橙"]=180;
            d["深蓝"]=35; d["深绿"]=55;  d["深紫"]=160; d["金"]=240;
        }
        m_templateData[tIdx].customColor = QColor(200, 200, 200);
        m_dirty = true;
    }

    void initTemplatePreset(int tIdx)
    {
        if (tIdx < 0 || tIdx >= m_templateData.size()) return;
        auto &d = m_templateData[tIdx];
        QString name = m_templateNames.value(tIdx);

        if (name == "开场暖光") {
            d.colorValues = {{"红",255},{"绿",60},{"蓝",30},{"黄",255},{"品红",160},{"青",30},
                             {"紫",100},{"天蓝",30},{"粉",220},{"浅绿",60},{"浅紫",120},{"橙",255},
                             {"深蓝",20},{"深绿",25},{"深紫",80},{"金",240}};
            d.effectValues["主控"] = 200;
        } else if (name == "前奏渐亮") {
            d.colorValues = {{"红",100},{"绿",100},{"蓝",100},{"黄",100},{"品红",100},{"青",100},
                             {"紫",100},{"天蓝",100},{"粉",100},{"浅绿",100},{"浅紫",100},{"橙",100},
                             {"深蓝",100},{"深绿",100},{"深紫",100},{"金",100}};
            d.effectValues["主控"] = 128;
            d.effectValues["主渐亮"] = 120;
        } else if (name == "频闪效果") {
            d.colorValues = {{"红",255},{"绿",30},{"蓝",255},{"黄",30},{"品红",255},{"青",255},
                             {"紫",255},{"天蓝",30},{"粉",255},{"浅绿",30},{"浅紫",255},{"橙",30},
                             {"深蓝",255},{"深绿",30},{"深紫",255},{"金",255}};
            d.effectValues["主控"] = 128;
            d.effectValues["频闪"] = 120;
        } else if (name == "左-右") {
            d.colorValues = {{"红",200},{"绿",180},{"蓝",200},{"黄",180},{"品红",200},{"青",180},
                             {"紫",200},{"天蓝",180},{"粉",200},{"浅绿",180},{"浅紫",200},{"橙",180},
                             {"深蓝",200},{"深绿",180},{"深紫",200},{"金",180}};
            d.effectValues["主控"] = 150;
            d.effectValues["从左到右亮"] = 100;
            d.effectValues["从左到右一只流水"] = 60;
        } else if (name == "渐变呼吸") {
            d.colorValues = {{"红",120},{"绿",140},{"蓝",160},{"黄",120},{"品红",140},{"青",160},
                             {"紫",130},{"天蓝",160},{"粉",140},{"浅绿",150},{"浅紫",140},{"橙",120},
                             {"深蓝",150},{"深绿",140},{"深紫",150},{"金",130}};
            d.effectValues["主控"] = 100;
            d.effectValues["主渐亮"] = 80;
            d.effectValues["主渐暗"] = 80;
        } else if (name == "快速跑灯") {
            d.colorValues = {{"红",255},{"绿",200},{"蓝",255},{"黄",200},{"品红",255},{"青",200},
                             {"紫",255},{"天蓝",200},{"粉",255},{"浅绿",200},{"浅紫",255},{"橙",200},
                             {"深蓝",255},{"深绿",200},{"深紫",255},{"金",255}};
            d.effectValues["主控"] = 180;
            d.effectValues["单只跑灯"] = 100;
            d.effectValues["两只跑灯"] = 80;
            d.effectValues["多只跑灯"] = 60;
        } else if (name == "收光") {
            d.colorValues = {{"红",80},{"绿",60},{"蓝",50},{"黄",70},{"品红",60},{"青",50},
                             {"紫",60},{"天蓝",50},{"粉",70},{"浅绿",50},{"浅紫",60},{"橙",80},
                             {"深蓝",40},{"深绿",40},{"深紫",50},{"金",100}};
            d.effectValues["主控"] = 60;
            d.effectValues["主渐暗"] = 100;
        }
    }

    void flipPage(int dir)
    {
        m_page += dir;
        refreshTemplates();
    }

    void refreshTemplates()
    {
        int perPage = 7;
        int totalPages = qMax((m_templateCount + perPage - 1) / perPage, 1);
        if (m_page >= totalPages) m_page = totalPages - 1;
        m_leftArrow->setVisible(m_page > 0);
        m_rightArrow->setVisible(m_page < totalPages - 1);

        for (int i = 0; i < 7; i++) {
            int idx = m_page * perPage + i;
            if (idx < m_templateCount) {
                m_blocks[i]->setText(m_templateNames.value(idx, QString("模板%1").arg(idx + 1)));
                m_blocks[i]->setVisible(true);
            } else {
                m_blocks[i]->setVisible(false);
            }
        }
    }

    static QColor colorByName(const QString &name)
    {
        static QMap<QString, QColor> map = {
            {"红", QColor(220,40,40)}, {"绿", QColor(40,180,40)}, {"蓝", QColor(40,80,220)},
            {"黄", QColor(220,220,40)}, {"品红", QColor(220,40,180)}, {"青", QColor(40,200,200)},
            {"紫", QColor(140,40,200)}, {"天蓝", QColor(80,180,240)}, {"粉", QColor(255,160,180)},
            {"浅绿", QColor(140,220,140)}, {"浅紫", QColor(200,160,220)}, {"橙", QColor(240,160,40)},
            {"深蓝", QColor(20,40,140)}, {"深绿", QColor(20,100,20)}, {"深紫", QColor(80,20,120)},
            {"金", QColor(220,180,40)},
        };
        return map.value(name, QColor(180,180,180));
    }

    static QString arrowStyle()
    {
        return "QPushButton { background:#e0e0e0; border:1px solid #bbb; border-radius:3px; color:#555; font-size:16px; font-weight:bold; }"
               "QPushButton:hover { background:#d0d0ff; }";
    }

    // Color list
    const QStringList m_colors = {
        "红","绿","蓝","黄","品红","青","紫","天蓝","粉","浅绿",
        "浅紫","橙","深蓝","深绿","深紫","金","自定义"
    };
    // Effect list
    const QStringList m_effects = {
        "主控","主渐亮","主渐暗","频闪","换色",
        "单只跑灯","两只跑灯","多只跑灯",
        "从左到右亮","从右往左亮","从左到右一只流水","从右往左一只流水",
        "单只流水换色","单只流水不换色",
        "自定义1","自定义2","自定义3"
    };

    QList<QPushButton *> m_blocks;
    QPushButton *m_leftArrow = nullptr;
    QPushButton *m_rightArrow = nullptr;
    QStackedWidget *m_editStack = nullptr;
    int m_page = 0;
    int m_templateCount = 7;
    int m_currentTemplate = -1;
    QList<TemplateData> m_templateData;
    QStringList m_templateNames;
    QList<TemplateData> m_savedTemplateData;
    QStringList m_savedTemplateNames;
    bool m_dirty = false;
    QLabel *m_infoName = nullptr;
    QLabel *m_infoChannels = nullptr;
    QLabel *m_infoFixtures = nullptr;
    QLabel *m_artnetStatus = nullptr;
    QLineEdit *m_groupEdit = nullptr;
    QString m_fixtureModel;
    int m_fixtureChannels = 0;
    QStringList m_boundPositions;
    StageLayout *m_stageLayout = nullptr;
};

#endif // PROGRAMPAGE_H
