#include "Kernel.hpp"

#include "Common.hpp"

TrojanKernelThread *TrojanKernelThread::self = nullptr;

void TrojanPluginKernelLogger(const std::string &str, Log::Level)
{
    TrojanKernelThread::self->OnKernelLogAvaliable_s(QString::fromStdString(str));
}

TrojanKernel::TrojanKernel(QObject *parent) : Qv2rayPlugin::QvPluginKernel(parent), thread(this)
{
    connect(&thread, &TrojanKernelThread::OnKernelCrashed_s, this, &TrojanKernel::OnKernelCrashed);
    connect(&thread, &TrojanKernelThread::OnKernelLogAvaliable_s, this, &TrojanKernel::OnKernelLogAvaliable);
    connect(&thread, &TrojanKernelThread::OnKernelStatsAvailable_s, this, &TrojanKernel::OnKernelStatsAvailable);
}

bool TrojanKernel::StartKernel()
{
    if (hasHttpConfigured)
    {
        httpHelper.httpListen(QHostAddress(httpListenAddress), httpPort, socksPort);
    }
    thread.start();
    return true;
}
bool TrojanKernel::StopKernel()
{
    if (hasHttpConfigured)
    {
        httpHelper.close();
    }
    thread.stop();
    return true;
}
void TrojanKernel::SetConnectionSettings(const QString &listenAddress, const QMap<QString, int> &inbound, const QJsonObject &settings)
{
    hasHttpConfigured = inbound.contains("http");
    hasSocksConfigured = inbound.contains("socks");
    //
    httpPort = inbound["http"];
    socksPort = inbound["socks"];
    httpListenAddress = listenAddress;
    //
    Config config;
    TrojanObject o;
    o.loadJson(settings);
    config.run_type = Config::CLIENT;
    //
    config.password[Config::SHA224(o.password.toStdString())] = o.password.toStdString();
    config.remote_addr = o.address.toStdString();
    config.remote_port = o.port;
    config.ssl.sni = o.sni.toStdString();
    config.ssl.verify = !o.ignoreCertificate;
    config.ssl.verify_hostname = !o.ignoreHostname;
    config.ssl.reuse_session = o.reuseSession;
    config.ssl.session_ticket = o.sessionTicket;
    config.tcp.reuse_port = o.reusePort;
    config.tcp.fast_open = o.tcpFastOpen;
    //
    config.local_addr = listenAddress.toStdString();
    config.local_port = socksPort;
    thread.SetConfig(config);
}

const QList<Qv2rayPlugin::QvPluginOutboundProtocolObject> TrojanKernel::KernelOutboundCapabilities() const
{
    return { { "Trojan", "trojan" } };
}

TrojanKernel::~TrojanKernel()
{
    thread.stop();
}

// ================================================== Kernel Thread ==================================================

void TrojanKernelThread::stop()
{
    if (isRunning() && service)
    {
        service->stop();
        wait();
        service.reset();
    }
}

TrojanKernelThread::~TrojanKernelThread()
{
}

void TrojanKernelThread::run()
{
    try
    {
        service = std::make_unique<Service>(config);
        Log::level = Log::Level::INFO;
        Log::logger = TrojanPluginKernelLogger;
        service->run();
    }
    catch (const std::exception &e)
    {
        // Log::log_with_date_time(std::string("fatal: ") + e.what(), Log::FATAL);
        emit OnKernelCrashed_s(QString(e.what()));
        stop();
    }
}
