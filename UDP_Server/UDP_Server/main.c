//#include <iostream>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

#define _USE_MATH_DEFINES
#include <math.h>


#pragma comment (lib, "ws2_32.lib")


#define MSG_CAR_STATE_REQ (0x01)
#define MSG_CAR_STATE     (0x02)
#define MSG_ENGAGE_REQ    (0x03)
#define MSG_SET_REFERENCE (0x04)


struct InternalValues{
	bool engaged;
	int16_t actualSpeed;      // in cm/s
	double previousSWA;
	double actualSWA;         // in deg
	int16_t targetSpeed;      // in cm/s
	double targetSWA;         // in deg
	double sineShift;
};


struct sockaddr_in serverHint;

int16_t uint8ToInt16(int8_t firstValue, int8_t secondValue)
{
	int16_t retVal = (((int16_t)firstValue << 8) && (0xFF00)) + (int16_t)secondValue;
	return retVal;
}

int16_t kmPerHourToCmPerSec(int16_t kmPerh)
{
	return (kmPerh * 1000 / 36);
}

int16_t DegreeToMilliRad(double degree)
{
	return (int16_t)(degree*1000 * (M_PI / 180.0));
}

double MilliRadToDegree(double Radian)
{
	return (double)(Radian * (180.0 / M_PI));
}

void calculateSineShift(struct InternalValues* intValues)
{
	if (0.0 != (*intValues).actualSWA)
	{
		(*intValues).sineShift = asin((*intValues).actualSWA / 60.0); // 60 is the amplitude.
	}
}


/*
 This function is check a message type and handle it.
*/
void MessageHandler(struct InternalValues* intValues, SOCKET in, char message[], int len, struct sockaddr_in client)
{
	if (MSG_CAR_STATE_REQ == (uint8_t)message[0]) // If Server got CarStateReq message.
	{
		char returnMessage[7];
		int16_t aSWA = DegreeToMilliRad((*intValues).actualSWA);
		//struct sockaddr_in to;
		//memset(&to, 0, sizeof(to));

		returnMessage[0] = (char)MSG_CAR_STATE;
		returnMessage[1] = message[1]; //sequenceValue
		returnMessage[2] = (char)(*intValues).engaged;
		returnMessage[3] = (char)(((*intValues).actualSpeed >> 8) & 0xFF); 
		returnMessage[4] = (char)((*intValues).actualSpeed & 0xFF);
		returnMessage[5] = (char)((aSWA >> 8) & 0xFF);
		returnMessage[6] = (char) (aSWA & 0xFF);

		printf("actualSWA: %f aSWA: %d\n", (*intValues).actualSWA, aSWA);

		//int sendOk = sendto(in, returnMessage, 7, 0, (struct sockaddr*)&to, sizeof(to));
		int sendOk = sendto(in, returnMessage, 7, 0, (struct sockaddr*)&client, sizeof(client));
		if (sendOk == SOCKET_ERROR)
		{
			printf("That didn't work! %d \n", WSAGetLastError());
		}
	}
	else if (MSG_ENGAGE_REQ == (uint8_t)message[0]) // If Server got EngageReq message.
	{
		printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
		(*intValues).engaged = (bool)message[2];

		if (false == (bool)message[2]) // When not self-driving, the speed at the moment of disengagement shall be maintained.
		{
			(*intValues).targetSpeed = (*intValues).actualSpeed;

			calculateSineShift(intValues);

		}
	}
	else if (MSG_SET_REFERENCE == (uint8_t)message[0]) // If Server got SetReference message.
	{
		if (true == (*intValues).engaged)
		{
			(*intValues).targetSpeed = uint8ToInt16((int8_t)message[2], (int8_t)message[3]);
			(*intValues).targetSWA = uint8ToInt16((int8_t)message[4], (int8_t)message[5]);
		}
	}
}

int8_t setTimeoutSocket(SOCKET in)
{
	fd_set fds;
	int n;
	struct timeval tv;

	// Set up the file descriptor set.
	FD_ZERO(&fds);
	FD_SET(in, &fds);

	// Set up the struct timeval for the timeout.
	tv.tv_sec = 0;
	tv.tv_usec = 1;

	// Wait until timeout or data received.
	n = select(in, &fds, NULL, NULL, &tv);
	if (n == 0)
	{
		return 0;
	}
	else if (n == -1)
	{
		return 1;
	}
	return 1;
}

/*
This funtion checks all the received message, what a server got in a 10ms period time. 
*/
void CheckValidMessages(struct InternalValues* intValues, SOCKET in, struct sockaddr_in client, int clientLength)
{
	char buf[1024];
	int bytesIn = 0;
	int validTimeout;
	do {						// If more than one message is received from the client in a loop time.
		validTimeout = setTimeoutSocket(in);
		if (0 < validTimeout)
		{

			ZeroMemory(buf, 1024);
			bytesIn = recvfrom(in, buf, 1024, 0, (struct sockaddr*)&client, &clientLength);
			if (bytesIn == SOCKET_ERROR)
			{
				printf("Error receiving from client  %d\n", WSAGetLastError());
				continue;
			}
			if (0 < bytesIn)		// If recvfrom() did not receive a message the return value is zero. If bigger than zero the return value is the message byte number. 
			{
				printf("Message received: %d %d\n", (uint8_t)buf[0], (uint8_t)buf[1]);
				MessageHandler(intValues, in, buf, bytesIn, client);
			}
		}
		else
		{
			bytesIn = 0;
		}

	} while (bytesIn > 0);
}

void calculateSine(struct InternalValues* intValues)
{
	double value, temporary, something;
	temporary= (*intValues).actualSWA /60.0;
	value = asin(temporary); // radian
	something = 2  * M_PI * 1.2 * 0.01; // 1.2 is a ferquency in Hz. 0.01 is the loop time in sec. 60.0 is the amplitude.
	//printf("value: %f\n", value);
	if((*intValues).previousSWA < (*intValues).actualSWA)
	{
		(*intValues).previousSWA = (*intValues).actualSWA;
		(*intValues).actualSWA = 60.0 *sin((value + something));
	}
	else
	{
		(*intValues).previousSWA = (*intValues).actualSWA;
		(*intValues).actualSWA = 60.0 *sin((value - something));
	}
	//printf("actualSWA: %f\n", (*intValues).actualSWA);
}

void CheckSWA(struct InternalValues* intValues)
{
	if ((*intValues).engaged) // The degree change is 40 deg/s that is 40/1000 deg/ms = 0,04 deg/ms. In a loop period it is a 0,4 deg/10ms
	{
		if ((*intValues).actualSWA < (*intValues).targetSWA)
		{
			(*intValues).actualSWA = (*intValues).actualSWA + 0.4;
		}
		else if ((*intValues).actualSWA > (*intValues).targetSWA)
		{
			(*intValues).actualSWA = (*intValues).actualSWA - 0.4;
		}
		else // If equals
		{
			(*intValues).actualSWA = (*intValues).actualSWA;
		}
	}
	else  // If SWA disengaged
	{
		calculateSine(intValues);
	}
}


void CheckSpeed(struct InternalValues* intValues)
{
	// 1 m/s^2 = 100 cm/s^2. To get the speed change it is need to multiply this value and the loop time. The loop time is 10 milli sec.
	//		Speed = 100 cm/s^2 * 10 milli s = 1000 milli cm/s = 1 cm/s. So every loop time the speed changes 1.

	if ((*intValues).targetSpeed > (*intValues).actualSpeed)
	{
		(*intValues).actualSpeed = (*intValues).actualSpeed + (int16_t)1;


	}
	else if ((*intValues).targetSpeed < (*intValues).actualSpeed)
	{
		(*intValues).actualSpeed = (*intValues).actualSpeed - (int16_t)1;

	}
	else // If equals
	{
		(*intValues).actualSpeed = (*intValues).actualSpeed;
	}
}



void main(int argc, int* argv[]) // We can pass in a command line option!!
{
	struct InternalValues intValues = {false, 0, 0, kmPerHourToCmPerSec((int16_t)argv[1]), 0, 0}; // The target speed read out from command line.

	// Startup Winsock
	WSADATA data;
	WORD version = MAKEWORD(2, 2);
	int wsOk = WSAStartup(version, &data);

	if (wsOk != 0)
	{
		printf("Can't start Winsock!  %d\n", wsOk);

		return;
	}
	// Bind socket to ip address and port 
	SOCKET in = socket(AF_INET, SOCK_DGRAM, 0);
	if (in < 0) {
		printf("socket creation failed");
	}
	struct sockaddr_in serverHint;
	serverHint.sin_family = AF_INET;
	serverHint.sin_addr.S_un.S_addr = ADDR_ANY;
	serverHint.sin_port = htons(54000); // Convert from little to big endian

	if (bind(in, (const struct sockaddr *)&serverHint, sizeof(serverHint)) == SOCKET_ERROR)
	{
		printf("Can't bind socket!  %d\n", WSAGetLastError());
		return;
	}

	struct sockaddr_in client;
	int clientLength = sizeof(client);
	ZeroMemory(&client, clientLength);

	double startTime = GetTickCount();

	// Enter a loop
	while (true)
	{
		double currentTime = GetTickCount() - startTime;
		if (currentTime >= 10) //10 milliseconds
		{

			// Wait for message
			CheckValidMessages(&intValues, in, client, clientLength);

			CheckSWA(&intValues);
			CheckSpeed(&intValues);
		}
	}
	// Close socket
	closesocket(in);

	// Shutdown winsock
	WSACleanup();
}