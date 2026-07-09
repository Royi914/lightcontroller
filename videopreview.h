#ifndef VIDEOPREVIEW_H
#define VIDEOPREVIEW_H

#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QCursor>
#include <QPushButton>
#include <QImage>
#include <QPixmap>
#include <QCoreApplication>
#include <QFile>
#include <QMap>
#include <QPoint>
#include <QEvent>

#include <QDir>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#endif

// ===== Windows Media Foundation 视频解码器 =====
class MfDecoder : public QObject
{
    Q_OBJECT
public:
    explicit MfDecoder(QObject *parent = nullptr) : QObject(parent) {}

    ~MfDecoder() { close(); }

    QString open(const QString &filePath, int targetWidth, int targetHeight)
    {
        close();
        m_lastError.clear();
#ifdef Q_OS_WIN
        // 初始化 Media Foundation（每个进程只需一次）
        static bool mfStarted = false;
        if (!mfStarted) {
            HRESULT mfhr = MFStartup(MF_VERSION);
            if (FAILED(mfhr)) {
                m_lastError = QString("MFStartup=0x%1").arg(static_cast<ulong>(mfhr), 8, 16, QChar('0'));
                return m_lastError;
            }
            mfStarted = true;
        }
        // MF 需要 file:// URL 格式
        QString url = "file:///" + filePath;
        url.replace('\\', '/');
        HRESULT hr = MFCreateSourceReaderFromURL(
            reinterpret_cast<LPCWSTR>(url.utf16()),
            nullptr, &m_reader);
        if (FAILED(hr) || !m_reader) {
            m_lastError = QString("open=0x%1").arg(static_cast<ulong>(hr), 8, 16, QChar('0'));
            return m_lastError;
        }

        // 用 YUY2（最广泛支持的 YUV 输出格式），后续手动转 RGB
        IMFMediaType *outType = nullptr;
        MFCreateMediaType(&outType);
        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        hr = m_reader->SetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            nullptr, outType);
        outType->Release();
        m_isYuy2 = true;
        if (FAILED(hr)) {
            // YUY2 失败，回退到 NV12
            MFCreateMediaType(&outType);
            outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            hr = m_reader->SetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                nullptr, outType);
            outType->Release();
            m_isYuy2 = false;
        }
        if (FAILED(hr)) { m_lastError = QString("setfmt=0x%1").arg(static_cast<ulong>(hr), 8, 16, QChar('0')); close(); return m_lastError; }
        return QString();
#else
        m_lastError = "not windows";
        return m_lastError;
#endif
    }

    void close()
    {
#ifdef Q_OS_WIN
        if (m_reader) { m_reader->Release(); m_reader = nullptr; }
#endif
    }

    QImage readFrame()
    {
#ifdef Q_OS_WIN
        if (!m_reader) return QImage();
        IMFSample *sample = nullptr;
        DWORD flags = 0;
        HRESULT hr = m_reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr) || !sample) {
            // 到末尾 → 重新定位到开头循环
            PROPVARIANT pos; PropVariantInit(&pos);
            pos.vt = VT_I8; pos.hVal.QuadPart = 0;
            m_reader->SetCurrentPosition(GUID_NULL, pos);
            PropVariantClear(&pos);
            return QImage();
        }

        IMFMediaBuffer *buf = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buf);
        if (FAILED(hr)) { sample->Release(); return QImage(); }

        BYTE *data = nullptr; DWORD maxLen = 0, curLen = 0;
        buf->Lock(&data, &maxLen, &curLen);
        // 获取输出帧宽高
        IMFMediaType *curType = nullptr;
        UINT32 width = 640, height = 360;
        if (SUCCEEDED(m_reader->GetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &curType))) {
            MFGetAttributeSize(curType, MF_MT_FRAME_SIZE, &width, &height);
            curType->Release();
        }
        int stride = (m_isYuy2) ? static_cast<int>(width) * 2 : static_cast<int>(width);

        QImage img;
        if (m_isYuy2)
            img = yuy2ToRgb(data, static_cast<int>(width), static_cast<int>(height), stride);
        else
            img = nv12ToRgb(data, static_cast<int>(width), static_cast<int>(height), stride);

        buf->Unlock();
        buf->Release();
        sample->Release();
        return img;
#else
        return QImage();
#endif
    }

    QString lastError() const { return m_lastError; }

private:
    // YUY2 → RGB32 转换（YUY2 = Y0 U0 Y1 V0 每4字节2像素）
    static QImage yuy2ToRgb(const BYTE *src, int w, int h, int stride)
    {
        QImage img(w, h, QImage::Format_RGB32);
        for (int y = 0; y < h; y++) {
            const BYTE *line = src + y * stride;
            QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < w; x += 2) {
                int y0 = line[x*2], u = line[x*2+1] - 128, y1 = line[x*2+2], v = line[x*2+3] - 128;
                auto clip = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
                dst[x]   = qRgb(clip(y0 + 1.402f*v), clip(y0 - 0.344f*u - 0.714f*v), clip(y0 + 1.772f*u));
                dst[x+1] = qRgb(clip(y1 + 1.402f*v), clip(y1 - 0.344f*u - 0.714f*v), clip(y1 + 1.772f*u));
            }
        }
        return img;
    }

    // NV12 → RGB32 转换（Y 平面 + 交错 UV 平面）
    static QImage nv12ToRgb(const BYTE *src, int w, int h, int stride)
    {
        QImage img(w, h, QImage::Format_RGB32);
        const BYTE *uvPlane = src + h * stride;
        for (int y = 0; y < h; y++) {
            const BYTE *yLine = src + y * stride;
            QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < w; x++) {
                int yy = yLine[x];
                int u = uvPlane[(y/2)*(stride) + (x&~1)] - 128;
                int v = uvPlane[(y/2)*(stride) + (x&~1)+1] - 128;
                auto clip = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
                dst[x] = qRgb(clip(yy + 1.402f*v), clip(yy - 0.344f*u - 0.714f*v), clip(yy + 1.772f*u));
            }
        }
        return img;
    }

#ifdef Q_OS_WIN
    IMFSourceReader *m_reader = nullptr;
#endif
    bool    m_isYuy2 = true;
    QString m_lastError;
};

// ===== 视频预览弹窗（悬停弹出，MF 解码 MP4 → QLabel 渲染，移开消失）=====
class VideoPreviewPopup : public QDialog
{
    Q_OBJECT
public:
    explicit VideoPreviewPopup(QWidget *parent = nullptr)
        : QDialog(parent, Qt::ToolTip | Qt::FramelessWindowHint)
    {
        setFixedSize(300, 210);
        setStyleSheet("background:#1a1a1a;border:2px solid #555;border-radius:4px");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(3, 3, 3, 0);
        root->setSpacing(0);

        m_titleLabel = new QLabel;
        m_titleLabel->setAlignment(Qt::AlignCenter);
        m_titleLabel->setFixedHeight(20);
        m_titleLabel->setStyleSheet("color:#ccc;font-size:10px;border:none;background:#2a2a2a;border-radius:2px");
        root->addWidget(m_titleLabel);

        m_frameLabel = new QLabel;
        m_frameLabel->setFixedSize(294, 170);
        m_frameLabel->setAlignment(Qt::AlignCenter);
        m_frameLabel->setStyleSheet("background:#000;border:none");
        root->addWidget(m_frameLabel);

        m_decoder = new MfDecoder(this);

        // 帧渲染定时器 ~25fps
        m_playTimer = new QTimer(this);
        m_playTimer->setInterval(40);
        connect(m_playTimer, &QTimer::timeout, this, [this]() {
            QImage frame = m_decoder->readFrame();
            if (!frame.isNull()) {
                m_frameLabel->setPixmap(
                    QPixmap::fromImage(frame).scaled(
                        m_frameLabel->size(), Qt::KeepAspectRatio,
                        Qt::SmoothTransformation));
            }
        });

        // 悬停消失自检
        m_hideCheckTimer = new QTimer(this);
        m_hideCheckTimer->setInterval(200);
        connect(m_hideCheckTimer, &QTimer::timeout, this, [this]() {
            bool onPopup = underMouse() || geometry().contains(QCursor::pos());
            bool onBtn   = m_anchorBtn && (m_anchorBtn->underMouse()
                        || m_anchorBtn->geometry().contains(
                            m_anchorBtn->mapFromGlobal(QCursor::pos())));
            if (!onPopup && !onBtn) {
                m_playTimer->stop();
                m_decoder->close();
                m_hideCheckTimer->stop();
                m_frameLabel->clear();
                hide();
            }
        });
    }

    void showForEffect(const QString &effectName, QPushButton *anchorBtn)
    {
        static const QMap<QString, QString> videoMap = {
            {"主渐亮", "主渐亮.mp4"},
            {"主渐暗", "主渐暗.mp4"},
            {"频闪",   "频闪.mp4"},
            {"换色",   "换色.mp4"},
        };
        if (!videoMap.contains(effectName)) return;

        m_anchorBtn = anchorBtn;
        m_titleLabel->setText("  " + effectName + " 效果演示");

        QString path = QCoreApplication::applicationDirPath()
                     + "/demo_videos/" + videoMap[effectName];
        if (!QFile::exists(path)) {
            m_titleLabel->setText("  视频文件未找到");
            return;
        }

        QString err = m_decoder->open(path, 294, 170);
        if (err.isEmpty()) {
            m_playTimer->start();
            m_hideCheckTimer->start();
        } else {
            m_titleLabel->setText("  " + err + " " + path.mid(path.lastIndexOf('/')+1));
        }
        show();
    }

protected:
    void mousePressEvent(QMouseEvent *) override
    {
        m_playTimer->stop();
        m_decoder->close();
        m_hideCheckTimer->stop();
        m_frameLabel->clear();
        hide();
    }

private:
    QLabel      *m_titleLabel = nullptr;
    QLabel      *m_frameLabel = nullptr;
    MfDecoder   *m_decoder = nullptr;
    QTimer      *m_playTimer = nullptr;
    QTimer      *m_hideCheckTimer = nullptr;
    QPushButton *m_anchorBtn = nullptr;
};

#endif // VIDEOPREVIEW_H
