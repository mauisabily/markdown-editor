#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QTextDocument>
#include <QTextStream>
#include <QPageSize>
#include <QPageLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QDialog>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPainter>

const QString MARKDOWN_STYLESHEET = 
    "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif, 'Apple Color Emoji', 'Segoe UI Emoji', 'Noto Color Emoji'; color: #24292f; line-height: 1.5; }"
    "h1 { color: #1f2328; border-bottom: 1px solid #d0d7de; padding-bottom: 0.3em; font-size: 2em; margin-top: 24px; margin-bottom: 16px; font-weight: 600; }"
    "h2 { color: #1f2328; border-bottom: 1px solid #d0d7de; padding-bottom: 0.3em; font-size: 1.5em; margin-top: 24px; margin-bottom: 16px; font-weight: 600; }"
    "h3 { color: #1f2328; font-size: 1.25em; margin-top: 24px; margin-bottom: 16px; font-weight: 600; }"
    "a { color: #0969da; text-decoration: none; }"
    "code { background-color: rgba(175,184,193,0.2); padding: 0.2em 0.4em; border-radius: 6px; font-family: ui-monospace, SFMono-Regular, SF Mono, Menlo, Consolas, Liberation Mono, monospace; font-size: 85%; }"
    "pre { padding: 16px; background-color: #f6f8fa; border-radius: 6px; font-family: ui-monospace, SFMono-Regular, SF Mono, Menlo, Consolas, Liberation Mono, monospace; font-size: 85%; line-height: 1.45; }"
    "blockquote { padding: 0 1em; color: #57606a; border-left: .25em solid #d0d7de; margin: 0; }"
    "table { border-collapse: collapse; width: 100%; margin-top: 0; margin-bottom: 16px; }"
    "th, td { padding: 6px 13px; border: 1px solid #d0d7de; }"
    "tr:nth-child(even) { background-color: #f6f8fa; }";

// ─── PDF Preview Dialog ──────────────────────────────────────────────
class PdfPreviewDialog : public QDialog {
    Q_OBJECT
public:
    PdfPreviewDialog(const QString &markdown,
                     QPageSize::PageSizeId sizeId,
                     QPageLayout::Orientation orient,
                     QWidget *parent = nullptr)
         : QDialog(parent)
    {
        setWindowTitle("PDF Preview");
        resize(750, 900);

        auto *mainLayout = new QVBoxLayout(this);

        // scroll area that holds rendered page images
        auto *scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setStyleSheet("background:#888;");

        auto *container = new QWidget();
        auto *pages = new QVBoxLayout(container);
        pages->setAlignment(Qt::AlignHCenter);

        // build a document and set its page size
        QPageLayout pageLayout(QPageSize(sizeId), orient,
                               QMarginsF(15, 15, 15, 15),
                               QPageLayout::Millimeter);
        QSizeF pagePt = pageLayout.paintRect(QPageLayout::Point).size();

        QTextDocument doc;
        doc.setDefaultStyleSheet(MARKDOWN_STYLESHEET);
        doc.setMarkdown(markdown);
        doc.setPageSize(pagePt);

        int pageCount = doc.pageCount();
        if (pageCount < 1) pageCount = 1;

        double scale = 1.5;

        for (int i = 0; i < pageCount; ++i) {
            int pw = static_cast<int>(pagePt.width()  * scale);
            int ph = static_cast<int>(pagePt.height() * scale);

            QPixmap pix(pw, ph);
            pix.fill(Qt::white);

            QPainter painter(&pix);
            painter.scale(scale, scale);
            // clip to this page only
            QRectF clip(0, i * pagePt.height(),
                        pagePt.width(), pagePt.height());
            painter.translate(0, -i * pagePt.height());
            doc.drawContents(&painter, clip);
            painter.end();

            // thin border around the page
            QPainter border(&pix);
            border.setPen(QPen(Qt::darkGray, 1));
            border.drawRect(pix.rect().adjusted(0, 0, -1, -1));
            border.end();

            auto *lbl = new QLabel();
            lbl->setPixmap(pix);
            lbl->setAlignment(Qt::AlignCenter);
            pages->addWidget(lbl);

            auto *num = new QLabel(
                QString("— Page %1 / %2 —").arg(i + 1).arg(pageCount));
            num->setAlignment(Qt::AlignCenter);
            num->setStyleSheet("color:white; font-size:11px;");
            pages->addWidget(num);
        }

        scrollArea->setWidget(container);
        mainLayout->addWidget(scrollArea);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttons);
    }
};

// ─── Main Editor Window ──────────────────────────────────────────────
class EditorWindow : public QMainWindow {
    Q_OBJECT
public:
    EditorWindow(QWidget *parent = nullptr)
        : QMainWindow(parent),
          pageSizeId(QPageSize::A4),
          orientation(QPageLayout::Portrait)
    {
        // --- central split: editor | live preview ---
        auto *splitter = new QSplitter(this);
        editor  = new QTextEdit(this);
        preview = new QTextEdit(this);
        preview->setReadOnly(true);
        preview->setStyleSheet("background:#f0f0f0;");
        preview->document()->setDefaultStyleSheet(MARKDOWN_STYLESHEET);
        splitter->addWidget(editor);
        splitter->addWidget(preview);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 1);
        setCentralWidget(splitter);

        // --- menu bar ---
        auto *fileMenu = menuBar()->addMenu("&File");

        auto *newAct = fileMenu->addAction("&New");
        newAct->setShortcut(QKeySequence::New);
        connect(newAct, &QAction::triggered, this, &EditorWindow::newFile);

        auto *openAct = fileMenu->addAction("&Open…");
        openAct->setShortcut(QKeySequence::Open);
        connect(openAct, &QAction::triggered, this, &EditorWindow::openFile);

        auto *saveAct = fileMenu->addAction("&Save");
        saveAct->setShortcut(QKeySequence::Save);
        connect(saveAct, &QAction::triggered, this, &EditorWindow::saveMarkdown);

        auto *saveAsAct = fileMenu->addAction("Save &As…");
        saveAsAct->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_S));
        connect(saveAsAct, &QAction::triggered, this, &EditorWindow::saveMarkdownAs);

        fileMenu->addSeparator();

        auto *pdfPreviewAct = fileMenu->addAction("PDF Pre&view…");
        pdfPreviewAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
        connect(pdfPreviewAct, &QAction::triggered, this, &EditorWindow::showPdfPreview);

        auto *pdfAct = fileMenu->addAction("&Export PDF…");
        pdfAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
        connect(pdfAct, &QAction::triggered, this, &EditorWindow::exportPdf);

        fileMenu->addSeparator();

        auto *quitAct = fileMenu->addAction("&Quit");
        quitAct->setShortcut(QKeySequence::Quit);
        connect(quitAct, &QAction::triggered, this, &QWidget::close);

        // Settings menu
        auto *settingsMenu = menuBar()->addMenu("&Settings");
        auto *paperAct = settingsMenu->addAction("&Paper Settings…");
        connect(paperAct, &QAction::triggered, this, &EditorWindow::showPaperSettings);

        // --- toolbar ---
        auto *toolbar = addToolBar("Main");
        toolbar->addAction(newAct);
        toolbar->addAction(openAct);
        toolbar->addAction(saveAct);
        toolbar->addSeparator();
        toolbar->addAction(pdfPreviewAct);
        toolbar->addAction(pdfAct);

        // live preview connection
        connect(editor, &QTextEdit::textChanged, this, &EditorWindow::updatePreview);

        statusBar()->showMessage("Ready  |  Paper: A4 Portrait");
        setWindowTitle("mdedit-gui");
        resize(900, 650);
    }

private slots:
    // ── live preview ──
    void updatePreview() {
        preview->setMarkdown(editor->toPlainText());
    }

    // ── New ──
    void newFile() {
        editor->clear();
        currentFile.clear();
        setWindowTitle("mdedit-gui");
        statusBar()->showMessage("New file");
    }

    // ── Open ──
    void openFile() {
        QString fn = QFileDialog::getOpenFileName(this,
            "Open Markdown", "",
            "Markdown Files (*.md *.markdown *.txt);;All Files (*)");
        if (fn.isEmpty()) return;
        QFile f(fn);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Error", "Cannot read file.");
            return;
        }
        QTextStream in(&f);
        editor->setPlainText(in.readAll());
        currentFile = fn;
        setWindowTitle("mdedit-gui  —  " + fn);
        statusBar()->showMessage("Opened: " + fn);
    }

    // ── Save ──
    void saveMarkdown() {
        if (currentFile.isEmpty()) { saveMarkdownAs(); return; }
        writeFile(currentFile);
    }

    void saveMarkdownAs() {
        QString fn = QFileDialog::getSaveFileName(this,
            "Save Markdown", "",
            "Markdown Files (*.md);;All Files (*)");
        if (fn.isEmpty()) return;
        writeFile(fn);
    }

    // ── Paper Settings ──
    void showPaperSettings() {
        QDialog dlg(this);
        dlg.setWindowTitle("Paper Settings");
        auto *layout = new QVBoxLayout(&dlg);

        // size group
        auto *sizeGrp = new QGroupBox("Paper Size", &dlg);
        auto *sizeL   = new QVBoxLayout(sizeGrp);
        QRadioButton *rA4     = new QRadioButton("A4",     sizeGrp);
        QRadioButton *rA5     = new QRadioButton("A5",     sizeGrp);
        QRadioButton *rB5     = new QRadioButton("B5",     sizeGrp);
        QRadioButton *rLetter = new QRadioButton("Letter", sizeGrp);
        QRadioButton *rLegal  = new QRadioButton("Legal",  sizeGrp);
        switch (pageSizeId) {
            case QPageSize::A5:     rA5->setChecked(true);     break;
            case QPageSize::B5:     rB5->setChecked(true);     break;
            case QPageSize::Letter: rLetter->setChecked(true); break;
            case QPageSize::Legal:  rLegal->setChecked(true);  break;
            default:                rA4->setChecked(true);     break;
        }
        sizeL->addWidget(rA4);
        sizeL->addWidget(rA5);
        sizeL->addWidget(rB5);
        sizeL->addWidget(rLetter);
        sizeL->addWidget(rLegal);

        // orientation group
        auto *oriGrp = new QGroupBox("Orientation", &dlg);
        auto *oriL   = new QVBoxLayout(oriGrp);
        QRadioButton *rPortrait  = new QRadioButton("Portrait",  oriGrp);
        QRadioButton *rLandscape = new QRadioButton("Landscape", oriGrp);
        if (orientation == QPageLayout::Landscape)
            rLandscape->setChecked(true);
        else
            rPortrait->setChecked(true);
        oriL->addWidget(rPortrait);
        oriL->addWidget(rLandscape);

        layout->addWidget(sizeGrp);
        layout->addWidget(oriGrp);

        auto *btns = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        layout->addWidget(btns);

        if (dlg.exec() == QDialog::Accepted) {
            if      (rA5->isChecked())     pageSizeId = QPageSize::A5;
            else if (rB5->isChecked())     pageSizeId = QPageSize::B5;
            else if (rLetter->isChecked()) pageSizeId = QPageSize::Letter;
            else if (rLegal->isChecked())  pageSizeId = QPageSize::Legal;
            else                           pageSizeId = QPageSize::A4;

            orientation = rLandscape->isChecked()
                ? QPageLayout::Landscape : QPageLayout::Portrait;

            statusBar()->showMessage(
                QString("Paper: %1 %2")
                    .arg(QPageSize(pageSizeId).name())
                    .arg(orientation == QPageLayout::Portrait
                         ? "Portrait" : "Landscape"));
        }
    }

    // ── PDF Preview ──
    void showPdfPreview() {
        PdfPreviewDialog dlg(editor->toPlainText(),
                             pageSizeId, orientation, this);
        dlg.exec();
    }

    // ── Export PDF ──
    void exportPdf() {
        QString fn = QFileDialog::getSaveFileName(this,
            "Export PDF", "", "PDF Files (*.pdf);;All Files (*)");
        if (fn.isEmpty()) return;

        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fn);
        printer.setPageSize(QPageSize(pageSizeId));
        printer.setPageOrientation(orientation);

        // Use consistent 15mm margins
        QPageLayout layout = printer.pageLayout();
        layout.setMargins(QMarginsF(15, 15, 15, 15));
        printer.setPageLayout(layout);

        QTextDocument doc;
        doc.setDefaultStyleSheet(MARKDOWN_STYLESHEET);
        doc.setMarkdown(editor->toPlainText());
        doc.setPageSize(printer.pageLayout().paintRect(QPageLayout::Point).size());

        doc.print(&printer);

        statusBar()->showMessage("PDF exported: " + fn);
    }

private:
    void writeFile(const QString &fn) {
        QFile f(fn);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Error", "Cannot write to file.");
            return;
        }
        QTextStream out(&f);
        out << editor->toPlainText();
        currentFile = fn;
        setWindowTitle("mdedit-gui  —  " + fn);
        statusBar()->showMessage("Saved: " + fn);
    }

    QTextEdit *editor;
    QTextEdit *preview;
    QString    currentFile;

    QPageSize::PageSizeId     pageSizeId;
    QPageLayout::Orientation  orientation;
};

// ─── Entry Point ─────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    EditorWindow win;
    win.show();
    return app.exec();
}

#include "main.moc"
