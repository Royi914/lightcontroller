#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include <QDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QtMath>

// ===== HSV 色轮 =====
class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(280, 280);
        setMaximumSize(400, 400);
    }

    QColor selectedColor() const { return m_color; }
    void setColor(const QColor &c) { m_color = c; update(); }

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int side = qMin(width(), height());
        int cx = width() / 2, cy = height() / 2;
        int r = side / 2 - 6;

        // 绘制 HSV 色轮
        for (int x = -r; x < r; x++) {
            for (int y = -r; y < r; y++) {
                float dist = sqrt(x*x + y*y);
                if (dist > r) continue;
                float hue = atan2(-y, x) * 180.0f / M_PI + 180.0f;
                float sat = dist / r;
                QColor c = QColor::fromHsv(static_cast<int>(hue) % 360,
                                            static_cast<int>(sat * 255),
                                            255);
                p.setPen(c);
                p.drawPoint(cx + x, cy + y);
            }
        }

        // 选中标记
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(cx + m_selX - 4, cy + m_selY - 4, 8, 8);
    }

    void mousePressEvent(QMouseEvent *e) override { pickColor(e->pos()); }
    void mouseMoveEvent(QMouseEvent *e) override { pickColor(e->pos()); }

private:
    void pickColor(const QPoint &pos)
    {
        int cx = width() / 2, cy = height() / 2;
        int r = qMin(width(), height()) / 2 - 6;
        int x = pos.x() - cx, y = pos.y() - cy;
        float dist = sqrt(x*x + y*y);
        if (dist > r) return;

        float hue = atan2(-y, x) * 180.0f / M_PI + 180.0f;
        float sat = dist / r;
        m_color = QColor::fromHsv(static_cast<int>(hue) % 360,
                                  static_cast<int>(qMin(sat, 1.0f) * 255), 255);
        m_selX = x; m_selY = y;
        update();
        emit colorChanged(m_color);
    }

    QColor m_color = Qt::white;
    int m_selX = 0, m_selY = 0;
};

// ===== 调色盘弹窗 =====
class ColorPickerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ColorPickerDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("调色盘");
        auto *root = new QVBoxLayout(this);

        m_wheel = new ColorWheel;
        root->addWidget(m_wheel);

        // 预览 + 十六进制输入
        auto *preview = new QHBoxLayout;
        preview->addWidget(new QLabel("预览:"));
        m_preview = new QLabel;
        m_preview->setFixedSize(60, 30);
        m_preview->setStyleSheet("border:1px solid #555");
        preview->addWidget(m_preview);
        m_hexEdit = new QLineEdit;
        m_hexEdit->setMaxLength(7);
        m_hexEdit->setPlaceholderText("#000000");
        m_hexEdit->setMaximumWidth(80);
        preview->addWidget(m_hexEdit);
        preview->addStretch();
        root->addLayout(preview);

        connect(m_hexEdit, &QLineEdit::textEdited, this, [this](const QString &t) {
            QColor c(t);
            if (c.isValid()) {
                m_wheel->setColor(c);
                m_preview->setStyleSheet(QString("background:%1;border:1px solid #555").arg(c.name()));
            }
        });

        // 按钮
        auto *btns = new QHBoxLayout;
        btns->addStretch();
        auto *ok = new QPushButton("确定");
        auto *cancel = new QPushButton("取消");
        btns->addWidget(ok); btns->addWidget(cancel);
        root->addLayout(btns);

        connect(m_wheel, &ColorWheel::colorChanged, this, [this](const QColor &c) {
            m_preview->setStyleSheet(QString("background:%1;border:1px solid #555").arg(c.name()));
            m_hexEdit->setText(c.name().toUpper());
        });
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    QColor selectedColor() const { return m_wheel->selectedColor(); }

private:
    ColorWheel *m_wheel;
    QLabel *m_preview;
    QLineEdit *m_hexEdit;
};

#endif // COLORWHEEL_H
