#
/*
 *    Copyright (C) 2022
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the wspr plugin
 *
 *    wspr plugin is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    wspr plugin is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with wspr plugin; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	"psk-reporter.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include	"SDRunoPlugin_wsprUi.h"

#pragma  comment (lib,"ws2_32.lib")

const char name [] = "report.pskreporter.info";
const char soft [] = "SDRunoPlugin_wspr";

uint8_t header[] = {
//
//	kop.modified dynamically
    0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//	pattern for receiver callsign
    0x00, 0x03, 0x00, 0x2C, 0x99, 0x92, 0x00, 0x04,
    0x00, 0x00,
    0x80, 0x02, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x04, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x08, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x09, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x00, 0x00,
//	pattern for sender callsign
    0x00, 0x02, 0x00, 0x44, 0x99, 0x93, 0x00, 0x08,
    0x80, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x05, 0x00, 0x04, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x07, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x0A, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
    0x80, 0x03, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
    0x00, 0x96, 0x00, 0x04
};

	reporterWriter::reporterWriter  (SDRunoPlugin_wsprUi *m_form) {
struct hostent *host;

	this	-> m_form	= m_form;
	homeCall	= m_form -> load_callSign	();
	homeGrid	= m_form -> load_grid		();
	antenna		= m_form -> load_antenna	();	
	this	-> programName	= "SDRunoPlugin_wspr";
	sequence		= 1;
	reporterOK		= false;

	if (homeCall == "")
	   throw (19);

	fprintf (stderr, "homedata: %s, %s, (%s)\n",
	                           homeCall. c_str (),
	                           homeGrid. c_str (),
	                           antenna. c_str ());

	WSADATA wsaData;

	int res = WSAStartup (MAKEWORD (2, 2), &wsaData);
	if (res != NO_ERROR) {
	   fprintf (stderr, "WSAstartup failed\n");
	   throw (21);
	}

	if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	   fprintf (stderr, "Cannot open socket %d.\n", errno);
	   throw (22);
 	}

	if ((host = gethostbyname (name)) == NULL) {
	   m_form -> show_printStatus ("Cannot find remote host address.\n");
	   throw (23);
	}

	memset (&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	memcpy (&addr. sin_addr.s_addr, host -> h_addr, host -> h_length);
//	addr.sin_port = htons (14739);
	addr.sin_port = htons (4739);
//

	offset_for_size			= 2;
	offset_for_transmission_time	= 4;
	offset_for_sequence_number	= 8;
	offset_for_random		= 12;
//
//	The random number is "static" for all  transmissions in this session
	struct timespec ts;
//	clock_gettime (CLOCK_REALTIME, &ts);
	SYSTEMTIME st;
	GetSystemTime (&st);
	
	srand (st. wSecond);
	(void)copy_int4 (&header [offset_for_random], rand ());
//	create the buffer
	memcpy (buffer, header, sizeof (header));
//
	data_offset		= sizeof (header);
	int receiver_start	= data_offset;
	data_offset		+= copy_int2 (&buffer [data_offset], 0x9992);
	int sizeOffset		= data_offset;
	data_offset += 2;	// to be filled in later
	data_offset += copy_char (&buffer [data_offset], homeCall. c_str ());
	data_offset += copy_char (&buffer [data_offset], homeGrid. c_str ());
	data_offset += copy_char (&buffer [data_offset], programName. c_str ());
	data_offset += copy_char (&buffer [data_offset], antenna. c_str ());
	
	int padding = (4 - data_offset % 4) % 4;
	memset (&buffer [data_offset], 0, padding);
	data_offset	+= padding;
	copy_int2 (&buffer [sizeOffset], data_offset - receiver_start);

	start_of_transmitters	= data_offset;
	m_form -> show_printStatus ("ready for posting");
}

	reporterWriter::~reporterWriter	() {
	closesocket (sock);
}

bool	reporterWriter::reporterReady	() {
	return true;
}

void	reporterWriter::addMessage	(struct decoder_results res) {
pskMessage m;
	m. call		= std::string (res. call);
	m. grid		= std::string (res. loc);
	m. freq		=  (int)(res. freq * 1000000);
	m. snr		=  (int)(res. snr);
	m. seconds	= time (NULL);
	messageStack. push_back (m);
}

int	reporterWriter::sendMessages	() {
	if (messageStack. size () < 1)
	   return 0;

	data_offset	= start_of_transmitters;
	data_offset	+= copy_int2 (&buffer [data_offset], 0x9993);
	int sizeOffset	= data_offset;
	data_offset	+= 2;	// to be filled in later
	for (int i = 0; i < messageStack. size (); i ++) {
	   pskMessage m = messageStack.at(i);
	   int localBase = data_offset;
	   data_offset += copy_char (&buffer [data_offset], m. call. c_str ());
	   data_offset += copy_int4 (&buffer [data_offset], m. freq);
	   data_offset += copy_int1 (&buffer [data_offset], (int8_t)(m. snr));
	   data_offset += copy_int1 (&buffer [data_offset], 1); // IMD
	   data_offset += copy_char (&buffer [data_offset], "WSPR");
	   data_offset += copy_int1 (&buffer [data_offset], 1); // automatic
	   data_offset += copy_char (&buffer [data_offset], m. grid. c_str ());
	   data_offset += copy_int4 (&buffer [data_offset], m. seconds);
	}

	int padding = (4 - data_offset % 4) % 4;
	memset (&buffer [data_offset], 0, padding);
	data_offset += padding;
//
//	size of seen transmitters records
	(void)copy_int2 (&buffer [sizeOffset], data_offset - start_of_transmitters);

//	global data
	(void)copy_int2 (&buffer [offset_for_size], data_offset);
	(void)copy_int4 (&buffer [offset_for_transmission_time], time (NULL));
	(void)copy_int4 (&buffer [offset_for_sequence_number], sequence);
	sequence ++;

	int bytesSent = sendto (sock, (const char *)buffer, data_offset, 0,
	                        (struct sockaddr *)&addr, sizeof(addr));
	if (bytesSent <= 0)
		m_form	-> show_printStatus ("Transfer failed");
	else
		m_form	-> show_printStatus ("posting OK");
		
	messageStack. resize (0);
	return EXIT_SUCCESS;
}

int	reporterWriter::copy_char (uint8_t *pointer, const char *value) {
int8_t size = strlen (value);
	memcpy (pointer, &size, 1);
	pointer += 1;
	memcpy (pointer, value, size);
	return size + 1;
}

int	reporterWriter::copy_int1 (uint8_t *pointer, int8_t value) {
	memcpy (pointer, &value, 1);
	return 1;
}

int	reporterWriter::copy_int2 (uint8_t *pointer, int16_t value) {
	value = htons (value);
	memcpy (pointer, &value, 2);
	return 2;
}

int	reporterWriter::copy_int4 (uint8_t *pointer, int32_t value) {
	value = htonl (value);
	memcpy (pointer, &value, 4);
	return 4;
}

