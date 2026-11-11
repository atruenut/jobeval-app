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
QLabel#bigNumber { font-size: 20pt; font-weight: 600; }
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
    cli.addOption({"demo", "Developer: open this pack id immediately.", "packId"});
    cli.addOption({"page", "Developer: with --screenshot, 'region' shows the region page.", "name"});
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
    MainWindow w(&service, suffix);

    const bool shot = cli.isSet("screenshot");
    if (cli.isSet("demo")) w.openRegion(cli.value("demo"));
    if (cli.value("page") == "region") w.showRegionPage();
    w.show();

    if (shot) {
        QTimer::singleShot(400, [&]() {
            w.grab().save(cli.value("screenshot"));
            QApplication::quit();
        });
    } else {
        service.refreshAsync();  // background update check; the saved catalog keeps working offline
    }
    return app.exec();
}
