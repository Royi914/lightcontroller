#ifndef ADDRESSPAGE_H
#define ADDRESSPAGE_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QLineEdit>
#include <QFrame>

class AddressPage : public QWidget
{
    Q_OBJECT
public:
    explicit AddressPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);

        // === 顶部：域选择 + 新建域 ===
        auto *topBar = new QHBoxLayout;
        m_domainCombo = new QComboBox;
        m_domainCombo->addItem("域 1");
        topBar->addWidget(new QLabel("选择域:"));
        topBar->addWidget(m_domainCombo);

        auto *newDomainBtn = new QPushButton("+ 新建域");
        topBar->addWidget(newDomainBtn);
        auto *backBtn = new QPushButton("← 返回2D视图");
        topBar->addWidget(backBtn);
        topBar->addStretch();

        m_totalLabel = new QLabel("共 512 通道");
        topBar->addWidget(m_totalLabel);
        root->addLayout(topBar);

        connect(backBtn, &QPushButton::clicked, this, &AddressPage::goBackRequested);

        // === 网格：512 通道，32 列 × 16 行 ===
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        m_gridWidget = new QWidget;
        m_gridLayout = new QGridLayout(m_gridWidget);
        m_gridLayout->setSpacing(1);

        buildGrid();

        scroll->setWidget(m_gridWidget);
        root->addWidget(scroll);

        // === 连接 ===
        connect(newDomainBtn, &QPushButton::clicked, this, [this]() {
            emit newDomainRequested();
        });

        connect(m_domainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) { emit domainChanged(idx); });
    }

    void setChannelValue(int ch, uint8_t value)
    {
        if (ch < 0 || ch >= 512) return;
        m_channelEdits[ch]->setText(QString::number(value));
    }

    uint8_t channelValue(int ch) const
    {
        if (ch < 0 || ch >= 512) return 0;
        return static_cast<uint8_t>(m_channelEdits[ch]->text().toInt());
    }

    int currentDomain() const { return m_domainCombo->currentIndex(); }

    void addDomain(const QString &name)
    {
        m_domainCombo->addItem(name);
        m_domainCombo->setCurrentIndex(m_domainCombo->count() - 1);
    }

signals:
    void goBackRequested();
    void domainChanged(int index);
    void newDomainRequested();
    void channelEdited(int channel, uint8_t value);

private:
    void buildGrid()
    {
        // 清空旧格子
        for (auto *w : m_channelEdits) delete w;
        m_channelEdits.fill(nullptr, 512);

        // 清除所有 item
        QLayoutItem *child;
        while ((child = m_gridLayout->takeAt(0)) != nullptr)
            delete child;

        const int cols = 32;
        for (int ch = 0; ch < 512; ch++)
        {
            int row = ch / cols;
            int col = ch % cols;

            auto *frame = new QFrame;
            frame->setStyleSheet("background:#222;border:1px solid #444");
            auto *vl = new QVBoxLayout(frame);
            vl->setContentsMargins(2, 1, 2, 1);
            vl->setSpacing(0);

            auto *label = new QLabel(QString("%1").arg(ch + 1));
            label->setStyleSheet("color:#888;font-size:8px;border:none");
            label->setAlignment(Qt::AlignCenter);

            auto *edit = new QLineEdit("0");
            edit->setStyleSheet("background:transparent;color:#0f0;font-size:11px;border:none;padding:0");
            edit->setAlignment(Qt::AlignCenter);
            edit->setMaxLength(3);
            edit->setMaximumWidth(36);

            connect(edit, &QLineEdit::textEdited, this, [this, ch](const QString &t) {
                bool ok;
                int v = t.toInt(&ok);
                if (ok && v >= 0 && v <= 255)
                    emit channelEdited(ch, static_cast<uint8_t>(v));
            });

            vl->addWidget(label);
            vl->addWidget(edit);
            m_gridLayout->addWidget(frame, row, col);
            m_channelEdits[ch] = edit;
        }
    }

    QComboBox                *m_domainCombo;
    QLabel                   *m_totalLabel;
    QWidget                  *m_gridWidget;
    QGridLayout              *m_gridLayout;
    QList<QLineEdit *>        m_channelEdits;
};

#endif // ADDRESSPAGE_H
