#ifndef ADDRESSPAGE_H
#define ADDRESSPAGE_H

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
#include <QKeyEvent>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <functional>
#include "Fixture.h"

struct AddrEntry { QString model; int channels; int quantity = 0; };

class AddrDomainDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddrDomainDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("新建域"); setStyleSheet("background:#fff");
        auto *root = new QVBoxLayout(this); auto *form = new QFormLayout;
        m_model = new QLineEdit; m_model->setPlaceholderText("例如：万锐帕灯");
        m_model->setStyleSheet("color:#000;border:1px solid #aaa;padding:4px");
        form->addRow("灯型号：", m_model);
        m_ch = new QSpinBox; m_ch->setRange(1,64); m_ch->setValue(8);
        m_ch->setStyleSheet("color:#000;border:1px solid #aaa;padding:2px");
        form->addRow("通道数：", m_ch);
        root->addLayout(form);
        auto *btns = new QDialogButtonBox; btns->addButton("创建", QDialogButtonBox::AcceptRole); btns->addButton(QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(btns);
    }
    QString model() const { return m_model->text(); }
    int channels() const { return m_ch->value(); }
private: QLineEdit *m_model; QSpinBox *m_ch;
};

class AddressPage : public QWidget
{
    Q_OBJECT
public:
    explicit AddressPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        auto *oroot = new QHBoxLayout(this); oroot->setContentsMargins(0,0,0,0);
        auto *tb = new QFrame; tb->setFixedWidth(140);
        tb->setStyleSheet("background:#e8e8e8;border-right:1px solid #ccc");
        auto *tbl = new QVBoxLayout(tb); tbl->setSpacing(4);
        auto makeBtn = [&](const char *text, std::function<void()> fn){
            auto *b = new QPushButton(QString("  %1").arg(text)); b->setFixedHeight(36);
            b->setStyleSheet("background:#f5f5f5;color:#222;border:1px solid #ccc;border-radius:4px;text-align:left;padding-left:12px");
            QObject::connect(b, &QPushButton::clicked, this, fn); tbl->addWidget(b);
        };
        makeBtn("新建域", [this](){
            AddrDomainDialog d(this);
            if (d.exec() == QDialog::Accepted && !d.model().isEmpty()) {
                m_doms << AddrEntry{d.model(), d.channels(), 0};
                if (m_doms.size() == 1) m_cur = 0;
                rebuildList();
                emit newDomainRequested(d.model(), d.channels());
            }
        });
        makeBtn("打开",   [this](){ onOpen(); });
        makeBtn("保存",   [this](){ onSave(); });
        makeBtn("另存为", [this](){ onSaveAs(); });
        // 删除域按钮
        auto *delDomBtn = new QPushButton(m_deleteMode ? "  完成删除" : "  删除域");
        delDomBtn->setFixedHeight(36);
        delDomBtn->setStyleSheet("background:#ffe0e0;color:#822;border:1px solid #caa;border-radius:4px;text-align:left;padding-left:12px");
        connect(delDomBtn, &QPushButton::clicked, this, [this, delDomBtn]() {
            m_deleteMode = !m_deleteMode;
            delDomBtn->setText(m_deleteMode ? "  完成删除" : "  删除域");
            delDomBtn->setStyleSheet(m_deleteMode
                ? "background:#fcc;color:#c00;border:1px solid #c88;border-radius:4px;text-align:left;padding-left:12px"
                : "background:#ffe0e0;color:#822;border:1px solid #caa;border-radius:4px;text-align:left;padding-left:12px");
            rebuildList();
        });
        tbl->addWidget(delDomBtn);
        tbl->addStretch();
        auto *bk = new QPushButton("  返回主界面"); bk->setFixedHeight(36);
        bk->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px");
        connect(bk,&QPushButton::clicked,this,&AddressPage::backRequested);
        tbl->addWidget(bk); oroot->addWidget(tb);

        auto *main = new QFrame; main->setStyleSheet("background:#fff");
        auto *hs = new QHBoxLayout(main); hs->setContentsMargins(0,0,0,0);
        // 左：域列表
        auto *left = new QFrame; left->setMinimumWidth(300);
        left->setStyleSheet("background:#fafafa;border-right:1px solid #ddd");
        auto *ll = new QVBoxLayout(left);
        auto *hr = new QHBoxLayout;
        hr->addWidget(new QLabel("域")); hr->addStretch();
        ll->addLayout(hr);
        auto *sc = new QScrollArea; sc->setWidgetResizable(true);
        m_listW = new QWidget; m_listL = new QVBoxLayout(m_listW); m_listL->setSpacing(2); m_listL->setAlignment(Qt::AlignTop);
        sc->setWidget(m_listW); ll->addWidget(sc, 1);
        hs->addWidget(left, 1);
        // 右：详情
        m_detail = new QFrame; m_detail->setStyleSheet("background:#f5f5f5");
        m_detailL = new QVBoxLayout(m_detail); m_detailL->setAlignment(Qt::AlignTop);
        m_detailL->addWidget(new QLabel("点击域查看地址码"));
        hs->addWidget(m_detail, 2);
        oroot->addWidget(main, 1);
    }

    void addDomain(const QString &model, int ch, int qty = 1) { m_doms << AddrEntry{model, ch, qty}; rebuildList(); }
    QList<AddrEntry> getDomains() const { return m_doms; }
    void updateFixtures(const QList<Fixture *> &fixtures) {
        m_all = fixtures;
        // 自动创建域：有灯具还没域→新建
        QSet<QString> models;
        for (auto *f : fixtures) models << f->name();
        for (auto &m : models) {
            bool found = false;
            for (auto &d : m_doms) if (d.model == m) { found = true; break; }
            if (!found) {
                m_doms << AddrEntry{m, fixtures[0]->channelCount()};
                if (m_doms.size() == 1) m_cur = 0;
            }
        }
        rebuildList();
    }
    int curDomain() const { return m_cur; }

signals:
    void backRequested();
    void goBackRequested();
    void newDomainRequested(const QString &model, int channels);
    void deleteDomainRequested(const QString &model);
    void domainSwitched(int index);

private slots:
    void onNew() {
        m_doms.clear(); m_cur = 0; rebuildList(); clearDetail();
    }
    void onOpen() {
        QString path = QFileDialog::getOpenFileName(this, "打开域文件", "", "JSON (*.json)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            m_doms.clear();
            for (auto v : QJsonDocument::fromJson(f.readAll()).array()) {
                auto o = v.toObject();
                m_doms << AddrEntry{o["model"].toString(), o["channels"].toInt()};
            }
            f.close(); m_cur = m_doms.isEmpty() ? 0 : 0; rebuildList();
        }
    }
    void onSave() {
        if (m_cur < 0 || m_cur >= m_doms.size()) return;
        QString path = QFileDialog::getSaveFileName(this, "保存域文件", "域配置.json", "JSON (*.json)");
        if (path.isEmpty()) return;
        QJsonArray arr;
        for (auto &d : m_doms)
            arr << QJsonObject{{"model", d.model}, {"channels", d.channels}};
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(arr).toJson()); f.close(); }
    }
    void onSaveAs() { onSave(); }
    void onAddDomain() {
        AddrDomainDialog d(this);
        if (d.exec() == QDialog::Accepted && !d.model().isEmpty()) {
            m_doms << AddrEntry{d.model(), d.channels(), 0};
            if (m_doms.size() == 1) m_cur = 0;
            rebuildList();
            emit newDomainRequested(d.model(), d.channels());
        }
    }

private:
    void rebuildList() {
        QLayoutItem *c; while ((c = m_listL->takeAt(0))) { if (c->widget()) { c->widget()->setParent(nullptr); delete c->widget(); } delete c; }
        QMap<QString, int> cnts; for (auto *f : m_all) cnts[f->name()]++;

        for (int i = 0; i < m_doms.size(); i++) {
            auto &d = m_doms[i]; int qty = cnts.value(d.model, 0);
            auto *r = new QFrame; r->setMinimumHeight(40);
            r->setStyleSheet(i == m_selectedDom
                ? "QFrame { background:#dde; border:2px solid #66b; border-radius:3px; }"
                : "QFrame { background:#f9f9f9; border:1px solid #eee; border-radius:3px; }");
            r->setProperty("di", i); r->installEventFilter(this);
            auto *hl = new QHBoxLayout(r);
            auto *dot = new QLabel; dot->setFixedSize(10,10);
            dot->setStyleSheet(i == m_cur ? "background:#0f0;border-radius:5px" : "background:transparent;border-radius:5px");
            hl->addWidget(dot);
            auto *lbl = new QLabel(QString("灯型号：%1    通道：%2    数量：%3").arg(d.model).arg(d.channels).arg(cnts.value(d.model, 0)));
            lbl->setStyleSheet("border:none; background:transparent;");
            hl->addWidget(lbl);
            hl->addStretch();
            if (m_deleteMode) {
                auto *xBtn = new QPushButton("×"); xBtn->setFixedSize(24,24);
                xBtn->setStyleSheet("color:red;font-weight:bold;border:none;background:transparent;font-size:16px");
                int idx = i;
                connect(xBtn, &QPushButton::clicked, this, [this, idx, xBtn]() {
                    QString m = m_doms[idx].model;
                    auto answer = QMessageBox::question(this, "确认删除",
                        QString("确定要删除域「%1」吗？").arg(m),
                        QMessageBox::Yes | QMessageBox::No);
                    if (answer != QMessageBox::Yes) return;
                    m_doms.removeAt(idx);
                    if (m_cur >= m_doms.size()) m_cur = m_doms.size() - 1;
                    rebuildList();
                    emit deleteDomainRequested(m);
                });
                hl->addWidget(xBtn);
            }
            m_listL->addWidget(r);
        }
        m_listL->addStretch();
        m_listW->updateGeometry();
        m_listW->update();
        if (m_cur >= 0 && m_cur < m_doms.size()) showDetail(m_cur); else clearDetail();
    }

    void showDetail(int i) {
        m_cur = i;
        auto &d = m_doms[i];
        QLayoutItem *c; while ((c = m_detailL->takeAt(0))) { if (c->widget()) delete c->widget(); delete c; }
        QList<Fixture *> matched; for (auto *f : m_all) if (f->name() == d.model) matched << f;
        auto *h = new QLabel(QString("灯型号：%1   通道：%2   数量：%3").arg(d.model).arg(d.channels).arg(matched.size()));
        h->setStyleSheet("color:#222;font-size:15px;font-weight:bold;padding:8px 12px;background:#e8e8e8");
        m_detailL->addWidget(h);
        auto *sc = new QScrollArea; sc->setWidgetResizable(true); auto *w = new QWidget; auto *l = new QVBoxLayout(w); l->setSpacing(2); l->setAlignment(Qt::AlignTop);
        for (auto *f : matched) {
            auto *r = new QFrame; r->setStyleSheet("background:#fff;border-bottom:1px solid #eee"); r->setFixedHeight(36);
            auto *hl = new QHBoxLayout(r); hl->setContentsMargins(12,0,12,0);
            auto *a = new QLabel(QString("%1").arg(f->address(), 3, 10, QChar('0')));
            hl->addWidget(a); hl->addSpacing(20); hl->addWidget(new QLabel(f->name())); hl->addStretch();
            l->addWidget(r);
        }
        l->addStretch(); sc->setWidget(w); m_detailL->addWidget(sc, 1);
    }

    void clearDetail() {
        QLayoutItem *c; while ((c = m_detailL->takeAt(0))) { if (c->widget()) delete c->widget(); delete c; }
        m_detailL->addWidget(new QLabel("点击域查看地址码"));
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Delete && m_selectedDom >= 0
            && m_selectedDom < m_doms.size()) {
            QString m = m_doms[m_selectedDom].model;
            auto answer = QMessageBox::question(this, "确认删除",
                QString("确定要删除域「%1」吗？").arg(m),
                QMessageBox::Yes | QMessageBox::No);
            if (answer == QMessageBox::Yes) {
                m_doms.removeAt(m_selectedDom);
                if (m_cur >= m_doms.size()) m_cur = m_doms.size() - 1;
                m_selectedDom = -1;
                rebuildList();
                emit deleteDomainRequested(m);
            }
            return;
        }
        QWidget::keyPressEvent(event);
    }

    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::MouseButtonPress && !m_deleteMode && !m_rebuilding) {
            auto *f = qobject_cast<QFrame *>(o);
            if (f && f->property("di").isValid()) {
                int i = f->property("di").toInt();
                m_selectedDom = (i == m_selectedDom) ? -1 : i;
                setFocus();
                rebuildList();
                return true;
            }
        }
        if (e->type() == QEvent::MouseButtonDblClick && !m_deleteMode && !m_rebuilding) {
            auto *f = qobject_cast<QFrame *>(o);
            if (f && f->property("di").isValid()) {
                int i = f->property("di").toInt();
                if (i >= 0 && i < m_doms.size()) {
                    m_cur = i;
                    m_selectedDom = -1;
                    m_rebuilding = true;
                    int di = i;
                    QTimer::singleShot(0, this, [this, di]() {
                        emit domainSwitched(di);
                        rebuildList();
                        m_rebuilding = false;
                    });
                }
            }
        }
        return QWidget::eventFilter(o, e);
    }

    QWidget *m_listW; QVBoxLayout *m_listL; QFrame *m_detail; QVBoxLayout *m_detailL;
    QList<AddrEntry> m_doms; int m_cur = 0; QList<Fixture *> m_all;
    bool m_deleteMode = false; bool m_rebuilding = false;
    int m_selectedDom = -1;  // 单击选中的域（Delete 删除）
};

#endif
