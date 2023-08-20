#include	<sstream>
#include	<unoevent.h>
#include	<iunoplugincontroller.h>
#include	<vector>
#include	<sstream>
#include	<chrono>
#include	<Windows.h>
#include	"SDRunoPlugin_wspr.h"
#include	"SDRunoPlugin_wsprUi.h"
#include        <complex>
#include        <atomic>
#include	".\curl/curl.h"

const char wsprnet_app_version[]  = "uno-0.1";  // 10 chars max.!

static
int	gettimeofday (struct timeval* tv, struct timezone* tz) {
static LONGLONG birthunixhnsec = 116444736000000000;  /*in units of 100 ns */
FILETIME systemtime;

	GetSystemTimeAsFileTime (&systemtime);
	ULARGE_INTEGER utime;
	utime.LowPart  = systemtime. dwLowDateTime;
	utime.HighPart = systemtime. dwHighDateTime;

	ULARGE_INTEGER birthunix;
	birthunix. LowPart  = (DWORD)birthunixhnsec;
	birthunix. HighPart = birthunixhnsec >> 32;

	LONGLONG usecs;
	usecs = (LONGLONG)((utime.QuadPart - birthunix.QuadPart) / 10);
	tv -> tv_sec  = (long)(usecs / 1000000);
	tv -> tv_usec = (long)(usecs % 1000000);
	return 0;
}

std::string getTime () {
time_t localtime;
struct tm       *gtm;

	time (&localtime);
        gtm = gmtime (&localtime);
        return std::to_string (gtm -> tm_hour) + "-" +
                      std::to_string (gtm -> tm_min);
}
//
//	The incoming signal has a rate of 192000, in step 1 we decimate
//	to 6000, in step 2 to 375
	SDRunoPlugin_wspr::
	           SDRunoPlugin_wspr (IUnoPluginController& controller):
	                              IUnoPlugin (controller),
	                              m_form	(*this, controller),
	                              m_worker	(nullptr),
	                              decimator_1 (21, 1500, 
	                                                   SAMPLING_RATE, 8),
	                              decimator_2 (33, 1500, 
	                                                   SAMPLING_RATE / 8, 4),
	                              decimator_3 (33, 1500,
	                                                   SAMPLING_RATE / 32, 16),
	                              inputBuffer (8 * 32768),
	                              fft (512, true) {
	m_controller	= &controller;
	m_controller    -> RegisterAudioProcessor (0, this);
        m_controller    -> SetDemodulatorType (0,
                                 IUnoPluginController::DemodulatorIQOUT);

	memset (dec_options. rcall, 0, 13);
	memset (dec_options. rloc, 0, 7);

//
//	default frequency is in the 20m band
	rx_state. frequencyChange. store (false);
	rx_state. running. store (false);
	rx_state. savingSamples. store (false);
	rx_options. dialFreq	= 14095600;
	rx_options. realFreq	= rx_options. dialFreq;
	rx_options. report	= false;
	dec_options. freq	= rx_options. dialFreq;
	dec_options. usehashtable	= 0;
	dec_options. npasses	= 2;
	dec_options. subtraction	= true;
	dec_options. quickmode	= false;
	m_controller	-> SetVfoFrequency (0, (float)dec_options. freq);
	m_controller	-> SetCenterFrequency (0, (float)dec_options. freq + 1500);

	std::string cs	= m_form. load_callSign ();
	std::string gr	= m_form. load_grid ();
	if (cs == "")
	   cs = "NO_NAME";
	if (gr == "")
	   cs = "????";
	for (int i = 0; i <= cs. size (); i ++)
	   dec_options. rcall [i] = cs. c_str () [i];
	for (int i = 0; i < gr. size (); i ++)
	   dec_options. rloc [i] = gr. c_str () [i];
	m_worker        =
	        new std::thread (&SDRunoPlugin_wspr::workerFunction, this);
}

	SDRunoPlugin_wspr::~SDRunoPlugin_wspr () {
	rx_state. running. store (false);
	m_form.	show_results ("going to stop"); 
        m_worker	-> join ();
//      m_controller    -> UnregisterStreamProcessor (0, this);
        m_controller    -> UnregisterAudioProcessor (0, this);
}

void	SDRunoPlugin_wspr::HandleEvent (const UnoEvent& ev) {
	m_form. HandleEvent(ev);	
}

void    SDRunoPlugin_wspr::StreamProcessorProcess (channel_t channel,
	                                           Complex *buffer,
	                                           int length,
	                                           bool& modified) { 
	(void)channel; (void)buffer; (void)length;
//	modified = false;
}
//
//	we get the samples "in" with a specified rate of 192000,
//	we decimate in 2 steps to 375 Ss and store SIGNAL_LENGTH seconds of
//	data in the buffer
void    SDRunoPlugin_wspr::AudioProcessorProcess (channel_t channel,
	                                          float* buffer,
	                                          int length,
	                                          bool& modified) {
static	int teller = 0;
//	Handling IQ input, note that SDRuno interchanges I and Q elements

	if (!modified) {
	   for (int i = 0; i < length; i++) {
	      std::complex<float> sample =
	                   std::complex<float>(buffer [2 * i + 1],
	                                       buffer [2 * i]);

	      if (decimator_1. Pass (sample, &sample) &&
	          decimator_2. Pass (sample, &sample) &&
	          decimator_3. Pass (sample, &sample)) {
	         if (rx_state. savingSamples. load ()) {
	            inputBuffer. putDataIntoBuffer (&sample, 1);
	            teller ++;
	            if (teller % SIGNAL_SAMPLE_RATE == 0)
	               m_form.  show_status ("reading  " + std::to_string (teller / SIGNAL_SAMPLE_RATE));
	            if (inputBuffer. GetRingBufferReadAvailable () >=
	                               SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE) {
	               teller = 0;
				   rx_state.savingSamples.store(false);
	            }
	         }
	      }
	   }
	}
}
//
//	Data collection takes place in a function, called from
//	within amother thread. We create a worker thread that
//	will monitor the time, and  - if the time is right-
//	signal the data reader to collect samples for 116 seconds
//	The worker will collect the samples, most likely within
//	4 seconds, and then signal the reader that a new collecting phase
//	mau start. While the reader is collecting the data, the
//	worker will process the data

void	SDRunoPlugin_wspr::wait_to_start () {
struct timeval lTime;
//	on start up,  Wait for timing alignment, here we
//	compute the delay
        gettimeofday (&lTime, nullptr);
        uint32_t sec   = lTime. tv_sec % 120;
        uint32_t usec  = sec * 1000000 + lTime. tv_usec;
//
//      waiting is for an even time in minutes, i.e. 120 seconds
        uint32_t uwait	=  120000000 - usec - 1000;
	while (uwait / 1000000 > 1) {
	   m_form. show_status ("waiting  " + std::to_string (uwait / 1000000));
	   Sleep (1000); // sleep in seconds
	   uwait -= 1000000;
	   if (!rx_state. running.load())
		   break;
	}
	if (uwait > 0)
	   Sleep (uwait / 1000); 	// Sleep in milliseconds
}

void	SDRunoPlugin_wspr::workerFunction	() {
	rx_state. running. store (true);
static
std::complex<float> buffer [SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE];
int	cycleCounter	= 1;
	m_form. show_results ("running");
	rx_state. running. store (true);

	wait_to_start ();
	rx_state. savingSamples. store (true);
//	OK, collecting samples is ON
//
	while (rx_state. running. load ()) {
	   m_form. show_status ("reader is working");
	   while (rx_state. running. load () && 
	              (inputBuffer. GetRingBufferReadAvailable () <
	                                SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE)) { 
	      Sleep (100);	// here: micro seconds
	   }
	   if (!rx_state. running. load ())
	      break;
	   m_form. show_status ("samples read\n");
//	The "reader" probably has already set the flag to false
	   rx_state. savingSamples. store (false);
	   int N = inputBuffer.
	        getDataFromBuffer (buffer, SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE);
//
	   inputBuffer. FlushRingBuffer ();
//	This seems the right moment to honor switchof frequency
	   if (rx_state. frequencyChange. load ()) {
	      rx_options. dialFreq	= newFrequency;
	      dec_options. freq		= newFrequency;
	      m_controller	-> SetVfoFrequency (0, (float)newFrequency);
	      m_controller	-> SetCenterFrequency (0, (float)newFrequency + 1500);
	      rx_state. frequencyChange. store (false);
	      m_form. show_status ("new freq");
	   }
//
	   wait_to_start ();
	   rx_state. savingSamples. store (true);
//
//	and, while the reader callback is reading in the samples for the
//	next 114 to 117 seconds and putting them into the buffer,
//	there is ample time to process the data of the previous cycle
	   m_form. show_results ("buffer " +
	                        std::to_string (cycleCounter));
	   processBuffer (buffer);
	   cycleCounter++;
	}
}

//	filling the buffer takes about 116 seconds, during that
//	period we process the previously filled buffer
void	SDRunoPlugin_wspr::processBuffer (std::complex<float> *b) {
	for (int i = 0; i < SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE; i ++) {
	   samples_i [i]	= real (b [i]);
	   samples_q [i]	= imag (b [i]);
	}

//	Normalize the sample @-3dB 
	float maxSig = 1e-24f;
	for (int i = 0; i < SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE; i++) {
	   float absI = fabs (samples_i [i]);
	   float absQ = fabs (samples_q [i]);

	   if (absI > maxSig)
	      maxSig = absI;
	   if (absQ > maxSig)
	      maxSig = absQ;
	}

	maxSig = 0.5 / maxSig;
	for (int i = 0; i < SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE; i++) {
	   samples_i [i] *= maxSig;
	   samples_q [i] *= maxSig;
	}

	time_t unixtime; 
	time ( &unixtime );
	unixtime = unixtime - 120 + 1;
	rx_state. gtm = gmtime (&unixtime);

	int n_results = 0;
//	Search & decode the signal 
	wspr_decode (&fft,
	             samples_i,
	             samples_q,
	             SIGNAL_LENGTH * SIGNAL_SAMPLE_RATE,
	             dec_options,
	             dec_results,
	             &n_results);
	postSpots	(n_results);
	printSpots	(n_results);
}

void	SDRunoPlugin_wspr::printSpots (uint32_t n_results) {
time_t localtime;
struct tm	*gtm;

	time (&localtime);
	localtime = localtime - 120 + 1;
	gtm = gmtime (&localtime);
	std::string theTime	= std::to_string (gtm -> tm_hour) + "-" +
	                                     std::to_string (gtm -> tm_min);

	if (n_results == 0) {
	   std::string message = "No spots: " + theTime;
	   m_form. addMessage (message);
	   return;
	}
	   
	for (uint32_t i = 0; i < n_results; i++) {
	   std::string currentMessage  = "spot at " + theTime +
	                    std::to_string (dec_results [i]. snr) + "\t" +
		            std::to_string (dec_results [i]. dt) + "\t" +
	                    std::to_string (dec_results [i]. freq) + "\t" +
	                    std::to_string (dec_results [i]. drift) + "\t" +
                            std::string (dec_results[i].call) + "\t" +
                            std::string (dec_results[i].loc) + "\t" +
                            std::string (dec_results[i].pwr);
	   m_form. addMessage (currentMessage);
    }
}

//	Report on WSPRnet 
void	SDRunoPlugin_wspr::postSpots (uint32_t n_results) {
CURL *curl;
CURLcode res;
char url [256];

	if (!rx_options. report) {
//	   LOG(LOG_DEBUG, "Decoder thread -- Skipping the reporting\n");
	   return;
	}

	if (n_results == 0) {
//	No spot to report, stat option used 
//	"Table 'wsprnet_db.activity' doesn't exist" reported on web site...
//	Anyone has doc about this?
	   snprintf (url, sizeof(url) - 1,
	             "http://wsprnet.org/post?function=wsprstat&rcall=%s&rgrid=%s&rqrg=%.6f&tpct=%.2f&tqrg=%.6f&dbm=%d&version=%s&mode=2",
                    dec_options. rcall,
                    dec_options. rloc,
                    rx_options.dialFreq / 1e6,
                    0.0f,
                    rx_options.dialFreq / 1e6,
                    0,
                    wsprnet_app_version);

	   curl = curl_easy_init ();
	   if (curl) {
	      curl_easy_setopt (curl, CURLOPT_URL, url);
	      curl_easy_setopt (curl, CURLOPT_NOBODY, 1);
	      res = curl_easy_perform (curl);

	      if (res != CURLE_OK)
	          fprintf (stderr,
	               "curl_easy_perform() failed: %s\n",
	               curl_easy_strerror (res));

	      curl_easy_cleanup (curl);
	   }
	   return;
	}

	for (uint32_t i = 0; i < n_results; i++) {
	   snprintf (url, sizeof (url) - 1, "http://wsprnet.org/post?function=wspr&rcall=%s&rgrid=%s&rqrg=%.6f&date=%02d%02d%02d&time=%02d%02d&sig=%.0f&dt=%.1f&tqrg=%.6f&tcall=%s&tgrid=%s&dbm=%s&version=%s&mode=2",
                 dec_options.rcall,
                 dec_options.rloc,
                 dec_results[i].freq,
                 rx_state. gtm	-> tm_year - 100,
                 rx_state. gtm	-> tm_mon + 1,
                 rx_state. gtm	-> tm_mday,
                 rx_state. gtm	-> tm_hour,
                 rx_state. gtm	-> tm_min,
                 dec_results[i].snr,
                 dec_results[i].dt,
                 dec_results[i].freq,
                 dec_results[i].call,
                 dec_results[i].loc,
                 dec_results[i].pwr,
                 wsprnet_app_version);

 //       LOG(LOG_DEBUG, "Decoder thread -- Sending spot using this URL: %s\n", url);
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            res = curl_easy_perform(curl);

            if (res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

            curl_easy_cleanup(curl);
        }
    }
}

struct {
	std::string bandName;
	int	dialFreq;
} freqTable [] = {
	{"LF",	136000},
	{"MF",	474200},
	{"160m", 1836600},
	{"80m", 3568600},
	{"60m", 5287200},
	{"40m", 7038600},
	{"30m", 10138700},
	{"20m", 14095600},
	{"17m", 18104600},
	{"15m", 21094600},
	{"12m", 24924600},
	{"10m", 28124600},
	{"6m",  50293000},
	{"4m",  70091000},
	{"2m",  144489000},
	{"1m15", 222280000},
	{"70cm", 432300000},
	{"23cm", 1296500000},
	{"", -1}
};

void	SDRunoPlugin_wspr::set_band (const std::string &s) {
	for (int i = 0; freqTable [i]. dialFreq != -1; i ++) {
	   if (freqTable [i]. bandName == s) {
	      rx_state. frequencyChange. store (true);
	      newFrequency	= freqTable [i]. dialFreq;
	      m_form. show_status ("change waiting");
	      return;
	   }
	}
}

void    SDRunoPlugin_wspr::set_callSign () {
        nana::form fm;
        nana::inputbox::text callsign ("Callsign");
        nana::inputbox::text antenna ("antenna");
        nana::inputbox::text grid ("(maidenhead) grid");
        nana::inputbox inbox (fm, "please fill in");
        if (inbox. show (callsign, grid, antenna)) {
           std::string n = callsign. value ();          //nana::string
           std::string g = grid. value ();              //nana::string
           m_form. show_status (n + "  " + g);
           m_form. store_callSign (n);
           m_form. store_grid     (g);
	   for (int i = 0; i < 13; i ++)
	      dec_options. rcall [i] = 0;
	   for (int i = 0; n. c_str () [i] != 0; i ++)
	      dec_options. rcall [i] = n. c_str () [i];
	   for (int i = 0; i < 7; i ++)
	      dec_options. rloc [i] = 0;
	   for (int i = 0; g. c_str () != 0; i ++)
	      dec_options . rloc [i] = g. c_str () [i];
           return;
        }
}

void	SDRunoPlugin_wspr::set_subtraction (bool b) {
	dec_options. subtraction	= b;
}

void	SDRunoPlugin_wspr::set_quickMode (bool b) {
	dec_options. quickmode	= b;
}

void	SDRunoPlugin_wspr::set_reportMode (bool b) {
	rx_options. report	= b;
}

void	SDRunoPlugin_wspr::handle_reset	() {
	if (!rx_state. running. load ())
	   return;
	rx_state. running. store (false);
	Sleep (20);
	m_worker->join();
	delete m_worker;

	if (rx_state. frequencyChange. load ()) {
 	   rx_options. dialFreq	= newFrequency;
	   dec_options. freq	= newFrequency;
	   m_controller	-> SetVfoFrequency (0, (float)newFrequency);
	   m_controller      -> SetCenterFrequency (0, (float)newFrequency + 1500);
	}
	m_worker =
		new std::thread(&SDRunoPlugin_wspr::workerFunction, this);
	rx_state. running. store (true);
}

