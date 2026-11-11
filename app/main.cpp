#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "Config.hpp"
#include "MainWindow.hpp"
#include "PackService.hpp"
#include "evalcore/minisign.hpp"

static const char* kStyle = R"(
QWidget { font-size: 10.5pt; }
QLabel#pageTitle { font-size: 18pt; font-weight: 600; }
QLabel#muted { color: #5f6368; }
QLabel#error { color: #b3261e; }
QLabel#bigNumber { font-size: 17pt; font-weight: 600; }
QLabel#sectionTitle { font-size: 12pt; font-weight: 600; margin-top: 8px; }
QListWidget#jobList { border: 1px solid #d0d4da; border-radius: 6px; }
QListWidget#jobList::item { padding: 6px 8px; }
QListWidget#jobList::item:selected { background: #1a5fb4; color: white; }
QLineEdit#jobName { font-size: 15pt; font-weight: 600; padding: 4px 8px; }
QWidget#cardHot { background: #1a5fb4; border-radius: 8px; }
QLabel#cardHotCaption { color: #d6e4f7; }
QLabel#bigNumberHot { color: white; font-size: 19pt; font-weight: 700; }
QWidget#card { background: #f3f5f8; border-radius: 8px; }
QPushButton#primary { background: #1a5fb4; color: white; padding: 7px 22px; border-radius: 5px; }
QPushButton#primary:disabled { background: #a9b7cc; }
QGroupBox { font-weight: 600; margin-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; }
)";

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("jobeval");
    QApplication::setApplicationName("jobeval");
    QApplication::setStyle("Fusion");
    app.setStyleSheet(kStyle);

    QCommandLineParser cli;
    cli.addHelpOption();
    cli.addOption({"screenshot", "Developer: save a PNG of the window and exit.", "file"});
    cli.addOption({"demo", "Developer: start a new project with one job in this pack id.", "packId"});
    cli.addOption({"job", "Developer: select this job number (1-based).", "n"});
    cli.addOption({"tab", "Developer: show this tab (compare).", "name"});
    cli.addOption({"session-file", "Developer: use this autosave file.", "file"});
    cli.addPositionalArgument("file", "A .jobeval project to open.");
    cli.addOption({"page", "Developer: with --screenshot, 'region' shows the region page.", "name"});
    cli.addOption({"state", "Developer: set an input, key=value (repeatable).", "kv"});
    cli.addOption({"cache-dir", "Developer: use this cache directory.", "dir"});
    cli.process(app);

    QString url = qEnvironmentVariable("JOBEVAL_CATALOG_URL", config::kCatalogUrl);
    QString keyText = qEnvironmentVariable("JOBEVAL_DEV_PUBKEY", config::kPublicKey);
    QString suffix;
    if (qEnvironmentVariableIsSet("JOBEVAL_DEV_PUBKEY") || qEnvironmentVariableIsSet("JOBEVAL_CATALOG_URL")) suffix = " [DEV KEY]";

    jobeval::MinisignKey key;
    std::string err;
    if (!jobeval::parse_minisign_public_key(keyText.toStdString(), key, err)) return 2;

    QString cache = cli.isSet("cache-dir") ? cli.value("cache-dir")
                                           : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/packs";
    QDir().mkpath(cache);
    if (!url.endsWith('/')) url += '/';

    PackService service(cache, QUrl(url), key);
    service.loadCachedCatalog();
    const QString session = cli.isSet("session-file") ? cli.value("session-file")
                                                      : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/session.jobeval";
    MainWindow w(&service, suffix, session);

    const bool shot = cli.isSet("screenshot");
    const QStringList files = cli.positionalArguments();
    w.start(files.isEmpty() ? QString() : files.first());
    if (cli.isSet("demo") && w.jobCount() == 0) {
        QMetaObject::invokeMethod(&w, [&]() { w.addJobInRegion(cli.value("demo")); });
    }
    if (cli.isSet("state")) {
        QVariantMap m;
        for (const QString& kv : cli.values("state")) {
            const int eq = kv.indexOf('=');
            if (eq > 0) m[kv.left(eq)] = kv.mid(eq + 1).toDouble();
        }
        w.applyStateToCurrent(m);
    }
    if (cli.isSet("job")) w.selectJobRow(cli.value("job").toInt() - 1);
    if (cli.value("tab") == "compare") w.showCompareTab();
    if (cli.value("page") == "region") w.showRegionPage(true);
    w.show();

    if (shot) {
        QTimer::singleShot(500, [&]() {
            w.grab().save(cli.value("screenshot"));
            QApplication::quit();
        });
    } else {
        service.refreshAsync();  // background update check; the saved catalog keeps working offline
    }
    return app.exec();
}
