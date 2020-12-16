#include "ns3/command-line.h"
#include "ns3/core-module.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-helper.h"
#include "ns3/wave-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

uint32_t g_receivedPackets = 0;
uint32_t g_droppedPackets = 0;
uint32_t g_samples;
double   g_averageSignal;
double   g_averageNoise;

NS_LOG_COMPONENT_DEFINE ("wave-packet-loss");

void MonitorSniffRx (Ptr<const Packet>, uint16_t channelFreqMhz, WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise)
{

}

void Rx (std::string context, Ptr <const Packet> packet, uint16_t channelFreqMhz,  WifiTxVector txVector,MpduInfo aMpdu, SignalNoiseDbm signalNoise)
{
	g_samples++;
	g_averageSignal += ((signalNoise.signal - g_averageSignal) / g_samples);
	g_averageNoise += ((signalNoise.noise - g_averageNoise) / g_samples);
        g_receivedPackets++;
        std::cout << "Packet Received! #" << g_receivedPackets << std::endl;

}

void RxDrop (std::string context, Ptr<const Packet> packet)
{
        g_droppedPackets++;
        std::cout << "Packet Dropped! #" << g_droppedPackets << " , " << std::endl;
}


int main(int argc, char *argv[])
{
	//Take in arguments for nodes and simulation time
	CommandLine cmd;
	uint32_t nNodes = 2;
	double simTime = 20.0;
	double rss = 0;
	double distance = 0;
	cmd.AddValue("n", "Number of nodes", nNodes);
	cmd.AddValue("t", "Simulation Time", simTime);
	cmd.AddValue("r", "Rss", rss);
	cmd.AddValue("d", "Distance", distance);
	cmd.Parse(argc, argv);
	//Create nodes 
	NodeContainer nodes;
	nodes.Create(nNodes);
	
	//Add mobility!
	MobilityHelper mobility;
	mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
	mobility.Install(nodes);
	Ptr<ConstantVelocityMobilityModel> cvmm;
	//Random location and velocity
	for(uint32_t i=0; i<nodes.GetN(); i++)
	{
		cvmm = nodes.Get(i) -> GetObject<ConstantVelocityMobilityModel>();
		
		if(i == 0)
		{
			cvmm->SetVelocity(Vector(0,0,0));
			cvmm->SetPosition(Vector(150+i*2, 50+(i%2)*2, 0));
		}
		else
		{
			if(i%4 ==1)
			{
				cvmm->SetPosition(Vector(150-i*2, 65-(i%2)*2, 0));
				cvmm->SetVelocity(Vector(12, 0, 0));
			}
			else if(i%4 == 2)
			{
				cvmm->SetPosition(Vector(150+i*2, 65+(i%2)*2, 0));
				cvmm->SetVelocity(Vector(-2, 0, 0));
			} else if(i%4 == 3)
			{
				cvmm->SetPosition(Vector(140-(i%2)*2, 65-i*2, 0));
				cvmm->SetVelocity(Vector(0,8,0));
			}
			else
			{
				cvmm->SetPosition(Vector(160+(i%2)*2, 65+i*2, 0));
				cvmm->SetVelocity(Vector(0,-8,0));
			}
		}
	}
	
	// Physical layer setup
	YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
	waveChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
        waveChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(-40-rss) );
        waveChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange",DoubleValue(250));
        YansWifiPhyHelper wavePhy = YansWavePhyHelper::Default();
	wavePhy.SetChannel(waveChannel.Create());
	wavePhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
	
	wavePhy.Set("TxPowerStart", DoubleValue(15));
	wavePhy.Set("TxPowerEnd", DoubleValue(15));
	
	
	QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
	WaveHelper waveHelper=WaveHelper::Default();
	
	waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
  						"DataMode", StringValue ("OfdmRate27MbpsBW10MHz"),
  						"ControlMode",StringValue ("OfdmRate27MbpsBW10MHz"),
  						"NonUnicastMode", StringValue ("OfdmRate27MbpsBW10MHz"));


	NetDeviceContainer devices = waveHelper.Install (wavePhy, waveMac, nodes);
 	//wavePhy.EnablePcap ("WavePackets", devices);

  	Ptr <Packet> packet = Create <Packet> (200);

  
  	Mac48Address dest = Mac48Address::GetBroadcast();
  	uint16_t protocol = 0x88dc;

  	//We can also set the transmission parameters at the higher layeres
  	TxInfo tx;
  	tx.preamble = WIFI_PREAMBLE_LONG;
  	tx.channelNumber = CCH;
  	tx.dataRate = WifiMode ("OfdmRate12MbpsBW10MHz");
  	tx.priority = 7;
	Ptr<WaveNetDevice> wdi;
  	Ptr<NetDevice> di;
    	
    	for(uint32_t i=0; i < (simTime-1)*20; i++)
  	{

  		uint32_t s = i%(devices.GetN());
  	        di = devices.Get(s);
  	        wdi = DynamicCast <WaveNetDevice>(di);
  	        //Ptr<WaveNetDevice> wdi = di;
  	        double time = i * 0.05;
		Simulator::Schedule(Seconds(time), &WaveNetDevice::SendX, wdi, packet, dest, protocol, tx);
	}
	
	Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/MonitorSnifferRx", MakeCallback (&Rx) );

	Config::Connect("/NodeList/*/DeviceList/*/$ns3::WaveNetDevice/PhyEntities/*/PhyRxDrop", MakeCallback (&RxDrop) );

        Config::Connect("/NodeList/0/DeviceList/*/Phy/MonitorSnifferRx", MakeCallback (&MonitorSniffRx));

	Simulator::Stop(Seconds(simTime));
	AnimationInterface anim("wave-packet-loss.xml");
	Simulator::Run();
	Simulator::Destroy();
	double signal = g_averageSignal;
	double noise = g_averageNoise;
	double SNR = g_averageSignal - g_averageNoise;
	std::cout << "Dropped: " << g_droppedPackets << ", Received: "<< g_receivedPackets << ", Noise: " << noise << ", Signal: " << signal << ", SNR: " << SNR << std::endl;
}
