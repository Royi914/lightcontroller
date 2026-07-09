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
#include <QFileDialog>
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
        setMinimumWidth(420);
        auto *root = new QVBoxLayout(this);
        m_layout = new QVBoxLayout;
        root->addLayout(m_layout);
        addRow(); // 至少一行，默认 0-255

        auto *btnRow = new QHBoxLayout;
        auto *addBtn = new QPushButton("+ 添加行");
        btnRow->addWidget(addBtn);
        btnRow->addStretch();
        root->addLayout(btnRow);

        auto *dlgBtns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(dlgBtns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(dlgBtns, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(dlgBtns);

        connect(addBtn, &QPushButton::clicked, this, [this]() { addRow(true); });
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
                r.iconPath = row.iconPath;
                result << r;
            }
        }
        return result;
    }

    void setRanges(const QList<ChannelRange> &ranges)
    {
        for (auto &row : m_rows) delete row.container;
        m_rows.clear();
        while (m_layout->count() > 0) delete m_layout->takeAt(0);
        for (const auto &r : ranges) {
            addRow(false);
            auto &row = m_rows.last();
            row.nameEdit->setText(r.name);
            row.minSpin->setValue(r.minValue);
            row.maxSpin->setValue(r.maxValue);
            row.iconPath = r.iconPath;
            if (!r.iconPath.isEmpty() && row.iconBtn) {
                row.iconBtn->setText("🖼");
                row.iconBtn->setToolTip("已导入图标");
            }
        }
        if (m_rows.isEmpty()) addRow();
    }

private slots:
    void addRow(bool doSplit = true)
    {
        int newMin = 0, newMax = 255;

        // If splitting, halve the last row's range
        if (doSplit && !m_rows.isEmpty()) {
            auto &last = m_rows.last();
            int lastMin = last.minSpin->value();
            int lastMax = last.maxSpin->value();
            int span = lastMax - lastMin;
            if (span >= 2) {
                int mid = lastMin + span / 2;
                m_updatingSpins = true;
                last.maxSpin->setValue(mid);
                m_updatingSpins = false;
                newMin = mid + 1;
                newMax = lastMax;
            } else {
                // Range too small to split — place after with same size
                newMin = qMin(lastMax + 1, 255);
                newMax = qMin(newMin + qMax(span, 1), 255);
            }
        }

        auto *container = new QWidget;
        auto *hl = new QHBoxLayout(container);
        hl->setContentsMargins(0, 0, 0, 0);

        auto *nameEdit = new QLineEdit;
        nameEdit->setPlaceholderText("功能名(如:图案片1)");
        hl->addWidget(nameEdit, 2);

        auto *minSpin = new QSpinBox;
        minSpin->setRange(0, 255);
        minSpin->setValue(newMin);
        minSpin->setPrefix("从 ");
        hl->addWidget(minSpin);

        auto *sep = new QLabel(QString::fromUtf8("\xe2\x80\x94"));  // em-dash
        hl->addWidget(sep);

        auto *maxSpin = new QSpinBox;
        maxSpin->setRange(0, 255);
        maxSpin->setValue(newMax);
        maxSpin->setPrefix("到 ");
        hl->addWidget(maxSpin);

        auto *iconBtn = new QPushButton("📁");
        iconBtn->setFixedSize(22, 22);
        iconBtn->setToolTip("导入图标");
        iconBtn->setStyleSheet("border:none;background:transparent;font-size:11px;");
        hl->addWidget(iconBtn);

        auto *delBtn = new QPushButton(QString::fromUtf8("\xc3\x97"));  // ×
        delBtn->setFixedSize(24, 24);
        delBtn->setStyleSheet("color:red;font-weight:bold;border:none");
        hl->addWidget(delBtn);

        m_rows << Row{container, nameEdit, minSpin, maxSpin, iconBtn, QString()};
        m_layout->addWidget(container);

        // 连接要在 m_rows 追加之后，lambda 通过 container 反查当前行
        int rowIdx = m_rows.size() - 1;
        connect(iconBtn, &QPushButton::clicked, this, [this, rowIdx, iconBtn]() {
            if (rowIdx >= 0 && rowIdx < m_rows.size()) {
                QString path = QFileDialog::getOpenFileName(this, "选择图标图片", "",
                    "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.svg)");
                if (!path.isEmpty()) {
                    m_rows[rowIdx].iconPath = path;
                    iconBtn->setText("🖼");
                    iconBtn->setToolTip("已导入图标");
                }
            }
        });

        // Adjacency sync: when min changes, update previous row's max
        connect(minSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, container](int val) {
            if (m_updatingSpins) return;
            int idx = rowIndex(container);
            if (idx > 0) {
                m_updatingSpins = true;
                m_rows[idx - 1].maxSpin->setValue(val - 1);
                m_updatingSpins = false;
            }
        });

        // Adjacency sync: when max changes, update next row's min
        connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, container](int val) {
            if (m_updatingSpins) return;
            int idx = rowIndex(container);
            if (idx >= 0 && idx < m_rows.size() - 1) {
                m_updatingSpins = true;
                m_rows[idx + 1].minSpin->setValue(val + 1);
                m_updatingSpins = false;
            }
        });

    }

private:
    int rowIndex(QWidget *container) const
    {
        for (int i = 0; i < m_rows.size(); i++)
            if (m_rows[i].container == container) return i;
        return -1;
    }

    struct Row {
        QWidget     *container;
        QLineEdit   *nameEdit;
        QSpinBox    *minSpin;
        QSpinBox    *maxSpin;
        QPushButton *iconBtn;
        QString      iconPath;
    };
    QList<Row>    m_rows;
    QVBoxLayout  *m_layout;
    bool          m_updatingSpins = false;
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

        // 图标
        auto *iconLayout = new QHBoxLayout;
        iconLayout->addWidget(new QLabel("图标:"));
        m_iconPreview = new QLabel;
        m_iconPreview->setFixedSize(32, 32);
        m_iconPreview->setStyleSheet("background:#ddd;border-radius:16px;border:2px solid #bbb;color:#666;font-size:14px");
        m_iconPreview->setAlignment(Qt::AlignCenter);
        m_iconPreview->setText("灯");
        iconLayout->addWidget(m_iconPreview);
        auto *iconBtn = new QPushButton("选择图片");
        iconBtn->setStyleSheet("background:#e8e8e8;color:#222;border:1px solid #bbb;border-radius:3px;padding:4px 10px;font-size:11px");
        connect(iconBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(this, "选择灯具图标", "",
                "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.svg)");
            if (!path.isEmpty()) {
                m_iconPath = path;
                QPixmap pm = centerCropToSquare(path, 32);
                if (!pm.isNull()) {
                    QPixmap circle(32, 32); circle.fill(Qt::transparent);
                    QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
                    pp.setBrush(pm); pp.setPen(Qt::NoPen);
                    pp.drawEllipse(0, 0, 32, 32); pp.end();
                    m_iconPreview->setPixmap(circle);
                }
            }
        });
        iconLayout->addWidget(iconBtn);
        auto *clearIconBtn = new QPushButton("清除");
        clearIconBtn->setStyleSheet("background:#e8e8e8;color:#822;border:1px solid #bbb;border-radius:3px;padding:4px 10px;font-size:11px");
        connect(clearIconBtn, &QPushButton::clicked, this, [this]() {
            m_iconPath.clear();
            m_iconPreview->setPixmap(QPixmap());
            m_iconPreview->setText("灯");
        });
        iconLayout->addWidget(clearIconBtn);
        iconLayout->addStretch();
        root->addLayout(iconLayout);

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

    static QPixmap centerCropToSquare(const QString &path, int targetSize)
    {
        QImage img(path);
        if (img.isNull()) return QPixmap();
        int w = img.width(), h = img.height();
        int sq = qMin(w, h);
        int x = (w - sq) / 2, y = (h - sq) / 2;
        return QPixmap::fromImage(
            img.copy(x, y, sq, sq).scaled(targetSize, targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    FixtureDef getFixtureDef() const
    {
        FixtureDef def;
        def.name = m_nameEdit->text();
        def.manufacturer = m_mfrEdit->text();
        def.iconPath = m_iconPath;
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
        m_iconPath = def.iconPath;
        if (!m_iconPath.isEmpty()) {
            QPixmap pm = centerCropToSquare(m_iconPath, 32);
            if (!pm.isNull()) {
                QPixmap circle(32, 32); circle.fill(Qt::transparent);
                QPainter pp(&circle); pp.setRenderHint(QPainter::Antialiasing);
                pp.setBrush(pm); pp.setPen(Qt::NoPen);
                pp.drawEllipse(0, 0, 32, 32); pp.end();
                m_iconPreview->setPixmap(circle);
            }
        }
        rebuildParams(def.channels);
        for (int i = 0; i < def.channelNames.size(); i++)
            m_paramEdits[i]->setText(def.channelNames[i]);
        m_paramRanges = def.ranges;
        m_paramRanges.resize(def.channels);
    }

private slots:
    void rebuildParams(int channels)
    {
        // 保存旧的通道名称
        QStringList oldNames;
        for (auto *edit : m_paramEdits)
            oldNames << edit->text();
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
            // 恢复之前填过的通道名称
            if (i < oldNames.size()) edit->setText(oldNames[i]);
            m_paramEdits << edit;

            auto *customBtn = new QPushButton("自定义");
            customBtn->setFixedWidth(60);
            // 恢复已配(N)状态
            int existingCnt = m_paramRanges[i].size();
            if (existingCnt > 0)
                customBtn->setText(QString("已配(%1)").arg(existingCnt));
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
    QLabel         *m_iconPreview;
    QString         m_iconPath;
    QWidget        *m_paramWidget;
    QFormLayout    *m_paramLayout;
    QList<QLineEdit *> m_paramEdits;
    QList<QPushButton *> m_rangeBtns;
    QList<QList<ChannelRange>> m_paramRanges;
};

#endif // FIXTUREDIALOG_H
