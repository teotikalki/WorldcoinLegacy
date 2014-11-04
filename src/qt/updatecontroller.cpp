#include "updatecontroller.h"
#include "qsettings.h"
#include "qprocess.h"
#include "clientversion.h"

namespace UpdateController_NS
{
    // Configuration file name
    static const QString CONFIG_FILE_NAME = "WorldcoinBC.cfg";

    // Default period for checking of update
    static const int DEFAULT_PERIOD = 6;

    // Update definition in configuration file
    static const QString CONFIG_UPDATE = "Update";

    // Period definition in configuration file
    static const QString CONFIG_PERIOD = "Period";

    // Update app name
#ifdef WIN32
    static const QString UPDATE_APP = "WorldcoinBC.exe";
#else
    static const QString UPDATE_APP = "WorldcoinBC";
#endif

    // Update input argument 
    static const QString UPDATE_ARG = "--check-updates";

    // Version log file name
    static const QString VERSION_LOG_FILE_NAME = "VersionLog.cfg";

    // Updrade input argument
    static const QString INSTALL_ARG = "--install";
}
using namespace UpdateController_NS;

UpdateController::UpdateController() : QObject()
, m_action(eNone)
, m_settingsConfig(0)
, m_settingsVersion(0)
, m_process(0)
{
    // Connect timer timeout with slot for checking of udpate
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(checkUpdate()));

    // Connect timer timeout with slot for checking of udpate
    connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onProcessFinish(int, QProcess::ExitStatus)));

    // Initialize objects from QSettings
    m_settingsConfig = new QSettings(CONFIG_FILE_NAME, QSettings::IniFormat);
    m_settingsVersion = new QSettings(VERSION_LOG_FILE_NAME, QSettings::IniFormat);

    m_process = new QProcess(this);

    // Get period from configuration file
    double nPeriod = getPeriodFromConfig();

    // Convert period in miliseconds (period is in hours)
    int nPeriodInMs = nPeriod * 3600 * 1000;

    // Set proper interval on timer and start it
    m_timer.setInterval(nPeriodInMs);
    m_timer.start();
}


UpdateController::~UpdateController()
{
    m_timer.stop();
}

double UpdateController::getPeriodFromConfig()
{
    double retPeriod = DEFAULT_PERIOD;
    if (m_settingsConfig != 0)
    {
        // Get period from Update group from config file
        m_settingsConfig->beginGroup(CONFIG_UPDATE);
        retPeriod = m_settingsConfig->value(CONFIG_PERIOD, DEFAULT_PERIOD).toDouble();
        m_settingsConfig->endGroup();
    }

    return retPeriod;
}

void UpdateController::checkUpdate()
{
    // Set current action to be check for update
    m_action = eCheckUpdate;
    // Check for update
    QStringList arguments;
    arguments << UPDATE_ARG;

    // Call process for checking of update
    runProcess(arguments);
}

void UpdateController::makeUpgrate()
{
    // Set current action to be install
    m_action = eInstall;
    // Start upgrade
    QStringList arguments;

    // Call process for installing
    arguments << INSTALL_ARG;
    runProcess(arguments);
}

void UpdateController::runProcess(const QStringList &arguments)
{
    // Start process with proper arguments
    if (m_process != 0)
        m_process->start(UPDATE_APP, arguments);
}
void UpdateController::onProcessFinish(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Get standard output
    QByteArray result = m_process->readAllStandardOutput();

    // Call method that is responsible for processing of this output
    processOutput(result);
}

void UpdateController::processOutput(const QByteArray& execOutput)
{
    QString strOutput = execOutput.data();

    // Check action and call proper method when process is finish
    switch (m_action)
    {
        case eCheckUpdate:
            parseVersion(strOutput);
            break;
        case eInstall:
            processInstall(strOutput);
            break;
        default:
            break;
    }
}
void UpdateController::parseVersion(const QString& receivedVersion)
{
    // Emit signal about type of update, will be catch in GUI
    UpdateController::eTypeUpdate updateType = eUpToDate;
    
    updateType = checkVersion(receivedVersion);
    
    if (updateType != eUpToDate)
        emit updateVersion((int)updateType);
}
void UpdateController::processInstall(const QString& receivedInstallResult)
{
    // Emit signal when install is finished, will be catch in GUI
    emit installFinish();
}
UpdateController::eTypeUpdate UpdateController::checkVersion(const QString& inVersion)
{
    // Received version: example "1.0.2|Crypto Punisher|4|2"
    QStringList listVersion = inVersion.split("|");

    // TODO: Get current version for app
    QString currentVersion = getCurrentVersion();
    if (compareVersions(listVersion[0].toUtf8().data(), currentVersion.toUtf8().data()) == 0)
    {
        // Return last element from list
        return (eTypeUpdate)(listVersion[3].toInt());
    }
    else
        return eUpToDate;
}
int UpdateController::compareVersions(char* firstVer, char* secondVer)
{
    int a1 = 0;
    int b1 = 0;
    int ret;

    int a = strlen(firstVer);
    int b = strlen(secondVer);
    if (b > a) 
        a = b;
    
    for (int i = 0; i < a; i++) 
    {
        a1 += firstVer[i];
        b1 += secondVer[i];
    }
    if (b1 > a1) 
        ret = 1; // second version is fresher
    else if (b1 == a1) 
        ret = -1; // versions is equal
    else 
        ret = 0; // first version is fresher

    return ret;
}
QString UpdateController::getCurrentVersion()
{
    QString currentVersion = "";
    currentVersion.sprintf("%d.%d.%d", CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_REVISION);
    return currentVersion;
}
void UpdateController::getVersions(QMap<QString, QMap<QString, QString>> &feauture, QStringList &listGroup)
{
    QMap<QString, QMap<QString, QString>> returnMap;
    QStringList groups = m_settingsVersion->childGroups();
    QStringList retGroups;

    // We need from latest to the oldest
    for (int i = groups.size() - 1; i >= 0; i--)
    { 
        m_settingsVersion->beginGroup(groups.at(i));
        retGroups.append(groups.at(i));
        QStringList groupKeys = m_settingsVersion->allKeys();
        QMap<QString, QString> mapGroupKeys;
        for (int j = 0; j < groupKeys.size(); j++)
        {
            mapGroupKeys[groupKeys.at(j)] = m_settingsVersion->value(groupKeys.at(j)).toString();
        }

        m_settingsVersion->endGroup();

        returnMap[groups.at(i)] = mapGroupKeys;
    }

    // Set outputs
    listGroup = retGroups;
    feauture = returnMap;
}
UpdateController::eTypeVersion UpdateController::getTypeVersion(const QString& version)
{
    QString currentVersion = getCurrentVersion();
    int resCompare = compareVersions(version.toUtf8().data(), currentVersion.toUtf8().data());
    if (resCompare == 0)
        return eNew;
    else if (resCompare == -1)
        return eCurrent;
    else
        return ePast;
}