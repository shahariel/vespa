// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/fnet/transport.h>
#include <vespa/fnet/signalshutdown.h>
#include <vespa/fnet/simplepacketstreamer.h>
#include <vespa/fnet/channel.h>
#include <vespa/fnet/iserveradapter.h>
#include <vespa/fnet/connector.h>
#include <examples/ping/packets.h>
#include <vespa/fastos/app.h>

#include <vespa/log/log.h>
LOG_SETUP("pingserver");

class PingServer : public FNET_IServerAdapter,
                   public FNET_IPacketHandler,
                   public FastOS_Application
{
public:
    bool InitAdminChannel(FNET_Channel *) override { return false; }
    bool InitChannel(FNET_Channel *channel, uint32_t) override {
        channel->SetContext(FNET_Context(channel));
        channel->SetHandler(this);
        return true;
    }
    
    HP_RetCode HandlePacket(FNET_Packet *packet, FNET_Context context) override {
        if (packet->GetPCODE() == PCODE_PING_REQUEST) {
            fprintf(stderr, "Got ping request, sending ping reply\n");
            context._value.CHANNEL->Send(new PingReply());
        }
        packet->Free();
        return FNET_FREE_CHANNEL;
    }
    
    int Main() override;
};


int
PingServer::Main()
{
    FNET_SignalShutDown::hookSignals();
    if (_argc < 2) {
        printf("usage  : pingserver <listenspec>\n");
        printf("example: pingserver 'tcp/8000'\n");
        return 1;
    }

    FNET_Transport             transport;
    PingPacketFactory          factory;
    FNET_SimplePacketStreamer  streamer(&factory);
    FNET_Connector *listener =
        transport.Listen(_argv[1], &streamer, this);
    if (listener != nullptr)
        listener->SubRef();

    FNET_SignalShutDown ssd(transport);
    transport.Main();
    return 0;
}


int
main(int argc, char **argv)
{
    PingServer myapp;
    return myapp.Entry(argc, argv);
}
