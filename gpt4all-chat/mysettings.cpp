#include "mysettings.h"

#include "../gpt4all-backend/llmodel.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGlobalStatic>
#include <QIODevice>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QtLogging>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

namespace defaults {

static const int      threadCount             = std::min(4, (int32_t) std::thread::hardware_concurrency());
static const bool     saveChatsContext        = false;
static const bool     serverChat              = false;
static const QString  userDefaultModel        = "Application default";
static const bool     forceMetal              = false;
static const QString  lastVersionStarted      = "";
static const int      localDocsChunkSize      = 256;
static const QString  chatTheme               = "Dark";
static const QString  fontSize                = "Small";
static const int      localDocsRetrievalSize  = 3;
static const bool     localDocsShowReferences = true;
static const QString  networkAttribution      = "";
static const bool     networkIsActive         = false;
static const int      networkPort             = 4891;
static const bool     networkUsageStatsActive = false;
static const QString  device                  = "Auto";

} // namespace defaults

static QString defaultLocalModelsPath()
{
    QString localPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + "/";
    QString testWritePath = localPath + QString("test_write.txt");
    QString canonicalLocalPath = QFileInfo(localPath).canonicalFilePath() + "/";
    QDir localDir(localPath);
    if (!localDir.exists()) {
        if (!localDir.mkpath(localPath)) {
            qWarning() << "ERROR: Local download directory can't be created:" << canonicalLocalPath;
            return canonicalLocalPath;
        }
    }

    if (QFileInfo::exists(testWritePath))
        return canonicalLocalPath;

    QFile testWriteFile(testWritePath);
    if (testWriteFile.open(QIODeviceBase::ReadWrite)) {
        testWriteFile.close();
        return canonicalLocalPath;
    }

    qWarning() << "ERROR: Local download path appears not writeable:" << canonicalLocalPath;
    return canonicalLocalPath;
}

class MyPrivateSettings: public MySettings { };
Q_GLOBAL_STATIC(MyPrivateSettings, settingsInstance)
MySettings *MySettings::globalInstance()
{
    return settingsInstance();
}

MySettings::MySettings()
    : QObject{nullptr}
{
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QVector<QString> deviceList{ "Auto" };
#if defined(Q_OS_MAC) && defined(__aarch64__)
    deviceList << "Metal";
#else
    std::vector<LLModel::GPUDevice> devices = LLModel::Implementation::availableGPUDevices();
    for (LLModel::GPUDevice &d : devices)
        deviceList << QString::fromStdString(d.selectionName());
#endif
    deviceList << "CPU";
    setDeviceList(deviceList);
}

Q_INVOKABLE QVector<QString> MySettings::deviceList() const
{
    return m_deviceList;
}

void MySettings::setDeviceList(const QVector<QString> &deviceList)
{
    m_deviceList = deviceList;
    emit deviceListChanged();
}

void MySettings::restoreModelDefaults(const ModelInfo &model)
{
    setModelTemperature(model, model.m_temperature);
    setModelTopP(model, model.m_topP);
    setModelMinP(model, model.m_minP);
    setModelTopK(model, model.m_topK);;
    setModelMaxLength(model, model.m_maxLength);
    setModelPromptBatchSize(model, model.m_promptBatchSize);
    setModelContextLength(model, model.m_contextLength);
    setModelGpuLayers(model, model.m_gpuLayers);
    setModelRepeatPenalty(model, model.m_repeatPenalty);
    setModelRepeatPenaltyTokens(model, model.m_repeatPenaltyTokens);
    setModelPromptTemplate(model, model.m_promptTemplate);
    setModelSystemPrompt(model, model.m_systemPrompt);
}

void MySettings::restoreApplicationDefaults()
{
    setChatTheme(defaults::chatTheme);
    setFontSize(defaults::fontSize);
    setDevice(defaults::device);
    setThreadCount(defaults::threadCount);
    setSaveChatsContext(defaults::saveChatsContext);
    setServerChat(defaults::serverChat);
    setNetworkPort(defaults::networkPort);
    setModelPath(defaultLocalModelsPath());
    setUserDefaultModel(defaults::userDefaultModel);
    setForceMetal(defaults::forceMetal);
}

void MySettings::restoreLocalDocsDefaults()
{
    setLocalDocsChunkSize(defaults::localDocsChunkSize);
    setLocalDocsRetrievalSize(defaults::localDocsRetrievalSize);
    setLocalDocsShowReferences(defaults::localDocsShowReferences);
}

void MySettings::eraseModel(const ModelInfo &m)
{
    QSettings settings;
    settings.remove(QString("model-%1").arg(m.id()));
}

QString MySettings::modelName(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/name",
        !m.m_name.isEmpty() ? m.m_name : m.m_filename).toString();
}

void MySettings::setModelName(const ModelInfo &m, const QString &name, bool force)
{
    if ((modelName(m) == name || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if ((m.m_name == name || m.m_filename == name) && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/name");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/name", name);
    if (!force)
        emit nameChanged(m);
}

QString MySettings::modelFilename(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/filename", m.m_filename).toString();
}

void MySettings::setModelFilename(const ModelInfo &m, const QString &filename, bool force)
{
    if ((modelFilename(m) == filename || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_filename == filename && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/filename");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/filename", filename);
    if (!force)
        emit filenameChanged(m);
}

QString MySettings::modelDescription(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/description", m.m_description).toString();
}

void MySettings::setModelDescription(const ModelInfo &m, const QString &d, bool force)
{
    if ((modelDescription(m) == d || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_description == d && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/description");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/description", d);
}

QString MySettings::modelUrl(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/url", m.m_url).toString();
}

void MySettings::setModelUrl(const ModelInfo &m, const QString &u, bool force)
{
    if ((modelUrl(m) == u || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_url == u && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/url");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/url", u);
}

QString MySettings::modelQuant(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/quant", m.m_quant).toString();
}

void MySettings::setModelQuant(const ModelInfo &m, const QString &q, bool force)
{
    if ((modelUrl(m) == q || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_quant == q && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/quant");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/quant", q);
}

QString MySettings::modelType(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/type", m.m_type).toString();
}

void MySettings::setModelType(const ModelInfo &m, const QString &t, bool force)
{
    if ((modelType(m) == t || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_type == t && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/type");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/type", t);
}

bool MySettings::modelIsClone(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/isClone", m.m_isClone).toBool();
}

void MySettings::setModelIsClone(const ModelInfo &m, bool b, bool force)
{
    if ((modelIsClone(m) == b || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_isClone == b && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/isClone");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/isClone", b);
}

bool MySettings::modelIsDiscovered(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/isDiscovered", m.m_isDiscovered).toBool();
}

void MySettings::setModelIsDiscovered(const ModelInfo &m, bool b, bool force)
{
    if ((modelIsDiscovered(m) == b || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_isDiscovered == b && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/isDiscovered");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/isDiscovered", b);
}

int MySettings::modelLikes(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/likes", m.m_likes).toInt();
}

void MySettings::setModelLikes(const ModelInfo &m, int l, bool force)
{
    if ((modelLikes(m) == l || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_likes == l && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/likes");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/likes", l);
}

int MySettings::modelDownloads(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/downloads", m.m_downloads).toInt();
}

void MySettings::setModelDownloads(const ModelInfo &m, int d, bool force)
{
    if ((modelDownloads(m) == d || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_downloads == d && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/downloads");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/downloads", d);
}

QDateTime MySettings::modelRecency(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/recency", m.m_recency).toDateTime();
}

void MySettings::setModelRecency(const ModelInfo &m, const QDateTime &r, bool force)
{
    if ((modelRecency(m) == r || m.id().isEmpty()) && !force)
        return;

    QSettings setting;
    if (m.m_recency == r && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/recency");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/recency", r);
}

double MySettings::modelTemperature(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/temperature", m.m_temperature).toDouble();
}

void MySettings::setModelTemperature(const ModelInfo &m, double t, bool force)
{
    if (modelTemperature(m) == t && !force)
        return;

    QSettings setting;
    if (m.m_temperature == t && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/temperature");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/temperature", t);
    if (!force)
        emit temperatureChanged(m);
}

double MySettings::modelTopP(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/topP", m.m_topP).toDouble();
}

double MySettings::modelMinP(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/minP", m.m_minP).toDouble();
}

void MySettings::setModelTopP(const ModelInfo &m, double p, bool force)
{
    if (modelTopP(m) == p && !force)
        return;

    QSettings setting;
    if (m.m_topP == p && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/topP");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/topP", p);
    if (!force)
        emit topPChanged(m);
}

void MySettings::setModelMinP(const ModelInfo &m, double p, bool force)
{
    if (modelMinP(m) == p && !force)
        return;

    QSettings setting;
    if (m.m_minP == p && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/minP");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/minP", p);
    if (!force)
        emit minPChanged(m);
}

int MySettings::modelTopK(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/topK", m.m_topK).toInt();
}

void MySettings::setModelTopK(const ModelInfo &m, int k, bool force)
{
    if (modelTopK(m) == k && !force)
        return;

    QSettings setting;
    if (m.m_topK == k && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/topK");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/topK", k);
    if (!force)
        emit topKChanged(m);
}

int MySettings::modelMaxLength(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/maxLength", m.m_maxLength).toInt();
}

void MySettings::setModelMaxLength(const ModelInfo &m, int l, bool force)
{
    if (modelMaxLength(m) == l && !force)
        return;

    QSettings setting;
    if (m.m_maxLength == l && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/maxLength");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/maxLength", l);
    if (!force)
        emit maxLengthChanged(m);
}

int MySettings::modelPromptBatchSize(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/promptBatchSize", m.m_promptBatchSize).toInt();
}

void MySettings::setModelPromptBatchSize(const ModelInfo &m, int s, bool force)
{
    if (modelPromptBatchSize(m) == s && !force)
        return;

    QSettings setting;
    if (m.m_promptBatchSize == s && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/promptBatchSize");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/promptBatchSize", s);
    if (!force)
        emit promptBatchSizeChanged(m);
}

int MySettings::modelContextLength(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/contextLength", m.m_contextLength).toInt();
}

void MySettings::setModelContextLength(const ModelInfo &m, int l, bool force)
{
    if (modelContextLength(m) == l && !force)
        return;

    QSettings setting;
    if (m.m_contextLength == l && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/contextLength");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/contextLength", l);
    if (!force)
        emit contextLengthChanged(m);
}

int MySettings::modelGpuLayers(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/gpuLayers", m.m_gpuLayers).toInt();
}

void MySettings::setModelGpuLayers(const ModelInfo &m, int l, bool force)
{
    if (modelGpuLayers(m) == l && !force)
        return;

    QSettings setting;
    if (m.m_gpuLayers == l && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/gpuLayers");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/gpuLayers", l);
    if (!force)
        emit gpuLayersChanged(m);
}

double MySettings::modelRepeatPenalty(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/repeatPenalty", m.m_repeatPenalty).toDouble();
}

void MySettings::setModelRepeatPenalty(const ModelInfo &m, double p, bool force)
{
    if (modelRepeatPenalty(m) == p && !force)
        return;

    QSettings setting;
    if (m.m_repeatPenalty == p && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/repeatPenalty");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/repeatPenalty", p);
    if (!force)
        emit repeatPenaltyChanged(m);
}

int MySettings::modelRepeatPenaltyTokens(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/repeatPenaltyTokens", m.m_repeatPenaltyTokens).toInt();
}

void MySettings::setModelRepeatPenaltyTokens(const ModelInfo &m, int t, bool force)
{
    if (modelRepeatPenaltyTokens(m) == t && !force)
        return;

    QSettings setting;
    if (m.m_repeatPenaltyTokens == t && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/repeatPenaltyTokens");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/repeatPenaltyTokens", t);
    if (!force)
        emit repeatPenaltyTokensChanged(m);
}

QString MySettings::modelPromptTemplate(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/promptTemplate", m.m_promptTemplate).toString();
}

void MySettings::setModelPromptTemplate(const ModelInfo &m, const QString &t, bool force)
{
    if (modelPromptTemplate(m) == t && !force)
        return;

    QSettings setting;
    if (m.m_promptTemplate == t && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/promptTemplate");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/promptTemplate", t);
    if (!force)
        emit promptTemplateChanged(m);
}

QString MySettings::modelSystemPrompt(const ModelInfo &m) const
{
    QSettings setting;
    return setting.value(QString("model-%1").arg(m.id()) + "/systemPrompt", m.m_systemPrompt).toString();
}

void MySettings::setModelSystemPrompt(const ModelInfo &m, const QString &p, bool force)
{
    if (modelSystemPrompt(m) == p && !force)
        return;

    QSettings setting;
    if (m.m_systemPrompt == p && !m.shouldSaveMetadata())
        setting.remove(QString("model-%1").arg(m.id()) + "/systemPrompt");
    else
        setting.setValue(QString("model-%1").arg(m.id()) + "/systemPrompt", p);
    if (!force)
        emit systemPromptChanged(m);
}

int MySettings::threadCount() const
{
    QSettings setting;
    int c = setting.value("threadCount", defaults::threadCount).toInt();
    // The old thread setting likely left many people with 0 in settings config file, which means
    // we should reset it to the default going forward
    if (c <= 0)
        c = defaults::threadCount;
    c = std::max(c, 1);
    c = std::min(c, QThread::idealThreadCount());
    return c;
}

void MySettings::setThreadCount(int c)
{
    if (threadCount() == c)
        return;

    c = std::max(c, 1);
    c = std::min(c, QThread::idealThreadCount());
    QSettings setting;
    setting.setValue("threadCount", c);
    emit threadCountChanged();
}

bool MySettings::saveChatsContext() const
{
    QSettings setting;
    return setting.value("saveChatsContext", defaults::saveChatsContext).toBool();
}

void MySettings::setSaveChatsContext(bool b)
{
    if (saveChatsContext() == b)
        return;

    QSettings setting;
    setting.setValue("saveChatsContext", b);
    emit saveChatsContextChanged();
}

bool MySettings::serverChat() const
{
    QSettings setting;
    return setting.value("serverChat", defaults::serverChat).toBool();
}

void MySettings::setServerChat(bool b)
{
    if (serverChat() == b)
        return;

    QSettings setting;
    setting.setValue("serverChat", b);
    emit serverChatChanged();
}

int MySettings::networkPort() const
{
    QSettings setting;
    return setting.value("networkPort", defaults::networkPort).toInt();
}

void MySettings::setNetworkPort(int c)
{
    if (networkPort() == c)
        return;

    QSettings setting;
    setting.setValue("networkPort", c);
    emit networkPortChanged();
}

QString MySettings::modelPath() const
{
    QSettings setting;
    // We have to migrate the old setting because I changed the setting key recklessly in v2.4.11
    // which broke a lot of existing installs
    const bool containsOldSetting = setting.contains("modelPaths");
    if (containsOldSetting) {
        const bool containsNewSetting = setting.contains("modelPath");
        if (!containsNewSetting)
            setting.setValue("modelPath", setting.value("modelPaths"));
        setting.remove("modelPaths");
    }
    return setting.value("modelPath", defaultLocalModelsPath()).toString();
}

void MySettings::setModelPath(const QString &p)
{
    QString filePath = (p.startsWith("file://") ?
                        QUrl(p).toLocalFile() : p);
    QString canonical = QFileInfo(filePath).canonicalFilePath() + "/";
    if (modelPath() == canonical)
        return;
    QSettings setting;
    setting.setValue("modelPath", canonical);
    emit modelPathChanged();
}

QString MySettings::userDefaultModel() const
{
    QSettings setting;
    return setting.value("userDefaultModel", defaults::userDefaultModel).toString();
}

void MySettings::setUserDefaultModel(const QString &u)
{
    if (userDefaultModel() == u)
        return;

    QSettings setting;
    setting.setValue("userDefaultModel", u);
    emit userDefaultModelChanged();
}

QString MySettings::chatTheme() const
{
    QSettings setting;
    return setting.value("chatTheme", defaults::chatTheme).toString();
}

void MySettings::setChatTheme(const QString &u)
{
    if (chatTheme() == u)
        return;

    QSettings setting;
    setting.setValue("chatTheme", u);
    emit chatThemeChanged();
}

QString MySettings::fontSize() const
{
    QSettings setting;
    return setting.value("fontSize", defaults::fontSize).toString();
}

void MySettings::setFontSize(const QString &u)
{
    if (fontSize() == u)
        return;

    QSettings setting;
    setting.setValue("fontSize", u);
    emit fontSizeChanged();
}

QString MySettings::device() const
{
    QSettings setting;
    auto value = setting.value("device");
    if (!value.isValid())
        return defaults::device;

    auto device = value.toString();
    if (!device.isEmpty()) {
        auto deviceStr = device.toStdString();
        auto newNameStr = LLModel::GPUDevice::updateSelectionName(deviceStr);
        if (newNameStr != deviceStr) {
            auto newName = QString::fromStdString(newNameStr);
            qWarning() << "updating device name:" << device << "->" << newName;
            device = newName;
            setting.setValue("device", device);
        }
    }
    return device;
}

void MySettings::setDevice(const QString &u)
{
    if (device() == u)
        return;

    QSettings setting;
    setting.setValue("device", u);
    emit deviceChanged();
}

bool MySettings::forceMetal() const
{
    return m_forceMetal;
}

void MySettings::setForceMetal(bool b)
{
    if (m_forceMetal == b)
        return;
    m_forceMetal = b;
    emit forceMetalChanged(b);
}

QString MySettings::lastVersionStarted() const
{
    QSettings setting;
    return setting.value("lastVersionStarted", defaults::lastVersionStarted).toString();
}

void MySettings::setLastVersionStarted(const QString &v)
{
    if (lastVersionStarted() == v)
        return;

    QSettings setting;
    setting.setValue("lastVersionStarted", v);
    emit lastVersionStartedChanged();
}

int MySettings::localDocsChunkSize() const
{
    QSettings setting;
    return setting.value("localdocs/chunkSize", defaults::localDocsChunkSize).toInt();
}

void MySettings::setLocalDocsChunkSize(int s)
{
    if (localDocsChunkSize() == s)
        return;

    QSettings setting;
    setting.setValue("localdocs/chunkSize", s);
    emit localDocsChunkSizeChanged();
}

int MySettings::localDocsRetrievalSize() const
{
    QSettings setting;
    return setting.value("localdocs/retrievalSize", defaults::localDocsRetrievalSize).toInt();
}

void MySettings::setLocalDocsRetrievalSize(int s)
{
    if (localDocsRetrievalSize() == s)
        return;

    QSettings setting;
    setting.setValue("localdocs/retrievalSize", s);
    emit localDocsRetrievalSizeChanged();
}

bool MySettings::localDocsShowReferences() const
{
    QSettings setting;
    return setting.value("localdocs/showReferences", defaults::localDocsShowReferences).toBool();
}

void MySettings::setLocalDocsShowReferences(bool b)
{
    if (localDocsShowReferences() == b)
        return;

    QSettings setting;
    setting.setValue("localdocs/showReferences", b);
    emit localDocsShowReferencesChanged();
}

QString MySettings::networkAttribution() const
{
    QSettings setting;
    return setting.value("network/attribution", defaults::networkAttribution).toString();
}

void MySettings::setNetworkAttribution(const QString &a)
{
    if (networkAttribution() == a)
        return;

    QSettings setting;
    setting.setValue("network/attribution", a);
    emit networkAttributionChanged();
}

bool MySettings::networkIsActive() const
{
    QSettings setting;
    return setting.value("network/isActive", defaults::networkIsActive).toBool();
}

bool MySettings::isNetworkIsActiveSet() const
{
    QSettings setting;
    return setting.value("network/isActive").isValid();
}

void MySettings::setNetworkIsActive(bool b)
{
    QSettings setting;
    auto cur = setting.value("network/isActive");
    if (!cur.isValid() || cur.toBool() != b) {
        setting.setValue("network/isActive", b);
        emit networkIsActiveChanged();
    }
}

bool MySettings::networkUsageStatsActive() const
{
    QSettings setting;
    return setting.value("network/usageStatsActive", defaults::networkUsageStatsActive).toBool();
}

bool MySettings::isNetworkUsageStatsActiveSet() const
{
    QSettings setting;
    return setting.value("network/usageStatsActive").isValid();
}

void MySettings::setNetworkUsageStatsActive(bool b)
{
    QSettings setting;
    auto cur = setting.value("network/usageStatsActive");
    if (!cur.isValid() || cur.toBool() != b) {
        setting.setValue("network/usageStatsActive", b);
        emit networkUsageStatsActiveChanged();
    }
}
