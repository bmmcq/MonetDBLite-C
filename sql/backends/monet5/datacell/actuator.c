/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2012 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * @a M. Kersten, E. Liarou, R. Goncalves
The Actuator Simulation program

This program listens to an event stream on a particular port.
It collects statistics on the events.
 * @f actuator
 */
#ifndef ACTUATOR
#define ACTUATOR
#include "monetdb_config.h"
#include "gdk.h"
#ifndef atoll
#define atoll atol
#endif
#ifndef strtoll
#define strtoll strtol
#endif

#ifndef HAVE_GETOPT_LONG
#  include "monet_getopt.h"
#else
# ifdef HAVE_GETOPT_H
#  include "getopt.h"
# endif
#endif


#endif


#include "dcsocket.h"


/* #define _DEBUG_ACTUATOR_  */
#define ACout GDKout

FILE *fd;
typedef struct ACTUATORCLI {
	str name;
	stream *fromServer;
	SOCKET newsockfd;
	struct ACTUATORCLI *nxt, *prev;
} ACrecord, *Actuator;

static Actuator acAnchor = NULL;

static Actuator
ACnew(str nme)
{
	Actuator ac;
	ac = (Actuator)GDKzalloc(sizeof(ACrecord));
	ac->name = GDKstrdup(nme);
	ac->nxt = acAnchor;
	acAnchor = ac;
	return ac;
}

#define TCP 1
#define UDP 2
static int protocol = TCP;

#define ACTIVE 1
#define PASSIVE 2
static int mode = PASSIVE;

/*
The actuator.
Events leave the Datacell to trigger actions in the real world. It may be as simple 
as being displayed on a screen, upto and including calling the mergency fighters
by telephone. Since, the mapping from event message to real world action is situation
specific, the default provided here is to only show the event message.

Users interested in experimentation with real actuators, can hook up the event
stream with a SCADA system or directly send the events in a proper format to
the channel where the actuators is listening.

The actuator runs in two modes, active or passive. In the former case, the tool
contacts the server and establishes an UDF channel to pass CSV encoded event strings.
Optionally, the event can be tagged with a serial key and a time-stamp.

Events are generated using a built-in random number generator. They are sent over the
channel with an optional delay, expressed in number of microseconds.

The code is relatively straightforward to extend to create your favoured dummy sensor.
*/
static void
usage(void)
{
	mnstr_printf(ACout, "The actuator tool can be used to catch an event sequence \n");
	mnstr_printf(ACout, "from the emitter port. Data is in ascii .\n");
	mnstr_printf(ACout, "Several options are provided to control this process.\n");
	mnstr_printf(ACout, "\nactuator [options]\n");
	mnstr_printf(ACout, "--host=<host name>, default localhost\n");
	mnstr_printf(ACout, "--port=<portnr>, default 50600 \n");
	mnstr_printf(ACout, "--protocol=<name>  either tcp/udp\n");
	mnstr_printf(ACout, "--actuator=<actuator name> to identify the event received \n");
    mnstr_printf(ACout, "--active run as a active listener \n");
    mnstr_printf(ACout, "--passive run as a passive listener (default)\n");
	mnstr_printf(ACout, "--events=<number>  number of events to receive (default -1)\n");
	mnstr_printf(ACout, "--statistics=<number>  show statistics after a series of events\n");
	exit(-1);
}

static char *host = "localhost";
static int port = 50600;
static char *actuator = "X";
static int trace = 1;
static int timestamp = 1; /* first column is a timestamp*/

static int statistics = 0;	/* dump statistics after a certain number events */
/*if(statistics!=0)
    print statistics;
   else don't
   collect statistics based on assumption that first
   column denotes time of actuator emission */

static int received = 0;
static lng totallatency = 0;
static lng sendFirst = 0;
static lng receivedLast = 0;
static lng receivedFirst = 0;
static int characters = 0;

/* control the display and auto terminate the actuator */
static int events = -1;
static int tuples = 0;

/*
 * @-
 * The statistics are shown after a fixed number of tuples
 * have been received.Align this number with the batches
 * generated by the sensor and you get a good impression
 * of batch effect.
 */
static void showStatistics(void)
{
	double elaps;
	int tuplesIntheBuffer = 2222;
	if (received == 0) return;



	mnstr_printf(ACout, "MYBUFSIZ :%d\n", MYBUFSIZ);
	mnstr_printf(ACout, "Tuples in the buffer :%d\n", tuplesIntheBuffer);

	mnstr_printf(ACout, "\nEvents %d\n", received);
	mnstr_printf(ACout, "Latency per tuple %6.2f microsec\n", (((double)totallatency) / received) / tuplesIntheBuffer);
	elaps = (double)((receivedLast - sendFirst) * tuplesIntheBuffer / (received));
	/*mnstr_printf(ACout, "receivedLast-sendFirst %d microsec\n", receivedLast-sendFirst);*/
	mnstr_printf(ACout, "Elapsed per batch %5.2f microsec\n", elaps);
	mnstr_printf(ACout, "Throughput %8.2f tpl/sec\n", 1000000.0 / elaps * tuplesIntheBuffer);
	mnstr_printf(ACout, "Bandwidth %zu bytes\n", characters * sizeof(char));


	/* send a short tuple to stderr for gnuplot datafile*/
	fprintf(stderr, "events %d throughput %6.2f\n", received, 1000000.0 / elaps * tuplesIntheBuffer);
	received=0;
	totallatency=0;
	receivedFirst= receivedLast=0;
	characters=0;
}

static void terminate(Actuator ac)
{
#ifdef _DEBUG_ACTUATOR_
	mnstr_printf(ACout, "about to shutdown the actuator\n");
#endif
	shutdown(ac->newsockfd, SHUT_RDWR);
	exit(0);
}

static void stopListening(int i)
{
	(void)i;
	signal(i, SIG_IGN);
	if (statistics)
		showStatistics();
	exit(0);
}

extern int consumeStream(Actuator ac);

int
main(int argc, char **argv)
{
	int i;
	char *err = NULL;
	/*char name[MYBUFSIZ + 1];*/
	char hostname[1024];
	static SOCKET sockfd;
	Actuator ac = NULL;
	int option_index = 0;

	static struct option long_options[13] = {
		{ "port", 1, 0, 'p' },
		{ "actuator", 1, 0, 'a' },
		{ "active", 0, 0, 'a' },
		{ "passive", 0, 0, 'p' },
		{ "statistics", 0, 0, 's' },
		{ "protocol", 1, 0, 'o' },
		{ "events", 1, 0, 'e' },
		{ "host", 1, 0, 'h' },
		{ "help", 1, 0, '?' },
		{ "timestamp", 0, 0, 't' },
		{ "trace", 0, 0, 't' },
		{ 0, 0, 0, 0 }
	};
	THRdata[0] = (void *)file_wastream(stdout, "stdout");
	THRdata[1] = (void *)file_rastream(stdin, "stdin");
	for (i = 0; i < THREADS; i++) {
		GDKthreads[i].tid = i + 1;
	}

	for (;; ) {
		/*	int option_index=0;*/
		int c = getopt_long(argc, argv, "a:c:p:s:S:b:h:?:t:0", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 't':
			if (strcmp(long_options[option_index].name, "trace") == 0) {
				trace = optarg? atol(optarg): 1;
			} else
			if (strcmp(long_options[option_index].name, "timestamp") == 0) {
				timestamp = optarg? atol(optarg): 1;
			} else {
				usage();
				exit(0);
			}
			break;
		case 'e':
			if (strcmp(long_options[option_index].name, "events") == 0) {
				events = optarg? atol(optarg): 1;
			} else {
				usage();
				exit(0);
			}
			break;
		case 'a':
			if (strcmp(long_options[option_index].name, "active") == 0) {
				mode = ACTIVE;
				break;
			} else
				actuator = optarg? optarg: "dummy";
			break;
		case 'p':
			if (strcmp(long_options[option_index].name, "port") == 0) {
				port = optarg? atol(optarg): -1;
				break;
			} else
			if (strcmp(long_options[option_index].name, "passive") == 0) {
				mode = PASSIVE;
				break;
			} else
			if (strcmp(long_options[option_index].name, "protocol") == 0) {
                if (strcmp(optarg, "TCP") == 0 || strcmp(optarg, "tcp") == 0) {
                    protocol = TCP;
                    break;
                }
                if (strcmp(optarg, "UDP") == 0 || strcmp(optarg, "udp") == 0) {
                    protocol = UDP;
                    break;
                }
			}
			break;
		case 's':
			if (strcmp(long_options[option_index].name, "statistics") == 0) {
				statistics = optarg? atol(optarg): 100;
				break;
			} else {
				usage();
				exit(0);
			}
			break;
		case 'h':
			host = optarg;
			break;
		case '?':
		default:
			usage();
			exit(0);
		}
	}

	signal(SIGABRT, stopListening);
#ifdef SIGPIPE
	signal(SIGPIPE, stopListening);
#endif
#ifdef SIGHUP
	signal(SIGHUP, stopListening);
#endif
	signal(SIGTERM, stopListening);
	signal(SIGINT, stopListening);

	/* display properties */
	if (trace) {
		mnstr_printf(ACout, "--host=%s\n", host);
		mnstr_printf(ACout, "--port=%d\n", port);
		mnstr_printf(ACout, "--actuator=%s\n", actuator);
		mnstr_printf(ACout, "--events=%d\n", events);
		mnstr_printf(ACout, "--timestamp=%d\n", timestamp);
		mnstr_printf(ACout, "--statistics=%d\n", statistics);
		mnstr_printf(ACout, "--%s\n", mode == ACTIVE?"active":"passive");
	}
    strncpy(hostname,host,1024);
    if ( strcmp(host,"localhost")== 0 )
        gethostname(hostname,1024);
    host= hostname;


	ac = ACnew(actuator);
	/*name[0] = 0;*/
	err = NULL;
	if ( mode == PASSIVE) {
		ac->fromServer = udp_rastream(host, port, actuator);
		if ( ac->fromServer == 0){
			mnstr_printf(ACout, "Failed to access stream %s:%d\n",host,port);
			return 0;
		}
		consumeStream(ac);
#ifdef _DEBUG_ACTUATOR_
			mnstr_printf(ACout, "stream consumed\n");
#endif
	}

	if( mode == ACTIVE)
    do {
        err = socket_client_connect(&sockfd, host, port);
        if (err) {
            mnstr_printf(ACout, "actuator connect fails: %s\n", err);
            MT_sleep_ms(1000);
        }
    } while (err);

	if ( mode == PASSIVE )
	{
		err = socket_server_listen(sockfd, &(ac->newsockfd));
		if (err) {
			mnstr_printf(ACout, "ACTUATOR:server listen fails:%s\n", err);
			return 0;
		}
	}
	do {
		if (mode == PASSIVE) {
#ifdef _DEBUG_ACTUATOR_
			mnstr_printf(ACout, "Actuator listens\n");
#endif

			if ( protocol == UDP)
				ac->fromServer = udp_rastream(host, port, actuator);
			else
				ac->fromServer = socket_rastream(ac->newsockfd, actuator);
			if (ac->fromServer == NULL) {
				perror("Actuator: Could not open stream");
				mnstr_printf(ACout, "stream %s.%d.%s\n", host, port, actuator);
				return 0;
			}
			consumeStream(ac);
		} else
		if( mode == ACTIVE ) {
#ifdef _DEBUG_ACTUATOR_
			mnstr_printf(ACout, "Actuator connects\n");
#endif
			err = socket_client_connect(&(ac->newsockfd), host, port);
			if (err) {
				mnstr_printf(ACout, "ACTUATOR:start client:%s\n", err);
				continue;
			}

#ifdef _DEBUG_ACTUATOR_
			mnstr_printf(ACout, "Initialize stream\n");
#endif
			if ( protocol == UDP)
				ac->fromServer = udp_rastream(host, port, actuator);
			else
				ac->fromServer = socket_rastream(ac->newsockfd, actuator);

			if (ac->fromServer == NULL) {
				perror("Actuator: Could not open stream");
				mnstr_printf(ACout, "stream %s.%d.%s\n", host, port, actuator);
				continue;
			}
		}
		consumeStream(ac);
	} while (mode == PASSIVE && (events == -1 || tuples < events) );
	socket_close(sockfd);
	terminate(ac);
	return 0;
}

#define L(X) (X + strlen(X))


/*
   lng
   estimateOverhead(){
    int i;
    lng l;
    lng t0= GDKusec();
    for(i=0;i<10000; i++)
        l= GDKusec();
    t0= GDKusec()-t0;
    mnstr_printf(ACout, "Timing overhead " LLFMT " GDKusec/10000\n",t0);
    return l;
   }
 */

int
consumeStream(Actuator ac)
{
	char buf[MYBUFSIZ + 1];   /* use equal to Linux internal buffer size */
	lng clk;
	char *endptr;
	lng l = 0;
	int m = 0;

#ifdef _DEBUG_ACTUATOR_
	mnstr_printf(ACout, "Consume stream\n");
#endif
	buf[0] = 0;
	while (tuples != events  && (m = (int)mnstr_readline(ac->fromServer, buf, MYBUFSIZ))) {
		buf[m] = 0;
		characters += m;
		received++;
		tuples++;
		if (trace)
			mnstr_printf(ACout, "%s%s", buf, (buf[m-1]=='\n'?"":"\n"));
		/*mnstr_printf(ACout, "Received tuple[%d]:%s\n",received,buf);*/

		endptr = 0;
		clk = strtoll(buf + 1, &endptr, 0);
		if (sendFirst == 0)
			sendFirst = clk;
		receivedLast = GDKusec();
		if (receivedFirst == 0)
			receivedFirst = receivedLast;
		if (timestamp) {
			l = receivedLast - clk;
#ifdef _DEBUG_ACTUATOR_
			if (trace)
				mnstr_printf(ACout, "B#%d: " LLFMT " " LLFMT "\n",
					received, clk, l);
#endif
			totallatency += l;
		}

		m = 0;
		if( statistics && (received % statistics ) == 0) {
		    showStatistics();
		}
	}
	if (errno == EPIPE || errno == ECONNRESET) {
		mnstr_printf(ACout, "errno:%s\n", strerror(errno));
		close_stream(ac->fromServer);
	}
	if (statistics)
		showStatistics();
#ifdef _DEBUG_ACTUATOR_
	mnstr_printf(ACout, "tuples %d events %d \n",
		tuples, events);
#endif

	if (tuples >= events )
		terminate(ac);

	return 0;
}
