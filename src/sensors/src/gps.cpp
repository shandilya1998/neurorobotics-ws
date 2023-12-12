#include "sensors/gps.h"

UDPCommunication::UDPCommunication(rclcpp::Logger logger, std::function<void(FlightLogData)> cb): LOGGER(logger), originalCallback(cb){

	stopAcquireFlag = false;
	// Create Socket
	socketReceive = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketReceive == 0)
	{
		RCLCPP_INFO(LOGGER, "Receive Socket creation failed");
	}

	int broadcast = 1;
	int reuseaddr = 1;
	int reuseport = 1;
	unsigned long int	noBlock = 1;

	int result = ioctl(socketReceive, FIONBIO, &noBlock);
	if (result == -1) {
		RCLCPP_ERROR(LOGGER, "Error Occured while initializing sockets");
	}

	// Initialise Socket Address
	memset((char *)&receiveAddress, 0, sizeof(receiveAddress));
	receiveAddress.sin_family = AF_INET;
	receiveAddress.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr(IP_ADDR);
	receiveAddress.sin_port = htons(RECEIVE_PORT);
	addressLength = sizeof(receiveAddress);

	// Bind Socket to Address
	if (bind(socketReceive, (struct sockaddr *) &receiveAddress, sizeof(receiveAddress)) < 0) 
	{
		RCLCPP_ERROR(LOGGER, "Socket Bind Error - socket_receive ");
	}

	// Initialise Current UAV Data Cache
	currentUavData.groundSpeed = 0.0;
    currentUavData.altitudeAGL=0;
    currentUavData.altitudeMSL=0;
    currentUavData.latitude=0;
    currentUavData.longitude=0;
    currentUavData.mode = 0;
    currentUavData.date = 0;
    currentUavData.time = 0;
	currentUavData.Az = 0;
	currentUavData.Ay = 0;
	currentUavData.Ax = 0;
	currentUavData.Bz = 0;
	currentUavData.By = 0;
	currentUavData.Bx = 0;
	currentUavData.R = 0;
	currentUavData.Q = 0;
	currentUavData.P = 0;
	currentUavData.dRoll = 0;
	currentUavData.dPitch = 0;
	currentUavData.dYaw = 0;
	currentUavData.float_dYaw = 0.0;  
    currentUavData.float_dPitch = 0.0;
    currentUavData.float_dRoll = 0.0;

	callback = [this](FlightLogData data){
		RCLCPP_DEBUG(LOGGER, "Inside UDPCommunication callback");
		originalCallback(data);
	};

}

int UDPCommunication::start(){
	RCLCPP_INFO(LOGGER, "Starting Comm Thread");
	UDPCommunicationThread = new std::thread(&UDPCommunication::AcquireComData, this);
	usleep(1000);
	return 0;
}

void UDPCommunication::AcquireComData()
{
	std::chrono::system_clock::time_point ref_time = std::chrono::system_clock::now();   
	std::chrono::system_clock::time_point time_now = std::chrono::system_clock::now();
	int64_t elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(time_now - ref_time).count();

	bool noCommunicationFromLima = false;
    
	while (!stopAcquireFlag)
	{
		//std::cout << "Hello I'm Running in Client Code" << std::endl;  //commented 15.07.20
		time_now = std::chrono::system_clock::now();
		elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(time_now - ref_time).count();

		if(elapsed_time > communicationFailureTimeoutInSeconds && !noCommunicationFromLima){
			noCommunicationFromLima = true;
			std::lock_guard<std::mutex> lock(flightDataMutex);
			currentUavData.sensorData[0].hdop = 11.1;//do not trust UAV data
			currentUavData.sensorData[1].hdop = 11.1;					
		}
		
		int returnCode = recvfrom(socketReceive, (char*)RXPacket, sizeof(RXPacket), 0, (struct sockaddr *) &receiveAddress, (socklen_t*)&addressLength);
		RCLCPP_DEBUG(LOGGER, "Received packet of size %d from socket", returnCode);
		if (returnCode >= 0)
		{	
			memcpy(&receiveFromIPAddress, &receiveAddress.sin_addr.s_addr, 4);						
			RCLCPP_DEBUG(LOGGER, "Recieved from this IP address = %s  port = %d)", inet_ntoa(receiveFromIPAddress), ntohs(receiveAddress.sin_port));

			if (telemetryValidator.isTransmittedPacketValid(RXPacket, returnCode, flagExecuteRTH, flagExecuteALT, flagExecuteTIME))
			{				
				RCLCPP_DEBUG(LOGGER, "Received valid packed");
				{
					std::lock_guard<std::mutex> lock(flightDataMutex);
					telemetryValidator.getCurrentUavData(currentUavData);	
					// Callback Call Here	
					callback(currentUavData);				
				}
				noCommunicationFromLima = false;
				ref_time = std::chrono::system_clock::now();
			}			
		}
		else{
			RCLCPP_ERROR(LOGGER, "No Data Received");
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5));		
	}
	stopAcquireFlag = false;
}

UDPCommunication::~UDPCommunication()
{
	this->CloseCommunication();
}

void UDPCommunication::CloseCommunication(){
	stopAcquireFlag = true;
	if(UDPCommunicationThread!= NULL){
		if(UDPCommunicationThread->joinable()){
			UDPCommunicationThread->join();
		}

	}
	stopAcquireFlag = false;
	delete UDPCommunicationThread;
	UDPCommunicationThread = NULL;
}

void UDPCommunication::testReceiver(std::string gpsDataFilePath){
	std::cout<<gpsDataFilePath<<std::endl;
	RCLCPP_INFO(LOGGER, "Getting GPS data from file: " + gpsDataFilePath);
	socketTransmit = socket(AF_INET, SOCK_DGRAM, 0);
	if(socketTransmit == 0){
		RCLCPP_ERROR(LOGGER, "Error Creating Transmit socket");
	}
	unsigned long int	noBlock = 1;
	int result = ioctl(socketReceive, FIONBIO, &noBlock);
	if (result == -1) {
		RCLCPP_ERROR(LOGGER, "Error Occured while initializing sockets");
	}

	// Initialise Transmit Socket Address
	memset((char *)&transmitAddress, 0, sizeof(transmitAddress));
	transmitAddress.sin_family = AF_INET;
	transmitAddress.sin_addr.s_addr = htonl(INADDR_ANY); //inet_addr(IP_ADDR);
	transmitAddress.sin_port = htons(RECEIVE_PORT);
	transmitAddressLength = sizeof(transmitAddress);
	// Bind Socket to Address
	if (bind(socketTransmit, (struct sockaddr *) &transmitAddress, sizeof(transmitAddress)) < 0) 
	{
		RCLCPP_ERROR(LOGGER, "Socket Bind Error - socket_receive ");
	}

	std::thread* th = new std::thread(&UDPCommunication::testThreadFunc, this, gpsDataFilePath);
	th->join();
	RCLCPP_INFO(LOGGER, "Test Done");
}

void UDPCommunication::testThreadFunc(std::string filePath){
	fstream gpsFile;
	std::string line;
	gpsFile.open(filePath, ios::in);
	if(gpsFile.is_open()){
		RCLCPP_INFO(LOGGER, "File Open, Starting Transmission");
	}
	else{
		RCLCPP_ERROR(LOGGER, "Unable to open file");
	}
	while(getline(gpsFile, line)){
		RCLCPP_DEBUG(LOGGER, "Line Read: " + line);
		unsigned char char_arr[1024];
		std::copy(line.begin(), line.end(), char_arr);
		sendto(socketTransmit, (char*)char_arr, sizeof(char_arr), 0, (struct sockaddr *) &transmitAddress, (socklen_t)transmitAddressLength);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));		
	}
}

SimpleGpsPublisher::SimpleGpsPublisher(std::string nodeName): rclcpp::Node(nodeName){
	RCLCPP_INFO(this->get_logger(), "Creating Simple GPS Publisher");		
	setup();
}

void SimpleGpsPublisher::setup(){
	std::function<void(FlightLogData)> callbackFunction = [this](FlightLogData data){
		gpsCallback(data);
	};
	comm = new UDPCommunication(this->get_logger(), callbackFunction);
	comm->start();
	std::string p = "/ws/SkyNet/Flight Data/23 June 2023/FLIGHT 2/Log/2023_6_15_10_53_37/day_gps.txt";
	comm->testReceiver(p);
}

void 
SimpleGpsPublisher::gpsCallback(FlightLogData data){
	RCLCPP_DEBUG(this->get_logger(), "Inside Subscriber Callback");
	RCLCPP_DEBUG(this->get_logger(), "latitude: %f", data.latitude);
	RCLCPP_DEBUG(this->get_logger(), "longitude: %f", data.longitude);
}
