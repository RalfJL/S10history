//============================================================================
// Name        : S10history.cpp
// Author      : Ralf Lehmann
// Copyright   : Ralf Lehmann 02.2017
// Version     : 1.0
// Description : Reads historical data from a s10 solar power station (E3DC)
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//============================================================================

#include <iostream>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cstdlib>
#define RLOG_COMPONENT S10history
#include <rlog/rlog.h>
#include <rlog/StdioNode.h>
#include <rlog/RLogChannel.h>

using namespace rlog;
using namespace std;

#define REPORT_YEAR 1
#define REPORT_MONTH 2
#define REPORT_DAY 4

char *progname;
int debug = 0;    // no debug output by default

int usage(const char *errstr) {
	cerr << errstr << endl;
	cerr << "Usage: " << progname << " [OPTIONS] -u user -p password -a aes-password -i ip-addr" << endl;
	cerr << "This program reads historical data from a E3DC/S10 and prints it" << endl;
	cerr << "[-y year] [-m month] -d day prints data of the given day; details every 15 minutes" << endl;
	cerr << "[-y year] -m month          prints data of the given month; details once per day" << endl;
	cerr << "-y year                     print the sum of the given year, no details" << endl;
	cerr << "--user name              name of user for logon" << endl;
	cerr << "--password string        password in cleartext" << endl;
	cerr << "--Password env-variable  password is in ENV variable" << endl;
	cerr << "--aes aes-password       password for AES encryption (mandatory)" << endl;
	cerr << "--AES env-variable       password for AES encryption is in ENV variable (mandatory)" << endl;
	cerr << "--ip  IP-addr			  IP address of S10 solar power station" << endl;
	cerr << "Options:" << endl;
	cerr << "--version      version string" << endl;
	cerr << "--help         this message" << endl;
	cerr << "--brief        brief report; sum only" << endl;
	cerr << "--Debug num    debug level 1=Info 2= Debug" << endl;
	cerr << "--year num     year > 2014; current year if not present" << endl;
	cerr << "--month -+num  month; current month if not present" << endl;
	cerr << "--day +-num    day; current day if not present" << endl;
	cerr << "--service num  services port number (default: 5033)" << endl;

	return 1;
}

int main(int argc, char *argv[]) {
	progname = argv[0];    // save program name

	time_t rawtime;        // used for time calculation
	time(&rawtime);        // get current time to complete time arguments
	struct tm *l = localtime(&rawtime);

	// logging
	// RLogInit(argc, argv);
	StdioNode stdLog( STDERR_FILENO, StdioNode::OutputChannel);
	stdLog.subscribeTo(GetGlobalChannel("warning"));
	stdLog.subscribeTo(GetGlobalChannel("error"));

	// user managment
	char * user = 0;    // user name
	char * password = 0; // password
	char * aes = 0; 	// aes password

	// S10 ip addr
	char * ip = 0;		// ip
	int service = 5033; // service port number of RSCP server S10

	// report type
	int report_type = 0; // 1=year; 2=month, 4=day; 0=current day
	bool brief = false;	 // brief means only sum container to report

	// option struct
	const struct option longopts[] = { { "version", no_argument, 0, 'v' }, { "year", required_argument, 0, 'y' }, { "month", required_argument, 0, 'm' }, { "day",
	required_argument, 0, 'd' }, { "user",
	required_argument, 0, 'u' }, { "password", required_argument, 0, 'p' }, { "Password",
	required_argument, 0, 'P' }, { "aes", required_argument, 0, 'a' }, { "AES", required_argument, 0, 'A' }, { "Debug", required_argument, 0, 'D' },
			{ "help", no_argument, 0, 'h' }, { "ip", required_argument, 0, 'i' }, { "service", required_argument, 0, 's' }, { "brief", no_argument, 0, 'b' }, };

	// process arguments
	int index;
	int iarg = 0;
	long y, m, d = 0;

	// turn off getopt error message
	// opterr=1;
	while (iarg != -1) {
		iarg = getopt_long(argc, argv, "vhy:m:d:u:p:P:d:D:A:a:i:s:b", longopts, &index);
		switch (iarg) {
		case 'h':
			return usage("");
			break;
		case 'v':
			rError("version 1.0");
			return 0;
			break;
		case 'b':
			brief = true;
			break;
		case 'y':
			y = atol(optarg);
			if (y < 2012 || y > 2032) {
				return usage("ERROR: Invalid year");
			}
			l->tm_year = y - 1900;
			if (report_type & REPORT_YEAR) {
				return usage("ERROR: only one year please");
			}
			report_type |= REPORT_YEAR;
			break;

		case 'm':
			m = atol(optarg);
			if (m < 1 || m > 12) {
				return usage("ERROR: invalid month");
			}
			l->tm_mon = m - 1;
			if (report_type & REPORT_MONTH) {
				return usage("ERROR: only one month please");
			}
			report_type |= REPORT_MONTH;
			break;

		case 'd':
			d = atoi(optarg);
			if (d < 0) {
				l->tm_mday += d;
			} else {
				l->tm_mday = d;
			}
			rawtime = mktime(l);
			if (report_type & REPORT_DAY) {
				return usage("ERROR: only one day please");
			}
			report_type |= REPORT_DAY;
			break;
		case 's':
			service = atoi(optarg);
			if (service < 0 || service > 65000) {
				return usage("ERROR: port number out of range");
			}
			break;
		case 'u':
			user = optarg;
			break;
		case 'p':
			password = optarg;
			break;
		case 'P':
			password = getenv(optarg);
			if (!password) {
				return usage("ERROR: environment variable for password is not set");
			}
			break;
		case 'a':
			aes = optarg;
			break;
		case 'A':
			aes = getenv(optarg);
			if (!aes) {
				return usage("ERROR: environment variable for AES password is not set");
			}
			break;
		case 'i':
			ip = optarg;
			break;
		case 'D':
			debug = atoi(optarg);
			stdLog.subscribeTo(GetGlobalChannel("info"));
			if (debug > 1) {
				stdLog.subscribeTo(GetGlobalChannel("debug"));
			}
			break;
		}
	}

	// check user (mandatory)
	if (!user || !password || !aes) {
		return usage("ERROR: user name, password and aes key must be given");
	}
	if (!ip) {
		return usage("ERROR: no S10 address given");
	}
	rDebug("User: %s", user);
	rDebug("Password: %s", password);
	rDebug("AES pw: %s", aes);

	// check time
	l->tm_sec = l->tm_min = l->tm_hour = 0;
	rawtime = mktime(l);
	time_t now;
	time(&now);
	if (rawtime >= now) {
		return usage("ERROR: report date is in the future");
	}

	extern int RscpReader_Day(const char * user, const char *pw, const char *aes, const char * ip, int port, struct tm *l, bool brief);
	extern int RscpReader_Month(const char * user, const char *pw, const char *aes, const char * ip, int port, struct tm *l, bool brief);
	extern int RscpReader_Year(const char * user, const char *pw, const char *aes, const char * ip, int port, struct tm *l, bool brief);
	int (*report_func)(const char *, const char *, const char *, const char *, int port, struct tm *, bool brief) = RscpReader_Day;

	// check report span
	if (report_type == 0) {
		report_type |= REPORT_DAY;
	}
	switch (report_type) {
	case 1:
		l->tm_mday = 1;
		l->tm_mon = 0;
		rInfo("Reporting one year");
		report_func = &RscpReader_Year;
		break;
	case 2:
	case 3:
		l->tm_mday = 1;
		rInfo("Reporting one month");
		report_func = &RscpReader_Month;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		rInfo("Reporting one day");
		report_func = &RscpReader_Day;
		break;
	}
	rInfo("Report starts: %s", asctime(l));
	rInfo("S10 addr: %s, Port: %d", ip, service);
	return (*report_func)(user, password, aes, ip, service, l, brief);
	return 0;
}
