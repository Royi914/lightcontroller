#ifndef LIBRARYPAGE_H
#define LIBRARYPAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QFrame>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QList>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include "Fixture.h"
#include "fixturedialog.h"

// ===== 新建灯具名称弹窗 =====
class NewFixtureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewFixtureDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("新建灯具型号"); setStyleSheet("background:#fff");
        auto *root = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        m_nameEdit = new QLineEdit; m_nameEdit->setPlaceholderText("例如：LED PAR 8CH");
        m_nameEdit->setStyleSheet("color:#000;border:1px solid #aaa;padding:4px");
        form->addRow("灯具名称：", m_nameEdit);

        // 图标
        auto *iconRow = new QHBoxLayout;
        m_iconPreview = new QLabel;
        m_iconPreview->setFixedSize(36, 36);
        m_iconPreview->setStyleSheet("background:#ddd;border-radius:18px;border:2px solid #bbb;color:#666;font-size:16px");
        m_iconPreview->setAlignment(Qt::AlignCenter);
        m_iconPreview->setText("灯");
        iconRow->addWidget(m_iconPreview);
        iconRow->addSpacing(8);
        auto *pickBtn = new QPushButton("选择图标");
        pickBtn->setStyleSheet("background:#e8e8e8;color:#222;border:1px solid #bbb;border-radius:3px;padding:4px 10px;font-size:11px");
        connect(pickBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择灯具图标", "",
                "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.svg)");
            if (!p.isEmpty()) setIcon(p);
        });
        iconRow->addWidget(pickBtn);
        iconRow->addStretch();
        form->addRow("图标：", iconRow);

        root->addLayout(form);
        auto *btns = new QDialogButtonBox;
        btns->addButton("创建", QDialogButtonBox::AcceptRole);
        btns->addButton(QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(btns);
    }
    QString fixtureName() const { return m_nameEdit->text(); }
    QString iconPath() const { return m_iconPath; }
    void setIcon(const QString &path)
    {
        m_iconPath = path;
        QImage img(path);
        if (img.isNull()) return;
        int sq = qMin(img.width(), img.height());
        int cx = (img.width() - sq) / 2, cy = (img.height() - sq) / 2;
        QPixmap pm = QPixmap::fromImage(
            img.copy(cx, cy, sq, sq).scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QPixmap circle(36, 36); circle.fill(Qt::transparent);
        QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
        pp.setBrush(pm); pp.setPen(Qt::NoPen);
        pp.drawEllipse(0, 0, 36, 36); pp.end();
        m_iconPreview->setPixmap(circle);
    }
private:
    QLineEdit *m_nameEdit;
    QLabel    *m_iconPreview;
    QString    m_iconPath;
};

// ===== 灯库页面 =====
class LibraryPage : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);  // 接收键盘事件（Delete 删除）
        auto *outerRoot = new QHBoxLayout(this);
        outerRoot->setContentsMargins(0,0,0,0);

        // === 左侧工具栏 ===
        auto *toolbar = new QFrame;
        toolbar->setFixedWidth(140);
        toolbar->setStyleSheet("background:#e8e8e8;border-right:1px solid #ccc");
        auto *tbLayout = new QVBoxLayout(toolbar);
        tbLayout->setSpacing(4);

        auto addBtn = [&](const QString &text, void (LibraryPage::*sig)()) {
            auto *btn = new QPushButton("  " + text);
            btn->setFixedHeight(36);
            btn->setStyleSheet("background:#f5f5f5;color:#222;border:1px solid #ccc;border-radius:4px;text-align:left;padding-left:12px");
            connect(btn, &QPushButton::clicked, this, sig);
            tbLayout->addWidget(btn);
        };
        addBtn("新建",   &LibraryPage::onNewFixture);
        addBtn("打开",   &LibraryPage::onOpenFile);
        addBtn("保存",   &LibraryPage::onSaveCurrent);
        addBtn("另存为", &LibraryPage::onSaveAsFile);
        tbLayout->addStretch();

        m_listBtn = new QPushButton("  灯具列表");
        m_listBtn->setFixedHeight(36);
        m_listBtn->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px;text-align:left;padding-left:12px");
        connect(m_listBtn, &QPushButton::clicked, this, [this]() { if (maybeSave()) m_stack->setCurrentIndex(0); });
        tbLayout->addWidget(m_listBtn);

        auto *backBtn = new QPushButton("  返回主界面");
        backBtn->setFixedHeight(36);
        backBtn->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px;text-align:left;padding-left:12px");
        connect(backBtn, &QPushButton::clicked, this, [this]() { if (maybeSave()) emit goBackRequested(); });
        tbLayout->addWidget(backBtn);
        outerRoot->addWidget(toolbar);

        // === 右侧区域 ===
        auto *rightPanel = new QWidget;
        auto *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(0);

        auto *rightTopBar = new QWidget;
        rightTopBar->setFixedHeight(28);
        rightTopBar->setStyleSheet("background:#f8f8f8; border-bottom:1px solid #e0e0e0;");
        auto *rtbLayout = new QHBoxLayout(rightTopBar);
        rtbLayout->setContentsMargins(12, 2, 12, 2);
        rtbLayout->addStretch();
        m_artnetStatus = new QLabel("○ ArtNet 未连接");
        m_artnetStatus->setStyleSheet("color:#999; font-size:11px; font-weight:bold; border:none;");
        rtbLayout->addWidget(m_artnetStatus);
        rightLayout->addWidget(rightTopBar);

        // === 右侧 QStackedWidget ===
        m_stack = new QStackedWidget;
        rightLayout->addWidget(m_stack, 1);
        outerRoot->addWidget(rightPanel, 1);
        buildListView();
        buildDetailView();
    }

    /// 居中裁剪为正方形，再缩放到目标尺寸
    static QPixmap centerCropToSquare(const QString &path, int targetSize)
    {
        QImage img(path);
        if (img.isNull()) return QPixmap();
        int w = img.width(), h = img.height();
        int sq = qMin(w, h);
        int x = (w - sq) / 2, y = (h - sq) / 2;
        QImage cropped = img.copy(x, y, sq, sq);
        return QPixmap::fromImage(
            cropped.scaled(targetSize, targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void setLibrary(const QList<FixtureDef> &library) { m_library = library; m_selectedLibIndex = -1; refreshListView(); }
    QList<FixtureDef> library() const { return m_library; }
    bool isDirty() const { return m_dirty; }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Delete && m_selectedLibIndex >= 0
            && m_selectedLibIndex < m_library.size()) {
            auto answer = QMessageBox::question(this, "确认删除",
                QString("确定要删除灯具型号「%1」吗？").arg(m_library[m_selectedLibIndex].name),
                QMessageBox::Yes | QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                m_library.removeAt(m_selectedLibIndex);
                m_selectedLibIndex = -1;
                m_dirty = true;
                refreshListView();
                emit libraryUpdated(m_library);
            }
            return;
        }
        QWidget::keyPressEvent(event);
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

    /// Returns true if it's safe to navigate away (saved / ignored / not dirty)
    /// Returns false if user clicked X (stay on current page, keep dirty flag)
    bool maybeSave()
    {
        if (!m_dirty) return true;
        bool isNew = (m_editingIndex < 0);

        QMessageBox mb(this);
        mb.setWindowTitle(isNew ? "未保存的新建灯具" : "未保存的修改");
        mb.setText(isNew ? "当前新建灯具数据尚未保存，是否保存？"
                         : "当前灯具数据修改尚未保存，是否保存？");
        mb.setIcon(QMessageBox::Question);
        QPushButton *saveBtn   = mb.addButton("保存", QMessageBox::AcceptRole);
        QPushButton *ignoreBtn = mb.addButton("忽略", QMessageBox::DestructiveRole);
        mb.setDefaultButton(saveBtn);
        mb.setEscapeButton(nullptr);  // only X can close, not Escape
        mb.exec();

        if (mb.clickedButton() == saveBtn) {
            onSaveCurrent();                     // saves & clears m_dirty
            return true;
        } else if (mb.clickedButton() == ignoreBtn) {
            m_dirty = false;                     // discard changes
            m_stack->setCurrentIndex(0);         // back to library list
            return true;
        }
        // X clicked — stay on this page, keep m_dirty, try again next time
        return false;
    }

signals:
    void goBackRequested();
    void libraryUpdated(const QList<FixtureDef> &library);

private slots:
    void onNewFixture()
    {
        if (!maybeSave()) return;
        NewFixtureDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;
        QString name = dlg.fixtureName();
        if (name.isEmpty()) { QMessageBox::warning(this, "提示", "名称不能为空"); return; }
        m_editingIndex = -1;
        m_dirty = false;
        m_detailName = name;
        m_detailChannels = 0;
        m_detailRanges.clear();
        m_committedFlags.clear();
        m_committedNames.clear();
        m_pendingNames.clear();
        m_channelEdits.clear();
        m_pendingIconPath = dlg.iconPath();
        m_channelIcons.clear();
        rebuildDetail();
        m_stack->setCurrentIndex(1);
    }

    void onOpenFile()
    {
        if (!maybeSave()) return;
        QString path = QFileDialog::getOpenFileName(this, "打开灯具文件", "", "JSON 文件 (*.json);;所有文件 (*)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        for (auto v : arr) {
            QJsonObject o = v.toObject();
            FixtureDef def;
            def.name = o["name"].toString();
            QJsonArray chArr = o["channels"].toArray();
            for (auto cv : chArr) {
                QJsonObject co = cv.toObject();
                def.channelNames << co["name"].toString();
                QJsonArray rArr = co["ranges"].toArray();
                QList<ChannelRange> ranges;
                for (auto rv : rArr) {
                    QJsonObject ro = rv.toObject();
                    ranges << ChannelRange{ro["label"].toString(), ro["min"].toInt(), ro["max"].toInt()};
                }
                def.ranges << ranges;
            }
            def.channels = def.channelNames.size();
            m_library << def;
        }
        f.close();
        refreshListView();
        emit libraryUpdated(m_library);
    }

    void onSaveCurrent()
    {
        if (m_stack->currentIndex() != 1) return;
        FixtureDef def = collectDetail();
        if (def.name.isEmpty()) return;
        if (m_editingIndex >= 0 && m_editingIndex < m_library.size())
            m_library[m_editingIndex] = def;
        else
            m_library << def;
        m_dirty = false;
        refreshListView();
        emit libraryUpdated(m_library);
        m_stack->setCurrentIndex(0);
    }

    void onSaveAsFile()
    {
        QString path = QFileDialog::getSaveFileName(this, "另存为", "灯具配置.json", "JSON 文件 (*.json)");
        if (path.isEmpty()) return;
        QJsonArray arr;
        for (const auto &def : m_library) {
            QJsonObject o;
            o["name"] = def.name;
            QJsonArray chArr;
            for (int i = 0; i < def.channelNames.size(); i++) {
                QJsonObject co; co["name"] = def.channelNames[i];
                QJsonArray rArr;
                if (i < def.ranges.size()) for (auto &r : def.ranges[i])
                    rArr << QJsonObject{{"label", r.name}, {"min", r.minValue}, {"max", r.maxValue}};
                co["ranges"] = rArr; chArr << co;
            }
            o["channels"] = chArr; arr << o;
        }
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(arr).toJson()); f.close(); }
    }

    void saveUncommittedText()
    {
        m_pendingNames.resize(m_detailChannels);
        for (int i = 0; i < m_channelEdits.size() && i < m_detailChannels; i++) {
            if (!m_committedFlags.value(i, false))
                m_pendingNames[i] = m_channelEdits[i]->text();
        }
    }

    void onAddChannel()
    {
        m_dirty = true;
        saveUncommittedText();
        m_detailChannels++;
        m_detailRanges.resize(m_detailChannels);
        m_committedFlags.resize(m_detailChannels);
        m_committedFlags[m_detailChannels - 1] = false;
        m_committedNames.resize(m_detailChannels);
        m_pendingNames.resize(m_detailChannels);
        m_channelIcons.resize(m_detailChannels);
        rebuildDetail();
    }

    void onAddChannels(int count)
    {
        if (count <= 0) return;
        m_dirty = true;
        saveUncommittedText();
        int oldCount = m_detailChannels;
        m_detailChannels += count;
        m_detailRanges.resize(m_detailChannels);
        m_committedFlags.resize(m_detailChannels);
        m_committedNames.resize(m_detailChannels);
        m_pendingNames.resize(m_detailChannels);
        m_channelIcons.resize(m_detailChannels);
        for (int i = oldCount; i < m_detailChannels; i++)
            m_committedFlags[i] = false;
        rebuildDetail();
    }

    void onCommitChannel(int ch)
    {
        if (ch < 0 || ch >= m_detailChannels) return;
        saveUncommittedText();
        if (ch < m_channelEdits.size()) {
            QString t = m_channelEdits[ch]->text().trimmed();
            if (t.isEmpty()) return;  // 空文本不确认
            m_dirty = true;
            m_committedFlags[ch] = true;
            m_committedNames.resize(m_detailChannels);
            m_committedNames[ch] = t;
            // 行背景变色（内联更新，不重建整个表）
            if (auto *row = qobject_cast<QFrame *>(m_channelEdits[ch]->parent()))
                row->setStyleSheet("background:#efe;border:none");
            refreshParamPanel();
        }
    }

    void onModifyChannel(int ch)
    {
        if (ch < 0 || ch >= m_detailChannels) return;
        m_committedFlags[ch] = false;
        if (ch < m_channelEdits.size()) {
            m_channelEdits[ch]->setReadOnly(false);
            m_channelEdits[ch]->setFocus();
        }
    }

    void onRemoveChannel(int ch)
    {
        if (ch < 0 || ch >= m_detailChannels) return;
        if (m_detailChannels <= 1) return;
        m_dirty = true;
        saveUncommittedText();
        m_detailRanges.removeAt(ch);
        m_committedFlags.removeAt(ch);
        m_committedNames.removeAt(ch);
        m_pendingNames.removeAt(ch);
        m_detailChannels--;
        rebuildDetail();
    }
    void onRowDoubleClicked(int index)
    {
        if (index < 0 || index >= m_library.size()) return;
        if (!maybeSave()) return;
        m_editingIndex = index;
        m_dirty = false;
        auto &def = m_library[index];
        m_detailName = def.name;
        m_pendingIconPath = def.iconPath;
        m_detailChannels = def.channels;
        m_channelIcons = def.channelIcons;
        m_detailRanges = def.ranges;
        m_detailRanges.resize(m_detailChannels);
        m_committedFlags.clear(); m_committedNames.clear(); m_pendingNames.clear();
        m_committedFlags.resize(m_detailChannels);
        m_committedNames.resize(m_detailChannels);
        m_pendingNames.resize(m_detailChannels);
        for (int i = 0; i < m_detailChannels; i++) {
            m_committedFlags[i] = true;
            m_committedNames[i] = def.channelNames.value(i, "");
        }
        rebuildDetail();
        for (int i = 0; i < qMin(def.channelNames.size(), m_channelEdits.size()); i++)
            m_channelEdits[i]->setText(def.channelNames[i]);
        refreshParamPanel();
        m_stack->setCurrentIndex(1);
    }

private:
    void buildListView()
    {
        auto *page = new QFrame; page->setStyleSheet("background:#fff");
        auto *layout = new QVBoxLayout(page);
        auto *title = new QLabel("灯具库");
        title->setStyleSheet("color:#222;font-size:16px;font-weight:bold;padding:8px;border:none");
        layout->addWidget(title);
        auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
        m_listContainer = new QWidget;
        m_listLayout = new QVBoxLayout(m_listContainer);
        m_listLayout->setSpacing(4); m_listLayout->addStretch();
        scroll->setWidget(m_listContainer);
        layout->addWidget(scroll);
        m_stack->addWidget(page);
    }

    void buildDetailView()
    {
        auto *page = new QFrame; page->setStyleSheet("background:#fff");
        auto *root = new QHBoxLayout(page);
        // 左：参数预览
        auto *left = new QFrame; left->setStyleSheet("background:#fafafa;border-right:1px solid #ddd");
        auto *leftLayout = new QVBoxLayout(left);
        m_detailTitleLabel = new QLabel;
        m_detailTitleLabel->setStyleSheet("color:#222;font-size:16px;font-weight:bold;padding:8px;border:none");
        leftLayout->addWidget(m_detailTitleLabel);
        m_paramScroll = new QScrollArea; m_paramScroll->setWidgetResizable(true);
        auto *pw = new QWidget; m_paramPanel = new QVBoxLayout(pw); m_paramPanel->addStretch();
        m_paramScroll->setWidget(pw);
        leftLayout->addWidget(m_paramScroll, 1);
        root->addWidget(left, 2);
        // 右：通道表
        auto *right = new QFrame; right->setFixedWidth(380);
        right->setStyleSheet("background:#f5f5f5;border-left:1px solid #ddd");
        auto *rightLayout = new QVBoxLayout(right);
        auto *chTitle = new QLabel("通道表");
        chTitle->setStyleSheet("color:#333;font-size:14px;font-weight:bold;padding:8px;border:none");
        rightLayout->addWidget(chTitle);
        auto *addBtn = new QPushButton("+ 增加通道");
        addBtn->setStyleSheet("background:#e0e0ff;color:#223;border:1px solid #aab;border-radius:4px;padding:6px");
        connect(addBtn, &QPushButton::clicked, this, [this]() {
            bool ok; int n = QInputDialog::getInt(this, "批量增加通道", "要增加几个通道？(DMX512=512)", 1, 1, 512, 1, &ok);
            if (ok) onAddChannels(n);
        });
        rightLayout->addWidget(addBtn);
        m_channelScroll = new QScrollArea; m_channelScroll->setWidgetResizable(true);
        m_channelWidget = new QWidget;
        m_channelLayout = new QVBoxLayout(m_channelWidget);
        m_channelScroll->setWidget(m_channelWidget);
        rightLayout->addWidget(m_channelScroll);
        root->addWidget(right);
        m_stack->addWidget(page);
    }

    void rebuildDetail()
    {
        m_detailTitleLabel->setText(m_detailName);
        // 彻底删除旧通道表 UI
        QLayoutItem *pc;
        while ((pc = m_channelLayout->takeAt(0)) != nullptr) {
            if (pc->widget()) { pc->widget()->deleteLater(); }
            if (pc->layout()) {
                QLayout *sub = pc->layout();
                QLayoutItem *sc;
                while ((sc = sub->takeAt(0)) != nullptr) { if (sc->widget()) sc->widget()->deleteLater(); delete sc; }
            }
            delete pc;
        }
        m_channelEdits.clear();
        for (int i = 0; i < m_detailChannels; i++) {
            bool done = m_committedFlags.value(i, false);
            auto *row = new QFrame;
            row->setStyleSheet(QString("background:%1;border:none").arg(done ? "#efe" : "transparent"));
            row->setMinimumHeight(40);
            auto *hl = new QHBoxLayout(row);
            hl->addWidget(new QLabel(QString("通道%1").arg(i+1)));

            // 图标预览 + R/G/B 色标（通道表中）
            const int tblIconSz = 22;
            auto *iconPreview = new QLabel;
            iconPreview->setFixedSize(tblIconSz, tblIconSz);
            bool hasIcon = i < m_channelIcons.size() && !m_channelIcons[i].isEmpty();
            QString pendingName = (i < m_channelEdits.size()) ? m_channelEdits[i]->text() : "";
            QString chkName = hasIcon ? "" : (done ? m_committedNames.value(i, "") : pendingName);
            bool isR = chkName.contains("红") || chkName.toLower() == "r";
            bool isG = chkName.contains("绿") || chkName.toLower() == "g";
            bool isB = chkName.contains("蓝") || chkName.toLower() == "b";
            if (hasIcon) {
                QPixmap pm = centerCropToSquare(m_channelIcons[i], tblIconSz);
                if (!pm.isNull()) {
                    QPixmap circle(tblIconSz, tblIconSz); circle.fill(Qt::transparent);
                    QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
                    pp.setBrush(pm); pp.setPen(Qt::NoPen);
                    pp.drawEllipse(0, 0, tblIconSz, tblIconSz); pp.end();
                    iconPreview->setPixmap(circle);
                }
            } else if (isR || isG || isB) {
                QColor dotCol = isR ? QColor("#dc3c28") : (isG ? QColor("#28b43c") : QColor("#2850dc"));
                iconPreview->setStyleSheet(
                    QString("background:%1;border-radius:%2px;border:1px solid #999")
                    .arg(dotCol.name()).arg(tblIconSz/2));
            }
            hl->addWidget(iconPreview);

            auto *nameEdit = new QLineEdit;
            nameEdit->setPlaceholderText(QString("通道%1").arg(i + 1));
            nameEdit->setStyleSheet("color:#000;border:1px solid #bbb;padding:2px");
            if (done && i < m_committedNames.size() && !m_committedNames[i].isEmpty())
                nameEdit->setText(m_committedNames[i]);
            else if (!done && i < m_pendingNames.size() && !m_pendingNames[i].isEmpty())
                nameEdit->setText(m_pendingNames[i]);
            // 双击 → 进入编辑（已确认的行变回未确认状态）
            nameEdit->installEventFilter(this);
            nameEdit->setProperty("chIndex", i);
            // 失焦自动确认
            int capI = i; // capture for lambda
            connect(nameEdit, &QLineEdit::editingFinished, this, [this, capI, iconPreview]() mutable {
                saveUncommittedText();
                if (capI < m_channelEdits.size()) {
                    QString t = m_channelEdits[capI]->text().trimmed();
                    if (t.isEmpty()) return;
                    m_dirty = true;
                    m_committedFlags[capI] = true;
                    m_committedNames.resize(m_detailChannels);
                    m_committedNames[capI] = t;
                    // 行变绿
                    if (auto *par = qobject_cast<QFrame *>(m_channelEdits[capI]->parent()))
                        par->setStyleSheet("background:#efe;border:none");
                    // 内联更新 R/G/B 色标
                    bool hasIcon = capI < m_channelIcons.size() && !m_channelIcons[capI].isEmpty();
                    if (!hasIcon) {
                        bool isR = t.contains("红") || t.toLower() == "r";
                        bool isG = t.contains("绿") || t.toLower() == "g";
                        bool isB = t.contains("蓝") || t.toLower() == "b";
                        if (isR || isG || isB) {
                            QColor dotCol = isR ? QColor("#dc3c28") : (isG ? QColor("#28b43c") : QColor("#2850dc"));
                            iconPreview->setStyleSheet(
                                QString("background:%1;border-radius:11px;border:1px solid #999").arg(dotCol.name()));
                            iconPreview->setPixmap(QPixmap()); // 清除 pixmap
                        }
                    }
                    refreshParamPanel();
                }
            });
            hl->addWidget(nameEdit, 2);
            m_channelEdits << nameEdit;

            auto *customBtn = new QPushButton("自定义");
            // 初始化：如果已有配置好的值域，恢复"已配(N)"文字
            {
                int initCnt = (i < m_detailRanges.size()) ? m_detailRanges[i].size() : 0;
                if (initCnt > 0) customBtn->setText(QString("已配(%1)").arg(initCnt));
            }
            connect(customBtn, &QPushButton::clicked, this, [this,i,customBtn](){
                RangeDialog dlg(this);
                if (i < m_detailRanges.size()) dlg.setRanges(m_detailRanges[i]);
                if (dlg.exec() == QDialog::Accepted) {
                    if (i < m_detailRanges.size()) m_detailRanges[i] = dlg.getRanges();
                    customBtn->setText(m_detailRanges[i].size()>0?QString("已配(%1)").arg(m_detailRanges[i].size()):"自定义");
                    refreshParamPanel();
                }
            });
            hl->addWidget(customBtn);

            // 导入图标按钮
            auto *iconBtn = new QPushButton("📁");
            iconBtn->setFixedSize(22,22);
            iconBtn->setToolTip("导入图标图片");
            iconBtn->setStyleSheet("border:none;background:transparent;font-size:11px;");
            connect(iconBtn, &QPushButton::clicked, this, [this,i](){
                QString path = QFileDialog::getOpenFileName(this, "选择图标图片", "",
                    "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.svg)");
                if (!path.isEmpty()) {
                    if (i < m_channelIcons.size()) m_channelIcons[i] = path;
                    rebuildDetail();
                }
            });
            hl->addWidget(iconBtn);

            auto *delBtn = new QPushButton("×"); delBtn->setFixedSize(24,24);
            delBtn->setStyleSheet("color:#c44;font-weight:bold;border:none;background:transparent");
            connect(delBtn, &QPushButton::clicked, this, [this,i](){ onRemoveChannel(i); });
            hl->addWidget(delBtn);

            m_channelLayout->addWidget(row);
        }
        m_channelLayout->addStretch();
        refreshParamPanel();
    }

    void refreshParamPanel()
    {
        // 删除旧容器，创建新容器 —— 最彻底的清理方式
        QWidget *old = m_paramPanel->parentWidget();
        if (old) {
            old->deleteLater();
            // 重建
            auto *pw = new QWidget;
            m_paramPanel = new QVBoxLayout(pw);
            m_paramScroll->setWidget(pw);
        } else {
            // 首次调用，m_paramPanel 是 buildDetailView 里创建的
            QLayoutItem *pc;
            while ((pc = m_paramPanel->takeAt(0)) != nullptr) {
                if (pc->widget()) pc->widget()->deleteLater();
                if (pc->layout()) {
                    QLayout *sub = pc->layout();
                    QLayoutItem *sc;
                    while ((sc = sub->takeAt(0)) != nullptr) { if (sc->widget()) sc->widget()->deleteLater(); delete sc; }
                }
                delete pc;
            }
        }

        int idx = 0;
        for (int i = 0; i < m_detailChannels; i++) {
            if (!m_committedFlags.value(i, false)) continue;
            idx++;
            QString name = (i < m_committedNames.size() && !m_committedNames[i].isEmpty())
                ? m_committedNames[i] : QString("通道%1").arg(idx);

            auto *row = new QHBoxLayout;
            row->setAlignment(Qt::AlignVCenter);
            // 颜色标记 / 导入图标（统一尺寸 18×18）
            const int markSz = 18;
            auto *mark = new QLabel;
            mark->setFixedSize(markSz, markSz);
            bool hasIcon = i < m_channelIcons.size() && !m_channelIcons[i].isEmpty();
            if (hasIcon) {
                QPixmap pm = centerCropToSquare(m_channelIcons[i], markSz);
                if (!pm.isNull()) {
                    QPixmap circle(markSz, markSz);
                    circle.fill(Qt::transparent);
                    QPainter pp(&circle);
                    pp.setRenderHint(QPainter::Antialiasing);
                    pp.setBrush(pm);
                    pp.setPen(QPen(QColor(0x99,0x99,0x99), 1));
                    pp.drawEllipse(1, 1, markSz - 2, markSz - 2);
                    pp.end();
                    mark->setPixmap(circle);
                } else {
                    mark->setStyleSheet(
                        QString("background:#ccc;border-radius:%1px;border:1px solid #999").arg(markSz/2));
                }
            } else {
                QString nLower = name.toLower();
                if (name.contains("红") || nLower == "r")
                    mark->setStyleSheet(QString("background:#dc3c28;border-radius:%1px;border:1px solid #999").arg(markSz/2));
                else if (name.contains("绿") || nLower == "g")
                    mark->setStyleSheet(QString("background:#28b43c;border-radius:%1px;border:1px solid #999").arg(markSz/2));
                else if (name.contains("蓝") || nLower == "b")
                    mark->setStyleSheet(QString("background:#2850dc;border-radius:%1px;border:1px solid #999").arg(markSz/2));
                else mark->setStyleSheet("background:transparent;border:none");
            }
            const int MARK_SZ   = 18;   // 圆形标记尺寸
            const int LABEL_W   = 112;  // 主标签宽
            const int SUBINDENT = 50;   // 子项缩进（对齐参数名 "亮度"）
            const int SUBLABEL_W = LABEL_W - SUBINDENT;
            const int GAP       = 6;
            const int LEFT_W    = MARK_SZ + LABEL_W; // 主行和子行统一左列宽

            // 主行左侧：mark + label，用固定容器确保与子行对齐
            auto *mainLeft = new QWidget;
            mainLeft->setFixedWidth(LEFT_W);
            auto *mlL = new QHBoxLayout(mainLeft);
            mlL->setContentsMargins(0,0,0,0); mlL->setSpacing(0);
            mlL->addWidget(mark);
            QString numStr = QString("通道%1").arg(i + 1);
            QString displayName = name.isEmpty() || name == numStr ? numStr
                : QString("%1  %2").arg(numStr).arg(name);
            auto *label = new QLabel(displayName);
            label->setFixedWidth(LABEL_W);
            label->setStyleSheet("color:#222;font-size:12px;border:none;padding:2px 4px");
            mlL->addWidget(label);
            row->addWidget(mainLeft);
            row->addSpacing(GAP);

            if (i < m_detailRanges.size() && !m_detailRanges[i].isEmpty()) {
                row->addStretch();
                m_paramPanel->addLayout(row);
                // 收集本通道所有子项滑块（互斥：激活一个其余灰掉归零）
                QList<QSlider *>   groupSliders;
                QList<QLineEdit *> groupVals;
                QList<QLabel *>    groupLabels;
                int rangeCount = m_detailRanges[i].size();

                for (int ri = 0; ri < rangeCount; ri++) {
                    const auto &r = m_detailRanges[i][ri];
                    auto *subRow = new QHBoxLayout;
                    subRow->setAlignment(Qt::AlignVCenter);
                    auto *subLeft = new QWidget;
                    subLeft->setFixedWidth(LEFT_W);
                    auto *slL = new QHBoxLayout(subLeft);
                    slL->setContentsMargins(0,0,0,0); slL->setSpacing(0);
                    auto *ph = new QLabel;
                    ph->setFixedSize(MARK_SZ, MARK_SZ);
                    ph->setStyleSheet("background:transparent;border:none");
                    slL->addWidget(ph);
                    slL->addSpacing(SUBINDENT);
                    // 小功能图标
                    if (!r.iconPath.isEmpty()) {
                        QPixmap iconPm = centerCropToSquare(r.iconPath, MARK_SZ);
                        if (!iconPm.isNull()) {
                            QPixmap circ(MARK_SZ, MARK_SZ); circ.fill(Qt::transparent);
                            QPainter pp(&circ); pp.setRenderHint(QPainter::Antialiasing);
                            pp.setBrush(iconPm);
                            pp.setPen(QPen(QColor(0x99,0x99,0x99), 1));
                            pp.drawEllipse(1, 1, MARK_SZ - 2, MARK_SZ - 2);
                            pp.end();
                            auto *iconLbl = new QLabel;
                            iconLbl->setFixedSize(MARK_SZ, MARK_SZ);
                            iconLbl->setPixmap(circ);
                            slL->addWidget(iconLbl);
                            slL->addSpacing(2);
                        }
                    }
                    auto *subLabel = new QLabel(r.name);
                    subLabel->setFixedWidth(SUBLABEL_W - (r.iconPath.isEmpty() ? 0 : MARK_SZ + 2));
                    subLabel->setStyleSheet("color:#444;font-size:11px;border:none;padding:2px 4px");
                    slL->addWidget(subLabel);
                    subRow->addWidget(subLeft);
                    subRow->addSpacing(GAP);

                    auto *slider = new QSlider(Qt::Horizontal);
                    slider->setRange(r.minValue, r.maxValue);
                    slider->setValue(r.minValue);        // 初始归零（未激活）
                    slider->setMaximumWidth(180); slider->setFixedHeight(20);
                    auto *val = new QLineEdit(QString::number(r.minValue));
                    val->setMaxLength(3); val->setMaximumWidth(38); val->setAlignment(Qt::AlignCenter);
                    val->setStyleSheet("color:#999;border:1px solid #ccc;border-radius:3px;padding:1px 3px;font-size:10px");
                    connect(slider, &QSlider::valueChanged, val, [val](int v){ val->setText(QString::number(v)); });
                    subRow->addWidget(slider);
                    subRow->addWidget(val);
                    subRow->addStretch();
                    m_paramPanel->addLayout(subRow);

                    groupSliders << slider;
                    groupVals    << val;
                    groupLabels  << subLabel;
                }

                // 互斥逻辑：点击即激活，其余仅变灰（保留各自位置），不归零
                for (int ri = 0; ri < rangeCount; ri++) {
                    connect(groupSliders[ri], &QSlider::sliderPressed, this,
                        [this, groupSliders, groupVals, groupLabels, rangeCount, ri]() {
                            for (int j = 0; j < rangeCount; j++) {
                                if (j == ri) {
                                    groupLabels[j]->setStyleSheet("color:#222;font-size:11px;border:none;padding:2px 4px;font-weight:bold");
                                    groupVals[j]->setStyleSheet("color:#000;border:1px solid #888;border-radius:3px;padding:1px 3px;font-size:10px");
                                } else {
                                    groupLabels[j]->setStyleSheet("color:#bbb;font-size:11px;border:none;padding:2px 4px");
                                    groupVals[j]->setStyleSheet("color:#ccc;border:1px solid #ddd;border-radius:3px;padding:1px 3px;font-size:10px");
                                }
                            }
                        });
                }
            } else {
                auto *slider = new QSlider(Qt::Horizontal); slider->setRange(0,255); slider->setValue(255);
                slider->setMaximumWidth(180); slider->setFixedHeight(22);
                auto *val = new QLineEdit("255");
                val->setMaxLength(3); val->setMaximumWidth(38); val->setAlignment(Qt::AlignCenter);
                val->setStyleSheet("color:#000;border:1px solid #888;border-radius:3px;padding:1px 3px;font-size:11px");
                connect(slider, &QSlider::valueChanged, val, [val](int v){ val->setText(QString::number(v)); });
                row->addWidget(slider);
                row->addWidget(val);
                row->addStretch();
                m_paramPanel->addLayout(row);
            }
        }
        if (idx == 0)
            m_paramPanel->addWidget(new QLabel("（尚未添加通道，请在右侧通道表中填写并点击完成）"));
        m_paramPanel->addStretch();
    }

    void refreshListView()
    {
        while (m_listLayout->count() > 1) { auto *it = m_listLayout->takeAt(0); if (it->widget()) delete it->widget(); delete it; }
        for (int idx = 0; idx < m_library.size(); idx++) {
            auto &def = m_library[idx];
            auto *row = new QFrame;
            row->setStyleSheet("background:#f9f9f9;border:1px solid #ddd;border-radius:3px;cursor:pointer");
            row->setMinimumHeight(44);
            auto *hl = new QHBoxLayout(row);
            auto *icon = new QLabel; icon->setFixedSize(36,36);
            icon->setAlignment(Qt::AlignCenter);
            if (!def.iconPath.isEmpty()) {
                QImage img(def.iconPath);
                if (!img.isNull()) {
                    int sq = qMin(img.width(), img.height());
                    int cx = (img.width() - sq) / 2, cy = (img.height() - sq) / 2;
                    QPixmap pm = QPixmap::fromImage(
                        img.copy(cx, cy, sq, sq).scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    QPixmap circle(36, 36); circle.fill(Qt::transparent);
                    QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
                    pp.setBrush(pm); pp.setPen(Qt::NoPen);
                    pp.drawEllipse(0, 0, 36, 36); pp.end();
                    icon->setPixmap(circle);
                } else {
                    icon->setText("灯");
                    icon->setStyleSheet("background:#ddd;border-radius:18px;border:2px solid #bbb;color:#666");
                }
            } else {
                icon->setText("灯");
                icon->setStyleSheet("background:#ddd;border-radius:18px;border:2px solid #bbb;color:#666");
            }
            hl->addWidget(icon);
            auto *n = new QLabel(QString("%1 (%2通道)").arg(def.name).arg(def.channels));
            n->setStyleSheet("color:#222;font-weight:bold;font-size:13px;border:none");
            hl->addWidget(n); hl->addStretch();
            // 选中高亮
            if (idx == m_selectedLibIndex)
                row->setStyleSheet("background:#dde;border:2px solid #66b;border-radius:3px;cursor:pointer");
            int rowIdx = idx;
            row->installEventFilter(this);
            row->setProperty("libIndex", idx);
            m_listLayout->insertWidget(m_listLayout->count() - 1, row);
        }
    }

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        // 通道表 QLineEdit 双击 → 进入修改模式
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *edit = qobject_cast<QLineEdit *>(obj);
            if (edit && edit->property("chIndex").isValid()) {
                int ci = edit->property("chIndex").toInt();
                if (ci >= 0 && ci < m_detailChannels && m_committedFlags.value(ci, false)) {
                    m_committedFlags[ci] = false;
                    m_dirty = true;
                    if (auto *par = qobject_cast<QFrame *>(edit->parent()))
                        par->setStyleSheet("background:transparent;border:none");
                }
                return false; // 继续传递双击事件给 QLineEdit 以选中文本
            }
            // 灯库列表双击 → 编辑
            auto *frame = qobject_cast<QFrame *>(obj);
            if (frame && frame->property("libIndex").isValid()) {
                onRowDoubleClicked(frame->property("libIndex").toInt());
                return true;
            }
        }
        // 灯库列表单击 → 选中
        if (event->type() == QEvent::MouseButtonPress) {
            auto *frame = qobject_cast<QFrame *>(obj);
            if (frame && frame->property("libIndex").isValid()) {
                int idx = frame->property("libIndex").toInt();
                m_selectedLibIndex = (idx == m_selectedLibIndex) ? -1 : idx;
                setFocus();
                refreshListView();
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    FixtureDef collectDetail()
    {
        FixtureDef def; def.name = m_detailName;
        def.iconPath = m_pendingIconPath;
        def.channelIcons = m_channelIcons;
        // 只收集已确认的通道
        int cnt = 0;
        for (int i = 0; i < m_detailChannels; i++) {
            if (!m_committedFlags.value(i, false)) continue;
            cnt++;
            QString nm = i < m_committedNames.size() ? m_committedNames[i] : "";
            def.channelNames << (nm.isEmpty() ? QString("通道%1").arg(cnt) : nm);
            def.ranges << (i < m_detailRanges.size() ? m_detailRanges[i] : QList<ChannelRange>());
        }
        def.channels = cnt;
        return def;
    }

    QList<FixtureDef> m_library;
    int m_editingIndex = -1;
    bool m_dirty = false;
    QWidget *m_listContainer;
    QVBoxLayout *m_listLayout;
    QLabel *m_detailTitleLabel = nullptr;
    QScrollArea *m_paramScroll = nullptr;
    QVBoxLayout *m_paramPanel;
    QScrollArea *m_channelScroll;
    QWidget *m_channelWidget;
    QVBoxLayout *m_channelLayout;
    QList<QLineEdit *> m_channelEdits;
    QString m_detailName;
    int m_detailChannels = 0;
    QList<QList<ChannelRange>> m_detailRanges;
    QList<bool> m_committedFlags;
    QStringList m_committedNames;
    QStringList m_pendingNames;
    QStringList m_channelIcons;
    QString m_pendingIconPath;       // 新建灯具时暂存图标路径
    int m_selectedLibIndex = -1;     // 灯库列表选中行（-1=无选中）
    QStackedWidget *m_stack;
    QPushButton *m_listBtn;
    QLabel *m_artnetStatus = nullptr;
};

#endif
