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
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QList>
#include "Fixture.h"

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
        root->addLayout(form);
        auto *btns = new QDialogButtonBox;
        btns->addButton("创建", QDialogButtonBox::AcceptRole);
        btns->addButton(QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(btns);
    }
    QString fixtureName() const { return m_nameEdit->text(); }
private:
    QLineEdit *m_nameEdit;
};

// ===== 灯库页面 =====
class LibraryPage : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryPage(QWidget *parent = nullptr) : QWidget(parent)
    {
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
        connect(m_listBtn, &QPushButton::clicked, this, [this]() { m_stack->setCurrentIndex(0); });
        tbLayout->addWidget(m_listBtn);

        auto *backBtn = new QPushButton("  返回主界面");
        backBtn->setFixedHeight(36);
        backBtn->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px;text-align:left;padding-left:12px");
        connect(backBtn, &QPushButton::clicked, this, &LibraryPage::goBackRequested);
        tbLayout->addWidget(backBtn);
        outerRoot->addWidget(toolbar);

        // === 右侧 QStackedWidget ===
        m_stack = new QStackedWidget;
        outerRoot->addWidget(m_stack, 1);
        buildListView();
        buildDetailView();
    }

    void setLibrary(const QList<FixtureDef> &library) { m_library = library; refreshListView(); }
    QList<FixtureDef> library() const { return m_library; }

signals:
    void goBackRequested();
    void libraryUpdated(const QList<FixtureDef> &library);

private slots:
    void onNewFixture()
    {
        NewFixtureDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;
        QString name = dlg.fixtureName();
        if (name.isEmpty()) { QMessageBox::warning(this, "提示", "名称不能为空"); return; }
        m_editingIndex = -1;
        m_detailName = name;
        m_detailChannels = 0;
        m_detailRanges.clear();
        m_committedFlags.clear();
        m_committedNames.clear();
        m_channelEdits.clear();
        rebuildDetail();
        m_stack->setCurrentIndex(1);
    }

    void onOpenFile()
    {
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
        refreshListView();
        emit libraryUpdated(m_library);
        m_stack->setCurrentIndex(0);
    }

    void onSaveAsFile()
    {
        QString path = QFileDialog::getSaveFileName(this, "另存为", "fixtures.json", "JSON 文件 (*.json)");
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

    void onAddChannel()
    {
        m_detailChannels++;
        m_detailRanges.resize(m_detailChannels);
        m_committedFlags.resize(m_detailChannels);
        m_committedFlags[m_detailChannels - 1] = false;
        m_committedNames.resize(m_detailChannels);
        rebuildDetail();
    }

    void onCommitChannel(int ch)
    {
        if (ch < 0 || ch >= m_detailChannels) return;
        m_committedFlags[ch] = true;
        m_committedNames.resize(m_detailChannels);
        if (ch < m_channelEdits.size())
            m_committedNames[ch] = m_channelEdits[ch]->text();
        rebuildDetail();
    }

    void onRemoveChannel(int ch)
    {
        if (ch < 0 || ch >= m_detailChannels) return;
        // 至少保留一个
        if (m_detailChannels <= 1) return;
        m_detailRanges.removeAt(ch);
        m_committedFlags.removeAt(ch);
        m_committedNames.removeAt(ch);
        m_detailChannels--;
        rebuildDetail();
    }
    void onRowDoubleClicked(int index)
    {
        if (index < 0 || index >= m_library.size()) return;
        m_editingIndex = index;
        auto &def = m_library[index];
        m_detailName = def.name;
        m_detailChannels = def.channels;
        m_detailRanges = def.ranges;
        m_detailRanges.resize(m_detailChannels);
        m_committedFlags.clear(); m_committedNames.clear();
        m_committedFlags.resize(m_detailChannels);
        m_committedNames.resize(m_detailChannels);
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
        connect(addBtn, &QPushButton::clicked, this, &LibraryPage::onAddChannel);
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
            hl->addWidget(new QLabel(QString("通道 %1").arg(i+1)));

            auto *nameEdit = new QLineEdit;
            nameEdit->setPlaceholderText("参数名"); nameEdit->setStyleSheet("color:#000;border:1px solid #bbb;padding:2px");
            // 已确认则显示已保存的名字
            if (done && i < m_committedNames.size() && !m_committedNames[i].isEmpty())
                nameEdit->setText(m_committedNames[i]);
            nameEdit->setReadOnly(done);
            hl->addWidget(nameEdit, 2);
            m_channelEdits << nameEdit;

            if (!done) {
                auto *commitBtn = new QPushButton("完成");
                commitBtn->setStyleSheet("background:#cfc;color:#060;border:1px solid #8b8;border-radius:3px;padding:4px 10px");
                connect(commitBtn, &QPushButton::clicked, this, [this,i](){ onCommitChannel(i); });
                hl->addWidget(commitBtn);
            }

            auto *customBtn = new QPushButton("自定义");
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
                ? m_committedNames[i] : QString("通道 %1").arg(idx);

            auto *row = new QHBoxLayout;
            row->setAlignment(Qt::AlignVCenter);
            // 颜色标记（始终占位，保证滑块对齐）
            auto *mark = new QLabel;
            mark->setFixedSize(12,12);
            QString nLower = name.toLower();
            if (name.contains("红") || nLower == "r") mark->setStyleSheet("background:#dc3c28;border-radius:6px;border:1px solid #999");
            else if (name.contains("绿") || nLower == "g") mark->setStyleSheet("background:#28b43c;border-radius:6px;border:1px solid #999");
            else if (name.contains("蓝") || nLower == "b") mark->setStyleSheet("background:#2850dc;border-radius:6px;border:1px solid #999");
            else mark->setStyleSheet("background:transparent;border:none");
            row->addWidget(mark);
            auto *label = new QLabel(name);
            label->setFixedWidth(90); label->setStyleSheet("color:#222;font-size:12px;border:none;padding:2px 4px");
            row->addWidget(label);

            if (i < m_detailRanges.size() && !m_detailRanges[i].isEmpty()) {
                auto *combo = new QComboBox;
                for (auto &r : m_detailRanges[i]) combo->addItem(r.name);
                combo->setStyleSheet(
                    "QComboBox{color:#000;border:1px solid #bbb;border-radius:3px;padding:2px 24px 2px 6px;font-size:12px}"
                    "QComboBox::drop-down{width:20px}"
                    "QComboBox QAbstractItemView{color:#000;background:#fff;selection-background:#dde}");
                combo->setFixedHeight(24);
                row->addWidget(combo); row->addStretch();
            } else {
                auto *slider = new QSlider(Qt::Horizontal); slider->setRange(0,255); slider->setValue(255);
                slider->setMaximumWidth(180); slider->setFixedHeight(22);
                auto *val = new QLineEdit("255");
                val->setMaxLength(3); val->setMaximumWidth(38); val->setAlignment(Qt::AlignCenter);
                val->setStyleSheet("color:#000;border:1px solid #888;border-radius:3px;padding:1px 3px;font-size:11px");
                connect(slider, &QSlider::valueChanged, val, [val](int v){ val->setText(QString::number(v)); });
                row->addWidget(slider);
                row->addWidget(val);
            }
            m_paramPanel->addLayout(row);
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
            auto *icon = new QLabel("灯"); icon->setFixedSize(36,36);
            icon->setStyleSheet("background:#ddd;border-radius:18px;border:2px solid #bbb;color:#666");
            icon->setAlignment(Qt::AlignCenter); hl->addWidget(icon);
            auto *n = new QLabel(QString("%1 (%2通道)").arg(def.name).arg(def.channels));
            n->setStyleSheet("color:#222;font-weight:bold;font-size:13px;border:none");
            hl->addWidget(n); hl->addStretch();
            // 双击编辑
            int rowIdx = idx;
            row->installEventFilter(this);
            row->setProperty("libIndex", idx);
            m_listLayout->insertWidget(m_listLayout->count() - 1, row);
        }
    }

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *frame = qobject_cast<QFrame *>(obj);
            if (frame && frame->property("libIndex").isValid()) {
                onRowDoubleClicked(frame->property("libIndex").toInt());
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    FixtureDef collectDetail()
    {
        FixtureDef def; def.name = m_detailName;
        // 只收集已确认的通道
        int cnt = 0;
        for (int i = 0; i < m_detailChannels; i++) {
            if (!m_committedFlags.value(i, false)) continue;
            cnt++;
            QString nm = i < m_committedNames.size() ? m_committedNames[i] : "";
            def.channelNames << (nm.isEmpty() ? QString("通道 %1").arg(cnt) : nm);
            def.ranges << (i < m_detailRanges.size() ? m_detailRanges[i] : QList<ChannelRange>());
        }
        def.channels = cnt;
        return def;
    }

    QList<FixtureDef> m_library;
    int m_editingIndex = -1;
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
    QStackedWidget *m_stack;
    QPushButton *m_listBtn;
};

#endif
