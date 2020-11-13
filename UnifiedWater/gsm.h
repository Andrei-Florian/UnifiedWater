#include <MKRGSM.h>
#include <RTCZero.h>
#include "configure.h"
#include "io.h"

// globals
unsigned int localPort = 2390;		  // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48;		  // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];	  //buffer to hold incoming and outgoing packets

// initialize the library instance
GSMLocation location;
GSMClient client;
GSMModem modem;
GPRS gprs;
GSM gsmAccess;
RTCZero rtc;

// A UDP instance to let us send and receive packets over UDP
GSMUDP Udp;

// time vars

struct GCell
{
	public:
		char imei[50];

		void init()
		{
			Serial.println("[setup] Initialising SIM Card");

			// connection state
			bool connected = false;

			while (!connected)
			{
				if ((gsmAccess.begin() == GSM_READY) && (gprs.attachGPRS("hologram", "", "") == GPRS_READY))
				{
					connected = true;
				}
				else
				{
					Serial.print(".");
					neo.show(LOOP_ERROR, 0, 0);
					neo.show(SETUP_PROGRESS, 0, 4);
				}
			}
		}

		void readGSMInfo()
		{
			String response;

			// signal quality, SIM detection etc.
			MODEM.send("AT+CIND?");

			if (gsmInfo.imei[0] == 0)
				strcpy(imei, modem.getIMEI().c_str());
		}
};

GCell gsm;

struct Time
{
	private:
		unsigned long sendNTPpacket(IPAddress &address)
		{
			memset(packetBuffer, 0, NTP_PACKET_SIZE);
			packetBuffer[0] = 0b11100011; // LI, Version, Mode
			packetBuffer[1] = 0;		  // Stratum, or type of clock
			packetBuffer[2] = 6;		  // Polling Interval
			packetBuffer[3] = 0xEC;		  // Peer Clock Precision
			packetBuffer[12] = 49;
			packetBuffer[13] = 0x4E;
			packetBuffer[14] = 49;
			packetBuffer[15] = 52;

			Udp.beginPacket(address, 123); //NTP requests are to port 123
			Udp.write(packetBuffer, NTP_PACKET_SIZE);
			Udp.endPacket();
		}

		unsigned long getEpochTime()
		{
			Serial.println("[setup] asking server for time");
			sendNTPpacket(timeServer); // send an NTP packet to a time server
			delay(1000);			   // delay is essential, do not delete

			if (Udp.parsePacket())
			{
				Serial.println("[setup] packet received");
				Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

				unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
				unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

				// combine the four bytes (two words) into a long integer
				// this is NTP time (seconds since Jan 1 1900):
				unsigned long secsSince1900 = highWord << 16 | lowWord;

				// now convert NTP time into everyday time
				// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
				const unsigned long seventyYears = 2208988800UL;
				const unsigned long timeZoneDiff = timeZone * 3600; // adjust the time zone

				// subtract seventy years and add time zone
				unsigned long epoch = (secsSince1900 - seventyYears) + timeZoneDiff;

				Serial.println("[setup] Packet Read");
				Serial.println("[setup] Server epoch time (local)  " + String(epoch));
				return epoch;
			}
			else
			{
				return -1;
			}
		}

		void synchRTC(unsigned long epoch)
		{
			Serial.println("[setup] Initialising RTC with Time");
			Serial.println("[setup] Initialising RTC");
			rtc.begin();

			Serial.println("[setup] Setting Current Time");
			rtc.setEpoch(epoch);

			Serial.println("[setup] RTC Setup Complete");
		}

	public:
		void set()
		{
			Serial.println("[setup] Starting connection to server");
			Udp.begin(localPort);

			Serial.println("[setup] Synching onboard RTC to GNSS Time");

			// wait until we have the time appended to a variable
			unsigned long epoch = 0;
			while (!epoch)
			{
				epoch = getEpochTime();
			}

			// synch the time to the RTC
			synchRTC(epoch);
			Serial.println("");

			// print the time to the Serial Monitor
			get(true);
		}

		String get(bool string) // a public loop to convert time to string
		{
			String time;
			String date;
			String finalVal;

			date += "20"; // the RTC library can't return the full year, have to add 20 to start to get 20xx
			date += rtc.getYear();
			date += "-";
			date += rtc.getMonth();
			date += "-";
			date += rtc.getDay();

			if (rtc.getHours() < 10)
				time += "0";
			time += rtc.getHours();
			time += ":";
			if (rtc.getMinutes() < 10)
				time += "0";
			time += rtc.getMinutes();
			time += ":";
			if (rtc.getSeconds() < 10)
				time += "0";
			time += rtc.getSeconds();

			rtc.getY2kEpoch();
			unsigned long epoch = rtc.getEpoch();
			char timestamp[25];

			finalVal = date + String("T") + time + String(".0Z");
			finalVal.toCharArray(timestamp, finalVal.length());
			Serial.println("[time] Time is " + String(timestamp));

			return finalVal;
		}

		unsigned long get() // a public loop to convert time to string
		{
			String time;
			String date;
			String finalVal;

			date += "20"; // the RTC library can't return the full year, have to add 20 to start to get 20xx
			date += rtc.getYear();
			date += "-";
			date += rtc.getMonth();
			date += "-";
			date += rtc.getDay();

			if (rtc.getHours() < 10)
				time += "0";
			time += rtc.getHours();
			time += ":";
			if (rtc.getMinutes() < 10)
				time += "0";
			time += rtc.getMinutes();
			time += ":";
			if (rtc.getSeconds() < 10)
				time += "0";
			time += rtc.getSeconds();

			rtc.getY2kEpoch();
			unsigned long epoch = rtc.getEpoch();
			char timestamp[25];

			finalVal = date + String("T") + time + String(".0Z");
			finalVal.toCharArray(timestamp, finalVal.length());
			Serial.println("[time] Time is " + String(timestamp));

			return epoch;
		}
};

Time rtctime;

struct Location
{
	private:
		bool checkLocation(double lat, double lng, double alt)
		{
			int rawLat = lat;
			int rawLng = lng;

			if (rawLat == lat && rawLng == lng) // location is not precise
			{
				Serial.print(".");
				return false;
			}
			else
			{
				Serial.println("");
				Serial.println("[location] location is fixed");
				Serial.print("latitude  ");
				Serial.println(lat, 7);
				Serial.print("longitude ");
				Serial.println(lng, 7);

				latitude = lat;
				longitude = lng;
				altitude = alt;
				gpsacc = location.accuracy();
				Serial.println("");
				return true;
			}
		}

	public:
		double latitude = 0;
		double longitude = 0;
		double altitude = 0;
		long gpsacc = 0;

		void set()
		{
			Serial.println("");
			Serial.println("[setup] Enabling Location");
			location.begin();

			get();
		}

		void get()
		{
			bool fixed = false;

			while (!fixed)
			{
				if (location.available())
				{
					fixed = checkLocation(location.latitude(), location.longitude(), location.altitude());
				}
			}
		}
};

Location loc;