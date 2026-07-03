#ifndef FIXTUREDIALOG_H
#define FIXTUREDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFormLayout>
#include <QList>
#include <QDialogButtonBox>

#include "Fixture.h"

// ===== 通道范围编辑子窗口 =====
class RangeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RangeDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("自定义通道值域");
        setMinimumWidth(400);
        auto *root = new QVBoxLayout(this);
        m_layout = new QVBoxLayout;
        root->addLayout(m_layout);
        addRow(); // 至少一行

        auto *btnRow = new QHBoxLayout;
        auto *addBtn = new QPushButton("+ 添加行");
        btnRow->addWidget(addBtn);
        btnRow->addStretch();
        root->addLayout(btnRow);

        auto *dlgBtns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(dlgBtns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(dlgBtns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(dlgBtns);

        connect(addBtn, &QPushButton::clicked, this, &RangeDialog::addRow);
    }

    QList<ChannelRange> getRanges() const
    {
        QList<ChannelRange> result;
        for (const auto &row : m_rows) {
            if (!row.nameEdit->text().isEmpty()) {
                ChannelRange r;
                r.name = row.nameEdit->text();
                r.minValue = row.minSpin->value();
                r.maxValue = row.maxSpin->value();
                result << r;
            }
        }
        return result;
    }

    void setRanges(const QList<ChannelRange> &ranges)
    {
        // 清空：container 删除时会级联删除子控件
        for (auto &row : m_rows) delete row.container;
        m_rows.clear();
        while (m_layout->count() > 0) delete m_layout->takeAt(0);
        for (const auto &r : ranges) {
            addRow();
            auto &row = m_rows.last();
            row.nameEdit->setText(r.name);
            row.minSpin->setValue(r.minValue);
            row.maxSpin->setValue(r.maxValue);
        }
        if (m_rows.isEmpty()) addRow();
    }

private slots:
    void addRow()
    {
        auto *container = new QWidget;
        auto *hl = new QHBoxLayout(container);
        hl->setContentsMargins(0, 0, 0, 0);

        auto *nameEdit = new QLineEdit;
        nameEdit->setPlaceholderText("功能名(如:图案片1)");
        hl->addWidget(nameEdit, 2);

        auto *minSpin = new QSpinBox;
        minSpin->setRange(0, 255);
        minSpin->setPrefix("从 ");
        hl->addWidget(minSpin);

        auto *sep = new QLabel("—");
        hl->addWidget(sep);

        auto *maxSpin = new QSpinBox;
        maxSpin->setRange(0, 255);
        maxSpin->setValue(255);
        maxSpin->setPrefix("到 ");
        hl->addWidget(maxSpin);

        auto *delBtn = new QPushButton("×");
        delBtn->setFixedSize(24, 24);
        delBtn->setStyleSheet("color:red;font-weight:bold;border:none");
        connect(delBtn, &QPushButton::clicked, this, [this, container]() {
            for (int i = 0; i < m_rows.size(); i++) {
                if (m_rows[i].container == container) {
                    delete m_rows[i].nameEdit;
                    delete m_rows[i].minSpin;
                    delete m_rows[i].maxSpin;
                    delete m_rows[i].container;
                    m_rows.removeAt(i);
                    break;
                }
            }
        });
        hl->addWidget(delBtn);

        m_rows << Row{container, nameEdit, minSpin, maxSpin};
        m_layout->addWidget(container);
    }

private:
    struct Row {
        QWidget *container;
        QLineEdit *nameEdit;
        QSpinBox *minSpin;
        QSpinBox *maxSpin;
    };
    QList<Row> m_rows;
    QVBoxLayout *m_layout;
};

// ===== 添加灯具弹窗 =====
class FixtureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FixtureDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("添加灯具型号");
        setMinimumWidth(450);

        auto *root = new QVBoxLayout(this);

        // 名称
        auto *nameLayout = new QHBoxLayout;
        nameLayout->addWidget(new QLabel("灯具名称:"));
        m_nameEdit = new QLineEdit;
        m_nameEdit->setPlaceholderText("例如: LED PAR 64");
        nameLayout->addWidget(m_nameEdit);
        root->addLayout(nameLayout);

        // 厂商
        auto *mfrLayout = new QHBoxLayout;
        mfrLayout->addWidget(new QLabel("厂商:"));
        m_mfrEdit = new QLineEdit;
        m_mfrEdit->setPlaceholderText("例如: Generic");
        m_mfrEdit->setText("Generic");
        mfrLayout->addWidget(m_mfrEdit);
        root->addLayout(mfrLayout);

        // 通道数
        auto *chLayout = new QHBoxLayout;
        chLayout->addWidget(new QLabel("通道数:"));
        m_chSpin = new QSpinBox;
        m_chSpin->setRange(1, 64);
        m_chSpin->setValue(4);
        chLayout->addWidget(m_chSpin);
        chLayout->addStretch();
        root->addLayout(chLayout);

        // 通道参数区域
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        m_paramWidget = new QWidget;
        m_paramLayout = new QFormLayout(m_paramWidget);
        scroll->setWidget(m_paramWidget);
        root->addWidget(scroll);

        connect(m_chSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &FixtureDialog::rebuildParams);
        rebuildParams(4);

        // 按钮
        auto *btnLayout = new QHBoxLayout;
        btnLayout->addStretch();
        auto *okBtn = new QPushButton("添加");
        auto *cancelBtn = new QPushButton("取消");
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        root->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    FixtureDef getFixtureDef() const
    {
        FixtureDef def;
        def.name = m_nameEdit->text();
        def.manufacturer = m_mfrEdit->text();
        def.channels = m_chSpin->value();
        for (int i = 0; i < m_paramEdits.size(); i++)
            def.channelNames << (m_paramEdits[i]->text().isEmpty()
                ? QString("通道%1").arg(i+1) : m_paramEdits[i]->text());
        def.ranges = m_paramRanges;  // 每通道的自定义值域
        return def;
    }

    void setFixtureDef(const FixtureDef &def)
    {
        m_nameEdit->setText(def.name);
        m_mfrEdit->setText(def.manufacturer);
        m_chSpin->setValue(def.channels);
        rebuildParams(def.channels);
        for (int i = 0; i < def.channelNames.size(); i++)
            m_paramEdits[i]->setText(def.channelNames[i]);
        m_paramRanges = def.ranges;
        m_paramRanges.resize(def.channels);
    }

private slots:
    void rebuildParams(int channels)
    {
        // 保存旧的范围
        QList<QList<ChannelRange>> oldRanges = m_paramRanges;
        m_paramRanges.clear();
        m_paramRanges.resize(channels);
        for (int i = 0; i < qMin(channels, oldRanges.size()); i++)
            m_paramRanges[i] = oldRanges[i];

        // 清空旧行
        for (auto *w : m_rangeBtns) delete w;
        m_rangeBtns.clear();
        for (auto *edit : m_paramEdits) delete edit;
        m_paramEdits.clear();
        while (m_paramLayout->rowCount() > 0)
            m_paramLayout->removeRow(0);

        for (int i = 0; i < channels; i++) {
            auto *edit = new QLineEdit;
            edit->setPlaceholderText(QString("通道 %1 名称").arg(i + 1));
            m_paramEdits << edit;

            auto *customBtn = new QPushButton("自定义");
            customBtn->setFixedWidth(60);
            connect(customBtn, &QPushButton::clicked, this, [this, i, customBtn]() {
                RangeDialog dlg(this);
                if (i < m_paramRanges.size())
                    dlg.setRanges(m_paramRanges[i]);
                if (dlg.exec() == QDialog::Accepted) {
                    m_paramRanges[i] = dlg.getRanges();
                    int cnt = m_paramRanges[i].size();
                    customBtn->setText(cnt > 0 ? QString("已配(%1)").arg(cnt) : "自定义");
                }
            });
            m_rangeBtns << customBtn;

            auto *row = new QHBoxLayout;
            row->addWidget(edit);
            row->addWidget(customBtn);
            m_paramLayout->addRow(QString("通道 %1:").arg(i + 1), row);
        }
    }

private:
    QLineEdit      *m_nameEdit;
    QLineEdit      *m_mfrEdit;
    QSpinBox       *m_chSpin;
    QWidget        *m_paramWidget;
    QFormLayout    *m_paramLayout;
    QList<QLineEdit *> m_paramEdits;
    QList<QPushButton *> m_rangeBtns;
    QList<QList<ChannelRange>> m_paramRanges;
};

#endif // FIXTUREDIALOG_H
