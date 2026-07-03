#ifndef DOMAINPAGE_H
#define DOMAINPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
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
#include <QMap>
#include "Fixture.h"

class AddDomainDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddDomainDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("新建域"); setStyleSheet("background:#fff");
        auto *root = new QVBoxLayout(this); auto *form = new QFormLayout;
        m_modelEdit = new QLineEdit; m_modelEdit->setPlaceholderText("例如：万锐帕灯");
        m_modelEdit->setStyleSheet("color:#000;border:1px solid #aaa;padding:4px");
        form->addRow("灯型号：", m_modelEdit);
        m_chSpin = new QSpinBox; m_chSpin->setRange(1, 64); m_chSpin->setValue(8);
        m_chSpin->setStyleSheet("color:#000;border:1px solid #aaa;padding:2px");
        form->addRow("通道数：", m_chSpin);
        m_qtySpin = new QSpinBox; m_qtySpin->setRange(1, 512); m_qtySpin->setValue(1);
        m_qtySpin->setStyleSheet("color:#000;border:1px solid #aaa;padding:2px");
        form->addRow("数量：", m_qtySpin);
        root->addLayout(form);
        auto *btns = new QDialogButtonBox;
        btns->addButton("创建", QDialogButtonBox::AcceptRole);
        btns->addButton(QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(btns);
    }
    QString model() const { return m_modelEdit->text(); }
    int channels() const { return m_chSpin->value(); }
    int quantity() const { return m_qtySpin->value(); }
private:
    QLineEdit *m_modelEdit; QSpinBox *m_chSpin, *m_qtySpin;
};

class DomainPage : public QWidget
{
    Q_OBJECT
public:
    explicit DomainPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *outerRoot = new QHBoxLayout(this); outerRoot->setContentsMargins(0,0,0,0);
        auto *toolbar = new QFrame;
        toolbar->setFixedWidth(140);
        toolbar->setStyleSheet("background:#e8e8e8;border-right:1px solid #ccc");
        auto *tbLayout = new QVBoxLayout(toolbar); tbLayout->setSpacing(4);
        auto btn = [&](const QString &t, void (DomainPage::*s)()) {
            auto *b = new QPushButton("  " + t); b->setFixedHeight(36);
            b->setStyleSheet("background:#f5f5f5;color:#222;border:1px solid #ccc;border-radius:4px;text-align:left;padding-left:12px");
            connect(b, &QPushButton::clicked, this, s); tbLayout->addWidget(b);
        };
        btn("新建", &DomainPage::onNew); btn("打开", &DomainPage::onOpenFile);
        btn("保存", &DomainPage::onSaveFile); btn("另存为", &DomainPage::onSaveAsFile);
        tbLayout->addStretch();
        auto *back = new QPushButton("  返回主界面"); back->setFixedHeight(36);
        back->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px");
        connect(back, &QPushButton::clicked, this, &DomainPage::goBackRequested);
        tbLayout->addWidget(back);
        outerRoot->addWidget(toolbar);

        auto *mainArea = new QFrame; mainArea->setStyleSheet("background:#fff");
        auto *hSplit = new QHBoxLayout(mainArea); hSplit->setContentsMargins(0,0,0,0);

        auto *leftPanel = new QFrame; leftPanel->setMinimumWidth(300);
        leftPanel->setStyleSheet("background:#fafafa;border-right:1px solid #ddd");
        auto *leftLayout = new QVBoxLayout(leftPanel);
        auto *headerRow = new QHBoxLayout;
        auto *title = new QLabel("域");
        title->setStyleSheet("color:#222;font-size:16px;font-weight:bold;padding:8px;border:none");
        headerRow->addWidget(title); headerRow->addStretch();
        auto *addBtn = new QPushButton("+ 新建域"); addBtn->setStyleSheet("background:#e0e0ff;color:#224;border:1px solid #aab;border-radius:4px;padding:4px 10px");
        connect(addBtn, &QPushButton::clicked, this, &DomainPage::onNewDomain);
        headerRow->addWidget(addBtn);
        m_delBtn = new QPushButton("- 删除域"); m_delBtn->setStyleSheet("background:#ffe0e0;color:#422;border:1px solid #baa;border-radius:4px;padding:4px 10px");
        connect(m_delBtn, &QPushButton::clicked, this, &DomainPage::onToggleDelete);
        headerRow->addWidget(m_delBtn);
        leftLayout->addLayout(headerRow);

        auto *listScroll = new QScrollArea; listScroll->setWidgetResizable(true);
        m_listWidget = new QWidget;
        m_listLayout = new QVBoxLayout(m_listWidget); m_listLayout->setSpacing(2); m_listLayout->setAlignment(Qt::AlignTop);
        listScroll->setWidget(m_listWidget);
        leftLayout->addWidget(listScroll, 1);
        hSplit->addWidget(leftPanel, 1);

        m_detailArea = new QFrame; m_detailArea->setStyleSheet("background:#f5f5f5");
        m_detailLayout = new QVBoxLayout(m_detailArea); m_detailLayout->setAlignment(Qt::AlignTop);
        clearDetail();
        hSplit->addWidget(m_detailArea, 2);
        outerRoot->addWidget(mainArea, 1);
    }

    void updateFixtures(const QList<Fixture *> &fixtures)
    {
        m_allFixtures = fixtures;
        rebuildList();
    }

signals:
    void goBackRequested();
    void newDomainRequested(const QString &model, int channels, int quantity);
    void deleteDomainRequested(const QString &model);

private slots:
    void onNewDomain() {
        AddDomainDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;
        if (dlg.model().isEmpty()) { QMessageBox::warning(this, "提示", "灯型号不能为空"); return; }
        emit newDomainRequested(dlg.model(), dlg.channels(), dlg.quantity());
    }

    void onToggleDelete() {
        m_deleteMode = !m_deleteMode;
        if (m_deleteMode) {
            m_delBtn->setText("完成删除");
            m_delBtn->setStyleSheet("background:#c44;color:#fff;border-radius:4px;padding:4px 10px");
        } else {
            m_delBtn->setText("- 删除域");
            m_delBtn->setStyleSheet("background:#ffe0e0;color:#422;border:1px solid #baa;border-radius:4px;padding:4px 10px");
        }
        rebuildList();
    }

    void onNew() {}
    void onOpenFile() {}
    void onSaveFile() {}
    void onSaveAsFile() {}

private:
    void rebuildList() {
        while (m_listLayout->count() > 0) {
            auto *it = m_listLayout->takeAt(0); if (it->widget()) delete it->widget(); delete it;
        }
        QMap<QString, QList<Fixture *>> groups;
        for (auto *f : m_allFixtures) groups[f->name()] << f;

        if (groups.isEmpty()) {
            m_listLayout->addWidget(new QLabel("（暂无灯具）"));
            m_listLayout->addStretch(); clearDetail(); m_currentGroup.clear(); return;
        }
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            auto *row = new QFrame;
            row->setStyleSheet("background:#f9f9f9;border:1px solid #eee;border-radius:3px;cursor:pointer");
            row->setMinimumHeight(40);
            auto *hl = new QHBoxLayout(row);
            int ch = it.value().first()->channelCount();
            auto *label = new QLabel(QString("灯型号：%1    通道：%2    数量：%3")
                .arg(it.key()).arg(ch).arg(it.value().size()));
            label->setStyleSheet("color:#333;font-size:12px;border:none;background:transparent");
            hl->addWidget(label); hl->addStretch();

            if (m_deleteMode) {
                auto *xBtn = new QPushButton("×"); xBtn->setFixedSize(24,24);
                xBtn->setStyleSheet("color:red;font-weight:bold;border:none;background:transparent;font-size:16px");
                connect(xBtn, &QPushButton::clicked, this, [this, key = it.key()]() {
                    emit deleteDomainRequested(key);
                });
                hl->addWidget(xBtn);
            }
            row->setProperty("groupKey", it.key());
            row->installEventFilter(this);
            m_listLayout->addWidget(row);
        }
        m_listLayout->addStretch();
        if (!m_currentGroup.isEmpty() && groups.contains(m_currentGroup))
            showDetail(m_currentGroup, groups[m_currentGroup]);
        else clearDetail();
    }

    void showDetail(const QString &groupName, const QList<Fixture *> &list)
    {
        m_currentGroup = groupName;
        QLayoutItem *pc;
        while ((pc = m_detailLayout->takeAt(0)) != nullptr) { if (pc->widget()) delete pc->widget(); delete pc; }
        int ch = list.first()->channelCount();
        auto *h = new QLabel(QString("灯型号：%1   通道：%2   数量：%3").arg(groupName).arg(ch).arg(list.size()));
        h->setStyleSheet("color:#222;font-size:15px;font-weight:bold;padding:8px 12px;border:none;background:#e8e8e8");
        m_detailLayout->addWidget(h);
        auto *scroll = new QScrollArea; scroll->setWidgetResizable(true);
        auto *rw = new QWidget; auto *rl = new QVBoxLayout(rw); rl->setSpacing(2); rl->setAlignment(Qt::AlignTop);
        for (auto *f : list) {
            auto *row = new QFrame;
            row->setStyleSheet("background:#fff;border-bottom:1px solid #eee"); row->setFixedHeight(36);
            auto *hl = new QHBoxLayout(row); hl->setContentsMargins(12,0,12,0);
            auto *a = new QLabel(QString("%1").arg(f->address(), 3, 10, QChar('0')));
            a->setStyleSheet("color:#555;font-weight:bold;font-size:13px;border:none");
            hl->addWidget(a); hl->addSpacing(20);
            hl->addWidget(new QLabel(f->name())); hl->addStretch();
            rl->addWidget(row);
        }
        rl->addStretch(); scroll->setWidget(rw); m_detailLayout->addWidget(scroll, 1);
    }

    void clearDetail() {
        QLayoutItem *pc;
        while ((pc = m_detailLayout->takeAt(0)) != nullptr) { if (pc->widget()) delete pc->widget(); delete pc; }
        auto *t = new QLabel("点击左侧域查看地址码详情");
        t->setStyleSheet("color:#888;font-size:14px;padding:20px;border:none;background:transparent");
        t->setAlignment(Qt::AlignCenter); m_detailLayout->addWidget(t);
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonPress && !m_deleteMode) {
            auto *f = qobject_cast<QFrame *>(obj);
            if (f && f->property("groupKey").isValid()) {
                QString key = f->property("groupKey").toString();
                QList<Fixture *> list;
                for (auto *fx : m_allFixtures) if (fx->name() == key) list << fx;
                showDetail(key, list);
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    QWidget *m_listWidget; QVBoxLayout *m_listLayout;
    QFrame *m_detailArea; QVBoxLayout *m_detailLayout;
    QString m_currentGroup; QList<Fixture *> m_allFixtures;
    QPushButton *m_delBtn; bool m_deleteMode = false;
};

#endif
