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
#include <QTimer>
#include <QList>
#include <QMap>
#include "Fixture.h"

struct DomainEntry { QString model; int channels; };

class AddDomainDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddDomainDialog(QWidget *parent = nullptr) : QDialog(parent) {
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
private:
    QLineEdit *m_model; QSpinBox *m_ch;
};

class DomainPage : public QWidget
{
    Q_OBJECT
public:
    explicit DomainPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *oroot = new QHBoxLayout(this); oroot->setContentsMargins(0,0,0,0);
        auto *tb = new QFrame; tb->setFixedWidth(140);
        tb->setStyleSheet("background:#e8e8e8;border-right:1px solid #ccc");
        auto *tbl = new QVBoxLayout(tb); tbl->setSpacing(4);
        auto btn = [&](const QString &t, void (DomainPage::*s)()){
            auto *b = new QPushButton("  "+t); b->setFixedHeight(36);
            b->setStyleSheet("background:#f5f5f5;color:#222;border:1px solid #ccc;border-radius:4px;text-align:left;padding-left:12px");
            connect(b,&QPushButton::clicked,this,s); tbl->addWidget(b);
        };
        btn("新建",&DomainPage::onNew); btn("打开",&DomainPage::onOpen); btn("保存",&DomainPage::onSave); btn("另存为",&DomainPage::onSaveAs);
        tbl->addStretch();
        auto *bk = new QPushButton("  返回主界面"); bk->setFixedHeight(36);
        bk->setStyleSheet("background:#ddd;color:#333;border:1px solid #bbb;border-radius:4px");
        connect(bk,&QPushButton::clicked,this,&DomainPage::goBackRequested);
        tbl->addWidget(bk); oroot->addWidget(tb);

        auto *main = new QFrame; main->setStyleSheet("background:#fff");
        auto *hs = new QHBoxLayout(main); hs->setContentsMargins(0,0,0,0);

        auto *left = new QFrame; left->setMinimumWidth(300);
        left->setStyleSheet("background:#fafafa;border-right:1px solid #ddd");
        auto *ll = new QVBoxLayout(left);
        auto *hr = new QHBoxLayout;
        hr->addWidget(new QLabel("域")); hr->addStretch();
        auto *addBtn = new QPushButton("+ 新建域");
        addBtn->setStyleSheet("background:#e0e0ff;color:#224;border:1px solid #aab;border-radius:4px;padding:4px 10px");
        connect(addBtn,&QPushButton::clicked,this,&DomainPage::onNewDomain);
        hr->addWidget(addBtn);
        auto *delBtn = new QPushButton("- 删除域");
        delBtn->setStyleSheet("background:#ffe0e0;color:#422;border:1px solid #baa;border-radius:4px;padding:4px 10px");
        connect(delBtn,&QPushButton::clicked,this,[this,delBtn](){
            m_deleteMode=!m_deleteMode;
            delBtn->setText(m_deleteMode?"完成删除":"- 删除域");
            rebuild();
        });
        hr->addWidget(delBtn);
        ll->addLayout(hr);
        auto *sc = new QScrollArea; sc->setWidgetResizable(true);
        m_listW = new QWidget; m_listL = new QVBoxLayout(m_listW); m_listL->setSpacing(2); m_listL->setAlignment(Qt::AlignTop);
        sc->setWidget(m_listW); ll->addWidget(sc,1);
        hs->addWidget(left,1);

        m_detail = new QFrame; m_detail->setStyleSheet("background:#f5f5f5");
        m_detailL = new QVBoxLayout(m_detail); m_detailL->setAlignment(Qt::AlignTop);
        m_detailL->addWidget(new QLabel("点击域查看地址码"));
        hs->addWidget(m_detail,2);
        oroot->addWidget(main,1);
    }

    void updateFixtures(const QList<Fixture*> &fixtures) { m_all = fixtures; rebuild(); }
    void addDomain(const QString &model, int ch) { m_doms << DomainEntry{model,ch}; if(m_doms.size()==1) m_cur=0; rebuild(); }
    int curDomain() const { return m_cur; }
    void setCurDomain(int i) { if(i>=0&&i<m_doms.size()){m_cur=i; rebuild();} }

signals:
    void goBackRequested();
    void newDomainRequested(const QString&,int);
    void domainSwitched(int);

private slots:
    void onNewDomain(){
        AddDomainDialog d(this);
        if(d.exec()==QDialog::Accepted && !d.model().isEmpty())
            emit newDomainRequested(d.model(), d.channels());
    }
    void onNew(){} void onOpen(){} void onSave(){} void onSaveAs(){}

private:
    void rebuild(){
        QLayoutItem *c; while((c=m_listL->takeAt(0))!=nullptr){if(c->widget()){c->widget()->setParent(nullptr);delete c->widget();}delete c;}
        QMap<QString,int> cnts; for(auto*f:m_all) cnts[f->name()]++;

        for(int i=0;i<m_doms.size();i++){
            auto&d=m_doms[i]; int qty=cnts.value(d.model,0);
            auto*r=new QFrame; r->setStyleSheet("background:#f9f9f9;border:1px solid #eee;border-radius:3px;cursor:pointer"); r->setMinimumHeight(40);
            r->setProperty("di",i); r->installEventFilter(this);
            auto*hl=new QHBoxLayout(r);
            auto*dot=new QLabel; dot->setFixedSize(10,10);
            dot->setStyleSheet(i==m_cur?"background:#0f0;border-radius:5px":"background:transparent;border-radius:5px");
            hl->addWidget(dot);
            hl->addWidget(new QLabel(QString("灯型号：%1    通道：%2    数量：%3").arg(d.model).arg(d.channels).arg(qty)));
            hl->addStretch();
            m_listL->addWidget(r);
        }
        m_listL->addStretch();
        if(m_cur>=0&&m_cur<m_doms.size()) showDetail(m_cur); else clrDetail();
    }

    void showDetail(int i){
        m_cur=i; auto&d=m_doms[i];
        QLayoutItem*c; while((c=m_detailL->takeAt(0))!=nullptr){if(c->widget())delete c->widget();delete c;}
        QList<Fixture*> matched; for(auto*f:m_all)if(f->name()==d.model)matched<<f;
        auto*h=new QLabel(QString("灯型号：%1   通道：%2   数量：%3").arg(d.model).arg(d.channels).arg(matched.size()));
        h->setStyleSheet("color:#222;font-size:15px;font-weight:bold;padding:8px 12px;background:#e8e8e8");
        m_detailL->addWidget(h);
        auto*sc=new QScrollArea; sc->setWidgetResizable(true); auto*w=new QWidget; auto*l=new QVBoxLayout(w); l->setSpacing(2); l->setAlignment(Qt::AlignTop);
        for(auto*f:matched){
            auto*r=new QFrame; r->setStyleSheet("background:#fff;border-bottom:1px solid #eee"); r->setFixedHeight(36);
            auto*hl=new QHBoxLayout(r); hl->setContentsMargins(12,0,12,0);
            auto*a=new QLabel(QString("%1").arg(f->address(),3,10,QChar('0')));
            hl->addWidget(a); hl->addSpacing(20); hl->addWidget(new QLabel(f->name())); hl->addStretch();
            l->addWidget(r);
        }
        l->addStretch(); sc->setWidget(w); m_detailL->addWidget(sc,1);
    }

    void clrDetail(){
        QLayoutItem*c; while((c=m_detailL->takeAt(0))!=nullptr){if(c->widget())delete c->widget();delete c;}
        m_detailL->addWidget(new QLabel("点击域查看地址码"));
    }

    bool eventFilter(QObject*o,QEvent*e)override{
        if(e->type()==QEvent::MouseButtonPress&&!m_deleteMode){
            auto*f=qobject_cast<QFrame*>(o);
            if(f&&f->property("di").isValid()){
                int i=f->property("di").toInt();
                if(i>=0&&i<m_doms.size()){m_cur=i;rebuild(); QTimer::singleShot(0,this,[this,i](){emit domainSwitched(i);});}
            }
        }
        return QWidget::eventFilter(o,e);
    }

    QWidget*m_listW; QVBoxLayout*m_listL; QFrame*m_detail; QVBoxLayout*m_detailL;
    QList<DomainEntry> m_doms; int m_cur=0;
    QList<Fixture*> m_all; bool m_deleteMode=false;
};

#endif
