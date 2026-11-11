#include <QtTest>

#include "../JobModel.hpp"

class JobModelTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void defaultsAreComplete() {
        const QVariantMap d = defaultState();
        for (const FieldSpec& f : fieldSpecs()) QVERIFY2(d.contains(f.key), f.key);
        QVERIFY(QDate::fromString(d["asof"].toString(), Qt::ISODate).isValid());
    }

    void roundTrip() {
        Project p;
        p.name = "Mine";
        Job a;
        a.id = "id-1";
        a.name = "Café – night shift";  // non-ASCII survives
        a.notes = "line one\nline two";
        a.packId = "CA-NB-2026";
        a.state = normalizedState({{"pay", 19.75}, {"hours", 44.5}, {"otmode", 2}, {"x.some_input", 12.5}});
        p.jobs.append(a);
        const QByteArray json = projectToJson(p, "project");
        Project q;
        QString err;
        QVERIFY2(projectFromJson(json, q, err), qPrintable(err));
        QCOMPARE(q.name, p.name);
        QCOMPARE(q.jobs.size(), 1);
        QCOMPARE(q.jobs[0].id, a.id);
        QCOMPARE(q.jobs[0].name, a.name);
        QCOMPARE(q.jobs[0].notes, a.notes);
        QCOMPARE(q.jobs[0].packId, a.packId);
        QCOMPARE(q.jobs[0].state, a.state);
    }

    void fileRoundTrip() {
        QTemporaryDir dir;
        Project p;
        p.name = "F";
        Job j;
        j.id = "j";
        j.name = "J";
        j.packId = "CA-2026";
        j.state = defaultState();
        p.jobs.append(j);
        const QString path = dir.filePath("x.jobeval");
        QString err;
        QVERIFY2(writeProjectFile(path, p, "project", err), qPrintable(err));
        Project q;
        QVERIFY2(readProjectFile(path, q, err), qPrintable(err));
        QCOMPARE(q.jobs[0].state, j.state);
    }

    void refusesNewerVersion() {
        Project q;
        QString err;
        QVERIFY(!projectFromJson(R"({"format":"jobeval","version":99,"jobs":[]})", q, err));
        QVERIFY(err.contains("newer"));
    }

    void refusesGarbage() {
        Project q;
        QString err;
        QVERIFY(!projectFromJson("not json", q, err));
        QVERIFY(!projectFromJson(R"({"format":"other","version":1})", q, err));
        QVERIFY(!projectFromJson(R"({"format":"jobeval","version":0})", q, err));
    }

    void toleratesUnknownFieldsAndClampsValues() {
        Project q;
        QString err;
        const QByteArray json = R"({"format":"jobeval","version":1,"future":true,"jobs":[
            {"name":"","region":"R","somethingNew":1,"inputs":{"pay":-5,"hours":9999,"weeks":"oops","unknownKey":3,"asof":"not-a-date"}}]})";
        QVERIFY2(projectFromJson(json, q, err), qPrintable(err));
        QCOMPARE(q.jobs.size(), 1);
        QVERIFY(!q.jobs[0].id.isEmpty());       // missing id is generated
        QVERIFY(!q.jobs[0].name.isEmpty());     // missing name gets a placeholder
        QCOMPARE(q.jobs[0].state["pay"].toDouble(), 0.0);       // clamped up to the minimum
        QCOMPARE(q.jobs[0].state["hours"].toDouble(), 168.0);   // clamped down to the maximum
        QCOMPARE(q.jobs[0].state["weeks"].toDouble(), 52.0);    // unparsable -> default
        QVERIFY(!q.jobs[0].state.contains("unknownKey"));
        QVERIFY(QDate::fromString(q.jobs[0].state["asof"].toString(), Qt::ISODate).isValid());
    }

    void engineInputsFollowState() {
        QVariantMap s = normalizedState({{"basis", 1}, {"pay", 52000}, {"hours", 40}, {"bonus", 1000}, {"otmode", 2}, {"otmult", 2}, {"otlegal", 0}, {"otthr", 42}});
        const jobeval::JobInputs j = inputsFromState(s);
        QVERIFY(j.basis == jobeval::PayBasis::Salary);
        QCOMPARE(QString::fromStdString(j.pay.to_string()), QString("52000"));
        QVERIFY(j.overtime == jobeval::OvertimeMode::Company);
        QVERIFY(!j.use_legal_threshold);
        QCOMPARE(QString::fromStdString(j.company_threshold_hours.to_string()), QString("42"));
        QCOMPARE(QString::fromStdString(j.bonus_annual.to_string()), QString("1000"));
    }
};

QTEST_APPLESS_MAIN(JobModelTest)
#include "jobmodel_test.moc"
