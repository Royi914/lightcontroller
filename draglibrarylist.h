#ifndef DRAGLIBRARYLIST_H
#define DRAGLIBRARYLIST_H

#include <QListWidget>
#include <QMimeData>

class DragLibraryList : public QListWidget
{
    Q_OBJECT
public:
    explicit DragLibraryList(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        if (items.isEmpty()) return nullptr;
        auto *mime = new QMimeData;
        mime->setData("application/x-light-fixture-index",
                      QByteArray::number(row(items.first())));
        return mime;
    }

    QStringList mimeTypes() const override
    {
        return {"application/x-light-fixture-index"};
    }
};

#endif // DRAGLIBRARYLIST_H
